# Randomizer seed profiles and reward ledger

The new C++17 library builds reproducible profiles from the imported Wind Waker
catalog. A profile includes the catalog digest, seed, explicit options, logic
and starting-policy identity, and location-to-item placements. Its bounded,
canonical encoding has a SHA-256 identity. Decoding rejects catalog changes,
unknown records, duplicate placements, malformed options and noncanonical bytes.

Generation uses a specified SplitMix64 generator and rejection-sampled
Fisher-Yates shuffle over an explicit reward pool. Plando consumes an occurrence
from that pool. There is no implicit starting inventory or full-game item pool;
logic and starting-policy digests identify supplied metadata rather than prove
native compatibility or beatability.

The copied-value transaction core covers the audited chest beneath Link's house.
It distinguishes selection, asynchronous item creation, completed native award,
durable save and cancellation. It validates the chest and item PID, REL
materialization and registration lifetimes, scene, quest slot, CPU/RAM/code
ownership, native call site, and matching entry/return stack. Only the original
Orange Rupee and basic Picto Box codecs are admitted. The native item handler is
void, so completion requires its actual inventory postcondition, not a return
register treated as success.

The chest must be active with native init state 2. Demo_Item grants its reward
inside its Delete callback, after native init state changes to 3. Its actor-queue
registration still exists during that callback; its delete tag has already been
removed. Item binding and award evidence therefore require deleting state 3 and
successful creation state 2. An earlier initialized actor does not qualify this
award boundary.

Cancelling an accepted transaction does not establish that the game rolled back
its effects. A cancelled location blocks another selection and any checkpoint
in that ledger. Recovery requires reconstruction from a verified complete save
and a genuine native reload. Completed historical awards survive ordinary scene
changes; live actor and call observations do not. Saved records remain immutable.

The storage layer publishes the whole CARD and ledger as one flushed,
checksummed generation, retaining the previous complete pair. An exclusive
process lease and owner thread prevent concurrent writers. Stale generations,
profile mismatch, changed retained files, links and bounded-format violations
fail closed. An interrupted native multi-write save can be replaced from the
last complete pair before the backend opens. Retained pending candidates are
never replayed. Only a receipt minted by the store after publication, or verified
load, can confirm the exact proposed CARD/ledger pair.

These APIs are source foundations, not a shipping native reward adapter. The
game owner must prove the real entry/return and REL ownership, correlate genuine
serialization and SaveSync completion, snapshot the same isolated CARD under
the runtime lock, and validate starting-state compatibility. The storage layer
treats CARD bytes as opaque and does not claim native save success. The library
is included by source regression targets and is not linked into the game host.

Optimized and AddressSanitizer fixtures each pass 3,289 seed checks, 777 final
transaction checks and 647 storage checks. They cover deterministic canonical
profiles, pool/plando multiplicity, malformed decoding, stale actors and stacks,
quest mismatch, cancellation quarantine, exact store receipts and fresh-ledger
reload. The storage fixture also covers real Windows deny-delete file locks,
interrupted publication, previous-generation recovery, exclusive leases and
external mutation. Source hashes remain unchanged across the final runs.

Native reward replacement, tracker presentation, full starting-state policy,
reachability spheres, beatability verification and entrance shuffling remain
open. The fixtures use explicit synthetic copied observations and opaque cards;
they do not establish a playable randomized game.
