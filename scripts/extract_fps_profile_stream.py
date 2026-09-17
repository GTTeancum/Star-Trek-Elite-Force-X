"""Recover every retained FPS window from a flight recording, failing on gaps."""
import argparse
import json
from pathlib import Path
import re


def extract(path, endpoint=None):
    lines = {}
    previous = 0
    for raw in path.read_text().splitlines():
        row = json.loads(raw)
        if row.get('kind') != 'fps_profiles':
            continue
        if row['dropped_records']:
            raise ValueError('FPS publication history was lost')
        for record in row['records']:
            serial = record['publication']
            text = record['line']
            match = re.search(r'\bsample=(\d+)\b', text)
            if serial != previous + 1 or not match or int(match[1]) != serial:
                raise ValueError('FPS publication sequence mismatch')
            lines[serial] = text
            previous = serial
        if row['index'] != previous:
            raise ValueError('FPS publication index mismatch')
    if not lines:
        raise ValueError('No streamed FPS profiles')
    if endpoint:
        for text in endpoint.read_text().splitlines():
            match = re.search(r'\bsample=(\d+)\b', text)
            if not match:
                raise ValueError('Malformed endpoint profile')
            serial = int(match[1])
            if serial in lines and lines[serial] != text:
                raise ValueError('Stream and paused endpoint disagree')
            lines[serial] = text
    if sorted(lines) != list(range(1, max(lines) + 1)):
        raise ValueError('Incomplete FPS history')
    return [lines[i] for i in sorted(lines)]


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('flight', type=Path)
    p.add_argument('--endpoint', type=Path)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    records = extract(a.flight, a.endpoint)
    a.output.write_text('\n'.join(records) + '\n')
    print('Complete FPS windows:', len(records), '(contiguous; endpoint overlap verified when supplied)')
