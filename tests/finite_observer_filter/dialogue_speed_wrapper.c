#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../../runtime/host/src/dialogue_speed.c"
#include "decls.h"
void test_dialogue_state(CPUState* c,unsigned m,uint32_t p){(void)p;g_cpu=(m&1)?c:NULL;memset(g_pending,0,sizeof g_pending);memset(g_legacy_pending,0,sizeof g_legacy_pending);g_pending[0].active=(m&2)!=0;g_legacy_pending[0].active=(m&4)!=0;assert(bluewake_dialogue_speed_configure((m&8)?3.0f:1.0f));}

size_t test_dialogue_pcs(uint32_t* out,size_t cap) {
    const uint32_t values[] = { kCharacter, kCharacterReturn, kLegacyString, kLegacyReturn, kLegacyCharacterFirst, kLegacyCharacterNext };
    return test_copy_pcs(out,cap,values,sizeof values/sizeof *values);
}
