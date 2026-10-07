#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../../runtime/host/src/health_host.c"
#include "decls.h"
void test_health_state(CPUState* c,unsigned m,uint32_t p){memset(&host,0,sizeof host);host.attached=(m&1)!=0;host.module_ok=(m&2)!=0;host.bound=(m&4)!=0;host.suspended=(m&8)!=0;host.saving=(m&16)!=0;host.cpu=c;host.ram=c->ram;host.runtime.cpu=c;host.runtime.ram=c->ram;host.runtime.ram_size=c->ram_size;host.runtime.abi=BW_HEALTH_RULES_ABI_GZLE01;host.runtime.config=(BwHealthRulesConfig){(m&32)?512:256,(m&64)?512:256};host.runtime.pending[0].active=(m&128)!=0;host.runtime.pending[0].return_pc=p;host.runtime.pending[0].kind=(m&256)?1:2;}
