# Trigger modifiers for D-pad equipment shortcuts

The Quick Items modifier can now bind to either analog controller trigger as
well as a digital button. LB and Tab remain the defaults. In F1 settings, open
Controls, find the controller's D-pad shortcut modifier, choose Bind, release
held inputs and press ZL/LT or ZR/RT. Other controller actions remain digital.
Enable D-pad equipment shortcuts in Enhancements: modifier + Up uses an owned
Wind Waker, Left deploys the owned cannon at sea, and Right deploys the owned
salvage crane at sea. Plain D-pad retains the native map controls.

Activation uses SDL trigger value 16384; release uses 12000. Hysteresis avoids
repeated activation near the threshold. Menu close, rebinding, profile loading
and controller replacement require held sources to release before taking over.
Successful loading now cancels host actions even when the saved mappings are
identical. A regression exposed that missing reset; failed or malformed loads
continue to preserve current mappings and held-input state.

Each GUID/serial profile stores its trigger in controls.ini format 4. Formats
1 through 3 migrate while preserving the existing keyboard/controller maps and
LB default. A trigger and digital modifier cannot be selected together; missing,
duplicate, invalid or conflicting format-4 fields reject the whole load.
The new format requires this build or later when reloading saved profiles.

Binding the modifier leaves native controller mappings, analog trigger values,
D-pad and X/Y/Z output intact. Its conflict mask identifies the actual emulated
digital L/R side, including remapped trigger axes, without exempting X/Y. This
mask does not suppress native analog trigger actions.

The ordinary optimized host compiles and links with the committed conducting
direction fix. Exactly three source objects replace the prior tester's controls,
menu and camera objects; 166 other inputs, the translated module and runtime
are retained. Its imports and embedded manifest match the prior tester.

The bindings, menu capture, real SDL virtual-controller backend and separate
writer/reader persistence fixtures pass in optimized and AddressSanitizer
builds. Checks cover activation/release boundaries, device selection and
reconnection, native PAD preservation, profile migration/rejection, held-input
reset and saving after a real Windows file lock. They read no physical input.

The sanitizer backend rebuilds Abseil's actual hash-set support with
instrumentation to match its instrumented callers. SDK string/vector container
annotations are disabled for compatibility with the retained fmt library;
heap/stack instrumentation and Abseil iterator generation checks remain active.
Other retained third-party libraries are uninstrumented. Earlier failed build
recipes, mixed-library failures and the genuine held-reload regression remain
preserved with their diagnostic results.

The updated host passes all 14 hidden native CARD startup checks at 3,300 VIs,
reaches ready gameplay and exits cleanly. Cards and settings remain unchanged;
the run uses no window, live input or audio device. Physical trigger operation,
native analog-trigger interactions and graphical presentation remain open.
No Tingle wait-skip work is required by these checks; that optional feature
remains disabled and deferred.
