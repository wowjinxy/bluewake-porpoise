These are the exact scripts used to prepare and qualify the authored interface
fixture in `build/k7-interfaces-20261008/cpu1`. `run_cpu.py` is the frozen runner
copy from successful attempt1. Its control header/generator rename only
`gxruntime::gxcore` to `gxruntime::interface_control` in the exact repaired
uniform source2 files (plus newline normalization).

The scripts depend on the original private layout, compiler/runtime bank and
owned hidden-process helper. Copying this folder does not make a standalone
public test runner. No private outputs or binary dependencies are included.
See `../../CPU_QUALIFICATION.md` for evidence boundaries and private receipts.
