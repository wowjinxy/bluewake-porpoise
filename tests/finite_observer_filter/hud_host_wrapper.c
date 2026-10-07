#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../../runtime/host/src/hud_host.c"
#include "decls.h"
void test_hud_state(CPUState* c,unsigned m,uint32_t p){(void)p;memset(&host,0,sizeof host);host.runtime.stats.enabled=(m&1)!=0;host.runtime.memory.bytes=(m&2)?c->ram:NULL;host.runtime.actor=(m&4)?0x80010000:0;host.runtime.stats.meter_active=(m&8)!=0;host.runtime.stats.leaf_active=(m&16)!=0;}
