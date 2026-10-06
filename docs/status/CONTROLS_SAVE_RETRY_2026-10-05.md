# Controls save retry

Changed bindings remain active when saving fails as settings closes. A retained
notice stays visible outside the Controls page, and later closed-menu frames
retry the current complete profiles at most once per second. Opening settings
or capturing a binding pauses retries. Successful save or explicit reload clears
the notice and retires the retry. Queued requests never retain stale profile bytes.

The persistence writer and its bounded atomic replacement policy are unchanged.
This fixes missing menu-close retries; it does not diagnose the separately
retained intermittent concurrent-load failure.

An optimized Windows fixture holds the real binding file open without delete
sharing, verifies failed saves preserve its bytes, edits bindings again, releases
the handle, and verifies the later retry saves the newest complete profiles.
A fresh hidden fixture process reloads those profiles. Retries do not reapply
PAD mappings or leak held action input.

Optimized and AddressSanitizer offscreen fixtures exercise the real settings
menu and ImGui notice. They verify closed-menu error visibility, one-second
throttling, successful retry and notice removal. The notice accepts no inputs
and cannot take focus. These fixtures use local ImGui IO and mocked providers;
they initialize no SDL devices, native windows, game, GPU or desktop input.

The production Windows build also compiles and links successfully. Only the
controls-menu and settings objects change; the other 167 audited linker inputs
are retained. Imports and the application manifest match the preceding tester.
This build keeps the diagnostic inventory collector disabled.
