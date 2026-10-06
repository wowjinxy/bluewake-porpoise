# Native inventory collector qualification

The opt-in collector now works in one genuine native CARD-load route. It remains
disabled by default and limited to the read-only Wind Waker, Grappling Hook and
Wind's Requiem subset. This does not complete the randomizer milestone.

The first run loaded the game successfully but failed collector admission. Its
PE relocation table contains 2,219,084 entries, exceeding the parser's former
1,048,576-entry limit. Commit `f3becfb` raises that bounded limit to 4,194,304
without removing type, range, overlap or overflow validation. Optimized and
AddressSanitizer parser fixtures each pass 1,093 checks. A separate file-only
probe also parses the real module at three address bases.

The second run used the repaired collector host and the same qualified game
module, a fresh copy of a genuinely earned CARD, and the original title/file
selection PAD inputs. It used headless rendering, no live input, no desktop
interaction, no STATE load, no warp, no autosave and no gameplay edits.

All 26 native checks passed. Preparation, file admission, loaded image binding,
descriptor binding and collector startup were accepted. A completed native
player-update return produced the earned Waker, Hook and Requiem observations
with the native CARD-load epoch and live CPU/RAM/module/alias identities. The
five immediate source capabilities evaluated as available and true. The
"Can Defeat Gohma" result is the imported Hook requirement, not a boss defeat.

The game completed the exact 3,300-VI route and exited normally. A new read-only
checkpoint independently matched the loaded inventory, sea entry, quest slot
and inactive event state. Source and copied cards, settings, runtime libraries,
module and protected inputs were preserved. Logged history carries expired
authorization; it cannot authorize later guest reads. The prior failed run is
retained as a negative result.

An independent file-only review passes 29 checks. It verifies the exact native
CARD-load epoch, player identity and checkpoint and confirms that all 473
protected inputs remain unchanged.

Private evidence is retained under `build/native-collector-first-native-run-v2`.
The native result receipt has SHA-256
`d1c1e7b661131d00ba1bb55a67460ac0d8741c22cc9fa882d565707f8e3470fe`.
Personal disc assets, CARD/STATE files and raw native logs are not public artifacts.

Wider inventory coverage, player-visible collection, native location identities,
exact-once reward transactions, a CARD-bound ledger, seed identity, placement
logic, tracker, entrance shuffling and plandomizer remain required. The current
headless collector and passive catalog cannot stand in for those features.
