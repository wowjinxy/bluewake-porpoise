// SPDX-License-Identifier: GPL-3.0-or-later
// Experimental loaded-code admission. No private artifact digest is embedded here.
#pragma once
#include "StaticRecompABI.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#define BW_IC_CODE_NOEXCEPT noexcept
extern "C" {
#else
#define BW_IC_CODE_NOEXCEPT
#endif
typedef struct BwIcLoadedCode BwIcLoadedCode;
// approved_policy_sha256 is from an owner-approved private compile input,
// never selected at runtime from the policy file or an environment variable.
BwIcLoadedCode* bw_ic_code_prepare(const char* module_utf8,const char* policy_utf8,
    const char* approved_policy_sha256, uint64_t* issued_generation) BW_IC_CODE_NOEXCEPT;
const char* bw_ic_code_load_path(const BwIcLoadedCode*,uint64_t) BW_IC_CODE_NOEXCEPT;
bool bw_ic_code_bind(BwIcLoadedCode*,uint64_t,void* actual_lib,const void* actual_getter) BW_IC_CODE_NOEXCEPT;
bool bw_ic_code_bind_descriptor(BwIcLoadedCode*,uint64_t,const StaticRecompModuleDesc*) BW_IC_CODE_NOEXCEPT;
uint64_t bw_ic_code_generation(const BwIcLoadedCode*) BW_IC_CODE_NOEXCEPT;
// Cached game-thread lease identity only. NO callback code/page/file rehash.
bool bw_ic_code_is_live(const BwIcLoadedCode*,uint64_t,const StaticRecompModuleDesc*) BW_IC_CODE_NOEXCEPT;
void bw_ic_code_revoke(BwIcLoadedCode*,uint64_t) BW_IC_CODE_NOEXCEPT;
void bw_ic_code_destroy(BwIcLoadedCode*,uint64_t) BW_IC_CODE_NOEXCEPT;
#ifdef __cplusplus
}
#endif
