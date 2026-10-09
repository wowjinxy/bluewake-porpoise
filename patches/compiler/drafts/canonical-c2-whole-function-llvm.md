# Canonical C2 whole-function LLVM prototype

Experimental source patch; inactive in every game build. No game performance
improvement or runtime equivalence is established.

Apply `canonical-c2-whole-function-llvm.patch` to ExpansionPak/DolRecomp commit
`c876e2b9e022e084aa9a690fe15e2ad79a0706cd`. The patch preserves the ordinary
backend behind a separate `DOLRECOMP_CANONICAL_CPU` build and explicit
`canonical_c2` option. It retains the donor's GPL-3.0-or-later license. The
canonical CPU/types headers retain their source SPDX notices and match the
existing RecompCore headers; they do not define a replacement runtime.

This mode requires a complete, independently verified original C2 scheduling
ledger for every input instruction. It preserves precise/paid leader charges,
PC staleness, suffix/refund frontiers, callbacks, selected translated callees
and return continuations. It uses the existing 3552-byte CPU layout, ordinary
Windows C entry ABI, exact semantics and local register SSA. Unknown memory
accesses always use canonical C2 services and reload callback-mutated state.
Canonical LFS widening uses integer operations to preserve all source bits,
including signalling NaNs. Canonical Windows output requests asynchronous
unwind tables. Legacy defaults remain unchanged.

The patch declares integration services; it does not implement a general game
integration. A consumer must provide their exact original C2 call/memory
semantics. Native argument/result ABI, the ModernGekko runtime, lockstep and
state-in-memory modes are rejected. Object caches and PGO are disabled in the
tested private consumer. Arbitrary option combinations, other games/platforms
and general reuse are unqualified.

Actual LLVM 19 compilation covers all 49 generator translation units. A real
93-instruction function emits all 93 resume entries, its complete loop/call CFG,
and matching Win64 unwind coverage. Three original-C2 adapter translation units
also compile. No emitted game code was linked into a game module or executed.
The conservative memory version has substantial state traffic at 26 memory
service sites; compilation alone does not imply a useful speedup.

Outer routing still needs an explicit return-ownership channel. Inferring a
return from CPU.pc can confuse a budget/readiness side exit with an original
central transfer, or bypass the original scoped return gate. Do not route the
existing game to this prototype until that integration and differential tests
are complete.

The generic source patch has SHA-256
`ae24c1fbe7f622ffdf297dfda22b4d8fd214095e9a5ef5b3e9fab6687ddaa66d`.
Its private development commits are `19af1a8d` and `40d237d4`. The public patch
contains no generated game source, instruction words, scheduling ledger,
compiled module, disc content or player data. Actual local evidence is recorded
in `docs/research/DEEP_DEBUG_2026-10-08.md`.
