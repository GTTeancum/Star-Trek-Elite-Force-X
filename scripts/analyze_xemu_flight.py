"""Summarize asynchronous diagnostic observations, never FPS acceptance.

Addresses are translated by matching PE/XBE section occurrence, including
duplicate section names. Kernel addresses are deliberately not assigned the
nearest exported title function. Stack words are candidates, not a backtrace.
"""
import argparse
import bisect
import collections
import json
import re
import statistics
from pathlib import Path

from ja_xemu_smoke import pe_sections, xbe_sections


class Symbols:
    def __init__(self, path):
        base, pe = pe_sections(path.with_suffix('.exe').read_bytes())
        xb = xbe_sections(path.with_suffix('.xbe').read_bytes())
        self.sections = []
        seen = collections.Counter()
        for name, va, raw, size in pe:
            matches = [s for s in xb if s[0] == name]
            occurrence = seen[name]
            seen[name] += 1
            if occurrence < len(matches):
                _, address, length = matches[occurrence]
                self.sections.append((base + va, address, min(size, length), name))
        self.functions = []
        for line in path.read_text(errors='replace').splitlines():
            m = re.match(r'^\s*[\da-fA-F]+:[\da-fA-F]+\s+(\S+)\s+([\da-fA-F]{8})\s+f(?:\s+i)?\s+(\S+)', line)
            if not m:
                continue
            name, address, obj = m.groups()
            for linked, guest, length, section in self.sections:
                if linked <= int(address, 16) < linked + length:
                    self.functions.append((guest + int(address, 16) - linked, name, obj, section))
                    break
        self.functions.sort()
        self.addresses = [s[0] for s in self.functions]

    def resolve(self, address):
        if address >= 0x80000000:
            return ('kernel/unresolved', '0x%08x' % address)
        section = next((s for s in self.sections if s[1] <= address < s[1] + s[2]), None)
        if section is None:
            return ('unmapped', '0x%08x' % address)
        i = bisect.bisect_right(self.addresses, address) - 1
        if i < 0 or self.functions[i][0] < section[1]:
            return (section[3] + '/unresolved', '0x%08x' % address)
        start, name, obj, _ = self.functions[i]
        return (obj, '%s+0x%x' % (name, address - start))


def stage(value):
    raw = int(value).to_bytes(4, 'big')
    return raw.decode('ascii') if all(32 <= x < 127 for x in raw) else '0x%08x' % value


def analyze(records, symbols):
    samples = [r for r in records if r['kind'] == 'sample' and r['v'].get('ClsState') == 7]
    registers = [r for r in records if r['kind'] == 'registers']
    times = [r['t'] for r in samples]
    result = {'diagnostic_only': True, 'atomic': False,
              'stack_warning': 'Candidate code pointers only; not an unwound call stack.',
              'samples': len(samples), 'register_samples': len(registers), 'windows': []}
    costs = [r['v'] for r in records if r['kind'] == 'frame_cost']
    if costs:
        costs.sort(key=lambda r: r['PerfFrameMsec'])
        size = max(1, len(costs) // 4)
        result['completed_frame_costs'] = {
            'samples': len(costs),
            'warning': 'Sampled completed frames, not an FPS acceptance average; cycles and guest milliseconds are distinct units.',
            'nominal_tsc_millisecond_fields': ['PerfRenderTotalMsec', 'PerfRenderWorldMsec', 'PerfRenderEntitiesMsec'],
            'tsc_conversion_warning': 'These three legacy fields divide RDTSC deltas by nominal Xbox 733333 cycles/ms. Do not add them to Sys_Milliseconds fields under XEMU.',
            'quartiles': {}}
        for name, rows in [('fastest_quarter', costs[:size]), ('slowest_quarter', costs[-size:])]:
            medians = {key: statistics.median(r[key] for r in rows) for key in rows[0]
                       if key not in ('sample', 'schema', 'MainLoopCount', 'PerfDrawBeginPushMaxState')}
            draw = sum(r['PerfDrawCycles'] for r in rows)
            reserve = sum(r['PerfDrawReserveCycles'] for r in rows)
            push = sum(r['PerfDrawBeginPushCycles'] for r in rows)
            result['completed_frame_costs']['quartiles'][name] = {
                'samples': len(rows), 'medians': medians,
                'reservation_fraction_of_draw_cycles': reserve / draw if draw else None,
                'begin_push_fraction_of_draw_cycles': push / draw if draw else None}
        result['completed_frame_costs']['worst_frames'] = costs[-10:][::-1]
    if not samples:
        return result
    for start in range(int(samples[0]['t'] // 60) * 60, int(samples[-1]['t']) + 1, 60):
        group = [r for r in samples if start <= r['t'] < start + 60]
        if len(group) < 2:
            continue
        long = [r for r in group if r['frame_age'] >= .1]
        verylong = [r for r in group if r['frame_age'] >= .5]
        objects = collections.Counter()
        slow_objects = collections.Counter()
        slow_functions = collections.Counter()
        slow_stack = collections.Counter()
        for r in registers:
            if not start <= r['t'] < start + 60:
                continue
            obj, func = symbols.resolve(r['r'].get('EIP', 0))
            objects[obj] += 1
            i = bisect.bisect_left(times, r['t'])
            nearest = min(samples[max(0, i-1):i+1], key=lambda s: abs(s['t']-r['t']))
            if abs(nearest['t']-r['t']) > .2 or nearest['frame_age'] < .1:
                continue
            slow_objects[obj] += 1
            slow_functions[func] += 1
            candidates = {symbols.resolve(word)[0] for word in r.get('stack', []) if 0x10000 <= word < 0x400000}
            slow_stack.update(candidates - {'unmapped'})
        elapsed = group[-1]['t'] - group[0]['t']
        frames = group[-1]['v']['MainLoopCount'] - group[0]['v']['MainLoopCount']
        deltas = {name: group[-1]['v'].get(name, 0) - group[0]['v'].get(name, 0)
                  for name in ['ScratchDraws', 'ScratchFallbacks', 'ScratchWaitMsec']}
        result['windows'].append({'start_host_s': start, 'elapsed_host_s': elapsed,
            'mainloops_per_host_second': frames / elapsed,
            'scratch_counter_delta_per_mainloop': {k: v / frames if frames else None for k, v in deltas.items()},
            'observations': len(group), 'age_over_100ms_observations': len(long),
            'age_over_500ms_observations': len(verylong),
            'max_observed_frame_age_s': max(r['frame_age'] for r in group),
            'long_cl_stage': dict(collections.Counter(stage(r['v']['ClTailStage']) for r in long)),
            'long_native_stage': dict(collections.Counter(stage(r['v']['NativeSubmitStage']) for r in long)),
            'eip_objects': objects.most_common(12),
            'long_eip_objects': slow_objects.most_common(12),
            'long_eip_functions': slow_functions.most_common(12),
            'long_stack_candidate_objects': slow_stack.most_common(12)})
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flight', type=Path)
    parser.add_argument('map', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows = []
    for line in args.flight.read_text().splitlines():
        try:
            rows.append(json.loads(line))
        except json.JSONDecodeError:
            pass  # A live recorder may not have finished its final line.
    result = analyze(rows, Symbols(args.map))
    args.output.write_text(json.dumps(result, indent=2))
    for name, group in result.get('completed_frame_costs', {}).get('quartiles', {}).items():
        m = group['medians']
        print('%s: %s guest ms/frame, batches=%s indexes=%s audio=%s guest ms, BeginPush share=%s' %
              (name, m['PerfFrameMsec'], m['PerfBackendBatches'], m['PerfBackendIndexes'],
               m['PerfAudioMsec'], group['begin_push_fraction_of_draw_cycles']))
    for w in result.get('windows', []):
        print('%3ds: %.2f loops/host-s, max age %.3fs, slow phases %s; EIP %s' %
              (w['start_host_s'], w['mainloops_per_host_second'], w['max_observed_frame_age_s'],
               w['long_cl_stage'], w['long_eip_objects'][:4]))
