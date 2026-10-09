# Optional fixed-MEM1 accesses and central-return relay

Apply after `canonical-c2-whole-function-llvm.patch`. This GPL compiler delta
remains inactive in game builds. Both new options are disabled by default.
Its SHA-256 is
`fe55604403758ee0e6ade32fba76819f036e2a2d59719a186b002a3ec7fe598d`.

`canonical_c2_fixed_mem1` asserts the existing fixed-storage module ABI 5,
canonical CPU layout ABI 6, exact `bw_guest_mem1` provider and 32 MiB extent.
The original host validates and adopts disjoint CPU/MEM1 storage. This option
must stay off for other storage/provider policies. Each access freshly checks
raw address bounds and alias overlap; stores also check the write journal.
Normal RAM uses unaligned endian accesses and exact matching reservation
invalidation, keeping unrelated guest registers local. All other accesses use
the canonical service once and reload callback-mutated state. Deferred FPRF
is materialized locally before the fast/slow split, preserving its join.

`canonical_c2_return_dispatch_relay` requires a paired scoped outer adapter.
Only an actual guest return that passes its original budget check writes the
central-transfer reason. Other exits leave the adapter's default return reason
unchanged. The original central dispatcher, range checks and nested scoped
return gate remain authoritative. A C automatic reason and saved/restored
pointer isolate nested entries. No CPU field or public function ABI changes.
This prototype requires the existing single game thread; concurrent calls and
nonlocal native unwinding through that C scope are unqualified.

All 49 backend/consumer translation units compile with these options enabled,
and emit the complete private 93-instruction function. The routed original C2
chunk also compiles. Actual guarded-path and unwind inspection passes, and a
private module links with the original PE interface and resources preserved.
Runtime differential testing remains pending. These checks establish neither
gameplay parity nor a performance improvement. The generic patch excludes the original game
adapter, instruction words, scheduling ledger and compiled game code.

Private development commit: `c989f634`. Retained source, compile and inspection
receipts are recorded in `docs/research/DEEP_DEBUG_2026-10-08.md`.
