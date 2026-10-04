#ifndef BLUEWAKE_NATIVE_SEARCH_H
#define BLUEWAKE_NATIVE_SEARCH_H

/* The actor search by name natively (native_search.c): strcmp,
 * dStage_searchName and cTgIt_JudgeFilter with fopAcM_findObjectCB as its
 * judge, hooked at their entries, dStage_searchName also at its loop's
 * block leaders. All hooks require the native_entries_v1 host handshake. */

#include "core/cpu.h"

#define BLUEWAKE_SEARCH_STRCMP 0x8032DB44u      /* strcmp */
#define BLUEWAKE_SEARCH_STAGE_NAME 0x80041544u  /* dStage_searchName__FPCc */
#define BLUEWAKE_SEARCH_JUDGE_FILTER 0x80245640u /* cTgIt_JudgeFilter__FP16create_tag_classP12judge_filter */
/* dStage_searchName's loop, where a search the native stopped for the
 * window, or the translation began, goes on natively: its call block, a
 * strcmp's return (the result test) and its step. */
#define BLUEWAKE_SEARCH_NAME_LOOP 0x8004156Cu
#define BLUEWAKE_SEARCH_NAME_RESULT 0x80041578u
#define BLUEWAKE_SEARCH_NAME_STEP 0x80041588u

/* The function at `address`, entered with the return address in LR, through
 * its blr - or, for dStage_searchName (from its entry or a leader of its
 * loop), as far as the window holds, to a strcmp's return: nonzero with pc,
 * every register, flag, cycle and byte as the translation leaves them there;
 * zero, with nothing changed, where that is not certain or the address is
 * not one of these. No identifier here may be `ctx`. */
int bluewake_native_search(CPUState* cpu, u32 address);
void bluewake_native_search_report(void);

/* The donor's unversioned batched host walk is omitted. The existing host
 * actor-ID optimization owns that watched boundary; a name walk requires a
 * separate observation contract and routed qualification before exposure. */

#endif
