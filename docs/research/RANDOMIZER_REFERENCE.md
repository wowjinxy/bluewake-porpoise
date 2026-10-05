# Wind Waker randomizer reference

[LagoLunatic's MIT-licensed randomizer](https://github.com/LagoLunatic/wwrando/tree/9775811b6fd039992822fbaf30ea6259b339f4b0)
is pinned in [the reference manifest](../../config/wwrando-source-reference.json).
The source audit reads public catalog/logic data only. It executes no upstream
Python and accesses no disc, game module or card.

The public source catalog and C++17 logic library now contain 320 locations,
307 requirement macros, 237 item names and seven patch-path kinds. Seventy-seven
locations have multiple paths. A location's paths describe one reward implemented
at several sites; treating each path as a separate reward would duplicate it.
The imported data and compiled catalog retain the upstream
[MIT notice](../../LICENSES/wwrando-MIT.txt).

The passive library is defined by `cmake/BlueWakeRandomizer.cmake`. The Windows
regression build tests it without linking it into the game host. Optimized and
AddressSanitizer fixtures each pass 11,448 checks, including 3,135 comparisons
against an independent oracle over the actual requirement corpus. These use
explicit hypothetical inventories and options, with no assumed starting items.

A second passive library, `cmake/BlueWakeRandomizerNativeContext.cmake`, projects
copied observations of the Wind Waker, Grappling Hook and Wind's Requiem. It
distinguishes known absence from missing, contradictory, stale or unsafe data.
Only those three ownership queries and the two exactly pinned upstream
capabilities for playing Wind's Requiem and defeating Gohma can be evaluated.
It does not infer progressive swords from equipped or borrowed equipment.
Those capabilities describe imported requirements, rather than native access
to a song or boss encounter.

Optimized and AddressSanitizer synthetic fixtures each pass 14,810 checks. The
actual Windows regression caller also runs both logic and inventory fixtures
with `BUILD_TESTING=OFF`; duplicate includes register each once and the existing
aggregate discovers the new executable. This is a copied-value projection,
without a native reader, tracker, game-host link or seed consumer. A future
collector must establish genuine native boundary and lifetime ownership before
supplying these observations.

The parser rejects unknown conditions, missing or mistyped options, cycles and
ambiguous mixed operators. Each evaluation creates its own macro/location memo
and allows at most 32,768 counted context/member/node/reference visits; exceeding
that budget makes the entire result invalid and unreachable. Ordinary OR
branches still validate eagerly. Separate text/token/depth limits apply; this is
not a wall-clock or external-catalog-loader bound.

The compiled port needs different adapters. Upstream DOL/REL byte patches
cannot update translated instructions. Chest, event, actor and salvage indexes
need native ownership and scene/layer contracts. Upstream logic assumes its
starting state and gameplay changes; importing it does not establish vanilla
beatability.

Source-schema IDs include the pinned revision, record kind and name hash. They
are not native reward, seed, card or room identities. For example, the catalog's
`LinkUG/Stage.arc/Chest000` names an authored chest entry; the original game uses
that entry's parameters to select opened-chest bit 5 in saved MISC table 11.
The original chest's earned reward, genuine autosave and fresh reload are
qualified separately. That evidence does not supply a randomizer reward ledger.

Implement in this order:

1. Read-only native inventory context with explicit supported options and
   audited vanilla/start-state differences, then a passive tracker.
2. Native location identities and a durable exact-once reward ledger.
3. Deterministic placement, reachability spheres and goal verification.
4. Seed identity tied to card save/backup/import and co-op room compatibility.
5. Entrance shuffling and plando with the same verifier.

Seed generation and native reward replacement remain disabled. Reachability in
the imported logic does not yet establish access or beatability in the native
port.
