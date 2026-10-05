/* Standalone protocol tools/tests have no guest memory runtime. Game host links
 * the actual GXRuntime journal globals instead; never include this in host. */
#include "core/cpu.h"
PPCMemWriteJournal g_mem_write_journal = 0;
void* g_mem_write_journal_user = 0;
bool g_ppc_guest_aliases_overlap_mem1 = false;
bool ppc_guest_alias_resolve(u32 address,u32 size,u8** pointer,u32* journal_offset){
    (void)address;(void)size;(void)pointer;(void)journal_offset;return false;
}
