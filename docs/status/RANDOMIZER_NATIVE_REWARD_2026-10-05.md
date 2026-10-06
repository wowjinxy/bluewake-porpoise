# Experimental native LinkUG reward session

The first reward adapter is wired behind `BLUEWAKE_NATIVE_REWARD_SESSION`,
which defaults to OFF. It currently requires a Windows Clang/GNU host with
the existing admitted-code owner enabled. Runtime startup also requires
explicit headless mode, disabled live input and dialogs, a canonical
single-location profile, a dedicated seed directory and a compatible
confirmed CARD. It refuses concurrent inventory capture and STATE recovery.
The shipping tester does not include this adapter.

The adapter supports the audited chest beneath Link's house and the Orange
Rupee/basic Picto Box reward codecs. Only an accepted native creation changes
the item argument. The real creation return, deleting item owner, void award
return and inventory postcondition must agree before the ledger records a
completed reward. Other chest contexts are outside this experiment.

Main supplies actual shared-data commitments, raw REL materialization and
loader association, plus copied current slot/alias tables. Observation runs
before feature hooks at serviced edges and the first dispatch PC; dynamic
direct calls cannot skip a watched entry or pending return. Runtime ownership
tokens stay outside CARD, ledger and STATE data. Cleanup revokes the reward
owner before closing its native backend and freeing CPU/module storage.

Loading requires a genuine controller read, the native selected-quest load
and its correlated GameEvents reset. After main consumes that return, it
yields at the same PC and checks the locked confirmed CARD before allowing
guest continuation. A missing reset fails instead of waiting indefinitely.

Manual saving binds the genuine serializer, STORE and SaveSync calls and
copies the game's checksummed buffer. Ordinary quit and run limits wait for
an active native save to drain. Feature hooks and machine-state operations
remain held during that drain; scene callbacks discard borrowed enhancement
baselines and reattach after the held boundary clears. Native completion
yields before guest continuation so maintenance can validate both save
copies, preserve other quests/photos/unrelated files, and publish one
CARD/ledger generation through the Session's confirmed receipt. A rejected
proof retains the previous confirmed pair. Owned autosave publication is
still unqualified; the adapter waits for its genuine pipeline to finish.
Fatal loss of native continuation is reported separately and never counted
as a successful save or completed drain.

Qualification covers source compilation, build wiring, an initial native
CARD load, two unsaved native reward runs, one genuine manual save and a
separate native reload of its confirmed CARD/ledger pair. The
final C++17 adapter passed optimized and sanitizer-enabled compile checks,
each with 281 audited compiler dependencies. Its unchanged 17-function C ABI
also passed both compile configurations. Final main passed optimized builds
with the option disabled and enabled; the disabled object references none of
the reward API, and the enabled object references all 17 functions. An explicit
runtime request in a disabled build fails before module or CARD opening.
Eight configure-only cases verify the default, required platform/compiler/
admission guards, exact source graph and idempotent wiring. The authored
adapter fixture passes 8,181 checks in each optimized and sanitizer build,
using real core, storage, payload and alias code with explicit host stand-ins.
It covers sole-argument replacement, replay, stale owners, manual-save draining,
publication failures and confirmed-pair recovery. It executes no native module
or helper and grants no native admission or save-completion authority.

The isolated Windows host also passes all 17 compiler steps and its actual
link. All 3,444 compiler dependency mentions resolve to audited copies; all
184 linker inputs and the ordered 78 objects are accounted for. The embedded
manifest matches the shipping host, and added imports resolve in the matching
CRT or current system exports.

A first hidden native seed session also passes all 15 load-only gates after
3,300 retraces. Original controller input loads the copied CARD through the
game's normal path, emits one genuine load reset and returns to the expected
ready player/scene in the same epoch. The session confirms generation one
and closes cleanly. Its working CARD and paired CARD remain byte-identical
to the initial copy, and the ledger contains no rewards. Original inputs are
preserved. This case uses no machine state, warp, inventory capture, reward
or save. Its logic and start-policy digests identify explicit experimental
metadata; they do not establish starting inventory or beatability.

Two further hidden runs each pass all 17 reward gates after 4,000 retraces.
The same-item Orange control completes one selection and one native award.
The basic Picto replacement then changes the accepted creation argument to
0x23 (35 decimal), completes the native deleting-item owner and confirms the
Picto inventory postcondition. Each run emits exactly one native ItemAward
from the audited LinkUG return in the genuine CARD-load epoch. The void award
return value is not a success signal. Both sessions report one substitution,
one completed award, zero saves and a clean stop. Their working CARDs and
stored generation-one pairs remain unchanged, with empty stored ledgers.
Only original controller input, the explicitly reviewed NextStage scene
request and the accepted reward-argument substitution drive these runs.

The new permanent regression target links the complete GXRuntime core. Its
first optimized run stopped during seed-store creation in a deeply nested
fixture directory. With shorter owned
data paths, the unchanged optimized executable and a fresh sanitizer build
each pass all 8,181 checks with empty stderr and no native module/helper
invocations. Each build compiles 46 translation units, including all 34 core
units; all 46 sanitizer objects are instrumented. Actual compiler dependencies,
archive members and linker reproductions are closed before fixture execution.
Seven configure cases check caller guards, source/standard metadata and the
missing-core rejection. The new target is included under the existing Windows
regression and runtime-host testing guards. The original path failure and its
files are retained; Windows long-path publication remains unqualified.

A further hidden Picto run completes one genuine manual save through the
game's pause/save menu. It passes all 25 native log gates, including the
serializer, successful SaveSync return and one published generation, and
stops normally after 16,000 retraces. A separately qualified diagnostic host
delays the original controller save route until the reward finishes and
retires its scripted input after confirmed publication. These diagnostic
changes are not promoted to production main. The stored generation-two pair
contains exactly one completed Picto reward. Its CARD has the native Picto
inventory state, opened chest and incremented save count; prior equipment,
songs, other quests, photo bytes and unrelated records are preserved.

The first offline audit of that successful native save failed because it
read the song byte at the live-memory offset instead of the packed CARD
offset. That failed result remains unchanged. A separate read-only audit
corrects only those two offset literals and accepts the same saved files;
the native save is not rerun. Independent byte and raw-log audits agree.

A fresh hidden process using the normal qualified host then restores the
saved pair and passes all 18 reload log gates after 3,300 retraces. A genuine
CARD load and completed ready-player return precede generation-two
confirmation. The host compares the loaded quest, Picto slot, obtained byte
and chest state with the locked saved CARD before confirming readiness.
No new selection, award or save occurs. The restored working CARD and both
stored generations remain exact, with one reward record in the current
ledger. Cleanup follows the summary and normal stop. All 490 frozen inputs
remain unchanged, and an independent audit passes 74 byte checks plus its
18 log checks. Neither diagnostic audit mints a native save receipt.

Native reward replay/cancellation, other locations and broader save cases
remain acceptance steps. No playable randomizer, full item pool, starting
inventory, tracker, entrance shuffle or beatability claim follows from
this integration.
