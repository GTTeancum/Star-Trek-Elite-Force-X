"""Preserve a completed campaign probe and update its readable tracker row."""
import argparse
import json
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True)
    parser.add_argument('--map', required=True)
    parser.add_argument('--note', required=True)
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_]+', args.probe + args.map):
        parser.error('Probe and map names must contain only letters, digits, underscores')
    if '|' in args.note or '\n' in args.note:
        parser.error('The note must fit one table cell')
    research = ROOT / 'build/research/letterbox_perf'
    summary_path = research / (args.probe + '.summary.json')
    manifest_path = research / (args.probe + '.iso_transaction.json')
    summary = json.loads(summary_path.read_text())
    manifest = json.loads(manifest_path.read_text())
    if not manifest.get('restored'):
        raise ValueError('The ISO transaction has not finished restoring')
    if summary.get('map') != args.map:
        raise ValueError('Requested map does not match the probe')
    tracker = ROOT / 'notes/campaign_checks_2026-09-09.md'
    text = tracker.read_text(encoding='utf-8')
    pattern = re.compile(r'(^\| ' + re.escape(args.map) + r' \| )[^|]*( \|.*$)', re.M)
    if len(pattern.findall(text)) != 1:
        raise ValueError('Expected exactly one campaign tracker row')
    destination = ROOT / 'notes/evidence/campaign_20260909' / args.probe
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(summary_path, destination / 'summary.json')
    shutil.copyfile(manifest_path, destination / 'transaction.json')
    # Keep the bounded runtime evidence available after smoke-output rotation.
    for evidence_pattern, name in [('*_xblog_profiles.log', 'ram_candidates.log'),
                          ('*_final_engine_error_message.bin', 'engine_error_message.bin'),
                          ('*_final_last_command_text.bin', 'last_command_text.bin'),
                          ('*_final_log_mirror.bin', 'log_mirror.bin')]:
        sources = sorted(research.glob(args.probe + evidence_pattern))
        if sources:
            shutil.copyfile(sources[-1], destination / name)
    current_fps = research / (args.probe + '.current_profiles.log')
    if current_fps.exists():
        shutil.copyfile(current_fps, destination / 'current_fps.log')
    entities = sorted(research.glob(args.probe + '_*_entities.json'))
    if entities:
        shutil.copyfile(entities[-1], destination / 'entities.json')
    log = (research / (args.probe + '.run.log')).read_text(errors='replace')
    shots = re.findall(r'shot=\d+ .*?file=(.*)', log)
    if shots:
        for label, source in [('first', shots[0]), ('last', shots[-1])]:
            source = Path(source.strip())
            if source.exists():
                shutil.copyfile(source, destination / (label + '.png'))
    # This records evidence, not an automatic pass or a claim of route completion.
    text = pattern.sub(lambda match: match[1] + args.note + match[2], text)
    tracker.write_text(text, encoding='utf-8')
    print(json.dumps({'map': args.map, 'loaded_map': summary.get('loaded_map'),
                      'client_state': summary.get('final_client_state'),
                      'god_mode': summary.get('god_mode'),
                      'evidence': str(destination)}, indent=2))


if __name__ == '__main__':
    main()
