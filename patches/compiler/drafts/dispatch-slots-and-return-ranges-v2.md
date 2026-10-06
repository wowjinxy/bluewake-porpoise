Items 10 and 11 are unpromoted, opt-in source preparations. The slot rotation adapts AceSpectre's `b6d5e7cb7c16103f4c3ef68b4d721f4a82977380`; the budget-first return-range guard adapts elliotttate's `7aca42ade32d41b38a3810d5ba6f28bd37127e4b`. Human attribution is retained. Neither draft connects to the normal builder or changes live SDK/generated/module bytes.

The helper recognizes exact raw-PC entry, cold, and return switch shapes. Cases must be aligned, unique, and name one matching label in the same function; defaults and the existing return budget guard must match. Computed-goto entries, healing-return guards, unknown case bodies, malformed switches, and unsupported source are retained unchanged. Both transformations are selected explicitly, and file mutation additionally requires `--apply`. Prepare after native-body certification and before recording the final prepared-source digest; changed profiles need retraining.

The final source parser passed 64 positive and adversarial checks. It also declines changing CPU macros, slot macros, and unmarked existing slot helpers; these guards leave the actual O3/ASan and assembly fixture bodies byte-identical to the tested candidates. Authored translated bodies using the actual 3,552-byte CPU layout and maintained charge/precharge helpers passed 5,460,525 exact whole-CPU comparisons in both O3 and ASan. This covers legal cases, every gap and unaligned offset in the test range, aliases, outside/wrap targets, budget cuts, deadline state, and all four configurations (original, slots, ranges, both).

The regular-stride assembly control showed no benefit: Clang already generates the same rotated jump table. That negative is retained. A second assembly-only probe copies the exact 487-case irregular return map from current prepared chunk 0001, with authored pure instruction bodies. Actual optimized dispatch-prefix comparisons are 11 for the original, 2 for slots, 12 for ranges, and 3 for both. Whole-function comparison sites fall from 915 to 391 with slots. The range guard adds one early unsigned interval rejection before the unchanged return search; it adds work to in-range paths. These are code-generation facts, not measured gameplay or FPS gains.

Native gameplay A/B, profile retraining, combined preparation order, and frame-time/code-size effects remain pending. The current prepared entry path already uses a dense computed-goto table and stays untouched. The test-source draft is separate from the preparation draft and cannot activate it.

The v2 draft pair supersedes the earlier draft exports without changing their preserved evidence. Only the opt-in parser guard and its adversarial tests changed; compiled positive case bodies are byte-identical. No compiled target was rerun for this source-only guard update.

The reviewed preparer and authored fixtures are now available in the public tree. `python -B tests/prepare_dispatch_cases.py` passed all 64 parser checks after integration.

The later builder integration exposes `--dispatch-slots` and `--return-ranges`,
both off by default and reversible with their `--no-...` forms. Preparation runs
after all native certification and memory/direct-call preparation. Each option,
the selected preparer bytes and the final source digest bind preparation reuse,
training and provenance. Earlier statements above describe the inactive export
stage. Full translated-module/native performance qualification remains pending.
