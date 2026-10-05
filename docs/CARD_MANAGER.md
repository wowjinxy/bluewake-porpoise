# Memory-card management

`runtime/host/src/card_manager.h` exposes a C API for the actual persistent
GXRuntime `DOLCARD1` card container. It backs up every serialized card file and
its metadata, rather than exporting emulated RAM or a debug save state.

Import accepts validated BlueWake `.card` containers, GCI exports of the USA
Wind Waker `gczelda` save file, and standard raw GameCube memory-card images
containing that file. Format detection uses bytes and geometry, rather than the
filename extension. GCI/raw conversion preserves the complete native payload:
all three quest logs, the redundant game-save copy, and the pictures. Foreign
games from a raw card are not imported.

The converter requires the primary game's version, all three quest-log
checksums and its block checksum to be valid. A damaged redundant game-save
copy produces a warning while preserving its original bytes. A damaged primary
copy is rejected; the converter does not repair quest data or photos. Photo
checksums have not been qualified independently.

Raw import validates card geometry, header/directory/BAT checksums, the native
signed update-counter selection, free-block counts and the entire live
allocation graph, including other games. Cycles, overlaps, duplicate file
identities, orphan allocations and an invalid newest graph are rejected. A
single damaged metadata copy can use the other valid copy. Raw card serial,
encoding and size are retained; GCI receives a stable content-derived serial.

## Windows menu

Open **Sound & Saves** to back up the persistent card, refresh and select a
backup, or **Import save...** to select `.card`, `.gci` or `.raw`. Use the game's Save first to include its
latest progress. Backups contain every slot and are kept in
`Backups/<card filename without extension>` beside the current card.

Import and restore queue a replacement for the next launch. They leave the
running card and its pending guest callbacks intact. Cancel removes the queue
while retaining its bytes in an adjacent recovery file. Before applying a
queued replacement, the next launch preserves the latest current card in a
new backup. An invalid queue or incomplete replacement stops startup and
reports recovery information.

The backup list is cached between frames; Refresh discovers changes made
outside the menu. Damaged backups are displayed but cannot be selected for
restore. Backup copies existing disk bytes and does not serialize unsaved
gameplay. Autosave uses a separate native game-save transaction.

## Experimental autosave

Autosave is disabled by default in **Sound & Saves**. Its default interval is
five minutes, adjustable from one minute to one hour. First load an existing
quest from a normal memory card or complete the game's Save. Machine save
states do not authorize autosave, and autosave does not create or repair quests.

The interval is a minimum: menus, events, scene changes, death, minigames,
pending rewards and other unsafe work postpone the save. The native routine
chooses the same restart location and saved-life rules as the game's Save.
Disabling autosave lets a transaction already running finish. The panel shows
its status and successful/failed save counts.

The game thread calls Wind Waker's actual preparation, serializer, checksum,
card controller and completion routines. The controller's recursive lock
confirms adoption of the exact buffer before the native worker may write it.
The scheduler, VI, audio and card callbacks continue while gameplay work is
held. Caller ownership is checked before every continuation; queued mouse and
jump gestures are discarded before normal gameplay resumes.

Private optimized Windows runs earned the original Tingle Tuner, deferred
autosave until its event ended, then loaded the resulting card in a separate
native launch with the reward still owned and no repeated award. The other two
quest logs, photo region and card serial matched the original; native quest
and redundant-copy checksums passed. A second game also autosaved a received
Tuner to its isolated network-room card and retained it in a fresh offline
launch. With autosave disabled, the loaded card stayed unchanged. A genuine
OS write failure preserved the previous card byte for byte and resumed native
gameplay.

These runs used the baseline card's unpopulated photo region. Populated photos
and a native floating-point ownership handoff during an autosave still need
separate acceptance; guarded phase and ownership fixtures cover those paths.

## Runtime integration

Create a manager with the exact card path and a profile-specific `Backups`
directory. The manager normalizes the path to an absolute path. Live Backup
needs both snapshot callbacks. `begin_snapshot(expected_card_path, user)` must
verify that the requested path is this process's active persistent card, wait
for any in-flight dispatch, and hold the existing runtime card mutex until
`end_snapshot(user)` releases it. Normalize Windows separators and case before
comparing paths. These callbacks must not throw or re-enter manager methods.

Holding the mutex prevents dispatches from writing during the file copy; it
does not establish that the guest's complete multi-write save transaction or
SaveSync has completed. The UI should tell players to use the game's Save
first. The manager never replaces or reopens the live HLE object, clears its
pending callbacks, or interprets the guest's save buffer. Every Backup return
path releases a successfully acquired snapshot bridge. Without a bridge,
Backup must acquire the backend's OS card lock and therefore refuses a running
backend.

Stage, status, listing, and cancellation operate on managed files and need no
live-card suspension. Staging copies the selected file into a unique adjacent
candidate and flushes it. For original formats it converts those copied bytes
into another exclusive candidate, flushes that file, and validates the resulting
container with `dol_card_validate`. It then atomically publishes `<card>.pending`.
The current card and an earlier
pending request survive a failed stage. Cancellation consumes the queue into
an immutable adjacent `.cancelled-*` recovery file.

## Applying at launch

Call `bluewake_card_manager_apply_at_startup` before opening the game/card
backend. It acquires the same `<card>.lock` as GXRuntime. A held lock or invalid
pending file stops replacement. Existing current-card bytes are flushed into a
unique backup before the validated candidate is atomically published. A damaged
current card is also preserved verbatim, with `backup_valid=false`.

Open the backend only when the outcome's `startup_ready` is true. A publication
can succeed while durability or queue consumption fails: `card_changed=true`
then describes the actual disk change, while `startup_ready=false` requires
startup to stop. The old backup and pending replacement remain available. A
retry detects an already-published identical card and completes the transaction
without another replacement or backup. Success consumes the queue into an
immutable `.applied-*` file, preventing a later launch from replaying the import
over newer gameplay saves.

Each manager operation uses a process mutex and `<card>.manager.lock` to
serialize management of that card across processes. Backup names are unique and
created exclusively; existing backups are never rotated, overwritten, or
deleted by this foundation. List metadata reports the path, byte count,
modification time, and actual validation result, including failed/recovery
copies. Original inputs and complete failure artifacts are retained.

The Windows backend uses UTF-8 paths and wide file APIs for validation,
serialization, locks, temporary files and backup rotation. Atomic migration
and flushing use the same path encoding, so Unicode names identify the same
card throughout these operations.

## Regression

`bluewake_card_manager_test` compiles the real GXRuntime serializer and validator
with synthetic files in a unique isolated directory. It covers live-lock
refusal, balanced snapshot hooks, byte-exact backups, metadata, stage/cancel,
latest persistent bytes preserved at apply, damaged-current recovery,
first-launch import, injected write/flush/publication/durability/cleanup failures,
idempotent recovery, and absence of request replay over later saves. It does not
open a game, initialize SDL, access personal data, or use desktop input.

`bluewake_card_menu_test` exercises the actual ImGui handlers, manager and
serializer in an isolated in-memory UI. Its native picker is compiled out.
It checks Unicode active-card/import paths, cached listing, backup, queued
restore, cancellation, startup apply, OS lock refusal and replay prevention.
`bluewake_card_runtime_test` verifies that a live snapshot holds actual card
writes until release without cancelling queued callbacks.

`bluewake_card_import_test` exercises original-format checksums, metadata-copy
selection, allocation-graph corruption and payload preservation, then stages
through the actual manager and serializer. The fixture also checks Unicode
paths and conversion-flush failures without changing an existing card or queue.
Separate private native-game launch qualification is recorded in the roadmap;
synthetic parser fixtures alone do not establish native load compatibility.
