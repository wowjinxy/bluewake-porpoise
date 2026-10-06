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

Qualification currently covers source compilation and build wiring. The
final C++17 adapter passed optimized and sanitizer-enabled compile checks,
each with 281 audited compiler dependencies. Its unchanged 17-function C ABI
also passed both compile configurations. Final main passed optimized builds
with the option disabled and enabled; the disabled object references none of
the reward API, and the enabled object references all 17 functions. An explicit
runtime request in a disabled build fails before module or CARD opening.
Eight configure-only cases verify the default, required platform/compiler/
admission guards, exact source graph and idempotent wiring. Authored lifetime/
save fixtures, an actual linked host, genuine replacement/award/manual-save
and a fresh CARD reload remain separate acceptance steps. No playable randomizer,
full item pool, starting inventory, tracker, entrance shuffle or beatability
claim follows from this integration.
