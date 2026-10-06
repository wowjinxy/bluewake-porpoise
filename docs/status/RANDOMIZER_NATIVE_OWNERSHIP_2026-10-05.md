# Native randomizer ownership queries

Two read-only C helpers now identify the actors and loaded modules needed by
the first LinkUG reward adapter. They are source regression targets and are
not linked into the shipping host or included in the current tester ZIP.

The actor query traverses the bounded native actor queue, checks its owning
queue pointer, count, tail, backlinks and unique membership, then validates
the target's PID, type, profile, methods and lifecycle. It accepts an active
Link or chest, and a Demo_Item during its Delete callback. Demo_Item still
belongs to the actor queue at that boundary, although its separate delete tag
has already been removed. Results are copied before another resolver call,
so a resolver with ephemeral backing does not leak a stale pointer.

The module query validates the two audited REL identities, native OS module
list, live loader slot, bounded sections, unique executable mapping and fixed
compiled profile/method backing. It rejects conflicting live slots, missing
materialization tokens and partially shadowed mappings. It does not execute
guest functions or change memory.

Optimized and AddressSanitizer fixtures each pass 8,641 actor checks and 577
REL checks. The actor fixture covers the 1,024-entry bound, malformed queue
links, stale PIDs, profile and lifecycle mismatches, resolver failure and
ephemeral backing. The REL fixture covers list, slot, mapping, section and
profile corruption. Both runs use synthetic memory and copied descriptors.
The initial actor fixture failed because its own pointer arithmetic used
signed enum addresses; the corrected fixture uses bounded unsigned offsets.
The retained failure did not require a change to the helper.

These queries establish immediate copied facts, not lifetime authorization.
The native adapter must independently establish and recheck live CPU, RAM,
loaded-code and alias ownership, issue monotonic actor/materialization tokens
at genuine lifecycle boundaries, and revoke them on replacement or reload.
Actual reward replacement and native CARD/save-ledger qualification remain
pending.
