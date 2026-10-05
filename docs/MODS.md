# Mods

BlueWake has a **Mods** section in the in-game ⋯ menu with four mods. Each takes effect the next time
BlueWake starts, and none of them changes your saves.

| Mod | What it does | What you need |
| --- | --- | --- |
| Widescreen 16:9 | The community 16:9 code (Dolphin's GZLE01 Gecko code): a wider field of view with the HUD placed for 16:9, letterboxed on a 4:3 iPad | Nothing; it is built in |
| Widescreen 16:10 | The same code with its aspect-dependent values recomputed for 16:10 (Mac displays, most iPads' shape is closer to it); one widescreen mod at a time | Nothing; it is built in |
| HD Texture Pack | Replaces the game's textures with a Dolphin-format pack, such as [Hypatia's HD pack](https://forums.dolphin-emu.org/Thread-hypatia-s-tloz-the-wind-waker-hd-pack-v2-0001a) | The pack's `tex1_…` images in `Documents/BlueWake/Load/Textures/GZLE01` |
| Better Wind Waker | [Better Wind Waker](https://github.com/WideBoner/betterww)'s quality-of-life changes from Wind Waker HD, each its own setting: Swift or Brisk Sail, instant text, faster rolling, grappling, block pushing, climbing and crawling, Tingle Chests without the Tuner, an unrestricted boat, no song replays, turning while swinging, a faster Ballad of Gales, skipping the opening movie, an inverted camera, a revealed sea chart | Nothing; it is built in |

On the iPad, `Documents/BlueWake` is **On My iPad › BlueWake › BlueWake** in the Files app.

## Installing

**Widescreen:** turn on *Widescreen 16:9* or *Widescreen 16:10* (turning one on turns the other off) and
restart BlueWake. The 16:10 code is generated from the 16:9 one by `scripts/mods/widescreen_aspect.py`:
each value it changes (the camera aspect, which also widens the game's view culling, the 2D bounds, HUD
and map positions, a few instruction immediates) is interpolated between the game's 4:3 value and the
16:9 value, and the three HUD constants it loads from the game's pool take the nearest pool value. On the
Mac host, `BLUEWAKE_ASPECT=16:10` or `16:9` selects the mod and the frame buffer shape together. At the default 3× render resolution 16:9
renders 2560×1440 and 16:10 2304×1440 (the 4:3 picture's height, widened), so the game's letterbox bars line up with the scene;
if an older device struggles, 2× renders 1707×960.

**HD textures:** download a Dolphin-format pack for GZLE01. On the iPad, Hypatia's *Android-Lite* build
(3× resolution, about 530 MB of PNG files) fits comfortably; the full-size PC builds need several GB of
memory. Copy the folder of `tex1_…` images (in Hypatia's pack, the `GZL` folder) into
`Load/Textures/GZLE01`, turn on *HD Texture Pack* and restart. Subfolders are searched, `_mipN`
sidecar mipmaps are used, and PNG and DDS files both work. The menu shows how many textures it found.

**Better Wind Waker:** turn on *Better Wind Waker*, pick its settings in **⋯ › Mods › Better Wind Waker
Settings** and restart. Each of Better Wind Waker's settings is a switch of its own, on or off by Better
Wind Waker's defaults until you change it: instant text (and holding B to advance), Swift Sail (or Brisk
Sail, which brakes harder), faster rolling, grappling, block pushing and climbing, Tingle Chests without
the Tingle Tuner, turning while swinging, no song replays and a faster Ballad of Gales start on;
an unrestricted boat, skipping the opening movie, an inverted camera and revealing the sea chart start
off. No patched disc is needed. Swift Sail's new sail texture and icons are Wind Waker HD's art and are
not included (an HD texture pack can supply them); its name stays "Sail". On the Mac host,
`BLUEWAKE_MODS=betterww` turns the settings on and `BLUEWAKE_OPTIONS=name,-name,...` changes them
(`none` first turns them all off); `scripts/mac/run_host.sh` takes `BWW=1` and `OPTIONS=`.

## Experimental wall climbing

The Windows Enhancements tab's **Climb any wall (experimental)** uses Wind Waker's
ivy animations and collision checks on ordinary steep walls. Steer toward a
wall to grab it; neutral input does not start a grab. The game still decides
whether a surface supports climbing, sidling or pulling onto a ledge.

Ordinary walls can tilt up to 30 degrees from vertical. The initial grab's
collision probes can also land on neighboring faces whose normals differ by
up to 30 degrees, accommodating Wind Waker's sloped and faceted scenery.
Both probes must hit real plain-wall geometry in the same registered background.
The game's facing, ground and ledge checks still apply; a missed collision ray
does not keep Link attached across a gap. Natural ivy keeps its original limits.

Grab validation adapts the approach used by Shipwright's
[FixVineFall and climbing controls](https://github.com/HarbourMasters/Shipwright/blob/cb71e22a79bc5d1f688fa881795bbd93094895fc/soh/src/overlays/actors/ovl_player_actor/z_player.c#L11324)
to Wind Waker's player and collision data. A grab belongs to the current player
and collision query; old checks cannot keep supporting it after a scene change,
pause or cutscene.

The stamina wheel advances with the game's 60 Hz clock, independently of display
interpolation. Moving drains it at the selected rate; hanging still costs 40% of
that rate. When empty, Link lets go and must refill before grabbing another
ordinary wall. Standing or walking on solid ground starts refilling after half
a second, taking three seconds from empty to full. Natural ivy and ladders keep
their normal behavior and do not consume this stamina.

## Better Wind Waker's settings as game options

Better Wind Waker patches the disc: assembly patches to the executable and modules, added code, changed
values and changed message data. BlueWake does the same things at runtime, from
`mods/betterww/options.txt`:

- **Option sites.** The instructions a setting changes (a `nop`, a branch made unconditional, a
  different constant) are listed with the setting's switch. DolRecomp's `--option-sites` translates each
  site both ways and picks one by `dolrecomp_option_flags[switch]` at run time, so the game is translated
  once for every combination of settings. Only the 15 chunks that hold a site differ from the base
  translation; they are the `betterww` mod's variants (3 more combine with a widescreen mod).
- **Native code.** What Better Wind Waker's added assembly did is C in `runtime/host/src/game_options.c`,
  called from hook sites: turning with the stick while swinging on a rope, negating the camera's
  C-stick axis, turning the wind to blow from behind King of Red Lions (through the game's own
  `dKyw_tact_wind_set`, entered from the hook) and Swift Sail's braking.
- **Values.** The constants a setting changes (rolling speed, block pushing frames, the boat's speed) are
  written at boot while it is on; a module's go to its data at the linked address, where it stays for the
  session.
- **Message data.** For instant text the host patches the loaded messages as Better Wind Waker patches the
  disc's: every message draws its whole box at once and its timed waits wait no time.

Better Wind Waker's other changes are left out: its randomizer fixes, the custom player model and colours,
random enemy colours and the title and memory-card art.

## Named asset packs

Managed texture packs live in the app's data folder under
`AssetPacks/<folder>/pack.ini`. A pack is initially disabled. The Mods menu lists
its name, content fingerprint, availability and saved order on Windows. Enable HD textures
and the desired packs, then restart. Selection and order changes do not replace
registrations in a running renderer. Explicit Refresh rescans the catalog;
large packs can take time to fingerprint.

An example manifest is:

```ini
[pack]
version=1
id=my-textures
name=My texture pack
kind=textures
game=GZLE01
root=textures
```

Place the Dolphin-format `tex1_...png`/`.dds` files under that pack's `textures`
directory. Subdirectories and sidecar mipmaps use Aurora's existing loader.
`id` must be unique and use lowercase ASCII letters, digits, `_` or `-` (at most
64 characters). `name` is UTF-8. `root` is a relative directory inside the pack;
use `/` for subdirectories. Absolute paths, `.`/`..`, and linked files or
directories are rejected. Optional `author`, `license` and `pack_version` fields
are metadata; they do not grant permission to redistribute assets.

The saved list is ordered from lowest to highest priority. Later enabled packs
win when they register the same exact texture source key. Aurora provides the
keys and chooses the registry winner; the managed-to-managed conflict report does not infer keys
from image names. Within one pack, Aurora's deterministic path sort chooses the
first duplicate key. Exact, palette-wildcard and texture-wildcard lookups retain
Aurora's existing specificity rules; a wildcard is not an exact-key conflict.
The legacy `DOL_AURORA_TEXTURE_PACK` directory retains priority zero. Managed
packs use priorities one and above; unloading managed groups preserves legacy
and other registrations.

An enabled selection pins an XXH3-128 content fingerprint covering the manifest,
portable relative filenames and actual file bytes. This is a revision identifier,
not an authenticity signature. Missing packs, duplicate IDs, invalid manifests
and mismatched fingerprints are skipped. Explicitly re-enable a changed pack to
accept its new revision. Before guest execution, the managed loader registers
and validates files through Aurora's real PNG/DDS decoder. A broken higher pack
is fully unregistered so a valid lower pack or the original game texture remains
available. Startup inspection bounds each image to 16,384 pixels per dimension
and 256 MiB per file and aggregate decoded RGBA budget. Managed inspection rejects
malformed contiguous sidecars and the first surplus mip level. Native loading
stops at the complete legal mip chain; suffixes after a gap remain ignored.
Registration and
decode tests do not claim a rendered visual result. Avoid editing pack files while
the game is open: Aurora reads file replacements on demand.

On Windows, open **Mods > Named texture packs** and use **Refresh pack catalog**
after installing or editing files. The panel keeps its cached rows visible while
the filesystem scan runs. Enable packs and move them with **Lower priority** or
**Higher priority**; later enabled packs win exact-key conflicts. The panel shows
the requested revision separately from registrations loaded at this launch.
Selection/order changes are saved and require a restart. A failed save rolls the
unsaved request back and reports the error. HD textures must be enabled at launch
for managed packs to load. Shutdown waits for an outstanding filesystem task
before removing this renderer's managed registrations.

Pack selections are saved atomically in `asset_packs.ini`. Portable
`.bwpackpreset` files contain only pack IDs, enable state, order and fingerprints.
They contain no absolute local paths, texture files, game code, saves, control
bindings or scalar settings. A recipient installs matching packs separately;
missing or different revisions remain visible and are skipped. Malformed preset
imports preserve the current selection. Ordinary `.bwpreset` settings and
`controls.ini` remain separate.

Use **Import pack preset** or **Export pack preset** in that panel for these
portable arrangements. Export uses the separate `.bwpackpreset` format; importing
does not install or download the referenced textures. The loaded-conflict list
comes from Aurora's actual exact-key registrations for this launch. Legacy packs
and wildcard specificity retain Aurora's selection rules.

These manifests load **texture assets**. They cannot load arbitrary gameplay
code, Gecko patches, translated modules, models, audio or randomizer logic.
Widescreen and Better Wind Waker gameplay changes remain compiled options in the
app. A manifest with `kind=gameplay` is reported as unsupported rather than being
executed as an asset pack. Model/audio pack formats and gameplay-mod dependency
resolution remain separate work.

## How code mods work in a static recompilation

BlueWake runs the game from native code translated ahead of time, so a mod that rewrites game code in
memory (a Gecko code, or a patcher's assembly changes) has no effect at runtime: the instructions it
writes are never executed. Code mods are therefore built into the app:

1. The mod is applied to the game's executable on the Mac (`scripts/mods/gecko_apply.py` for a Gecko
   code), or for game options the translator is given the option sites (above).
2. The patched (or option-sited) `main.dol` and RELs are translated by the same DolRecomp as the base
   game and merged by `scripts/generate_composite.py` into a composite source tree of their own.
3. `scripts/mods/build_mod_variants.py` compares that tree with the base tree. Every translated chunk
   that differs is added to the base composite under a new name (`chunks_mod_<name>/`), code ranges
   only the mod has become extra chunks, and the bytes the mod
   changes in the executable's data and in the relocated REL data become writes. A Gecko code's data
   writes are re-applied at every retrace, as Dolphin does. When two mods change the same chunk, a
   translation of the game patched by both supplies the variant used when both are enabled.
4. At boot the host enables the mods named in `BLUEWAKE_MODS` (the menu sets it):
   `bluewake_composite_apply_mods` points the chunk table at the variants before the first dispatch,
   and the writes go into guest RAM. Chunks never call each other directly, so a variant replaces its
   chunk cleanly, and the base game is untouched when a mod is off.

The chunks include `generated.h`, which is unchanged, and only `module_export.c` sees the dispatcher
with the writable chunk table. Variant chunks are additional compile inputs alongside the 748 base
chunks. The builder reports their count; the October 4 Windows mod-enabled preparation generated
65 variant chunk files. Changes to shared preparation or optimization profiles can also rebuild the
base chunks.

HD textures need no build step. Aurora's Dolphin-compatible texture replacement (the
`tex1_WxH[_m]_<XXH64>[_<palette XXH64>]_<format>` names, with palettes hashed over the entries the
texture uses) is connected to GXRuntime's texture decode: the first time a texture's bytes are seen,
the pack is consulted before decoding, and the replacement is cached under the same key as a decoded
texture would be. Replacements are decoded on a background thread the first time they are used (the original texture
is drawn meanwhile), so a new area shows its HD textures a moment later instead of stalling a frame.

## Measured on the iPad Pro (M2)

These results use the developer's optimized build. They do not establish performance parity for a
fresh player build; see [the current performance comparison](BUILDER.md#optimization-profiles).

- Widescreen and the HD pack together, on the pier after loading slot 1: 30 FPS at 100% speed, no late
  frames over more than a minute, main thread about 82% busy (the same as without mods). 5,739 of
  the pack's textures registered; the first file-select frame with new textures took 83 ms.
- All three on the iPad: loading slot 1 and walking the village at 29.9-30 FPS; the pause-menu save
  writes the card, and the saved card reloads with all three mods and with none (30 FPS both). The
  menu tree, checked with `BLUEWAKE_MENU_DUMP=1`, lists the Mods section and the installed pack.
- Better Wind Waker (the patched disc, before the settings became game options): its added code section
  ran (guest program-counter samples), and the Swift Sail was used at sea on the iPad on 2026-09-28.

## Building

The Windows builder (`scripts/windows/build.py`) includes these variants by
default too. Keep them in normal builds; `--no-mods` is an explicit opt-out for
base-game comparisons and diagnosis. Compiling the variants does not turn every
mod on: use Windows Display for widescreen and Enhancements for Better Wind Waker
and its individual settings, then restart the game. Mods manages installed asset
packs and identifies compiled gameplay enhancements. A build made with `--no-mods` needs
rebuilding before those code mods can take effect.

The Builder (`scripts/builder/build.sh`, or `scripts/ios/build_device.sh`) adds the mods itself, as its step 6;
`--no-mods` leaves them out. It needs only python3. The step runs

```sh
scripts/mods/build_mods.sh BUILD_DIR
```

which applies the widescreen codes, translates the widescreen executables, the game with Better Wind
Waker's option sites (its executable and modules) and each widescreen executable with them, with the
build's own DolRecomp, generates a composite tree for each, and adds the variants, the option table and
the options' values to the build's composite source before the compile. Nothing needs copying to the
device.
