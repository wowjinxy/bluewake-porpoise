"""Inactive cold precise twin experiment. Pure source transform only.

No slow helper rejoins a transformed fast copy. A fresh original gather guard
miss publishes committed metadata and enters the original current paid part.
Unrecognized access/body/part forms are retained, never guessed.
"""
import importlib.util
import re
from pathlib import Path

def repository_root():
    for parent in Path(__file__).resolve().parents:
        if (parent / 'scripts/windows/lean_memory.py').is_file():
            return parent
    raise RuntimeError('Place this experiment inside the matching BlueWake source tree')


ROOT = repository_root()
_spec = importlib.util.spec_from_file_location('cold_twin_original_lean_parser', ROOT / 'scripts/windows/lean_memory.py')
lean = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(lean)
FUNCTION = re.compile(r'^(?:static )?void (\w+)\(CPUState\* (?:ctx|ctx_param)\) \{$')
ENTRY = re.compile(r'^    if \(cycle_block_prepaid\) goto bwfast_(\d+);$')
CHARGE = re.compile(r'^    if \(!cycle_block_prepaid && !dolrecomp_charge_precise\(ctx, \d+u, 0x([0-9A-F]{8})u\)\) return;$')
PRECISE_SUFFIX = re.compile(r'^    ctx->cycle_observation_suffix = cycle_block_prepaid \? (\d+)u : 0u;$')
SLOW = re.compile(r'^bwslow_(\d+)_\d+: ;$')
OP = re.compile(r'^    // ([0-9A-F]{8}):\s+(\w+)\b')
READ_OPS = set('lbz lbzx lbzu lbzux lhz lhzx lhzu lhzux lha lhax lhau lhaux lwz lwzx lwzu lwzux'.split())
WRITE_OPS = set('stb stbx stbu stbux sth sthx sthu sthux stw stwx stwu stwux'.split())
MARK = '/* private cold-twin RAM experiment: original paid precise tails */'


def access_shape(body):
    """Only braces, pure EA, one scalar access and optional RA=EA qualify."""
    live = [line for line in body if line.strip()]
    if len(live) not in (5, 6) or live[1] != '    {' or live[-1] != '    }':
        return None
    op = OP.match(live[0])
    if op is None or op.group(2) not in READ_OPS | WRITE_OPS:
        return None
    ea = re.fullmatch(r'        u32 ea = (.+);', live[2])
    if ea is None:
        return None
    expr = re.sub(r'ctx->gpr\[\d+\]', '0', ea.group(1))
    # Constants, casts and wrapping addition only; no CPU writes or calls.
    expr = re.sub(r'\b(?:u32|s32)\b', '', expr)
    if re.search(r'[^0-9a-fA-FxXuU() +\-]', expr):
        return None
    reads = re.fullmatch(r'        ctx->gpr\[\d+\] = (?:(?:\((?:u32|s32|s16|s8)\))*)mem_read(8|16|32)\(ctx, ea\);', live[3])
    writes = re.fullmatch(r'        mem_write(8|16|32)\(ctx, ea, \(u(?:8|16|32)\)ctx->gpr\[\d+\]\);', live[3])
    if reads is not None and op.group(2) in READ_OPS:
        kind, bits = 'read', int(reads.group(1))
    elif writes is not None and op.group(2) in WRITE_OPS:
        kind, bits = 'write', int(writes.group(1))
    else:
        return None
    if len(live) == 6 and re.fullmatch(r'        ctx->gpr\[\d+\] = ea;', live[4]) is None:
        return None
    return kind, bits


def current_part(lines, entry, end, pc, suffix, copy_id):
    hits = [i for i in range(entry + 1, end) if OP.match(lines[i]) and OP.match(lines[i]).group(1) == pc]
    if len(hits) != 1:
        return None
    c = hits[0]
    comments = [i for i in range(entry + 1, end) if OP.match(lines[i])]
    first = c == comments[0]
    if PRECISE_SUFFIX.match(lines[c - 1]) is None or int(PRECISE_SUFFIX.match(lines[c - 1]).group(1)) != suffix:
        return None
    j = c - 1
    if j > entry + 1 and CHARGE.match(lines[j - 1]):
        if CHARGE.match(lines[j - 1]).group(1) != pc:
            return None
        j -= 1
    elif not first:
        return None
    current_pc = None
    if j > entry + 1 and lean.PC.match(lines[j - 1]):
        current_pc = lean.PC.match(lines[j - 1]).group(1)
        if current_pc != pc:
            return None
        j -= 1
    if first:
        j = entry + 1
        # Step zero must skip the complete precharge/goto prefix. Only original
        # metadata and an optional existing slow label may lie before comment.
        for line in lines[j:c]:
            if not (lean.PC.match(line) or CHARGE.match(line) or PRECISE_SUFFIX.match(line) or SLOW.match(line)):
                return None
        pcs = [lean.PC.match(line).group(1) for line in lines[j:c] if lean.PC.match(line)]
        current_pc = pcs[-1] if pcs else None
    if j > entry + 1 and SLOW.match(lines[j - 1]) and SLOW.match(lines[j - 1]).group(1) == copy_id:
        label, insertion = lines[j - 1].split(':')[0], None
    else:
        label, insertion = f'bwcold_{copy_id}_{pc}', j
    return dict(label=label, insertion=insertion, first=first, precise_comment_line=c,
                precise_start_line=j, precise_current_pc=current_pc)


def rewrite_copy(body, copy_id, mappings):
    out, changed = [], []
    pending_pc = pending_suffix = None

    def publication(indent='    ', pc=None, suffix=None, explicit=False):
        p = pc if explicit else pending_pc
        s = suffix if explicit else pending_suffix
        result = []
        if p is not None:
            result.append(f'{indent}ctx->pc = 0x{p}u;')
        if s is not None:
            result.append(f'{indent}ctx->cycle_observation_suffix = {s}u;')
        return result

    def flush():
        nonlocal pending_pc, pending_suffix
        out.extend(publication())
        pending_pc = pending_suffix = None

    for item in lean.parse_copy(body):
        if item[0] == 'pc':
            flush()
            out.append(f'    ctx->pc = 0x{item[1]}u;')
            continue
        if item[0] == 'line':
            if lean.calls(item[1]) or lean.leaves(item[1]) or 'ctx->pc' in item[1] or 'ctx->cycle_observation_suffix' in item[1]:
                flush()
            out.append(item[1])
            continue
        _, header_pc, suffix, pc, insn, check = item
        shape = access_shape(insn)
        mapping = mappings.get(pc)
        if shape is not None and mapping is not None and suffix is not None and check is not None:
            kind, bits = shape
            prior_suffix = pending_suffix
            guard = 'BW_RAM_FAST' if kind == 'read' else 'BW_RAM_FAST_STORE'
            # A header can publish the prior pure instruction's PC, rather
            # than current metadata. That value is already committed on the
            # precise path and must be restored BEFORE the cold handoff.
            cold_pc = pending_pc
            if header_pc is not None and header_pc != mapping['precise_current_pc']:
                cold_pc = header_pc
            guarded = []
            for line in insn:
                guarded.append(line)
                if re.fullmatch(r'        u32 ea = .+;', line):
                    guarded.extend([f'        if (__builtin_expect(!{guard}(ctx, ea, {bits // 8}u), 0)) {{',
                                    *publication('            ', cold_pc, pending_suffix, True),
                                    f'            goto {mapping["label"]};', '        }'])
            access_name = f'bwcold_{"load" if kind == "read" else "store"}{bits}( '
            # Keep original casts, access value and RA update ordering verbatim.
            guarded = [line.replace(f'mem_{kind}{bits}(ctx, ', access_name).replace('( ea', '(ea') for line in guarded]
            out.extend(guarded)
            if header_pc is not None:
                pending_pc = header_pc
            pending_suffix = suffix
            out.extend(['    if (ctx->cycle_deadline_budget > 0 &&',
                        f'        (s64){suffix}u > ctx->cycle_deadline_budget) {{',
                        *publication('        '),
                        f'        ctx->downcount += (s64){suffix}u;',
                        '        cycle_block_prepaid = false;', check[4], '    }'])
            changed.append(dict(pc=pc, kind=kind, bits=bits, header_pc=header_pc,
                                suffix=suffix, cold_precurrent_pc=cold_pc,
                                cold_precurrent_suffix=prior_suffix,
                                refund_target=lean.REFUND_GOTO.match(check[4]).group(1), **mapping))
            continue
        text = '\n'.join(insn)
        # Only pure body code can carry pending metadata across this item.
        if header_pc is not None or suffix is not None or lean.calls(text) or lean.leaves(text) or check is not None or 'ctx->pc' in text or 'ctx->cycle_observation_suffix' in text:
            flush()
        if header_pc is not None:
            out.append(f'    ctx->pc = 0x{header_pc}u;')
        if suffix is not None:
            out.append(f'    ctx->cycle_observation_suffix = {suffix}u;')
        out.extend(insn)
        if check is not None:
            flush()
            out.extend(check)
    flush()
    return out, changed


def transform_function(lines):
    name_match = FUNCTION.match(lines[0])
    if name_match is None:
        raise ValueError('not a translated function')
    name = name_match.group(1)
    copies = [(i, lean.COPY.match(line).group(1)) for i, line in enumerate(lines) if lean.COPY.match(line)]
    if not copies:
        return lines, dict(function=name, changed=[])
    precise_end = copies[0][0]
    entries_by_id, ends_by_id = {}, {}
    for index, line in enumerate(lines[:precise_end]):
        entry_match = ENTRY.match(line)
        end_match = re.fullmatch(r'bwend_(\d+): ;', line)
        if entry_match:
            entries_by_id.setdefault(entry_match.group(1), []).append(index)
        if end_match:
            ends_by_id.setdefault(end_match.group(1), []).append(index)
    inserts, replacements, all_changed = {}, {}, []
    for start, number in copies:
        entries = entries_by_id.get(number, [])
        ends = ends_by_id.get(number, [])
        if len(entries) != 1 or len(ends) != 1 or not entries[0] < ends[0]:
            raise ValueError(f'{name} bwfast_{number}: ambiguous exact original entry/end')
        stop = start + 1
        while stop < len(lines) and lines[stop] != f'    goto bwend_{number};':
            stop += 1
        if stop >= len(lines):
            raise ValueError('fast copy has no closing goto')
        body = lines[start + 1:stop]
        mappings = {}
        for item in lean.parse_copy(body):
            if item[0] != 'insn' or item[2] is None or item[5] is None or access_shape(item[4]) is None:
                continue
            found = current_part(lines, entries[0], ends[0], item[3], item[2], number)
            if found is not None:
                c = found['precise_comment_line']
                if lines[c:c + len(item[4])] != item[4]:
                    raise ValueError(f'{name}/{number}/{item[3]}: precise/fast body mismatch')
                mappings[item[3]] = found
        new_body, changed = rewrite_copy(body, number, mappings)
        if not changed:
            continue
        replacements[start + 1] = (stop, new_body)
        for row in changed:
            if row['insertion'] is not None:
                inserts.setdefault(row['insertion'], []).append(row['label'] + ': ;')
            row.update(function=name, copy_id=number, precise_entry_line=entries[0],
                       fast_start_line=start, fast_end_line=stop)
        all_changed.extend(changed)
    out, i = [], 0
    while i < len(lines):
        out.extend(inserts.get(i, []))
        if i in replacements:
            stop, body = replacements[i]
            out.extend(body)
            i = stop
        else:
            out.append(lines[i])
            i += 1
    return out, dict(function=name, changed=all_changed)


def transform_source(text):
    if MARK in text:
        raise ValueError('already transformed')
    lines = text.split('\n')
    heads = [i for i, line in enumerate(lines) if FUNCTION.match(line)]
    if not heads:
        raise ValueError('no exact translated functions')
    out, reports = lines[:heads[0]], []
    for ordinal, start in enumerate(heads):
        stop = heads[ordinal + 1] if ordinal + 1 < len(heads) else len(lines)
        close = stop - 1
        while close > start and lines[close] != '}':
            close -= 1
        body, report = transform_function(lines[start:close + 1])
        for row in report['changed']:
            for key in ('precise_comment_line', 'precise_start_line', 'precise_entry_line', 'fast_start_line', 'fast_end_line'):
                row[key] += start + 1
            if row['insertion'] is not None:
                row['insertion'] += start + 1
        out.extend(body)
        out.extend(lines[close + 1:stop])
        reports.append(report)
    candidate = '\n'.join(out)
    include = '#include "inline_fp.h"\n'
    if candidate.count(include) != 1:
        raise ValueError('ambiguous helper include route')
    candidate = candidate.replace(include, include + MARK + '\n#include "cold_twin_memory.h"\n', 1)
    return candidate, dict(functions=reports, accesses=sum(len(row['changed']) for row in reports),
                           no_observed_flags=True, no_slow_rejoin=True, widths=[8,16,32])
