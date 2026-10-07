#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../runtime/host/src/hud_customization.c"
#include "decls.h"

size_t test_hud_pcs(uint32_t* out,size_t cap) {
    const uint32_t values[] = { BW_HUD_METER_CAPTURE, BW_HUD_METER_DELETE, BW_HUD_METER1_DRAW, BW_HUD_METER2_DRAW, BW_HUD_METER_RETURN, BW_HUD_MY_PICTURE_DRAW, BW_HUD_PICTURE_DRAW, BW_HUD_TEXT_DRAW, BW_HUD_LEAF_RETURN };
    return test_copy_pcs(out,cap,values,sizeof values/sizeof *values);
}
