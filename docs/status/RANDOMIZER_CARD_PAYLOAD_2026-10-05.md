# Randomizer native CARD payload inspection

The copied-byte helper now validates the DOLCARD1 container and Wind Waker's
redundant native save blocks through one bounded parser. Inspection returns
copied game records and card metadata; validation requires both complete save
copies to match the buffer captured after the game's native checksum operation.
Malformed containers, invalid native metadata or checksums, and checksum-valid
but stale copies fail with zero output. The parser performs no file, backend,
guest-memory or native-function operations.

Before a seed session opens the native backend, inspection can supply its
confirmed baseline. A separate comparison allows the selected quest, native
save counters and banner to change while requiring exact preservation of the
other two quests, all photograph bytes and unrelated file metadata/content.
File ordering may change; card identity and file IDs remain fixed. Comparisons
use actual bytes rather than a digest approximation.

Optimized and AddressSanitizer builds each pass 7,236 synthetic checks. The
fixtures use an independent algebraic checksum oracle and cover inspection
output ownership, malformed framing, valid stale copies, all selected quests,
photo boundaries, unrelated file changes and reordered files. An earlier
fixture incorrectly reused an oversized auxiliary file for its file-count
case; correcting that fixture preserved both separate rejection checks.

The helper is registered once with the source regression suite. These checks
establish copied-byte validation and preservation only. The native adapter
still must prove actual CARD loading, the serialization/STORE/SaveSync
relationship, ownership of the mounted working CARD, reward postconditions
and publication through a confirmed CARD/ledger receipt. Native adapter
qualification and inclusion in a new tester build remain pending.
