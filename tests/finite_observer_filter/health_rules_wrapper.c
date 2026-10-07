#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../runtime/host/src/health_rules.c"
#include "decls.h"
size_t test_health_lrs(uint32_t* out,size_t cap) {
    const uint32_t fixed[] = {BW_HEALTH_RULES_COLLISION_RETURN,BW_HEALTH_RULES_ITEM_RETURN};
    const size_t n = test_copy_pcs(out,cap,fixed,sizeof fixed/sizeof *fixed);
    return n + test_copy_pcs(out+n,cap-n,kDamageReturns,sizeof kDamageReturns/sizeof *kDamageReturns);
}
size_t test_health_pcs(uint32_t* out,size_t cap) {
    const uint32_t entries[] = {BW_HEALTH_RULES_COLLISION,BW_HEALTH_RULES_DAMAGE,BW_HEALTH_RULES_HEART,BW_HEALTH_RULES_FAIRY};
    const size_t n = test_copy_pcs(out,cap,entries,sizeof entries/sizeof *entries);
    return n + test_health_lrs(out+n,cap-n);
}
