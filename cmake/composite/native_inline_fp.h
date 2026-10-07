#ifndef BLUEWAKE_COMPOSITE_NATIVE_INLINE_FP_H
#define BLUEWAKE_COMPOSITE_NATIVE_INLINE_FP_H

/* Native leaves use the arithmetic helpers without a generated load helper.
 * Keep the target-wide load option for translated chunks, whose required order
 * remains gather_pipe.h, generated.h, inline_fp.h. */
#if defined(RECOMP_COMPOSITE_H) || defined(dolrecomp_f32_from_bits)
#error "native_inline_fp.h is only for native arithmetic translation units"
#endif
#pragma push_macro("BW_F32_LOAD_HW_WIDEN")
#undef BW_F32_LOAD_HW_WIDEN
#include "inline_fp.h"
#pragma pop_macro("BW_F32_LOAD_HW_WIDEN")

#endif
