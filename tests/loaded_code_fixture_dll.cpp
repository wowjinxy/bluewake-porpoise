// SPDX-License-Identifier: GPL-3.0-or-later
// Synthetic Windows loader fixture only. No DllMain, game or device API.
#include "StaticRecompABI.h"
#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
namespace {
int dispatch(CPUState*,u32){return 0;}
const StaticRecompModuleDesc descriptor={STATICRECOMP_ABI_VERSION,GXRUNTIME_CPU_ABI_VERSION,sizeof(CPUState),
    {'S','Y','N','T','H',0,0,0},0,dispatch,nullptr,nullptr,0,nullptr,0,nullptr,0,nullptr,nullptr,0};
}
extern "C" EXPORT const StaticRecompModuleDesc* staticrecomp_get_module(){return &descriptor;}
