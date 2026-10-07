#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../../runtime/host/src/enhancement_hooks.c"
#include "decls.h"
void test_enhancement_state(CPUState* c,unsigned m,uint32_t p){(void)p;g_cpu=(m&1)?c:NULL;g_wind.active=(m&2)!=0;g_boots.active=(m&4)!=0;bluewake_enhancement_faster_wind((m&8)!=0);bluewake_enhancement_faster_boots((m&16)!=0);}

size_t test_enhancement_pcs(uint32_t* out,size_t cap) {
    const uint32_t values[] = { BLUEWAKE_ENHANCEMENT_WIND_COMMIT, BLUEWAKE_ENHANCEMENT_WIND_RETURN, BLUEWAKE_ENHANCEMENT_BOOTS_ANIMATION, BLUEWAKE_ENHANCEMENT_BOOTS_RETURN };
    return test_copy_pcs(out,cap,values,sizeof values/sizeof *values);
}

size_t test_enhancement_lrs(uint32_t* out,size_t cap) {
    const uint32_t values[] = {BLUEWAKE_ENHANCEMENT_WIND_RETURN,BLUEWAKE_ENHANCEMENT_BOOTS_RETURN};
    return test_copy_pcs(out,cap,values,sizeof values/sizeof *values);
}
