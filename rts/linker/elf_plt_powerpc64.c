#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "elf_compat.h"
#include "ghcplatform.h"

#if defined(powerpc64_HOST_ARCH) || defined(powerpc64le_HOST_ARCH)

#include "Elf.h"
#include "elf_plt.h"
#include "elf_util.h"
#include "util.h"

#if defined(OBJFORMAT_ELF)

__attribute__((noreturn))
bool needStubForRelPowerPC64 (ObjectCode * oc __attribute__((unused)),
                              unsigned sectionIndex __attribute__((unused)),
                              Elf_Rel * rel __attribute__((unused)))
{
    abort(/* The OpenPOWER ELF v2 ABI uses Rela Entries Exclusively. See OpenPOWER PPC64 ELF v2 ABI, Section 3.5 - Relocation Types. */);
}

bool isRelocationForExternalSymbol(ObjectCode * oc,
                                   unsigned sectionIndex,
                                   Elf_Rela * rela)
{
    Section section = oc->sections[sectionIndex];
    ElfSymbol *symbol = findSymbol(oc,
                                   section.info->sectionHeader->sh_link,
                                   ELF64_R_SYM(rela->r_info));

    bool symbolHasName = !strcmp(symbol->name, "(noname)");
    bool symbolTypeUnknown = STT_NOTYPE == ELF64_R_TYPE(symbol->elf_sym->st_info);

    return (symbolHasName || symbolTypeUnknown);
}

/*
 * Per OpenPOWER ELFv2 ABI version 1.1 page 83-84, Immediate Relative Branch Instructions may address Symbols within
 * a 64 Megabyte signed range (+/- 32 Megabytes) relative to the current location. If the relocation is outside of
 * this range a jump island/stub must be inserted for indirect access.
 */
bool isRelocationOutsideImmediateJumpRange(ObjectCode * oc,
                                           unsigned sectionIndex,
                                           Elf_Rela * rela)
{
    Section section = oc->sections[sectionIndex];

    uint64_t relocationOffsetTarget = *(((uint64_t *)section.start) + rela->r_offset);
    uint64_t relocationAddendTarget = *(((uint64_t *)section.start) + rela->r_addend);
    int64_t relocationDelta = relocationOffsetTarget - relocationAddendTarget;

    return (signExtend64(26, relocationDelta) != relocationDelta);
}

bool needStubForRelaPowerPC64(ObjectCode * oc,
                              unsigned sectionIndex,
                              Elf_Rela * rela)
{
    if(ELF64_R_TYPE(rela->r_info) == COMPAT_R_PPC64_REL24) {

        bool isExternalSymbol = isRelocationForExternalSymbol(oc, sectionIndex, rela);
        bool isOutsideValidImmediateJumpRange = isRelocationOutsideImmediateJumpRange(oc, sectionIndex, rela);

        return  (isExternalSymbol || isOutsideValidImmediateJumpRange);
    }

    return false;
}

/* NOTE: regarding endian neutrality in instruction encoding
 *
 * Because we 'chunk' the memory at s->addr into uint32_t's, endianness is handled for us. We insert opcodes in
 * register order - big endian - and the language unsigned integer abstraction handles in-memory byte ordering for
 * us automatically. See the uint32_t *P definition in makeStubPowerPC64.
 */
#define lis_r12_opcode         0x3D800000
#define ori_r12_r12_opcode     0x618C0000
#define sldi_r12_r12_32_opcode 0x798C07C6
#define oris_r12_r12_opcode    0x658C0000
#define std_r2_24_r1_opcode    0xF8410018
#define mtctr_r12_opcode       0x7D8903A6
#define bctr_opcode            0x4E800420

#define retargetInstr(instr, target) ((instr & 0xFFFF0000) | (target & 0x0000FFFF))
#define lis_r12(target)      retargetInstr(lis_r12_opcode, target)
#define ori_r12_r12(target)  retargetInstr(ori_r12_r12_opcode, target)
#define sldi_r12_r12_32      sldi_r12_r12_32_opcode
#define oris_r12_r12(target) retargetInstr(oris_r12_r12_opcode, target)
#define std_r2_24_r1         std_r2_24_r1_opcode
#define mtctr_r12            mtctr_r12_opcode
#define bctr                 bctr_opcode

bool makeStubPowerPC64(Stub * s)
{
    uint16_t targetHighestAddr = ((uint64_t)s->target >> 48) & 0xFFFF;
    uint16_t targetHigherAddr  = ((uint64_t)s->target >> 32) & 0xFFFF;
    uint16_t targetHighAddr    = ((uint64_t)s->target >> 16) & 0xFFFF;
    uint16_t targetLowAddr     = ((uint64_t)s->target >> 00) & 0xFFFF;

    uint32_t *P = (uint32_t*)s->addr;

    /* P Jump Island Program Overview:
     *   - load the target address into r12, per ELFv2 ABI, using 16-bit Immediate Values in the Jump Island/Stub
     *   - store a reference to the TOC base into the TOC Save Slot per
     *   - store r12 into the count register
     *   - branch to that count register
     */

    P[0] = lis_r12(targetHighestAddr); // alias for (addis r12, 0, targetHighestAddr); PowerISA 3.0B page 67
    // concats 16 bits of zero to target highest address and loads it into r12, sign extended
    // after P[0] -> r12: UUUU UUUU (highest) 0000; U = unknown
    P[1] = ori_r12_r12(targetHigherAddr); // PowerISA 3.0B page 92
    // logical OR of r12 w/ immediate value targetHigherAddr, stored to r12
    // after P[1] -> r12: 0000 0000 (highest) (higher)
    P[2] = sldi_r12_r12_32; // alias for (rldicr r12, r12, 32, 63-32); PowerISA 3.0B page 104
    // shift the highest and higher values stored in r12 32 bits setting the lower 32 bits to zero
    // after P[2] -> r12: (highest) (higher) 0000 0000
    P[3] = oris_r12_r12(targetHighAddr); // PowerISA 3.0B page 93
    // take 32 bits of zero + targetHighAddr + 16 bits of zero, logically or w/ r12, store into r12
    // after P[3] -> r12: (highest) (higher) (high) 0000
    P[4] = ori_r12_r12(targetLowAddr); // PowerISA 3.0B page 92
    // logical OR of r12 w/ immediate value targetLowAddr, stored to r12
    // after P[4] -> r12: (highest) (higher) (high) (low)
    P[5] = std_r2_24_r1; // PowerISA 3.0B page 57
    // store into memory, at address referenced by (r1) w/ offset 24, the value in r2
    // per OpenPOWER ELF v2 ABI, the value in r2, at the point this stub is called, references the Table of Contents
    //   section base address. The 24 byte offset refers to the the TOC Save Slot per OpenPOWER ELF v2 ABI (v1.1) page 83
    //   paragraph 3.
    P[6] = mtctr_r12;
    P[7] = bctr;
    // Go, Speed Racer, Go!

    /* !!!! WARNING !!!!
     *   If the length of the P[n] instruction sequence is changed, the instruction count referenced by
     *     stubSizePowerPC64 MUST be modified to match the new instruction count.
     *
     *   The instruction count returned by stubSizePowerPC64 is used elsewhere to allocate the
     *     correct amount of memory into which we'll be JITing these instructions.
     *
     *   Fail to do so at your own risk - Segfaults and Memory Corruption Await those who risk it.
     */

    return EXIT_SUCCESS;
}

const size_t stubSizePowerPC64 = 8 /* length of P, from makeStubPowerPC64(...), instructions */ \
                               * 4 /*bytes per instruction, length fixed by PowerPC ISA */;
#endif
#endif