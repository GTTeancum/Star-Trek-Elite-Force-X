"""Compare matching-binary completed packed-index memory snapshots."""
import argparse
import json
from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(root / 'scripts'))
from run_sp_perf_probe import exact_probe_files

fields = ['physical_free', 'zone_size', 'zone_used', 'zone_free', 'largest_free',
          'zone_lifetime_peak', 'zone_overhead', 'filesys_bytes', 'bsp_bytes', 'free_blocks']
phases = ['before_indices', 'indices_ready', 'after_triangles', 'after_faces',
          'indices_released', 'after_flares']


def read(name):
    out = root / 'build/research/letterbox_perf'
    manifest = json.loads((out / (name + '.iso_transaction.json')).read_text())
    summary = json.loads((out / (name + '.summary.json')).read_text())
    assert manifest['restored']
    assert summary['final_liveness_verified'] and summary['finished_in_game']
    assert not summary['engine_error_message']
    files = exact_probe_files(out, name)('_final_index_stream_memory.bin')
    assert len(files) == 1, files
    raw = files[0].read_bytes()
    assert len(raw) == 256
    words = struct.unpack('<64I', raw)
    assert words[0] == 0x49534D31 and words[3] == 63, words[:4]
    return dict(name=name, xbe_sha256=manifest['xbe_sha256'], map=manifest['map'],
                streaming=words[1], index_file_bytes=words[2],
                phases={phase: dict(zip(fields, words[4+i*10:14+i*10]))
                        for i, phase in enumerate(phases)})


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('control')
    parser.add_argument('stream')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    control, stream = read(args.control), read(args.stream)
    assert control['streaming'] == 0 and stream['streaming'] == 1
    for key in ['xbe_sha256', 'map', 'index_file_bytes']:
        assert control[key] == stream[key], key
    delta = {phase: {field: stream['phases'][phase][field] - control['phases'][phase][field]
                     for field in fields} for phase in phases}
    result = dict(control=control, stream=stream, stream_minus_control=delta,
                  limits='Phase snapshots, not continuous extrema. Zone peak is lifetime-to-date. '
                         'Physical reserve and zone capacity are unchanged by the candidate; '
                         'loading scratch reduction need not increase physical free RAM.')
    args.output.write_text(json.dumps(result, indent=2))
    print(json.dumps(delta, indent=2))
