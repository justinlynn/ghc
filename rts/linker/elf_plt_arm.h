#pragma once

#include "LinkerInternals.h"

#if defined(OBJFORMAT_ELF)

extern const size_t stubSizeArm;
bool needStubForRelArm(ObjectCode * oc, unsigned sectionIndex, Elf_Rel * rel);
bool needStubForRelaArm(ObjectCode * oc, unsigned sectionIndex, Elf_Rela * rel);
bool makeStubArm(Stub * s);

#endif
