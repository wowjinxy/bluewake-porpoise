"""Generate only synthetic differential sources; never compile or run them.

Usage: python -B -S experiments/cold-twin-memory/generate_fixture.py --output FRESH_DIRECTORY
"""
import argparse
import difflib
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

sys.dont_write_bytecode = True
BASE = Path(__file__).resolve().parent
for ROOT in BASE.parents:
    if (ROOT / 'tests/test_prepaid_rewrites.py').is_file():
        break
else:
    raise RuntimeError('Place the experiment inside the matching BlueWake source tree')

def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def pin(path):
    data = path.read_bytes()
    return {'name': path.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}

def write(path, value):
    with path.open('x', encoding='utf8', newline='\n') as stream:
        stream.write(value)

def save(path, value):
    write(path, json.dumps(value, indent=2) + '\n')

def translated(original, bits, kind='ordinary'):
    """Keep the old entry/markers/refund layout; replace only operation bodies.
    The raw32 shape family is retained separately as a decline control."""
    width = str(bits)
    read = {8: 'lbz', 16: 'lhz', 32: 'lwz', 64: 'ld'}[bits]
    store = {8: 'stb', 16: 'sth', 32: 'stw', 64: 'std'}[bits]
    operations = {
        0: [read, 'ctx->gpr[4]', f'ctx->gpr[3] = (u32)mem_read{width}(ctx, ea);', None],
        2: [store, 'ctx->gpr[5]', f'mem_write{width}(ctx, ea, (u{width})ctx->gpr[3]);', None],
        6: [store, 'ctx->gpr[6]', f'mem_write{width}(ctx, ea, (u{width})ctx->gpr[7]);', None],
    }
    if kind == 'miss_after_store':
        operations[4] = [read, 'ctx->gpr[6]', f'ctx->gpr[7] = (u32)mem_read{width}(ctx, ea);', None]
    elif kind == 'load_update':
        operations[0] = ['lwzu', 'ctx->gpr[4] + ctx->gpr[10]', 'ctx->gpr[3] = (u32)mem_read32(ctx, ea);', 'ctx->gpr[4] = ea;']
    elif kind == 'load_update_alias':
        operations[0] = ['lwzu', 'ctx->gpr[3] + ctx->gpr[10]', 'ctx->gpr[3] = (u32)mem_read32(ctx, ea);', 'ctx->gpr[3] = ea;']
    elif kind == 'indexed_load_alias':
        operations[0] = ['lwzx', 'ctx->gpr[4] + ctx->gpr[10]', 'ctx->gpr[4] = (u32)mem_read32(ctx, ea);', None]
    elif kind == 'store_update':
        operations[2] = ['stwu', 'ctx->gpr[5] + ctx->gpr[10]', 'mem_write32(ctx, ea, (u32)ctx->gpr[3]);', 'ctx->gpr[5] = ea;']
    elif kind == 'store_update_alias':
        operations[2] = ['stwu', 'ctx->gpr[3] + ctx->gpr[10]', 'mem_write32(ctx, ea, (u32)ctx->gpr[3]);', 'ctx->gpr[3] = ea;']
    elif kind == 'indexed_store_alias':
        operations[2] = ['stwx', 'ctx->gpr[3] + ctx->gpr[10]', 'mem_write32(ctx, ea, (u32)ctx->gpr[3]);', None]
    lines = original.splitlines()
    for i, operation in operations.items():
        pc = 0x80004000 + 4*i
        start = lines.index(f'    // {pc:08X}: synthetic instruction')
        # Each old synthetic body is exactly one line at these indices.
        body = [f'    // {pc:08X}: {operation[0]} synthetic', '    {',
                '        u32 ea = ' + operation[1] + ';', '        ' + operation[2]]
        if operation[3]:
            body.append('        ' + operation[3])
        body.append('    }')
        lines[start:start+2] = body
    return '\n'.join(lines) + '\n'

def special(original, kind):
    text=translated(original,32,'miss_after_store')
    if kind=='first_inherits_callback':
        # A syntactically valid standard leader with no unconditional PC write
        # before its first operation. The callback's PC must survive cold entry.
        text=text.replace('    ctx->pc = 0x80004000u;\n', '    observe(ctx);\n', 1)
    elif kind=='prior_pure_pc':
        # Remove the first memory instruction, retaining its pure instruction.
        # Force a PC marker on the following pure instruction and remove the
        # store's own PC. fast_blocks moves that PREVIOUS PC to the store header.
        first='\n'.join(['    ctx->cycle_observation_suffix = cycle_block_prepaid ? 7u : 0u;',
              '    // 80004000: lwz synthetic','    {','        u32 ea = ctx->gpr[4];',
              '        ctx->gpr[3] = (u32)mem_read32(ctx, ea);','    }',*load_fast().REFUND])
        assert text.count(first)==1
        text=text.replace(first,'    // 80004000: addi synthetic\n    ctx->gpr[3] += 1u;',1)
        text=text.replace('    ctx->pc = 0x80004008u;\n','',1)
        if '    ctx->pc = 0x80004004u;' not in text:
            text=text.replace('label_80004004:\n','label_80004004:\n    ctx->pc = 0x80004004u;\n',1)
    else:
        raise ValueError(kind)
    return text

def load_fast():
    return load('cold_fixture_fast_for_special',ROOT/'scripts/windows/fast_blocks.py')


def main():
    global OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    OUT = parser.parse_args().output
    if OUT.exists():
        raise RuntimeError('Fresh output directory required')
    expected_inputs = {'tests/test_prepaid_rewrites.py': '0f147c36af1e66cb3397e8af6e02993f629f319c2ca03379bb6b2ba8f323a7a7', 'scripts/windows/fast_blocks.py': 'd463882939dd402f5cd37fb9299ff61ba7d9b2eecd44313805e34988a69142f3', 'scripts/windows/lean_memory.py': '4966151cad472a84ff26e96e3b78959525047e691ad84e6e6bef9186ce71a875'}
    for relative, expected in expected_inputs.items():
        if hashlib.sha256((ROOT / relative).read_bytes().replace(b'\r\n', b'\n')).hexdigest() != expected:
            raise RuntimeError('Source dependency changed; requalify: ' + relative)
    test = load('cold_fixture_old_shapes', ROOT / 'tests/test_prepaid_rewrites.py')
    cold = load('cold_fixture_transform', BASE / 'transform.py')
    sources=[]
    # Exact old32 remain an independent unsupported-operand/64-bit control.
    for bits in (8,16,32,64):
        for loop in (False,True):
            for early in (False,True):
                for omit in (False,True):
                    original=test.source(bits,loop,early,omit)
                    sources.append((f'raw_{bits}_{int(loop)}_{int(early)}_{int(omit)}',original,False))
                    sources.append((f'translated_{bits}_{int(loop)}_{int(early)}_{int(omit)}',translated(original,bits),bits!=64))
    for kind in ('miss_after_store','load_update','load_update_alias','indexed_load_alias',
                 'store_update','store_update_alias','indexed_store_alias'):
        for loop in (False,True):
            for omit in (False,True):
                sources.append((f'{kind}_{int(loop)}_{int(omit)}',translated(test.source(32,loop,False,omit),32,kind),True))
    for kind in ('first_inherits_callback','prior_pure_pc'):
        for loop in (False,True):
            for omit in (False,True):
                sources.append((f'{kind}_{int(loop)}_{int(omit)}',special(test.source(32,loop,False,omit),kind),True))
    functions=[]; tables=[]; records=[]; inverse_files=[]
    selected=None
    OUT.mkdir(parents=True)
    for index,(name,original,expect_changed) in enumerate(sources):
        fast,count=test.fast.transform(original)
        assert count==1,(name,count)
        lines=fast.splitlines()
        head=next(i for i,line in enumerate(lines) if cold.FUNCTION.match(line))
        close=max(i for i,line in enumerate(lines) if line=='}')
        rewritten,report=cold.transform_function(lines[head:close+1])
        candidate='\n'.join(lines[:head]+rewritten+lines[close+1:])+'\n'
        assert bool(report['changed'])==expect_changed,(name,report)
        # A complete source inverse proves the unmodified input is retained.
        delta=list(difflib.ndiff(fast.splitlines(keepends=True),candidate.splitlines(keepends=True)))
        assert ''.join(difflib.restore(delta,1))==fast
        assert ''.join(difflib.restore(delta,2))==candidate
        diff=OUT/(name+'.ndiff')
        write(diff,''.join(delta)); inverse_files.append(pin(diff))
        names=[]
        for variant,body in [('original',original),('fast',fast),('cold',candidate)]:
            function='fixture_'+name+'_'+variant
            names.append(function)
            body=body.replace(test.fast.INCLUDE,'').replace('test_chunk',function)
            # The same header works with a real pointer and an explicit fixed
            # CPU macro. The parameter itself must remain a valid declaration.
            body=body.replace('(CPUState* ctx) {','(CPUState* fixture_ctx) {\n#ifndef FIXTURE_FIXED_CPU\n    CPUState* ctx = fixture_ctx;\n#else\n    (void)fixture_ctx;\n#endif')
            functions.append(body)
        tables.append('{ {'+','.join(names)+'},"'+name+'" }')
        records.append({'name':name,'index':index,'changed':len(report['changed']),
                        'report':report,'original_sha256':hashlib.sha256(original.encode()).hexdigest(),
                        'fast_sha256':hashlib.sha256(fast.encode()).hexdigest(),
                        'candidate_sha256':hashlib.sha256(candidate.encode()).hexdigest()})
        if name=='translated_32_0_0_0':
            selected=(original,candidate,report)
    assert len(sources)==100
    assert selected is not None
    original,candidate,report=selected
    first=next(row for row in report['changed'] if row['pc']=='80004000')
    target=first['label']
    wrong=first['refund_target']
    needle='            goto '+target+';'
    assert candidate.count(needle)==1 and target!=wrong
    mutants={'mutant_original':original,
             'mutant_wrong_label':candidate.replace(needle,'            goto '+wrong+';',1),
             'mutant_prepaid_false':candidate.replace(needle,'            cycle_block_prepaid = false;\n'+needle,1)}
    for name,body in mutants.items():
        body=body.replace(test.fast.INCLUDE,'').replace('test_chunk',name)
        body=body.replace('(CPUState* ctx) {','(CPUState* fixture_ctx) {\n#ifndef FIXTURE_FIXED_CPU\n    CPUState* ctx = fixture_ctx;\n#else\n    (void)fixture_ctx;\n#endif')
        functions.append(body)
    header='\n'.join(functions)+'\nstatic const Shape cases[] = {\n'+',\n'.join(tables)+'\n};\n'
    write(OUT/'rewritten_cases.h',header)
    write(OUT/'fixture.c',(BASE/'fixture.c').read_text())
    write(OUT/'cold_twin_memory.h',(BASE/'cold_twin_memory.h').read_text())
    save(OUT/'shape-ledger1.json',records)
    assert sum(bool(row['changed']) for row in records) == 60
    assert sum(row['changed'] for row in records) == 160
    summary = {'status': 'GENERATED_SYNTHETIC_SOURCES_ONLY', 'shapes': 100,
               'changed_shapes': 60, 'changed_accesses': 160,
               'generated_header': pin(OUT/'rewritten_cases.h'),
               'fixture': pin(OUT/'fixture.c'), 'memory_header': pin(OUT/'cold_twin_memory.h'),
               'native_executed': False}
    save(OUT/'generation.json', summary)
    print(json.dumps(summary))

if __name__ == '__main__':
    main()
