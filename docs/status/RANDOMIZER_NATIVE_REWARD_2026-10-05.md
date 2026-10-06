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

Qualification covers source compilation, build wiring and an initial native
CARD load. The
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

Genuine replacement/award/manual-save and a fresh CARD reload remain separate
acceptance steps. No playable randomizer, full item pool, starting inventory,
tracker, entrance shuffle or beatability claim follows from this integration.
