#include <stdlib.h>
#include <assert.h>
#include "elf_compat.h"
#include "elf_reloc_powerpc64.h"
#include "util.h"
#include "elf_util.h"
#include "elf_plt.h"

#if (defined(powerpc64_HOST_ARCH) || defined(powerpc64le_HOST_ARCH)) \
 && defined(OBJFORMAT_ELF)

typedef uint64_t addr_t;
typedef uint32_t inst_t;

#define applyPPClo(x)       (x & 0xffff)
#define applyPPCoffset(x)   (x + 0x8000)
#define applyPPChi(x)       applyPPClo(x >> 16)
#define applyPPCha(x)       applyPPChi(applyPPCoffset(x))
#define applyPPChigher(x)   applyPPClo(x >> 32)
#define applyPPChighera(x)  applyPPChigher(applyPPCoffset(x))
#define applyPPChighest(x)  applyPPClo(x >> 48)
#define applyPPChighesta(x) applyPPChighest(applyPPCoffset(x))

/**
 * Compute the *new* addend for a relocation, given a pre-existing addend.
 * @param section The section the relocation is in.
 * @param rel     The Relocation struct.
 * @param symbol  The target symbol.
 * @param addend  The existing addend. Either explicit or implicit.
 * @return The new computed addend.
 */
static addr_t
computeAddend(Section * section __attribute__((unused)),
              Elf_Rel * rel __attribute__((unused)),
              ElfSymbol * symbol __attribute__((unused)),
              int64_t addend);

static addr_t
computeAddend(Section * section __attribute__((unused)),
              Elf_Rel * rel __attribute__((unused)),
              ElfSymbol * symbol __attribute__((unused)),
              int64_t addend)
{
    return addend; // TODO: implement
}

int64_t
decodeAddendPowerPC64(Section * section __attribute__((unused)),
                      Elf_Rel * rel __attribute__((unused)));

int64_t
decodeAddendPowerPC64(Section * section __attribute__((unused)),
                      Elf_Rel * rel __attribute__((unused)))
{
    return 0; // TODO: implement
}

bool
encodeAddendPowerPC64(Section * section __attribute__((unused)),
                      Elf_Rel * rel __attribute__((unused)),
                      int64_t addend __attribute__((unused)));

bool
encodeAddendPowerPC64(Section * section __attribute__((unused)),
                      Elf_Rel * rel __attribute__((unused)),
                      int64_t addend __attribute__((unused)))
{
    return EXIT_SUCCESS;
}

/* Definitions below from OpenPOWER ELF v2 ABI for PowerPC64 (version 1.1)
 * `A`: Represents the addend used to compute the value of the relocatable field.
 * `B`: Represents the base address at which a shared object file has been loaded into
 *      memory during execution. Generally, a shared object file is built with a 0 base
 *      virtual address, but the execution address will be different. See Program Header in
 *      the System V ABI for more information about the base address.
 * `G`: Represents the offset from .TOC. at which the address of the relocation entry’s
 *      symbol resides during execution. This implies the creation of a .got section. For
 *      more information, see Section 2.3 Coding Examples on page 66 and Section 4.2.3
 *      Global Offset Table on page 135.
 * `L`: Represents the section offset or address of the procedure linkage table entry for
 *      the symbol. This implies the creation of a .plt section if one does not already exist.
 *      It also implies the creation of a procedure linkage table (PLT) entry for resolving the
 *      symbol. For an unresolved symbol, the PLT entry points to a PLT resolver stub. For
 *      a resolved symbol, a procedure linkage table entry holds the final effective address
 *      of a dynamically resolved symbol (see Section 4.2.5 Procedure Linkage Table on
 *      page 137).
 * `M`: Similar to G, except that the address that is stored may be the address of the
 *      procedure linkage table entry for the symbol.
 * `P`: Represents the place (section offset or address) of the storage unit being relocated
 *      (computed using r_offset).
 * `R`: Represents the offset of the symbol within the section in which the symbol is
 *      defined (its section-relative address).
 * `S`: Represents the value of the symbol whose index resides in the relocation entry.
 */

bool
relocateObjectCodePowerPC64(ObjectCode * oc) {
    for(ElfRelocationATable *relaTab = oc->info->relaTable;
        relaTab != NULL; relaTab = relaTab->next) {
        /* only relocate interesting sections */
        if (SECTIONKIND_OTHER == oc->sections[relaTab->targetSectionIndex].kind)
            continue;

        Section *targetSection = &oc->sections[relaTab->targetSectionIndex];

        for(unsigned i=0; i < relaTab->n_relocations; i++) {

            Elf_Rela *rel = &relaTab->relocations[i];

            ElfSymbol *symbol =
                    findSymbol(oc,
                               relaTab->sectionHeader->sh_link,
                               ELF64_R_SYM((Elf64_Xword)rel->r_info));

            assert(0x0 != symbol);

            /* take explicit addend */
            int64_t addend = rel->r_addend;

            addend = computeAddend(targetSection, (Elf_Rel*)rel,
                                   symbol, addend);
            encodeAddendPowerPC64(targetSection, (Elf_Rel*)rel, addend);
        }
    }
    return EXIT_SUCCESS;
}

#endif