# Camera shake accessibility boundaries

This is a source audit for the planned reduced-shake option, not a linked
feature or native gameplay qualification. The reference is the CC0 GZLE01
revision 0 decompilation at commit
`49f2e3484e5814cbafde68525128669589e86bb2`.

The native [camera implementation](https://github.com/zeldaret/tww/blob/49f2e3484e5814cbafde68525128669589e86bb2/src/d/d_camera.cpp)
and [vibration implementation](https://github.com/zeldaret/tww/blob/49f2e3484e5814cbafde68525128669589e86bb2/src/d/d_vibration.cpp)
establish why suppressing a shake function is unsafe. Vibration constructs
shock/quake patterns using the game RNG. `shakeCamera` advances the pattern,
uses additional RNG calls, updates shake contributions and runs blur timers.
The camera also decays the previous contributions after a pattern ends.
Skipping these operations changes native state and RNG progression. Multiplying
the persistent shake fields after each update changes their subsequent decay.

The contributions live in `dCamera_c`, inside `camera_process_class` at
offset `0x244`:

| Contribution | Offset in dCamera_c | Base value |
| --- | --- | --- |
| Center translation | `0x568` | center at `0x010` |
| Eye translation | `0x574` | eye at `0x01C` |
| Field of view | `0x580` | FOV at `0x038` |
| Bank | `0x584` | bank at `0x034` |

`store` (`8017BFAC`) adds those contributions when copying camera-body values
to `view_class`, unless a demo camera or the native camera flag owns the view.
The view stores FOV at `0xD0`, eye at `0xD8`, center at `0xE4`, up at `0xF0`
and bank at `0xFC`.

Changing those shared view values throughout `camera_draw` is also broader
than a visual change. That function passes eye and the view matrix to the audio
engine and performs a ground/material query at the eye position. A reduced
shake implementation must account for those users rather than describing
whole-function view substitution as presentation only.

The matrix inputs provide narrower candidate boundaries:

| Native caller | Callee | Return | Inputs |
| --- | --- | --- | --- |
| `camera_draw`, call at `8017C444` | `C_MTXPerspective`, `8030DB78` | `8017C448` | matrix pointer and FOV/aspect/near/far |
| `camera_draw`, call at `8017C45C` | five-argument `mDoMtx_lookAt`, `8000D148` | `8017C460` | matrix at camera+`0x140`; eye/center/up pointers; bank |

The [look-at helper](https://github.com/zeldaret/tww/blob/49f2e3484e5814cbafde68525128669589e86bb2/src/m_Do/m_Do_mtx.cpp)
copies eye, center and up to local stack values before calling `C_MTXLookAt`,
then builds and concatenates a bank rotation. Reducing those copies would still
change the resulting shared native matrix. `camera_draw` copies it into
`j3dSys.mViewMtx`; `JAIZelBasic::getCameraInfo` retains that matrix pointer, and
`JAIZelAnime` later uses it to transform sound positions. Restoring the matrix
only during audio registration would therefore still change later audio.

The current optimized perspective/look-at callers retain supplied-CPU entry and
real-return queries. Separate optimized and sanitizer fixtures each passed 112
paths through the actual caller bodies with synthetic leaves. This establishes
observation admission, not native camera ownership or a working shake option.

A presentation-only implementation needs a separately owned host display
transform. Canonical native matrices, audio, color/depth peeks, EFB copies,
intermediate textures and photographs must retain the original output. Camera
and render-pass ownership, native helper substitutions, display cost and native
equivalence still need qualification before that approach can be implemented.

The admission contract must identify the current native camera process,
back-pointer, profile/methods, module and memory aliases. Demo-owned or modified
views must fall back unchanged. Default 100% must perform no camera reads,
watches or writes. Optional changes must preserve native shock/quake patterns,
timers, RNG calls, rumble and original return behavior. No camera-state resets
or arbitrary caller interception are authorized by these addresses alone.

Reduced flashes and blur are separate options. They need their own rendering
ownership audit; the shake flags do not establish a safe universal flash hook.
