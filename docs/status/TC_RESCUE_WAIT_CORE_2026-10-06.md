# Tingle rescue wait foundation

The cutscene-wait core recognizes six specific WAIT cuts in `TC_RESCUE` for
GZLE01. After the original common cut procedure returns successfully, it can
set the actor's four-byte timer to one. The following original WAIT invocation
still decrements the timer and ends the cut. Dialogue, movement, item awards and
event cleanup retain their original code paths. The six rules save at most 94
WAIT procedure invocations; this is not a measured frame or time reduction.

The core requires an admitted-code, CPU/RAM, CARD, scene, actor and REL owner
binding from the game thread. It validates the completed call, stack, event
tables, cut identity, exact timer value and writable span before changing that
one field. Lifecycle changes and holds discard pending calls. The preference
defaults to off, and UI-facing status contains only copied atomic metadata.

The permanent `bluewake_cutscene_wait_test` target exercises the actual core
with authored issuer, owner, memory and completed-call inputs. Optimized and
AddressSanitizer builds each passed 634 checks. Their eight compile/link/test
roles used the actual CPU layout, closed dependencies and matching runtime
libraries. Independent source and actual-result reviews passed.

The source qualification digest is
`32f1e1ae33ce6680fcbeaa74c63ab8f6b0907f80c9089b73f3d1f5ab4b0453a0`;
the actual result digest is
`30f7c160394a7daddc7b9bcc95b2a7db41995b7a0f328ac0b57055784e1a09ad`.

This commit adds the core and its regression target. The maintainer deferred
the optional speedup on October 6. Host ownership, menu integration, genuine
rescue runs, original awards and cleanup, optimized replay, save recovery and
graphical-session support remain pending. Existing work and diagnostics are
preserved. The shipping game and tester leave this feature disabled; it is not
a prerequisite for core port, gameplay, other milestones or tester builds.
