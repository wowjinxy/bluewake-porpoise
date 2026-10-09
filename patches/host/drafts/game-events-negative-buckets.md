# Event observer negative-bucket experiment

Unapplied host-source patch. The active observer and installed game are unchanged.

This patch rejects addresses outside a conservative union of the six existing
static observer entry buckets and live pending-return buckets. Bucket hits still
run the exact original predicates. Cancellation can leave stale bits; those
cause extra original checks rather than a cached observation verdict.

The constants bind the current static entries and `return_bucket` function.
Changing either requires regenerating and qualifying the bucket set. No observer
mask, callback lifetime, pending capacity, or return expiration rule changes.

Private whole-translation-unit differential fixtures passed 37,758 comparisons
in both optimized and sanitizer profiles, covering all 2,048 masks and all 256
buckets, static collisions, return arming/clearing/expiry, callback mutations and
capacity. Missing-static-entry and missing-armed-return mutants failed the
full-state comparison. A one-object host link changed only this observer;
fresh title replay comparisons matched six complete states and full P6 pixels.

The fixed eight-run clean comparison had descriptive mean reductions of 7.59%
dispatch-thread CPU and 8.25% wall time. Its third CPU pair increased 0.44%,
failing the declared requirement that all four CPU and wall pairs improve.
Background load was uncontrolled. This is preserved as an inconclusive candidate,
with no rescue runs or accepted performance/promotion claim.

Validation receipts and scope are recorded in
`docs/research/DEEP_DEBUG_2026-10-08.md`, under the event-filter experiment.
The private fixture, build, code-generation, correctness and independent raw-log
audit receipts remain under `build/deep-debug-20261008/event-negative1/`.
