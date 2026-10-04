/* Certified function entries and explicit name-search resume leaders only. */
#include "native_entries.h"
#include "native_fifo.h"
#include "native_bg.h"
#include "native_vec.h"
#include "native_mtxcalc.h"
#include "native_search.h"

#if defined(_WIN32)
#define BW_ENTRIES_EXPORT __declspec(dllexport)
#else
#define BW_ENTRIES_EXPORT __attribute__((visibility("default")))
#endif

static BluewakeNativeEntriesReady s_ready;
static void* s_user;

BW_ENTRIES_EXPORT int bluewake_composite_native_entries_v1(
    bool enabled, BluewakeNativeEntriesReady ready, void* user) {
    s_ready = enabled ? ready : NULL;
    s_user = s_ready != NULL ? user : NULL;
    return s_ready != NULL;
}

bool bluewake_native_entries_ready(const CPUState* cpu, u32 address) {
    return cpu != NULL && s_ready != NULL && s_ready(s_user, cpu, address);
}

int bluewake_native_entries_try(CPUState* cpu, u32 address) {
    if (!bluewake_native_entries_ready(cpu, address)) return 0;
    switch (address) {
    case BLUEWAKE_J3D_FIFO_POS_MTX:
    case BLUEWAKE_J3D_FIFO_NRM_MTX:
    case BLUEWAKE_J3D_FIFO_NRM_MTX33:
        return bluewake_native_fifo(cpu, address);
    case BLUEWAKE_BG_CHK_SAME_ACTOR_PID:
    case BLUEWAKE_BG_CHK_GRP_THROUGH:
        return bluewake_native_bg(cpu, address);
    case BLUEWAKE_PSMTX_MULT_VEC_SR:
        return bluewake_native_vec_sr(cpu);
    case BLUEWAKE_MTXCALC_BASIC:
    case BLUEWAKE_MTXCALC_SOFTIMAGE:
    case BLUEWAKE_MTXCALC_MAYA:
        return bluewake_native_mtxcalc(cpu, address);
    case BLUEWAKE_SEARCH_STRCMP:
    case BLUEWAKE_SEARCH_STAGE_NAME:
    case BLUEWAKE_SEARCH_NAME_LOOP:
    case BLUEWAKE_SEARCH_NAME_RESULT:
    case BLUEWAKE_SEARCH_NAME_STEP:
    case BLUEWAKE_SEARCH_JUDGE_FILTER:
        return bluewake_native_search(cpu, address);
    default: return 0;
    }
}

BW_ENTRIES_EXPORT void bluewake_native_entries_report(void) {
    bluewake_native_fifo_report();
    bluewake_native_bg_report();
    bluewake_native_vec_sr_report();
    bluewake_native_mtxcalc_report();
    bluewake_native_search_report();
}
