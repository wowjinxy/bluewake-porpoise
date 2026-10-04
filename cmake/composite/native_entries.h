#ifndef BLUEWAKE_NATIVE_ENTRIES_H
#define BLUEWAKE_NATIVE_ENTRIES_H

#include "core/cpu.h"

/* The second certified entry batch. A host must explicitly approve skipped
 * observations through this versioned, read-only predicate. Old hosts and a
 * NULL predicate leave every translated fallback intact. The predicate must
 * not depend on memory/registers changed inside an approved routine, except
 * for the explicitly reconstructed register-save/restore probes. */
typedef bool (*BluewakeNativeEntriesReady)(void*, const CPUState*, u32);
int bluewake_composite_native_entries_v1(bool enabled, BluewakeNativeEntriesReady ready, void* user);
int bluewake_native_entries_try(CPUState* cpu, u32 address);
bool bluewake_native_entries_ready(const CPUState* cpu, u32 address);
void bluewake_native_entries_report(void);

#endif
