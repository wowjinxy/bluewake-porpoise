"""Extract the actual host predicates for the source-only contract fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def function(text, name):
    match = re.search(r'^static (?:inline )?(?:bool|u32) ' + re.escape(name) +
                      r'\([^;]*?\)\s*\{', text, re.M)
    if match is None:
        raise RuntimeError('Missing production function: ' + name)
    end, depth = match.end(), 1
    while depth:
        if end >= len(text):
            raise RuntimeError('Unterminated production function: ' + name)
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


def _replace_once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError('Unreviewed facts0 normalization drift: ' + old)
    return text.replace(old, new, 1)


def legacy_chassis_predicate(text):
    """Keep the older regression oracle's exact facts0 source guard useful."""
    wrapper = function(text, 'host_chassis_requires_full')
    if wrapper != ('static inline bool host_chassis_requires_full(const CPUState* cpu, u32 address) {\n'
                   '    return host_chassis_requires_full_facts(cpu, address, 0u);\n}'):
        raise RuntimeError('Legacy chassis wrapper is no longer facts0')
    actual = function(text, 'host_chassis_requires_full_facts')
    pairs = [
        ('host_chassis_requires_full_facts(const CPUState* cpu, u32 address, u32 facts)',
         'host_chassis_requires_full(const CPUState* cpu, u32 address)'),
        ('((facts & BW_OBSERVATION_FACT_QUIET) == 0u &&\n                              g_interrupt_sources_dirty)',
         'g_interrupt_sources_dirty'),
        ('((facts & BW_OBSERVATION_FACT_NO_STATIC_INTERCEPT) == 0u &&\n         bluewake_edge_maybe_intercept',
         '(bluewake_edge_maybe_intercept'),
        ('if ((facts & BW_OBSERVATION_FACT_QUIET) == 0u &&\n        (cpu->msr & PPC_MSR_EE) != 0u &&',
         'if ((cpu->msr & PPC_MSR_EE) != 0u &&')]
    for old, new in pairs:
        actual = _replace_once(actual, old, new)
    if 'facts' in actual:
        raise RuntimeError('Unnormalized chassis facts expression')
    return actual


def legacy_observation_predicate(text):
    wrapper = function(text, 'host_can_skip_observation')
    if wrapper != ('static bool host_can_skip_observation(void* user, const CPUState* cpu, u32 address) {\n'
                   '    return host_can_skip_observation_facts(user, cpu, address, 0u);\n}'):
        raise RuntimeError('Legacy observation wrapper is no longer facts0')
    actual = function(text, 'host_can_skip_observation_facts')
    pairs = [
        ('static inline bool host_can_skip_observation_facts(\n    void* user, const CPUState* cpu, u32 address, u32 facts)',
         'static bool host_can_skip_observation(void* user, const CPUState* cpu, u32 address)'),
        ('(void)cpu; (void)address; (void)facts;', '(void)cpu; (void)address;'),
        ('host_chassis_requires_full_facts(cpu, address, facts)', 'host_chassis_requires_full(cpu, address)')]
    for old, new in pairs:
        actual = _replace_once(actual, old, new)
    if 'facts' in actual:
        raise RuntimeError('Unnormalized observation facts expression')
    return actual


def prepare(repo, output, watch=None):
    main_path = repo / 'runtime/host/src/main.c'
    main = main_path.read_text(encoding='utf-8')
    names = ['host_chassis_requires_full_facts', 'host_chassis_requires_full',
             'host_can_skip_observation_facts', 'host_can_skip_observation',
             'host_direct_can_skip', 'host_direct_can_skip_facts']
    actual = '\n\n'.join(function(main, name) for name in names) + '\n'
    output.mkdir(parents=True, exist_ok=False)
    (output / 'actual_predicates.inc').write_text(actual, encoding='utf-8', newline='\n')
    reference = Path(__file__).with_name('reference.inc')
    (output / 'reference.inc').write_bytes(reference.read_bytes())
    edge = (repo / 'runtime/host/src/edge_intercept_table.h').read_text(encoding='utf-8')
    keys = re.findall(r'0x[0-9A-Fa-f]+u', edge.split('g_edge_keys_all[')[1].split('};')[0])
    if len(keys) != 56:
        raise RuntimeError('V1 requires the complete reviewed 56-key host table')
    if watch is None:
        extras = ['0x8008A870u', '0x81F10624u', '0x8012821Cu', '0x801198BCu']
        content = 'static const u32 bw_edge_watch_list[] = {\n' + ','.join(keys + extras) + '\n};\n'
        (output / 'fixture_watch.inc').write_text(content, encoding='utf-8', newline='\n')
    else:
        (output / 'fixture_watch.inc').write_bytes(watch.read_bytes())
    paths = [main_path, reference, repo / 'cmake/composite/direct_calls.c',
             repo / 'cmake/composite/direct_calls.h', repo / 'cmake/composite/dispatch_loop.h',
             repo / 'cmake/composite/observation_facts.h',
             repo / 'runtime/host/src/edge_intercept_table.h',
             repo / 'runtime/host/src/finite_observer_filter.h', Path(__file__),
             Path(__file__).with_name('oracle.c')]
    if watch is not None:
        paths.append(watch)
    receipt = {'status': 'PREPARED_NOT_COMPILED', 'inputs': [
        {'path': str(path.resolve()), 'bytes': path.stat().st_size,
         'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for path in paths]}
    (output / 'source-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    output = parser.add_mutually_exclusive_group(required=True)
    output.add_argument('--output', type=Path)
    output.add_argument('--legacy-observation-output', type=Path)
    parser.add_argument('--watch', type=Path)
    args = parser.parse_args()
    if args.legacy_observation_output:
        actual = legacy_observation_predicate((args.repo / 'runtime/host/src/main.c').read_text())
        reference = function(Path(__file__).with_name('reference.inc').read_text(), 'reference_can_skip')
        reference = reference.replace('reference_can_skip', 'host_can_skip_observation', 1).replace(
            'reference_requires_full', 'host_chassis_requires_full')
        if actual != reference:
            raise RuntimeError('Facts0 observation is not the frozen original predicate')
        args.legacy_observation_output.write_text(actual + '\n', encoding='utf-8', newline='\n')
    else:
        prepare(args.repo.resolve(), args.output.resolve(), args.watch)
