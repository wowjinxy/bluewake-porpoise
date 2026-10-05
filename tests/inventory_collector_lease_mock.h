// Synthetic bridge fixture only; never part of host source list.
#pragma once
#include "loaded_code_admission.h"
#ifdef __cplusplus
extern "C" {
#endif
BwIcLoadedCode* bw_ic_fixture_lease(const StaticRecompModuleDesc*,uint64_t*);
#ifdef __cplusplus
}
#endif
