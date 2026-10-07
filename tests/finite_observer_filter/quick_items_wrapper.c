#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../../runtime/host/src/quick_items.c"
#include "decls.h"
void test_quick_state(CPUState* c,unsigned m,uint32_t p){(void)c;(void)p;memset(&g_overlay,0,sizeof g_overlay);g_overlay.active=(m&1)!=0;g_pending=(m&2)?TACT:NONE;g_crane_held=(m&4)!=0;g_fire=(m&8)!=0;bluewake_quick_items_configure((m&16)!=0);}

size_t test_quick_pcs(uint32_t* out,size_t cap) {
    const uint32_t values[] = { BLUEWAKE_QUICK_ITEMS_FRAME, BLUEWAKE_QUICK_ITEMS_SHIP_ENTRY, BLUEWAKE_QUICK_ITEMS_SHIP_RETURN, kExecute, kWait, kFreeWait, kMove, kSteer, kPaddle, kCannon, kCrane };
    return test_copy_pcs(out,cap,values,sizeof values/sizeof *values);
}
