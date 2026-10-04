#!/usr/bin/env python3
"""Certify native entries and search resume leaders against original GZLE01 translations.

Run after mod variants, before other native hooks, inline-GPR, fixed-CPU,
prepaid-block, direct-call and memory rewrites. Every base/mod fragment and
nested callee must match before any source is changed. Certificates describe
this preparation stage; the builder fingerprints the final translated tree.
Only body hashes are distributed, never generated game source.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from direct_calls import watched_addresses

MARK = '/* bluewake: certified native entries (scripts/windows/native_entries.py) */\n'
INCLUDE = '#include "../generated.h"\n'
MARKER = '#define BLUEWAKE_NATIVE_ENTRIES_PREPARED 1\n'
HOOK = re.compile(r'    if \(bluewake_native_entries_try\(ctx, 0x([0-9A-F]{8})u\)\)\n'
                  r'        goto return_dispatch_([0-9A-F]{8});\n')

# name -> chunk, first label, end label, SHA256 of the original translated body.
# Shared matrix/save callees retain the established BlueWake certificates.
FRAGMENTS = {
    'fifo_pos': (0x802D56E0, 0x802D8BD8, 0x802D8C58, 'f34113280227b04dfc801d60bea93c3802b1d6ed4e646ec6698c1ed9190a1129'),
    'fifo_nrm': (0x802D56E0, 0x802D8C58, 0x802D8CC4, '254777e9a533104f0b8151d92b30a4a181a73ad19fbf0122a4e11ec2bfe1a52e'),
    'fifo_nrm33': (0x802D56E0, 0x802D8CC4, 0x802D8D30, '12baba3a6412f207ddd3555a5cd723c457be54fc7d560c2a31cba7593f74ebd0'),
    'same_actor': (0x802456E0, 0x8024734C, 0x8024738C, '09baab9113434b3db3c5837ec63003540fe26ee1666f4646c3126ffa5e4a531f'),
    'grp_entry': (0x800A56E0, 0x800A9684, None, '2aac53647ca3b155e9ca83341a50a1b91a3f0e663f4255e5b3f42f6ed65e8848'),
    'grp_tail': (0x800A96E0, 0x800A96E0, 0x800A974C, '8ec5e8ae4618ab9e2dd98b03e68d9c0353a95c8c51324033241015f823e81469'),
    'vec_sr': (0x8030D6E0, 0x8030DB24, 0x8030DB78, '5361ef59bab3c7db8a52275d6697df16adeec57ac23c7efddba26bebd26d4c32'),
    'basic': (0x802F16E0, 0x802F5090, 0x802F525C, '8b7c53db530bb6da31ac47e7c522210ab79f10ec42ce0927160ce508150b734d'),
    'softimage': (0x802F16E0, 0x802F52BC, 0x802F5508, '3cdecb06f224f0bd7e36bd3f1c8f7bdf4e8a046f96533cc7a9db910d15e3a5f4'),
    'maya_entry': (0x802F16E0, 0x802F5508, None, '8bea6c85d90f39af1560e02e949dd1d3b147f613be98cca8ed3580215a30856c'),
    'maya_tail': (0x802F56E0, 0x802F56E0, 0x802F5724, '2ce12f5facc5075e1c151b2ed1c61e1a77242517045dab27232318e5f5a75323'),
    'j3d_info': (0x802D96E0, 0x802DA64C, 0x802DA724, 'd8a04c8f0630a9253afbfd0161ea1580993372eb9dbbea9d220085c2c2fe7e11'),
    'j3d_values': (0x802D96E0, 0x802DA724, 0x802DA7E4, '364429ec9fb17e6dbf237ca49863689c05e8f6a3fb8e6ec64c72ca2b1224aa70'),
    'concat': (0x803096E0, 0x8030D0FC, 0x8030D1C8, '8ab502958cfcc5e434d35b0848d89ae59a9af3b00b160835ec3dde947709f1ea'),
    'copy': (0x803096E0, 0x8030D0C8, 0x8030D0FC, '8d89ffc5251b41bc39532c918e0855ceceab319616a5c0ef12acfb9635d9b55d'),
    'save': (0x803256E0, 0x80328F04, 0x80328F50, 'b7fa7b91c185412cce8d7dfc7eccafd5c50d69f7f49b66d111c582d11ab8df1b'),
    'restore': (0x803256E0, 0x80328F50, 0x80328F9C, 'f525cbda6f2bed00dbaa48533f1a32c12ed19b571328e7045945a08b9c3ca2d4'),
    'strcmp': (0x8032D6E0, 0x8032DB44, 0x8032DC6C, 'e4a953744010f165ec41e010775db92a273b5fcdcf5c38abc25fe764d5549683'),
    'stage_name': (0x8003D6E0, 0x80041544, 0x800415B4, 'd0d8a0291847e1b5901d52c0491e9718676133a2656e874c7f8ec1406ebec5d6'),
    'judge_filter': (0x802416E0, 0x80245640, 0x80245674, 'e78e6d3f9ff0c731b7bb189e73f32c726605bba79863cba8b501f6ed8b52da53'),
    'find_object': (0x800256E0, 0x8002833C, 0x80028410, '89a98e1bbb6a1d1db4c08ae6c62f04d6e5b5d463cdb197b6b691eeeb1a791d96'),
}
ENTRIES = {
    0x802D8BD8: ('fifo_pos',), 0x802D8C58: ('fifo_nrm',), 0x802D8CC4: ('fifo_nrm33',),
    0x8024734C: ('same_actor',), 0x800A9684: ('grp_entry', 'grp_tail'), 0x8030DB24: ('vec_sr',),
    0x802F5090: ('basic', 'j3d_info', 'concat', 'copy', 'save', 'restore'),
    0x802F52BC: ('softimage', 'j3d_values', 'concat', 'copy', 'save', 'restore'),
    0x802F5508: ('maya_entry', 'maya_tail', 'j3d_info', 'concat', 'copy', 'save', 'restore'),
    0x8032DB44: ('strcmp',),
    0x80041544: ('stage_name', 'strcmp', 'save', 'restore'),
    0x8004156C: ('stage_name', 'strcmp', 'save', 'restore'),
    0x80041578: ('stage_name', 'strcmp', 'save', 'restore'),
    0x80041588: ('stage_name', 'strcmp', 'save', 'restore'),
    0x80245640: ('judge_filter', 'find_object', 'stage_name', 'strcmp', 'save', 'restore'),
}


def hook(entry, chunk):
    return (f'    if (bluewake_native_entries_try(ctx, 0x{entry:08X}u))\n'
            f'        goto return_dispatch_{chunk:08X};\n')


def fragment(text, chunk, start, end):
    begin = text.find(f'\nlabel_{start:08X}:\n')
    stop = text.find(f'\nlabel_{end:08X}:\n', begin) if end else text.find(f'\nreturn_dispatch_{chunk:08X}:\n', begin)
    if begin < 0 or stop <= begin:
        return None
    return ' '.join(HOOK.sub('', text[begin:stop]).split())


def validate_hooks(text, chunk, path):
    # A hook with a changed address or destination cannot be silently ignored.
    for match in HOOK.finditer(text):
        entry = int(match[1], 16)
        if (entry not in ENTRIES or int(match[2], 16) != chunk or
                FRAGMENTS[ENTRIES[entry][0]][0] != chunk or
                not text[:match.start()].endswith(f'\nlabel_{entry:08X}:\n')):
            raise ValueError(f'modified native-entry hook in {path}')
    if text.count('bluewake_native_entries_try(') != len(HOOK.findall(text)):
        raise ValueError(f'modified native-entry hook in {path}')


def certify(chunks, watched):
    watched = {pc & ~0x40000000 for pc in watched}
    verified = set()
    for name, (chunk, start, end, expected) in FRAGMENTS.items():
        paths = [p for p in chunks if p.name.endswith(f'_{chunk:08X}.c')]
        valid = bool(paths)
        for path in paths:
            text = path.read_text(encoding='utf-8')
            validate_hooks(text, chunk, path)
            body = fragment(text, chunk, start, end)
            if body is None or hashlib.sha256(body.encode()).hexdigest() != expected:
                print(f'{path.name}: unverified {name}')
                valid = False
            if body and name not in ('save', 'restore'):
                observed = body
                if name in ('basic', 'softimage', 'maya_entry', 'maya_tail'):
                    # These precise save/restore calls have read-only CPU probes
                    # in native_mtxcalc.c; their bodies are certified separately.
                    observed = re.sub(r'ctx->pc = 0x80328F(?:38|40|84|8C)u;', '', observed)
                if name == 'stage_name':
                    # The known save/restore calls have private CPU probes in
                    # native_search.c; the helpers are certified separately.
                    observed = re.sub(r'ctx->pc = 0x80328F(?:40|8C)u;', '', observed)
                if name == 'find_object':
                    # The assertion call is reachable only for a NULL search
                    # parameter, which judge_search explicitly rejects.
                    observed = observed.replace('ctx->pc = 0x80006C4Cu;', '')
                names = re.findall(r'label_([8C][0-9A-F]{7})|// ([8C][0-9A-F]{7}):|'
                                   r'ctx->(?:pc|lr) = 0x([8C][0-9A-F]{7})u', observed)
                pcs = {int(next(v for v in group if v), 16) for group in names}
                if name == 'judge_filter':
                    # Its watched entry is approved by native_entries_v1;
                    # every internal observation still rejects the fragment.
                    pcs.discard(start)
                if any(pc in watched or (pc & ~0x40000000) in watched for pc in pcs):
                    print(f'{name}: host watches an internal address')
                    valid = False
        if valid:
            verified.add(name)
    return {entry for entry, names in ENTRIES.items() if set(names) <= verified}


def prepare(root):
    header = root / 'generated.h'
    if not header.is_file():
        raise ValueError('missing generated.h')
    chunks = sorted(root.glob('chunks_*/*.c'))
    entries = certify(chunks, watched_addresses())
    if entries != set(ENTRIES):
        raise ValueError('uncertified required native entries: ' +
                         ', '.join(f'{e:08X}' for e in sorted(set(ENTRIES) - entries)))
    prepared = {}
    for path in chunks:
        match = re.search(r'_([0-9A-F]{8})\.c$', path.name)
        if not match:
            continue
        chunk = int(match[1], 16)
        text = path.read_text(encoding='utf-8')
        if chunk not in {FRAGMENTS[names[0]][0] for names in ENTRIES.values()}:
            continue
        if INCLUDE not in text or f'\nreturn_dispatch_{chunk:08X}:\n' not in text:
            raise ValueError(f'unsupported native-entry chunk {path}')
        for entry, names in ENTRIES.items():
            if FRAGMENTS[names[0]][0] != chunk:
                continue
            label = f'\nlabel_{entry:08X}:\n'
            if text.count(label) != 1:
                raise ValueError(f'missing or duplicate native-entry label in {path}')
            if hook(entry, chunk) not in text:
                text = text.replace(label, label + hook(entry, chunk), 1)
        if MARK not in text:
            text = text.replace(INCLUDE, INCLUDE + MARK + '#include "native_entries.h"\n', 1)
        prepared[path] = text
    files = {}
    for path, text in prepared.items():
        if path.read_text(encoding='utf-8') != text:
            temporary = path.with_suffix('.c.tmp')
            temporary.write_text(text, encoding='utf-8', newline='\n')
            temporary.replace(path)
        files[path.relative_to(root).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = root / 'native_entries.json'
    data = json.dumps({'abi': 1, 'entries': sorted(entries), 'files': files}, indent=2) + '\n'
    if not manifest.exists() or manifest.read_text() != data:
        temporary = manifest.with_suffix('.json.tmp')
        temporary.write_text(data, newline='\n'); temporary.replace(manifest)
    if MARKER not in header.read_text():
        header.write_text(header.read_text() + '\n' + MARKER)
    print(f'native entries: {len(entries)} certified entries in {len(files)} chunks')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('composite', type=Path)
    args = parser.parse_args()
    try:
        prepare(args.composite)
    except ValueError as error:
        parser.exit(1, f'{error}\n')
