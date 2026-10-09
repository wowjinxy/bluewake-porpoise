"""Check the reviewed host observer domain; failure only disables the shortcut.

This is a build-time certificate check, not an arbitrary-C predicate analyzer.
Changing a pinned observer requires reviewing its complete true-address domain,
updating the certificate, regenerating the table, and rerunning implication and
runtime parity tests. No ROM, translated source, target load, or tool is needed.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path


def render_table(certificate):
    domain = certificate['domains']
    spans = domain['raw_main_code_intervals']
    members = lambda a: a % 4 == 0 and any(lo <= a < hi for lo, hi in spans)
    excluded = set()
    for family in ('finite', 'features', 'events', 'static_intercepts', 'special'):
        excluded.update(domain[family])
    for lo, hi in domain['feature_inclusive_intervals']:
        excluded.update(range(lo, hi + 1, 4))
    excluded = {a for a in excluded if members(a)}
    base = 0x80000000
    pages = (spans[-1][1] - base + 4095) // 4096
    directory = [0] * pages
    masks = [[0] * 16]
    for address in sorted(excluded):
        page = (address - base) >> 12
        if directory[page] == 0:
            directory[page] = len(masks)
            masks.append([0] * 16)
        bit = (address & 4095) >> 2
        masks[directory[page]][bit >> 6] |= 1 << (bit & 63)
    assert len(domain['finite']) == 48
    assert sum((hi - lo) // 4 for lo, hi in spans) == 841008
    assert len(excluded) == certificate['static_excluded_main_slots']
    header = '''/* Generated immutable static exclusion; zero grants no final answer. */
#ifndef BLUEWAKE_HOST_OBSERVATION_DOMAINS_H
#define BLUEWAKE_HOST_OBSERVATION_DOMAINS_H
#include <stdbool.h>
#include <stdint.h>
static inline bool bw_host_observation_code_member(uint32_t a) {
 return (a&3u)==0u && ((a>=0x80003100u&&a<0x80005620u) ||
                      (a>=0x800056E0u&&a<0x80338680u));
}
'''
    assert spans == [[0x80003100, 0x80005620], [0x800056E0, 0x80338680]]
    header += 'static const uint16_t bw_host_observation_pages[%d]={\n' % pages
    header += ''.join(' ' + ','.join(str(i) for i in directory[x:x + 24]) + ',\n'
                      for x in range(0, pages, 24)) + '};\n'
    header += 'static const uint64_t bw_host_observation_masks[%d][16]={\n' % len(masks)
    header += ''.join(' {' + ','.join('UINT64_C(0x%016X)' % v for v in row) + '},\n'
                      for row in masks) + '};\n'
    header += '''static inline bool bw_host_observation_static_excluded(uint32_t a) {
 if(!bw_host_observation_code_member(a)) return false;
 const uint32_t bit=(a&4095u)>>2;
 return ((bw_host_observation_masks[bw_host_observation_pages[(a-0x80000000u)>>12]][bit>>6]>>(bit&63u))&1u)!=0u;
}
#endif
'''
    return header.encode()


def check(root, certificate, profile):
    if profile != certificate['supported_profile']:
        return False
    assert certificate['schema'] == 'bluewake-host-observation-domains-v1'
    assert certificate['text_normalization'] == 'CRLF-to-LF-only'
    assert set(certificate['finite_family_true_domains']) == {
        'hud', 'health', 'quick', 'dialogue', 'enhancement', 'autosave'}
    finite = set()
    for keys in certificate['finite_family_true_domains'].values():
        finite.update(keys)
    assert finite == set(certificate['domains']['finite'])
    for row in certificate['source_inputs']:
        relative = Path(row['path'])
        assert not relative.is_absolute() and '..' not in relative.parts
        path = root / relative
        data = path.read_bytes().replace(b'\r\n', b'\n')
        if len(data) != row['bytes'] or hashlib.sha256(data).hexdigest() != row['sha256']:
            return False
    table = root / 'runtime/host/src/host_observation_domains.h'
    return table.read_bytes().replace(b'\r\n', b'\n') == render_table(certificate)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    parser.add_argument('--compiler-id', required=True)
    parser.add_argument('--compiler-version', required=True)
    parser.add_argument('--system', required=True)
    parser.add_argument('--pointer-bytes', required=True)
    parser.add_argument('--configuration', required=True)
    parser.add_argument('--system-processor', required=True)
    parser.add_argument('--compiler-target', default='')
    parser.add_argument('--developer', required=True)
    parser.add_argument('--census', required=True)
    parser.add_argument('--collector', required=True)
    parser.add_argument('--reward', required=True)
    args = parser.parse_args()
    profile = dict(compiler_id=args.compiler_id, compiler_version=args.compiler_version,
                   system=args.system, pointer_bytes=args.pointer_bytes,
                   configuration=args.configuration,
                   system_processor=('x86_64' if args.system_processor.casefold() in ('amd64', 'x86_64') else args.system_processor),
                   compiler_target=(args.compiler_target or 'native'), developer=args.developer,
                   census=args.census, collector=args.collector, reward=args.reward)
    try:
        certificate = json.loads(args.certificate.read_bytes())
        passed = check(args.root.resolve(), certificate, profile)
    except (OSError, ValueError, KeyError, TypeError, AssertionError):
        passed = False
    print('CERTIFIED' if passed else 'DISABLED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
