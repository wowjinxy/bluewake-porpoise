#!/usr/bin/env python3
"""Synthetic Windows build-cache checks; no disc or translated game source."""
import importlib.util
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("windows_builder", REPO / "scripts/windows/build.py")
bw = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bw)
fast_spec = importlib.util.spec_from_file_location("fast_blocks", REPO / "scripts/windows/fast_blocks.py")
fast = importlib.util.module_from_spec(fast_spec)
fast_spec.loader.exec_module(fast)

# Keep synthetic translation fixtures LF on every host, matching DolRecomp output.
# A small invented instruction block with the translator's public charge form.
CHUNK = '''#include "../generated.h"
void synthetic(CPUState* ctx) {
    bool cycle_block_prepaid;
    ctx->pc = 0x80001000u;
    cycle_block_prepaid = dolrecomp_block_can_precharge(ctx, 2u);
    if (ctx->downcount <= -(s64)DOLRECOMP_C_LOOP_CYCLE_BUDGET) {
        ctx->pc = 0x80001000u;
        return;
    }
    ctx->downcount -= cycle_block_prepaid ? 2u : 1u;
    ctx->gpr[3] += 1u;
    ctx->pc = 0x80001004u;
    if (!cycle_block_prepaid && !dolrecomp_charge_precise(ctx, 1u, 0x80001004u)) return;
    ctx->gpr[4] += 1u;
    ctx->pc = ctx->lr;
    return;
}
'''
MARK = "bluewake: prepaid block copies"


class PreparedBlockSelectionTest(unittest.TestCase):
    def test_native_vector_certification_survives_block_preparation(self):
        source = CHUNK.replace('80001000', '8030DEAC').replace('80001004', '8030DEB0')
        self.assertEqual(fast.transform(source)[1], 1)
        routed = source.replace('#include "../generated.h"',
                                '#include "../generated.h"\n#include "native_vec.h"')
        self.assertEqual(fast.transform(routed), (routed, 0))

    def test_retains_pc_and_prepaid_observation_suffix(self):
        source = CHUNK.replace("    ctx->gpr[4] += 1u;",
                               "    ctx->cycle_observation_suffix = cycle_block_prepaid ? 1u : 0u;\n"
                               "    ctx->gpr[4] += 1u;")
        converted, count = fast.transform(source)
        self.assertEqual(count, 1)
        copy = converted.split("bwfast_0:\n", 1)[1]
        self.assertIn("ctx->pc = 0x80001004u;", copy)
        self.assertIn("ctx->cycle_observation_suffix = 1u;", copy)
        self.assertNotIn("dolrecomp_charge_precise", copy)
        self.assertEqual(fast.transform(converted), (converted, 0))

    def test_refund_and_unknown_prepaid_forms_keep_original_body(self):
        refund = """    if (cycle_block_prepaid &&
        ctx->cycle_deadline_budget > 0 &&
        (s64)ctx->cycle_observation_suffix > ctx->cycle_deadline_budget) {
        ctx->downcount += (s64)ctx->cycle_observation_suffix;
        cycle_block_prepaid = false;
    }
"""
        for unsupported in (refund, "    cycle_block_prepaid = false;\n",
                            "    synthetic_observe(cycle_block_prepaid);\n"):
            with self.subTest(form=unsupported):
                source = CHUNK.replace("    ctx->gpr[4] += 1u;",
                                       unsupported + "    ctx->gpr[4] += 1u;")
                self.assertEqual(fast.transform(source), (source, 0))


class PreparedCacheTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for script in ("scripts/ios/composite_manifest.py", "scripts/windows/fast_blocks.py",
                       "scripts/windows/global_guest_cpu.py", "scripts/windows/chunk_headers.py",
                       "cmake/composite/inline_fp.h", "cmake/composite/gather_pipe.h",
                       "cmake/composite/gather_pipe.c", "cmake/composite/gather_pipe_batch.h",
                       "scripts/windows/direct_calls.py", "cmake/composite/direct_calls.c",
                       "cmake/composite/direct_calls.h", "cmake/composite/inline_gpr.h",
                       "scripts/windows/inline_save_restore_gpr.py", "scripts/mods/prepare_native_j3d.py",
                       "cmake/composite/native_j3d.c", "cmake/composite/native_j3d.h",
                       "scripts/mods/prepare_native_vec.py", "cmake/composite/native_vec.c", "cmake/composite/native_vec.h",
                       "scripts/windows/native_game_math.py", "cmake/composite/native_game_math.c", "cmake/composite/native_game_math.h",
                       "scripts/windows/native_skin.py", "cmake/composite/native_skin.c", "cmake/composite/native_skin.h",
                       "scripts/mods/prepare_native_math.py", "cmake/composite/native_math.c", "cmake/composite/native_math.h",
                       "cmake/composite/native_work_pool.c", "cmake/composite/native_work_pool.h",
                       "scripts/windows/lean_memory.py", "scripts/windows/native_entries.py",
                       "cmake/composite/native_entries.c", "cmake/composite/native_entries.h",
                       "cmake/composite/native_fifo.c", "cmake/composite/native_fifo.h",
                       "cmake/composite/native_bg.c", "cmake/composite/native_bg.h",
                       "cmake/composite/native_mtxcalc.c", "cmake/composite/native_mtxcalc.h"):
            dst = self.root / script
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REPO / script, dst)
        self.base = self.root / "synthetic-base"
        (self.base / "chunks_dol").mkdir(parents=True)
        (self.base / "generated.h").write_text("/* synthetic fixture */\n", newline="\n")
        (self.base / "generated_composite.h").write_text(
            "static DolRecompFunction s_dolrecomp_chunk_fns[] = {func_80001000};\n", newline="\n")
        for name in ("a.c", "b.c"):
            (self.base / "chunks_dol" / name).write_text(CHUNK, newline="\n")
        self.out = self.root / "build"
        self.out.mkdir()
        self.args = SimpleNamespace(out=self.out, accept_new_composite=False, prepared_blocks=False, fixed_cpu=False, fixed_mem1=False, inline_fp=False, gather_pipe=False, direct_calls=False, inline_gpr=False, native_j3d=False, native_vec=False, native_math=False, native_skin=False, native_game_math=False, native_entries=False, lean_memory=False)
        self.builder = bw.Builder(self.args)
        self.builder.mods = False
        self.builder.composite = lambda *args: shutil.copytree(self.base, args[-2])
        self.addCleanup(patch.stopall)
        patch.object(bw, "ROOT", self.root).start()
        self.digest = bw.tree_digest(self.base)
        patch.object(bw, "profile_value", lambda name: self.digest).start()

    def cycle(self):
        self.builder.generate()
        self.builder.prepare_blocks()

    def chunk(self, name="a.c"):
        return self.out / "composite-src/chunks_dol" / name

    def test_direct_calls_reuse_disable_and_host_watch_changes(self):
        source = '''#include "../generated.h"
void synthetic(CPUState* ctx) {
    // 80004000: bl      0x80006000
    {
            ctx->lr = 0x80004004u;
            ctx->pc = 0x80006000u;
            return;
    }
label_80004004:
    return;
}
'''
        (self.base / "chunks_dol/a.c").write_text(source, newline="\n")
        (self.base / "generated_composite.h").write_text(
            "static DolRecompFunction s_dolrecomp_chunk_fns[] = {func_80004000, func_80006000};\n", newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.direct_calls = True
        self.cycle()
        self.assertIn("bw_chunk_fns[1](ctx)", self.chunk().read_text())
        receipt = json.loads((self.out / "prepared-blocks.json").read_text())
        self.assertTrue(receipt["direct_calls"])
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        host = self.root / "runtime/host/src/synthetic.c"
        host.parent.mkdir(parents=True)
        host.write_text("/* host now watches 0x80006000u */\n", newline="\n")
        self.cycle()
        self.assertNotIn("bw_chunk_fns", self.chunk().read_text())
        self.assertIn("0x80006000u", (self.out / "composite-src/bw_edge_watch.inc").read_text())
        self.args.direct_calls = False
        self.cycle()
        self.assertEqual(self.chunk().read_text(), source)
        self.assertNotIn("BLUEWAKE_DIRECT_CALLS_PREPARED",
                         (self.out / "composite-src/generated.h").read_text())
        self.assertFalse((self.out / "composite-src/bw_edge_watch.inc").exists())

    def test_host_implementation_edits_reuse_cache_but_watch_changes_invalidate(self):
        self.args.direct_calls = True
        host = self.root / "runtime/host/src/synthetic.c"
        host.parent.mkdir(parents=True)
        host.write_text("/* observed 0x80006000u */\n", newline="\n")
        self.cycle()
        original = (self.out / "composite-inputs.digest").read_text()
        tree_before = bw.tree_digest(self.out / "composite-src")
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        # Same observation set, including the mirrored spelling, still uses
        # the exact prepared tree; non-address host edits cannot change it.
        host.write_text("int unrelated_host_work(void) { return 7; }\n/* observed 0xC0006000u */\n", newline="\n")
        self.cycle()
        self.assertEqual(original, (self.out / "composite-inputs.digest").read_text())
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        self.assertEqual(tree_before, bw.tree_digest(self.out / "composite-src"))
        # A separate Windows host file introduces a new observation boundary.
        window = self.root / "windows/src/fixture.cpp"; window.parent.mkdir(parents=True)
        window.write_text("/* watched 0x80007000 */\n", newline="\n")
        self.cycle()
        changed = (self.out / "composite-inputs.digest").read_text()
        self.assertNotEqual(original, changed)
        self.assertIn("0x80007000u", (self.out / "composite-src/bw_edge_watch.inc").read_text())
        window.unlink()
        self.cycle()
        self.assertEqual(original, (self.out / "composite-inputs.digest").read_text())
        self.assertNotIn("0x80007000u", (self.out / "composite-src/bw_edge_watch.inc").read_text())

    def test_watch_scanner_and_direct_helpers_remain_fingerprinted(self):
        self.args.direct_calls = True
        self.cycle()
        for helper in ("scripts/windows/direct_calls.py", "cmake/composite/direct_calls.h", "cmake/composite/direct_calls.c"):
            original = (self.out / "composite-inputs.digest").read_text()
            path = self.root / helper
            path.write_text(path.read_text() + "\n", newline="\n")
            self.cycle()
            self.assertNotEqual(original, (self.out / "composite-inputs.digest").read_text())

    def test_lean_memory_enable_reuse_disable_and_script_change(self):
        self.args.prepared_blocks = self.args.gather_pipe = True
        self.cycle()
        baseline = (self.out / "composite-inputs.digest").read_text()
        self.args.lean_memory = True
        self.cycle()
        enabled = (self.out / "composite-inputs.digest").read_text()
        self.assertNotEqual(baseline, enabled)
        receipt = json.loads((self.out / "prepared-blocks.json").read_text())
        self.assertTrue(receipt["lean_memory"])
        self.assertEqual(receipt["lean_memory_script_sha256"], bw.sha256_file(self.root / "scripts/windows/lean_memory.py"))
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        path = self.root / "scripts/windows/lean_memory.py"
        path.write_text(path.read_text() + "\n# fixture revision\n", newline="\n")
        self.cycle()
        self.assertNotEqual(enabled, (self.out / "composite-inputs.digest").read_text())
        self.args.lean_memory = False
        self.cycle()
        self.assertFalse(json.loads((self.out / "prepared-blocks.json").read_text())["lean_memory"])

    def test_native_entries_certification_reuse_disable_and_helper_invalidation(self):
        body = '\nlabel_80004100:\n    ctx->gpr[3] = 1;\n'
        digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
        script = self.root / "scripts/windows/native_entries.py"
        text = script.read_text()
        begin = text.index('FRAGMENTS = {')
        end = text.index('def hook(', begin)
        definitions = (f"FRAGMENTS = {{'fixture': (0x80004000, 0x80004100, 0x80004104, '{digest}')}}\n"
                       "ENTRIES = {0x80004100: ('fixture',)}\n\n")
        script.write_text(text[:begin] + definitions + text[end:], newline="\n")
        (self.base / 'chunks_dol/chunk_80004000.c').write_text(
            '#include "../generated.h"\n' + body + '\nlabel_80004104:\n\nreturn_dispatch_80004000:\n', newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.native_entries = True
        self.cycle()
        source = self.out / 'composite-src/chunks_dol/chunk_80004000.c'
        self.assertIn('bluewake_native_entries_try', source.read_text())
        receipt = json.loads((self.out / 'prepared-blocks.json').read_text())
        self.assertTrue(receipt['native_entries'])
        manifest = self.out / 'composite-src/native_entries.json'
        self.assertEqual(receipt['native_entries_manifest_sha256'], bw.sha256_file(manifest))
        before = source.read_bytes(), source.stat().st_mtime_ns
        self.builder.generate()
        self.assertTrue(self.builder.preparation_current)
        # Do not recertify the original stage after subsequent transforms.
        with patch.object(self.builder, 'source_step', side_effect=AssertionError('unnecessary recertification')):
            self.builder.prepare_blocks()
        self.assertEqual(before, (source.read_bytes(), source.stat().st_mtime_ns))
        for helper in ('scripts/windows/native_entries.py', 'cmake/composite/native_entries.c',
                       'cmake/composite/native_mtxcalc.h'):
            old = (self.out / 'composite-inputs.digest').read_text()
            path = self.root / helper; path.write_text(path.read_text() + '\n', newline='\n')
            self.cycle()
            self.assertNotEqual(old, (self.out / 'composite-inputs.digest').read_text())
        # An altered certificate is part of the final source digest and forces
        # regeneration/certification, even though the chunk is unchanged.
        manifest.write_text('{"tampered":true}', newline='\n')
        self.cycle()
        self.assertEqual(json.loads(manifest.read_text())['entries'], [0x80004100])
        self.args.native_entries = False
        self.cycle()
        self.assertNotIn('native_entries_try', source.read_text())
        self.assertFalse(manifest.exists())

    def test_missing_or_invalid_receipt_regenerates_prepared_source(self):
        self.args.prepared_blocks = True
        self.cycle()
        receipt = self.out / 'prepared-blocks.json'
        for corrupt in (None, '{broken', '[]', '{"final_digest":"wrong"}'):
            with self.subTest(receipt=corrupt):
                if corrupt is None:
                    receipt.unlink()
                else:
                    receipt.write_text(corrupt)
                self.builder.generate()
                self.assertFalse(self.builder.preparation_current)
                self.assertNotIn(MARK, self.chunk().read_text())
                self.builder.prepare_blocks()
                self.assertIn(MARK, self.chunk().read_text())

    def test_edit_after_cache_validation_cannot_skip_preparation(self):
        self.args.prepared_blocks = True
        self.cycle()
        self.builder.generate()
        self.assertTrue(self.builder.preparation_current)
        self.chunk().write_text(self.chunk().read_text() + '\n/* edit after validation */\n', newline='\n')
        with self.assertRaises(bw.BuildError):
            self.builder.prepare_blocks()

    def test_native_j3d_reuse_and_disable(self):
        body = '\nlabel_00000100:\n    ctx->gpr[3] = 1;\n'
        digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
        script = self.root / "scripts/mods/prepare_native_j3d.py"
        text = script.read_text()
        start = text.index('LEAVES = (')
        end = text.index('INCLUDE = ', start)
        script.write_text(text[:start] + f"LEAVES = ((0x100, 0x104, {{'{digest}'}}),)\n" + text[end:], newline="\n")
        (self.base / 'chunks_dol/chunk_802D96E0.c').write_text(
            '#include "../generated.h"\n' + body + '\nlabel_00000104:\nreturn_dispatch_802D96E0:\n', newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.native_j3d = True
        self.cycle()
        source = self.out / 'composite-src/chunks_dol/chunk_802D96E0.c'
        self.assertIn('bluewake_native_j3d_try', source.read_text())
        self.assertIn('BLUEWAKE_NATIVE_J3D_PREPARED', (source.parent.parent / 'generated.h').read_text())
        before = source.read_bytes(), source.stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (source.read_bytes(), source.stat().st_mtime_ns))
        self.args.native_j3d = False
        self.cycle()
        self.assertNotIn('bluewake_native_j3d_try', source.read_text())
        self.assertNotIn('BLUEWAKE_NATIVE_J3D_PREPARED', (source.parent.parent / 'generated.h').read_text())
        self.assertFalse((source.parent.parent / 'native_j3d.json').exists())

    def test_native_math_reuse_and_disable(self):
        body = '\nlabel_8030D0C8:\n    ctx->gpr[3] = 1;\n'
        digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
        script = self.root / "scripts/mods/prepare_native_math.py"
        text = script.read_text()
        start = text.index('LEAVES = (')
        end = text.index('DECLARATION = ', start)
        script.write_text(text[:start] + f"LEAVES = ((0x8030D0C8, 0x8030D0FC, '803096E0', '{digest}'),)\n" + text[end:], newline="\n")
        (self.base / 'chunks_dol/chunk_803096E0.c').write_text('#include "../generated.h"\n' + body + '\nlabel_8030D0FC:\n', newline="\n")
        header = self.base / 'generated_composite.h'
        header.write_text('typedef void (*DolRecompFunction)(CPUState* ctx);\n'
                         '    if (s_cached_pc[cache_index] == address)\n'
                         '        return s_cached_pc_fn[cache_index];\n'
                         'static DolRecompFunction s_dolrecomp_chunk_fns[] = {func_80004000, func_803096E0};\n', newline="\n")
        self.chunk_source = '#include "../generated.h"\nvoid synthetic(CPUState* ctx) {\n    // 80004000: bl      0x8030D0C8\n    {\n            ctx->lr = 0x80004004u;\n            ctx->pc = 0x8030D0C8u;\n            return;\n    }\nlabel_80004004:\n    return;\n}\n'
        (self.base / 'chunks_dol/a.c').write_text(self.chunk_source, newline="\n")
        self.args.direct_calls = True
        self.digest = bw.tree_digest(self.base)
        self.args.native_math = True
        self.cycle()
        prepared = self.out / 'composite-src/generated_composite.h'
        self.assertIn('BLUEWAKE_NATIVE_MATH_CACHED', prepared.read_text())
        self.assertIn('bw_native_call(ctx, 0x8030D0C8u)', self.chunk().read_text())
        before = prepared.read_bytes(), prepared.stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (prepared.read_bytes(), prepared.stat().st_mtime_ns))
        self.args.native_math = False
        self.cycle()
        self.assertNotIn('BLUEWAKE_NATIVE_MATH_CACHED', prepared.read_text())
        self.assertNotIn('bw_native_call', self.chunk().read_text())
        self.assertIn('bw_chunk_fns[1](ctx)', self.chunk().read_text())
        self.assertFalse((prepared.parent / 'native_math.json').exists())

    def test_native_game_math_reuse_host_change_and_disable(self):
        body = '\nlabel_80000100:\n    ctx->gpr[3] = 1;\n'
        digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
        script = self.root / "scripts/windows/native_game_math.py"
        text = script.read_text()
        begin = text.index('FRAGMENTS = {')
        end = text.index('def canonical(', begin)
        definitions = (f"FRAGMENTS = {{'fixture': (0x80000100, 0x80000100, 0x80000104, '{digest}')}}\n"
                       "ENTRIES = {0x80000100: ('fixture',)}\n\n")
        script.write_text(text[:begin] + definitions + text[end:], newline="\n")
        (self.base / 'chunks_dol/chunk_80000100.c').write_text(
            '#include "../generated.h"\n' + body + '\nlabel_80000104:\n\nreturn_dispatch_80000100:\n', newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.native_game_math = True
        self.cycle()
        source = self.out / 'composite-src/chunks_dol/chunk_80000100.c'
        self.assertIn('bluewake_native_game_math_try', source.read_text())
        before = source.read_bytes(), source.stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (source.read_bytes(), source.stat().st_mtime_ns))
        fingerprint = (self.out / 'composite-inputs.digest').read_text()
        host = self.root / 'runtime/host/src/test_watch.c'; host.parent.mkdir(parents=True)
        host.write_text('/* watched 0x80001000 */\n', newline="\n")
        self.cycle()
        self.assertNotEqual(fingerprint, (self.out / 'composite-inputs.digest').read_text())
        self.args.native_game_math = False
        self.cycle()
        self.assertNotIn('native_game_math_try', source.read_text())
        self.assertNotIn('BLUEWAKE_NATIVE_GAME_MATH_PREPARED', (source.parent.parent / 'generated.h').read_text())
        self.assertFalse((source.parent.parent / 'native_game_math.json').exists())

    def test_native_skin_reuse_and_disable(self):
        body = '\nlabel_00000100:\n    ctx->gpr[3] = 1;\n'
        digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
        script = self.root / "scripts/windows/native_skin.py"
        text = script.read_text()
        start = text.index('LEAVES = (')
        end = text.index('INCLUDE = ', start)
        script.write_text(text[:start] + f"LEAVES = ((0x100, 0x104, {{'{digest}'}}),)\n" + text[end:], newline="\n")
        (self.base / 'chunks_dol/chunk_802ED6E0.c').write_text(
            '#include "../generated.h"\n' + body + '\nlabel_00000104:\nreturn_dispatch_802ED6E0:\n', newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.native_skin = True
        self.cycle()
        source = self.out / 'composite-src/chunks_dol/chunk_802ED6E0.c'
        self.assertIn('bluewake_native_skin_try', source.read_text())
        self.assertIn('BLUEWAKE_NATIVE_SKIN_PREPARED', (source.parent.parent / 'generated.h').read_text())
        before = source.read_bytes(), source.stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (source.read_bytes(), source.stat().st_mtime_ns))
        self.args.native_skin = False
        self.cycle()
        self.assertNotIn('bluewake_native_skin_try', source.read_text())
        self.assertNotIn('BLUEWAKE_NATIVE_SKIN_PREPARED', (source.parent.parent / 'generated.h').read_text())
        self.assertFalse((source.parent.parent / 'native_skin.json').exists())

    def test_native_vec_reuse_and_disable(self):
        body = '\nlabel_00000100:\n    ctx->gpr[3] = 1;\n'
        digest = hashlib.sha256(' '.join(body.split()).encode()).hexdigest()
        script = self.root / "scripts/mods/prepare_native_vec.py"
        text = script.read_text()
        start = text.index('LEAVES = (')
        end = text.index('INCLUDE = ', start)
        script.write_text(text[:start] + f"LEAVES = ((0x100, 0x104, {{'{digest}'}}),)\n" + text[end:], newline="\n")
        (self.base / 'chunks_dol/chunk_8030D6E0.c').write_text(
            '#include "../generated.h"\n' + body + '\nlabel_00000104:\nreturn_dispatch_8030D6E0:\n', newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.native_vec = True
        self.cycle()
        source = self.out / 'composite-src/chunks_dol/chunk_8030D6E0.c'
        self.assertIn('bluewake_native_vec_try', source.read_text())
        self.assertIn('BLUEWAKE_NATIVE_VEC_PREPARED', (source.parent.parent / 'generated.h').read_text())
        before = source.read_bytes(), source.stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (source.read_bytes(), source.stat().st_mtime_ns))
        self.args.native_vec = False
        self.cycle()
        self.assertNotIn('bluewake_native_vec_try', source.read_text())
        self.assertNotIn('BLUEWAKE_NATIVE_VEC_PREPARED', (source.parent.parent / 'generated.h').read_text())
        self.assertFalse((source.parent.parent / 'native_vec.json').exists())

    def test_inline_gpr_reuse_and_disable_rebuilds_callers(self):
        source = '''#include "../generated.h"
void synthetic(CPUState* ctx) {
    // 80004000: bl      0x80328F84
    {
            ctx->lr = 0x80004004u;
            ctx->pc = 0x80328F84u;
            return;
    }
label_80004004:
    return;
}
'''
        (self.base / "chunks_dol/a.c").write_text(source, newline="\n")
        (self.base / "generated_composite.h").write_text(
            "static DolRecompFunction s_dolrecomp_chunk_fns[] = {func_80004000, func_803256E0};\n", newline="\n")
        # Invented helper text and fixture-local certificates: no game code.
        bodies = ["\nlabel_80328F04:\n    return;\n", "\nlabel_80328F50:\n    return;\n"]
        helper = self.base / "chunks_dol/synthetic_803256E0.c"
        helper.write_text('#include "../generated.h"\nvoid func_803256E0(CPUState* ctx) {\n' +
                          ''.join(bodies) + "\nlabel_80328F9C:\n    return;\n}\n", newline="\n")
        script = self.root / "scripts/windows/inline_save_restore_gpr.py"
        text = script.read_text()
        for old, body in zip((
            'b7fa7b91c185412cce8d7dfc7eccafd5c50d69f7f49b66d111c582d11ab8df1b',
            'f525cbda6f2bed00dbaa48533f1a32c12ed19b571328e7045945a08b9c3ca2d4'), bodies):
            text = text.replace(old, hashlib.sha256(' '.join(body.split()).encode()).hexdigest())
        script.write_text(text, newline="\n")
        self.digest = bw.tree_digest(self.base)
        self.args.direct_calls = self.args.inline_gpr = True
        self.cycle()
        self.assertIn('bw_inline_gpr_memory_ready', self.chunk().read_text())
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        self.args.inline_gpr = False
        self.cycle()
        self.assertNotIn('bw_inline_gpr_memory_ready', self.chunk().read_text())
        self.assertIn('bw_chunk_fns[1](ctx)', self.chunk().read_text())
        receipt = json.loads((self.out / "prepared-blocks.json").read_text())
        self.assertFalse(receipt["inline_gpr"])
        self.assertTrue(receipt["direct_calls"])

    def test_explicit_enable_reuse_and_disable(self):
        self.cycle()
        self.assertNotIn(MARK, self.chunk().read_text())
        self.args.prepared_blocks = True
        self.cycle()
        self.assertIn(MARK, self.chunk().read_text())
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        receipt = json.loads((self.out / "prepared-blocks.json").read_text())
        self.assertTrue(receipt["enabled"])
        self.assertEqual(receipt["final_digest"], bw.tree_digest(self.out / "composite-src"))
        self.args.prepared_blocks = False
        self.cycle()
        self.assertEqual(self.chunk().read_text(), CHUNK)
        self.assertFalse(json.loads((self.out / "prepared-blocks.json").read_text())["enabled"])

    def test_fixed_cpu_can_be_selected_combined_reused_and_disabled(self):
        self.cycle()
        self.args.fixed_cpu = True
        self.cycle()
        self.assertIn("#define ctx (&bw_guest_cpu)", self.chunk().read_text())
        self.assertNotIn(MARK, self.chunk().read_text())
        self.args.prepared_blocks = True
        self.cycle()
        self.assertIn("#define ctx (&bw_guest_cpu)", self.chunk().read_text())
        self.assertIn(MARK, self.chunk().read_text())
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        self.assertTrue(json.loads((self.out / "prepared-blocks.json").read_text())["fixed_cpu"])
        script = self.root / "scripts/windows/global_guest_cpu.py"
        script.write_text(script.read_text() + "\n# synthetic CPU transform revision\n", newline="\n")
        self.builder.generate()
        self.assertEqual(self.chunk().read_text(), CHUNK)
        self.builder.prepare_blocks()
        self.assertIn("#define ctx (&bw_guest_cpu)", self.chunk().read_text())
        self.args.fixed_cpu = False
        self.cycle()
        self.assertNotIn("bw_guest_cpu", self.chunk().read_text())
        self.assertIn(MARK, self.chunk().read_text())
        self.args.prepared_blocks = False
        self.cycle()
        self.assertEqual(self.chunk().read_text(), CHUNK)

    def test_fixed_mem1_selection_is_recorded_and_invalidates_inputs(self):
        self.args.fixed_cpu = True
        self.cycle()
        prior = (self.out / "composite-inputs.digest").read_text()
        self.args.fixed_mem1 = True
        self.cycle()
        self.assertNotEqual(prior, (self.out / "composite-inputs.digest").read_text())
        self.assertTrue(json.loads((self.out / "prepared-blocks.json").read_text())["fixed_mem1"])
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        self.args.fixed_mem1 = False
        self.cycle()
        self.assertFalse(json.loads((self.out / "prepared-blocks.json").read_text())["fixed_mem1"])
        self.assertIn("#define ctx (&bw_guest_cpu)", self.chunk().read_text())

    def test_inline_fp_independent_combined_reuse_disable_and_helper_changes(self):
        self.cycle()
        self.assertNotIn('"inline_fp.h"', self.chunk().read_text())
        self.args.inline_fp = True
        self.cycle()
        self.assertIn('#include "../generated.h"\n#include "inline_fp.h"', self.chunk().read_text())
        self.assertNotIn('gather_pipe', self.chunk().read_text())
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        for helper in ("scripts/windows/chunk_headers.py", "cmake/composite/inline_fp.h"):
            path = self.root / helper
            path.write_text(path.read_text() + "\n", newline="\n")
            self.builder.generate()
            self.assertEqual(self.chunk().read_text(), CHUNK)
            self.builder.prepare_blocks()
            receipt = json.loads((self.out / "prepared-blocks.json").read_text())
            self.assertTrue(receipt["inline_fp"])
            self.assertEqual(receipt["inline_fp_header_sha256"], bw.sha256_file(self.root / "cmake/composite/inline_fp.h"))
        self.args.fixed_cpu = self.args.prepared_blocks = True
        self.cycle()
        self.assertIn('"inline_fp.h"', self.chunk().read_text())
        self.assertIn("#define ctx (&bw_guest_cpu)", self.chunk().read_text())
        self.assertIn(MARK, self.chunk().read_text())
        self.args.inline_fp = False
        self.cycle()
        self.assertNotIn('"inline_fp.h"', self.chunk().read_text())
        self.assertIn(MARK, self.chunk().read_text())

    def test_gather_independent_combined_reuse_disable_and_helper_changes(self):
        self.cycle()
        self.assertNotIn('"gather_pipe.h"', self.chunk().read_text())
        self.args.gather_pipe = True
        self.cycle()
        self.assertIn('#include "gather_pipe.h"\n#include "../generated.h"', self.chunk().read_text())
        self.assertNotIn('"inline_fp.h"', self.chunk().read_text())
        before = self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns
        self.cycle()
        self.assertEqual(before, (self.chunk().read_bytes(), self.chunk().stat().st_mtime_ns))
        for helper in ("gather_pipe.h", "gather_pipe.c", "gather_pipe_batch.h"):
            path = self.root / "cmake/composite" / helper
            path.write_text(path.read_text() + "\n", newline="\n")
            self.builder.generate()
            self.assertEqual(self.chunk().read_text(), CHUNK)
            self.builder.prepare_blocks()
            receipt = json.loads((self.out / "prepared-blocks.json").read_text())
            self.assertTrue(receipt["gather_pipe"])
            self.assertEqual(receipt["gather_sha256"][helper], bw.sha256_file(path))
        self.args.fixed_cpu = self.args.prepared_blocks = self.args.inline_fp = True
        self.cycle()
        text = self.chunk().read_text()
        self.assertLess(text.index('#include "gather_pipe.h"'), text.index('#include "../generated.h"'))
        self.assertLess(text.index('#include "../generated.h"'), text.index('#include "inline_fp.h"'))
        self.assertIn("#define ctx (&bw_guest_cpu)", text)
        self.assertIn(MARK, text)
        self.args.gather_pipe = False
        self.cycle()
        self.assertNotIn('"gather_pipe.h"', self.chunk().read_text())
        self.assertIn('"inline_fp.h"', self.chunk().read_text())
        self.assertIn(MARK, self.chunk().read_text())

    def test_interrupted_preparation_is_not_reused(self):
        self.args.prepared_blocks = True
        self.builder.generate()
        # Emulate interruption after one atomic chunk rewrite, before receipt.
        self.chunk().write_text(self.chunk().read_text() + "/* partial preparation */\n", newline="\n")
        self.builder.generate()
        self.assertEqual(self.chunk().read_text(), CHUNK)
        self.builder.prepare_blocks()
        self.assertIn(MARK, self.chunk("a.c").read_text())
        self.assertIn(MARK, self.chunk("b.c").read_text())

    def test_transform_change_invalidates_prepared_cache(self):
        self.args.prepared_blocks = True
        self.cycle()
        script = self.root / "scripts/windows/fast_blocks.py"
        script.write_text(script.read_text() + "\n# synthetic revision change\n", newline="\n")
        self.builder.generate()
        self.assertNotIn(MARK, self.chunk().read_text())
        self.builder.prepare_blocks()
        self.assertIn(MARK, self.chunk().read_text())


if __name__ == "__main__":
    unittest.main()
