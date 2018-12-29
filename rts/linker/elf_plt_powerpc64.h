#pragma once

#include "LinkerInternals.h"

#if defined(OBJFORMAT_ELF)

extern const size_t stubSizePowerPC64;

bool isRelocationOutsideImmediateJumpRange(ObjectCode * oc, unsigned sectionIndex, Elf_Rela * rela);
bool isRelocationForExternalSymbol(ObjectCode * oc, unsigned sectionIndex, Elf_Rela * rela);

bool needStubForRelPowerPC64(ObjectCode * oc, unsigned sectionIndex, Elf_Rel * rel);
bool needStubForRelaPowerPC64(ObjectCode * oc, unsigned sectionIndex, Elf_Rela * rel);
bool makeStubPowerPC64(Stub * s);

#endif
