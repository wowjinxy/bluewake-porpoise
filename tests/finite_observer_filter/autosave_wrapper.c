#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include "../../runtime/host/src/autosave.c"
#include "decls.h"
void test_autosave_state(CPUState* c,unsigned m,uint32_t p){(void)c;(void)p;bluewake_autosave_configure((m&1)!=0,300);atomic_store_explicit(&public_phase,m%BW_AUTOSAVE_QUARANTINED,memory_order_release);}

size_t test_autosave_pcs(uint32_t* out,size_t cap) {
    const uint32_t values[] = { SERIALIZE, POLL, FPU_REOWN, BLUEWAKE_AUTOSAVE_RETURN, BLUEWAKE_AUTOSAVE_FRAME };
    return test_copy_pcs(out,cap,values,sizeof values/sizeof *values);
}
