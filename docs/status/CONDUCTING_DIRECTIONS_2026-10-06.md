# Wind Waker conducting directions

The fast controller camera applied its native horizontal camera correction to
every view it did not own. Wind Waker conducting is one of those views, but it
uses the C-stick as musical directions. Consequently, the correction changed
left into right and right into left with the default controller mapping.

The camera now samples Link's actual conducting procedure with its existing
per-frame context. Procedures `0x9A` through `0x9D` identify conducting, song
playback and its exit; `mCurProc` is at player offset `0x31D8`. A bounded read
rejects missing, incomplete, unaligned and out-of-range player state. The
conducting context resets on attachment and refreshes before camera-frame early
returns. During those procedures, the horizontal camera correction leaves
native C-stick directions alone. Other native camera views retain their
existing correction, including the signed `-128` saturation case.

The permanent `bluewake_mouse_camera_pad_test` target extracts the production
context reader, camera-frame handler and PAD functions, alongside its existing
input-blocking and gesture-cleanup code. It uses the actual CPU layout and
big-endian memory helpers, with authored camera inputs, an alias-provider
negative and an authored SDL event queue. It creates no game window and reads
no physical input.

Both optimized and optimized AddressSanitizer fixtures passed 85 conducting
checks plus the existing camera-click, menu-ownership and paused-input tests.
The checks cover all four procedures, left/right/up/down/neutral directions,
axis extremes, complete PAD/CPU/RAM preservation, context refresh, ordinary
camera correction after cancellation, keyboard input while the controller
rests, the existing inversion preference and invalid player spans. The full
production camera translation unit also compiled in both modes. All eight
compile/link/test roles completed successfully with closed dependencies,
fixture-only link inputs and matching runtime libraries.

Independent source and actual-result reviews accompany the local qualification.
The frozen source-plan digest is
`f493c1118857ce4d865d0f8940254b018baf41b88a8464072e92943f40045a5e`;
the actual-result digest is
`ba83b8ea63553bd279acd0099f03a96e9bce0fb51c4cba576b6b5620075eb841`.

This is source and authored regression qualification. A native conducting run,
physical-controller coverage and an updated packaged tester remain pending.
Explicit controller profile axis inversions still transform the native C-stick;
this change corrects the automatic camera fallback with the default mapping.
