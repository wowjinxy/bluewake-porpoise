#!/usr/bin/env python3
"""Synthetic preparation checks; no game input or generated game source."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('direct', ROOT / 'scripts/windows/direct_calls.py')
direct = importlib.util.module_from_spec(spec)
spec.loader.exec_module(direct)

CALL = '''#include "../generated.h"
void synthetic(CPUState* ctx) {
    // 80004000: bl      0x80006000
    {
            ctx->lr = 0x80004004u;
            ctx->pc = 0x80006000u;
            return;
    }
label_80004004:
    ctx->gpr[3] += 1;
}
'''

INDIRECT = '''#include "../generated.h"
void synthetic(CPUState* ctx) {
    // 80004000: bctrl
    {
        u32 target = ctx->ctr & ~3u;
        bool ctr_ok = true;
        bool cr_ok = true;
        if (ctr_ok && cr_ok) {
            ctx->lr = 0x80004004u;
            ctx->pc = target;
            return;
        }
    }
label_80004004:
    ctx->gpr[3] += 1;
}
'''

FALLBACK = '''#include "../generated.h"
void synthetic(CPUState* ctx) {
    // 80004000: synthetic interpreter instruction
    ppc_fallback_instruction(ctx, 0x00000000u, 0x80004000u);
    return;
label_80004004:
    ctx->gpr[3] += 1;
}
'''


def convert(source, watched=()):
    return direct.transform(source, 0x80004000, [0x80004000, 0x80006000],
                            {0x80004000: 0, 0x80006000: 1}, set(watched))


class DirectPreparationTests(unittest.TestCase):
    def test_inventory_data_does_not_watch_but_completion_boundaries_do(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            host=root/'runtime/host/src';host.mkdir(parents=True)
            (root/'windows/src').mkdir(parents=True)
            current_root=direct.ROOT
            direct.ROOT=root
            try:
                source=host/'collector.cpp'
                # Source scanning runs before macro processing; preserve real
                # completion boundaries and both mirror forms in the same file.
                source.write_text('#if DISABLED_COLLECTOR\n' +
                    '\n'.join(f'auto data_{i} = 0x{address:08X}u;' for i,address in enumerate((
                        0x803C4C46,0xC03C4C46,0x803C4C47,0x803C4C5B,0x803C4C5C,0x803C4CC5)))+
                    '\nauto entry=0x8003EF38u; auto returned=0xC0023960u;\n#endif\n',
                    encoding='utf-8',newline='\n')
                self.assertEqual(direct.watched_addresses(),{
                    0x8003EF38,0xC003EF38,0x80023960,0xC0023960})
                direct.write_watch_list(root,direct.watched_addresses())
                table=(root/'bw_edge_watch.inc').read_text(encoding='utf-8')
                self.assertIn('0x8003EF38u',table)
                self.assertIn('0x80023960u',table)
                self.assertNotIn('803C4C',table)
            finally:
                direct.ROOT=current_root

    def test_shared_healing_return_only_guarded_once(self):
        source=(ROOT / 'tests/healing_return_chunk.c.in').read_text()
        for source in (source,source.replace('CPUState* ctx)', 'CPUState* ctx_param)')):
            guarded,count=direct.transform_healing_return(source)
            self.assertEqual(count,1)
            self.assertEqual(direct.transform_healing_return(guarded),(guarded,0))
            self.assertIn('case 0x800C2E20u: goto label_800C2E20;',guarded.split('return_dispatch_800C16E0:')[0])
            self.assertEqual(guarded.count(direct.HEALING_GUARD),1)
            self.assertNotIn('bw_direct_call_ready',direct.HEALING_GUARD)
            self.assertEqual(direct.healing_return_contract(guarded)['leaves'],['800C2E7C','800C31C8'])
        for broken in (source.replace('return_dispatch_800C16E0:', 'other_dispatch:'),
                       source.replace('label_800C31C8:', 'label_800C31CC:'),
                       source.replace(direct.HEALING_CASE,'    case 0x800C2E24u: goto label_800C2E20;')):
            with self.assertRaises(ValueError):direct.transform_healing_return(broken)
        with self.assertRaises(ValueError):direct.transform_healing_return(guarded.replace(direct.HEALING_GUARD,direct.HEALING_CASE))

    def test_healing_manifest_checks_all_variants_and_abi(self):
        source=(ROOT / 'tests/healing_return_chunk.c.in').read_text()
        guarded,_=direct.transform_healing_return(source)
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'chunks_dol').mkdir();(root/'chunks_mod_test').mkdir()
            (root/'generated.h').write_text('/* authored */\n')
            base=root/'chunks_dol/base.c';variant=root/'chunks_mod_test/variant.c'
            base.write_text(guarded);variant.write_text(guarded)
            self.assertEqual(direct.write_healing_return_manifest(root),2)
            self.assertTrue(direct.validate_healing_return_manifest(root))
            manifest=(root/'healing_return.json').read_bytes()
            direct.write_healing_return_manifest(root)
            self.assertEqual(manifest,(root/'healing_return.json').read_bytes())
            variant.write_text(source)
            with self.assertRaises(ValueError):direct.validate_healing_return_manifest(root)
            variant.write_text(guarded);(root/'healing_return.json').write_text(manifest.decode().replace('"version": 1','"version": 2'))
            with self.assertRaises(ValueError):direct.validate_healing_return_manifest(root)
            (root/'healing_return.json').write_bytes(manifest);base.unlink()
            with self.assertRaises(ValueError):direct.validate_healing_return_manifest(root)

    def test_no_healing_capability_is_legal_and_utf8_repeatable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'chunks_dol').mkdir()
            (root/'chunks_dol/unsupported.c').write_text(CALL,encoding='utf-8')
            header=root/'generated.h';header.write_bytes('/* UTF8 \u2603 */\r\n'.encode('utf-8'))
            self.assertEqual(direct.write_healing_return_manifest(root),0)
            self.assertFalse(direct.validate_healing_return_manifest(root))
            before_header=header.read_bytes();before_manifest=(root/'healing_return.json').read_bytes()
            self.assertIn('\u2603'.encode('utf-8'),before_header)
            self.assertNotIn(b'\r\r\n',before_header)
            direct.write_healing_return_manifest(root)
            self.assertEqual(before_header,header.read_bytes())
            self.assertEqual(before_manifest,(root/'healing_return.json').read_bytes())

    def test_direct_and_fixed_cpu_calls_keep_both_boundary_queries(self):
        for source in (CALL, CALL.replace('CPUState* ctx)', 'CPUState* ctx_param)')):
            with self.subTest(fixed='ctx_param' in source):
                result, count = convert(source)
                self.assertEqual(count, 1)
                self.assertIn('bw_direct_call_ready(ctx, 0x80006000u)', result)
                self.assertIn('bw_direct_call_ready(ctx, 0x80004004u)', result)
                self.assertIn('bw_chunk_fns[1](ctx)', result)
                self.assertNotIn('bw_native_call', result)
                self.assertEqual(convert(result), (result, 0))

    def test_watched_and_missing_continuations_keep_original_dispatch(self):
        for watched in ({0x80004000}, {0x80004004}, {0x80006000}):
            self.assertEqual(convert(CALL, watched), (CALL, 0))
        for source in (CALL.replace('label_80004004:', 'label_80004008:'),
                       CALL.replace('0x80006000', '0x70006000')):
            self.assertEqual(convert(source), (source, 0))

    def test_mirror_uses_dispatcher_canonical_pc(self):
        source = CALL.replace('0x80006000', '0xC0006000')
        result, count = convert(source)
        self.assertEqual(count, 1)
        self.assertIn('ctx->pc = 0x80006000u;', result)
        self.assertEqual(convert(source, {0x80006000}), (source, 0))
        self.assertEqual(convert(source, {0xC0006000}), (source, 0))

    def test_equipment_boundaries_keep_dynamic_admission_and_return(self):
        for target, site, ret in ((0x8012821C, 0x801198B8, 0x801198BC),
                                 (0xC008A870, 0xC1F10620, 0xC1F10624)):
            with self.subTest(target=target):
                source = CALL.replace('80004000', f'{site:08X}').replace('80004004', f'{ret:08X}').replace('80006000', f'{target:08X}')
                run = target & ~direct.MIRROR
                own = site & ~direct.MIRROR
                starts = sorted((own, run))
                indexes = {value: index for index, value in enumerate(starts)}
                watched = {target, run, ret, ret & ~direct.MIRROR}
                result, count = direct.transform(source, own, starts, indexes, watched)
                self.assertEqual(count, 1)
                self.assertIn(f'bw_direct_call_ready(ctx, 0x{run:08X}u)', result)
                self.assertIn(f'bw_direct_call_ready(ctx, 0x{ret:08X}u)', result)
                self.assertIn(f'ctx->pc == 0x{ret:08X}u', result)
                self.assertEqual(direct.transform(source, own, starts, indexes, watched | {site}), (source, 0))
                missing = source.replace(f'label_{ret:08X}:', 'label_70004004:')
                self.assertEqual(direct.transform(missing, own, starts, indexes, watched), (missing, 0))

    def test_unrelated_callers_to_boots_animation_remain_dynamic(self):
        source = CALL.replace('80006000', '8012821C')
        result, count = direct.transform(source, 0x80004000, [0x80004000, 0x8012821C],
            {0x80004000: 0, 0x8012821C: 1}, {0x8012821C, 0xC012821C})
        self.assertEqual(count, 1)
        self.assertIn('bw_direct_call_ready(ctx, 0x8012821Cu)', result)
        self.assertIn('bw_direct_call_ready(ctx, 0x80004004u)', result)
        self.assertEqual(direct.transform(source, 0x80004000, [0x80004000, 0x8012821C],
            {0x80004000: 0, 0x8012821C: 1}, {0x8012821C, 0x80004004}), (source, 0))

    def test_indirect_and_interpreter_continuations_query_the_host(self):
        result, count = direct.transform_indirect(INDIRECT, set())
        self.assertEqual(count, 1)
        self.assertIn('bw_call_translated(ctx, target)', result)
        self.assertIn('bw_direct_call_ready(ctx, target)', result)
        self.assertIn('bw_direct_call_ready(ctx, 0x80004004u)', result)
        self.assertEqual(direct.transform_indirect(result, set()), (result, 0))
        result, count = direct.transform_fallback(FALLBACK, set())
        self.assertEqual(count, 1)
        self.assertIn('ctx->pc == 0x80004004u && bw_direct_call_ready(ctx, 0x80004004u)', result)
        self.assertEqual(direct.transform_fallback(result, set()), (result, 0))
        for convert_fn, source in ((direct.transform_indirect, INDIRECT),
                                   (direct.transform_fallback, FALLBACK)):
            for watched in ({0x80004000}, {0x80004004}):
                self.assertEqual(convert_fn(source, watched), (source, 0))

    def test_watch_list_collapses_mirrors_and_is_repeatable(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            self.assertEqual(direct.write_watch_list(root, {0x80004000, 0xC0004000}), 1)
            before = (root / 'bw_edge_watch.inc').read_bytes()
            direct.write_watch_list(root, {0xC0004000, 0x80004000})
            self.assertEqual(before, (root / 'bw_edge_watch.inc').read_bytes())

    def test_porpoise_calls_try_native_and_keep_translated_fallback(self):
        for entry in direct.DISPATCHER_PORPOISE:
            source = CALL.replace('0x80006000', f'0x{entry:08X}')
            result, count = direct.transform(source, 0x80004000, [0x80004000, 0x803096E0],
                {0x80004000: 0, 0x803096E0: 1}, set(), direct.DISPATCHER_PORPOISE)
            self.assertEqual(count, 1)
            self.assertIn(f'bw_native_call(ctx, 0x{entry:08X}u)', result)
            self.assertIn('bw_chunk_fns[1](ctx)', result)
            self.assertIn('bw_direct_call_ready(ctx, 0x80004004u)', result)
            self.assertEqual(direct.transform(source, 0x80004000, [0x80004000, 0x803096E0],
                {0x80004000: 0, 0x803096E0: 1}, {entry}, direct.DISPATCHER_PORPOISE), (source, 0))


if __name__ == '__main__':
    unittest.main()
