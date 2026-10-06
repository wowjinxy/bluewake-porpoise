# Core Windows tester: 2026-10-06

The local tester `BlueWake-tester-20261006-40b6725-windows-x64.zip` is ready.
Its code checkpoint is `40b67251502a1e322a1e2583dea05d3a6ad85fa4`.
It is 231,321,169 bytes; SHA-256:
`ac71a0e0d25222305cc9a683d346c5f647662b08d8df28558f82452e14975800`.
The checkpoint is committed locally and has not been pushed.

The build includes analog LT/ZL and RT/ZR binding for the D-pad equipment
shortcut modifier, held-action cancellation after successful identical-profile
reload, and the committed Wind Waker conducting left/right correction. LB/Tab
remain the default modifier. [Trigger behavior and verification](QUICK_ITEMS_TRIGGER_MODIFIER_2026-10-06.md) and [conducting verification](CONDUCTING_DIRECTIONS_2026-10-06.md) record their scope.

This is a layered update of the previous qualified tester. Three optimized
production objects replace its controls, menu and camera objects. The other
166 link inputs, translated module, runtime DLLs and dedicated server remain
unchanged. Imports and embedded manifest match the previous host; BUILD.json
labels current and retained validation separately. Uncommitted optimization
work is excluded.

All four controls fixtures pass optimized and AddressSanitizer execution,
including real SDL virtual controllers and separate-process persistence.
The sanitizer backend's retained-library limitations are recorded in the
controls status. The updated host also passes all 14 hidden native CARD
startup checks at 3,300 VIs, reaches ready gameplay and exits cleanly. Source
and copied cards, settings, original assets and staged inputs are preserved.
This startup check does not qualify native conducting, physical trigger input,
graphical presentation, audio-device output or full multiplayer/randomizer play.

The exact 46-file ZIP passes per-file hashes, CRC checks, DLL closure and the
mandatory public-assets wrapper using the real recovered upstream content
scanner. Discs, extracted game assets, saves, STATE files, console keys and
private diagnostic logs are excluded. The previous tester is preserved.

The optional Tingle rescue wait-skip remains disabled and deferred. Its source
and diagnostic results remain intact; none of its qualification work is a
prerequisite for this build or for unrelated milestones. Core graphics work
continues with the [Picto Box EFB recognition path](PICTO_EFB_RECOGNITION_2026-10-06.md).
