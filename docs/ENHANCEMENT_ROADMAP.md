# BlueWake enhancement roadmap

This roadmap follows the Shipwright feature comparison and the selected first
priority: controls, menu, and core enhancements. Mod-enabled Windows builds are
already the default. Sloped and faceted experimental wall climbing has been
implemented and exercised in the game. Multiplayer will support independent
exploration and shared permanent progress, with separate room saves.

| Milestone | Deliverables | Required evidence | Status |
| --- | --- | --- | --- |
| 1. Menu and controls | Shared setting definitions, search, built-in and shareable named presets, keyboard/controller rebinding, device profiles, deadzones, hotplug and input viewer | Restarts and reconnects preserve mappings; unrelated settings cannot reset mappings; opening and closing the menu cannot leak held input into gameplay | Windows implementation verified by offscreen UI, fresh-process persistence, SDL virtual devices and isolated gameplay; bounded retry for transient missing binding files passes four optimized and one AddressSanitizer full fixtures, including permanent absence preserving unsaved bindings; the controls-only optimized host passes a hidden native default run with both CPU/memory captures, events, direct-call counts and audio identical to the preceding build; failed menu-close saves now retain a visible notice and retry the latest profiles on later closed-menu frames, with a real Windows file-lock fixture, fresh-process reload and optimized/sanitizer offscreen menu checks passing; physical-controller coverage and the separate intermittent concurrent bindings-save failure remain open |
| 2. Gameplay events | Named item, progression, scene, player-update and save events with lifecycle invalidation | Hooks remain reachable in optimized translations; transaction events fire once; scene changes discard stale player state | Optimized native player/scene/save/reload, ordinary chest award, original Aryll Telescope gift, Hr's Wind's Requiem lesson, Orca's permanent sword and Medli's Grappling Hook verified; the song and equipment emitted one native award, autosaved and survived fresh card launches without award replay; broader reward routes remain pending |
| 3. Enhancements and saves | LB/Tab + D-pad equipment shortcuts, adjustable text speed, equipment conveniences, individually validated cutscene skips, memory-card backup/import/restore and guarded genuine autosave | Separate launch reloads a real card save; item awards and story progression remain correct | Native .card save/reload/restore verified; GCI and raw imports each passed two fresh game launches with one-time staging and byte preservation; original ordinary Orca dialogue verified at 1× and 2×; fixed shortcuts pass automated tests; genuine autosave retained an earned Tuner across a fresh native launch, preserved other quests/photo bytes and handled a real write failure; full native equipment, other conveniences/cutscene skips, populated photos and native FPU handoff remain pending |
| 4. Shared progress co-op | Host/join, server/room settings, roster, status, leave/rejoin, progression synchronization, then remote Links | Two game processes explore different scenes, share rewards once, converge after reconnect and keep isolated room saves | Experimental TCP service, room saves, menu and guarded permanent whitelist implemented; optimized native games in Pnezumi and at sea shared one genuine Tingle Tuner award, then converged after a real host/server restart with personal cards preserved; the receiver also autosaved it to its room card and retained it in a fresh offline native launch; other whitelist entries remain fixture qualified; durable server storage now preserves room identity, progress and replay receipts, with 36 optimized/sanitizer standalone tests passing across Windows and Linux; native qualification of the durable host, complete reward reconciliation and remote Links remain pending |
| 5. Mod management | Named asset packs, enable/order, manifests, conflicts and shareable presets | Deterministic order, clean missing/broken-pack fallback; distinguish compiled gameplay mods from loadable assets | Texture-pack catalog, manifests, panel, order, selected-texture conflicts and presets implemented; actual decoder/registry fallback and offscreen menu tests pass in the integrated optimized Windows build; model/audio pack kinds belong to later milestones |
| 6. HUD and cosmetics | HUD position/scale/opacity/visibility, colors, cosmetic presets, then character/model replacement | Supported aspects, menus and cutscenes work; reset restores original behavior | Default-off native pane customization and HUD settings are integrated; host regressions, actual ordered GXCore stream tests, 66 real shader/pipeline checks and 19 synthetic offscreen pixel cases pass; native capture-only diagnostics verify visible hearts/buttons/rupees ownership and ordered metadata with unchanged guest state; reading each sibling record together reduces resolver calls by 69.6% with identical native metadata and CPU/RAM checkpoints; game presentation, visible magic/keys, other classes and actual frame-time effects remain open; the first layout is gated to the audited 4:3 module |
| 7. Audio | Master volume/mute, genuine music/SFX separation, previews and replacement music with loops/transitions | Preserve effects, fanfares, pause/resume and original audio when disabled | Live persisted master/music/SFX/mute controls and presets pass integrated UI tests; two six-profile native batches qualify premix gains and complete-output master/mute; five reward profiles verify the Tuner fanfare at zero category gains, and five menu-pause profiles preserve native duck/resume and voice state with effective independent gains; external WAV preview and its Sound panel pass the integrated 52-test suite and six native callback/lifecycle runs; preview-off output remains byte-identical, native ownership/progress/cards preserved; replacements, other fanfares, streams and native 48 kHz preview remain pending |
| 8. Accessibility, display and tools | UI/text size, hold/toggle options, reduced shake/flashing, narration, actor/collision viewers, save editor, performance diagnostics and broader Smooth Motion qualification | Representative gameplay, unchanged progression and game timing; tools preserve game behavior | Live persisted menu sizing at 75–200% and independent keyboard/controller Sprint Hold/Toggle settings are integrated into the optimized Windows host; all 78 regressions pass; nine native Sprint cases verify 1.5× actual floor movement, Hold/Toggle, idle cancellation, real STATE capture and scene cancellation with held-input suppression; action sources are explicitly synthetic, so physical-provider coverage remains open; GPU menu presentation, game text sizing, reduced shake/flashing, narration, viewers, save editor and broader Smooth Motion remain pending |
| 9. Challenge modes | Damage/healing rules, enemy tuning, permadeath profiles and Boss Rush | Damage, healing, death and save/reload consistently follow the selected profile | Experimental ordinary collision/fall/lava damage and heart/fairy healing controls are integrated, default to Native and are unavailable for room saves; 78 regressions pass; Native damage/healing preserves all eight shipping CPU/RAM checkpoints, events and direct-call counts; seven genuine half-damage transactions scale correctly and preserve maximum health, inventory and resources; the original timed combat route died before its reward, and a bounded retry preserved health and the card but failed to clear its remaining enemy, so full changed-rate gameplay remains unqualified; a later ordinary second B press breaks a genuine pot, and a native pickup raises life from 7 to 11 while preserving the rest of saved progress and source cards; that older module's diagnostic trace missed the internal healing return; a certified optional internal-return callback now passes authored and actual translated-routine source fixtures in optimized, unoptimized and sanitizer builds, with native fallback for older modules; the current optimized module and Windows host also compile and link with audited dependencies and linker inputs; a hidden Native heart run with that module now captures all eight states, preserves the six pre-pickup CPU/RAM checkpoints and verifies its healing capability with no callback registration, bindings or core entries/adjustments; life rises from 7 to 11 with other saved progress and source cards preserved; a hidden Half heart run passes 51 gates, changing the genuine native queue from +4 to +2 and life from 7 to 9 while preserving CPU state, inventory, other saved progress and source cards; the same route on an older module passes 42 gates, retaining the Half preference while using native +4 healing with no callback binding; broader healing sources, death/save rules, enemy tuning, permadeath and Boss Rush remain open |
| 10. Randomizer | Wind Waker item/location catalog, deterministic seeds, placement logic, tracker, entrances and plandomizer | Reproducible seeds, exact-once rewards, persisted seed identity and verified beatability | MIT source catalog and passive C++17 logic are implemented with 320 locations, 307 macros and seven path kinds; 77 locations have multiple paths for one reward; optimized and sanitizer fixtures each pass 11,448 checks and 3,135 real-corpus oracle comparisons; per-evaluation memo and a counted-work limit prevent exponential expansion; a passive copied-value inventory projection for the Waker, Hook and Requiem passes 14,810 optimized and sanitizer checks each and rejects incomplete, contradictory, stale or unsafe observations; an opt-in collector foundation is integrated and disabled by default; its final public build wiring rejects invalid policy inputs and passes 59 lifetime negatives, 3,592 issuer/lease checks, 1,093 image checks and 46 authored Windows DLL checks in both optimized and sanitizer builds, plus eight main/producer compile-only cases; a genuine native CARD reload now passes all 26 collector checks, including loaded-image admission, earned Waker/Hook/Requiem capture and preserved cards; source-only canonical seed profiles, explicit reward-pool generation and plando now pass 3,289 optimized and sanitizer checks each; the copied-value LinkUG reward ledger passes 777 checks each, including stack replay and cancellation quarantine; paired CARD/ledger storage passes 647 checks each, including real Windows file locks and interrupted publication; session ownership adds 184 checks each and a private loss-of-acknowledgement variant adds 218; native actor/REL queries pass 8,641/577 checks each, and an admitted-artifact identity query passes 699 with authored DLLs; copied CARD inspection and exact other-quest/photo/unrelated-file preservation pass 7,236 checks each; a default-off single-location native reward adapter now has main lifetime/save wiring, final optimized and sanitizer adapter compiles, optimized main compiles with the option both disabled and enabled, and eight configure-only build-guard cases; the permanent authored adapter regression target links the complete core and passes 8,181 optimized and sanitizer checks each, with all 46 translation units rebuilt and seven configure guard cases verified; a deep-path publication failure is retained and long-path support remains unqualified; the isolated Windows host compiles and links with audited dependencies, linker inputs and matching CRT imports; a first hidden seed session also passes all 15 genuine native CARD-load gates, confirming generation one with an empty ledger, unchanged copied CARD and clean shutdown; hidden same-item Orange and basic Picto replacement runs each produce one accepted substitution and one native award, with the stored generation-one pair unchanged; native manual-save, replay/cancellation and fresh CARD reload remain pending; these foundations remain outside the shipping tester; wider inventory, durable save qualification, start-state compatibility, tracker, full seeds and beatability remain pending |

## Menu structure

Display · Controls · Enhancements · Mods · Network · Sound & Saves · Developer.

Enhancements owns BetterWW and movement options. Mods owns installed assets.
Network exposes the experimental service's connection and synchronization scope.
Developer contains diagnostics and experimental machine save states; these are
separate from ordinary memory-card saves.

Settings presets contain selected scalar settings and compiled enhancement
choices. Button and axis mappings remain in persistent device profiles, so
applying a display or gameplay preset cannot replace bindings. Existing camera
inversion and face-button swaps overlay profiles without rewriting them.

Equipment shortcuts use a held modifier, defaulting to left bumper or Tab.
Up selects the Wind Waker; Left deploys the boat cannon and Right deploys the
salvage crane at sea. Plain D-pad keeps the original map actions, and Down keeps
its native action. The shortcuts require owned equipment and native safe
contexts, and preserve X/Y/Z assignments.

The Wind Waker award, genuine autosave and separate native reload have passed.
With that earned item, held-modifier + Up starts the native conducting procedure;
native cancellation resumes movement and X/Y/Z assignments remain unchanged.
The earned Grappling Hook also deploys the native crane with modifier + Right.
Holding Right lowers the rope, releasing it raises the rope, and native A puts
the crane away. Autosave and a separate offline card launch retained the hook,
boat progress and X/Y/Z assignments. This check used the game's authored sea
entry; physical shore boarding, Sail purchase, salvage rewards and the cannon
remain separate gameplay requirements.

The wind-change and Iron Boots animation conveniences have separate live,
default-off controls. They intercept audited native calls and retain native
cleanup, equipment toggles and vibration checks. Guarded source fixtures cover
both optimized caller paths and reset/cancellation. The game taught Wind's
Requiem and autosaved it; a separate native launch retained the song. Faster wind
then finished 55 game frames earlier than the default animation with the same
conducting input, committed direction and native cleanup. Iron Boots gameplay
remains an acceptance requirement.

The shrine song observer verifies the actual loaded Hr module, native actor,
lesson state and imported item-handler invocation. It checks that ownership
again at the return before emitting one copied item event. A fresh game run
learned Wind's Requiem, emitted exactly one song award and autosaved it; a
separate native card launch restored the song without another award. Source
fixtures cover module reuse, alias changes, CPU/RAM replacement and reset
cancellation. This route does not authorize arbitrary REL callers or infer
awards from saved flags.
The original B-cancel route returned control with no song, award or save.

The original six-stage Orca training also awarded one permanent sword through
the native item handler. The save guard deferred throughout the practice
minigame, then autosaved after the reward and normal control resumed. Both
native save copies and a fresh offline card launch retained the collected and
equipped sword, the reward's story flag and prior earned equipment, with no
award or lesson replay. Borrowed practice equipment was not treated as an earned
item. The other quests and baseline photo bytes remained unchanged; populated
photos still need their own gameplay check.

The original Dragon Roost encounter also opened Medli's cage, ran her gift
dialogue and awarded one Grappling Hook. Guarded autosave wrote both native
quest copies after normal control resumed; a fresh offline card launch retained
the hook and story progress without another reward or save. The other quests,
baseline photos, existing sword and X/Y/Z assignments were preserved. The
original King of Red Lions meeting then completed and its progress survived
autosave and a separate offline launch. The earned hook's native crane operation
passed after an authored sea entry, followed by another autosave and fresh
reload. Sail purchase and physical shore boarding remain open.

The original chest beneath Link's house awarded one Orange Rupee through the
native item handler, raising the wallet from 1 to 101. Guarded autosave retained
both the wallet and opened-chest flag in both native quest copies. A separate
fresh card launch restored them without another award or save; existing
inventory, X/Y/Z assignments, other quests and baseline photos were preserved.
This qualifies the original reward route, not a randomizer location ledger.

Native Sprint checks use real game PAD movement and an explicitly synthetic
Sprint action source in a private adapter. On a clear native floor, successive
movement steps rise from 17 to 25.5 at 1.5×. Keyboard and controller Hold/Toggle,
controller idle cancellation, actual STATE capture and original scene changes
all restore native speed and suppress held input until release. These checks
do not establish physical controller, hotplug or displayed-menu behavior.

Display includes a live Menu size (%) control from 75 to 200. It multiplies
the existing automatic display sizing without changing the stored percentage
when the window resizes. At 100%, the original font metrics, glyph raster and
main menu geometry are preserved. Enlarged narrow menus wrap labels, stack
controls and scroll; small preset previews stay inside the viewport. Built-in
presets preserve accessibility size, while named presets can carry it when
Display is selected. A second font face is prepared once using the existing
texture-copy seam; changing size does not rebuild or upload the font atlas.
CPU-only UI qualification does not establish native GPU font presentation.

The HUD tab stores position, scale, opacity, visibility, pivot and color for
hearts, magic, buttons, rupees and keys. Original/reset settings restore native
identity; editing one component preserves unrelated launch overrides. The
game thread emits copied descriptors and the renderer applies them in draw
order, including draws resumed after a presentation flush. This first version
requires the exact audited GZLE01 module, 4:3 layout and GXCore. Independent
glows, particles, minimap, compass and timers remain outside these pane groups.
Synthetic offscreen pixels pass for transforms, scissor, tint, opacity,
alpha-test, destination alpha and unrelated draws, with identical specialized
and Uber shader results. These tests do not establish in-game visual
correctness; customization stays off by default while native checks continue.
Two fresh native card boots with HUD off retain byte-identical default audio,
known Music/SFX ownership and unchanged cards/settings.
Native capture-only diagnostics also verify loaded hearts, buttons and rupees
through native MyPicture/Picture draws, including optimized first-PC replay
and draws suspended across cycle-budget yields. At two checkpoints the complete
guest memory and CPU match identity; progress and cards are preserved. Magic
and keys remain naturally hidden in this scene, and Text draws are unobserved.
These diagnostics retain shipping headless unavailability and do not render
game pixels. A synchronous sibling traversal now resolves each complete record
once while retaining all ownership, ancestry, bounds and lifetime checks. A
paired native comparison preserves every 687,641 ordered metadata record and
complete CPU/RAM checkpoints, reducing resolver calls from 97,539,973 to
29,646,568. This measures read work; frame time and FPS remain unqualified.

Sound & Saves can preview an external PCM16 stereo WAV at 32 or 48 kHz when
its rate matches the game's current output. Choose WAV selects a file; Play
starts one pass and Stop cancels future copied output. Status shows playback,
rates and elapsed time. Preview starts off, mixes with native audio and uses
Music volume followed by Master volume/mute. Leaving the Sound page, closing
the menu or resetting the game cancels it. The selected path is transient.
Decoder bounds, worker cancellation, callback allocation/lifetime and actual
offscreen menu tests pass. Six hidden native runs also verify default-off
identity, sample-accurate Music/Master gains and saturation, Stop/Cancel,
natural completion, actual native CARD-reset cancellation and shutdown during
a pending decode. Native player/scene/event and voice ownership metadata,
cards and settings are preserved. The native route uses 32 kHz output;
48 kHz and partial final blocks retain source-fixture coverage. This does not
yet replace a native music track or establish looping/transition behavior.

The text multiplier handles JMessage cutscene characters and an audited ordinary
NPC legacy message path. Native waits, choices and page stops retain their
behavior. The optimized Windows host passed an original ordinary Orca conversation
at 1× and 2×: character progression speeds up, the native page stop matches, and
the original message data stays unchanged. Other ordinary messages and choices
currently have guarded source fixtures; the original scripted Grandma route
remains deliberately unchanged.
BetterWW Instant text takes priority; disabling it requires a restart to restore
message data already patched in memory.

Co-op currently shares an audited subset of permanent inventory, capacities,
passive collectibles and named flags. Chest markers, small keys, heart rewards,
bottles and derived sword/shield/bracelet equipment remain local. Temporary boss
recollection inventories are excluded. Sharing all opened-chest bits before
sharing their actual rewards would remove unclaimed items and can block dungeon
progress. These need an item-aware reward ledger before they can be enabled.

## Implementation and qualification rules

- Build and test each milestone using isolated player data. Test work belongs
  to the project implementation, rather than being handed to the user.
- Save backups and autosave must use real card/game-save transactions. Machine
  snapshots do not satisfy the save milestone.
- Music/SFX sliders must operate before the final mixed PCM output; one master
  gain cannot satisfy independent music and effect controls.
- Reuse Shipwright architecture where applicable, while implementing Wind
  Waker-specific addresses, events, progression and assets.
- Reuse the archived controller editor and proven backend behavior. Do not
  reuse the archived shared-simulation multiplayer architecture for independent
  exploration.
- No roadmap milestone is complete until its required evidence passes. Passing
  the menu milestone does not complete the full enhancement roadmap.
