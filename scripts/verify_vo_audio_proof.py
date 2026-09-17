"""Verify the borg1 dialogue acceptance gates for an existing captured Release run."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import tomllib
from analyze_vo_capture import measure

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--capture", type=Path, required=True)
p.add_argument("--assets", type=Path, required=True)
p.add_argument("--xbe", type=Path, required=True)
p.add_argument("--archived", action="store_true", help="Require the restored archived-audio fixture")
a = p.parse_args()
metadata = json.loads(a.capture.with_suffix(".json").read_text())
variant = json.loads(a.capture.with_suffix(".variant.json").read_text())
config = tomllib.loads(metadata["config"])
assert config["audio"]["use_dsp"] is True
assert config["audio"].get("hrtf", True) is True
assert not metadata["errors"] and metadata["exit_before_stop"] is None
assert set(metadata["monitor_point_frames"]) == {"4"}
assert a.capture.stat().st_size == metadata["bytes"]
assert variant["iso_restored"]
assert variant["xbe_sha256"] == hashlib.sha256(a.xbe.read_bytes()).hexdigest()
if a.archived:
    fixture = json.loads(a.capture.with_suffix('.directory_restore.json').read_text())
    assert fixture['restored'] and fixture['xbe_modified'] is False
    assert fixture['hidden_during_test'] == ['BaseEF/sound', 'BaseEF/soundbank']
paths = {
    "janeway": "janeway/cin/01/captainslog1.wav",
    "player_cutoff": "alexa/cin/01/ivebeencutoff.wav",
    "tuvok": "tuvok/cin/01/ms_isolated.wav",
    "player_ack": "alexa/cin/01/acknowledged.wav",
    "player_tertiary": "alexa/cin/01/tertiarywhat.wav",
}
results = {k: measure(a.capture, a.assets / "sound/voice" / v) for k,v in paths.items()}
for key, result in results.items():
    assert result["match_accepted"], key
    assert result["clipped_fraction"] == 0 and result["matched_segment_clipped_fraction"] == 0, key
    assert abs(result["presence_relative_to_low_db"]) < 2.0, key
    assert result["correlation"] >= (0.55 if key.startswith("player_") else 0.35), key
assert results["janeway"]["match_seconds"] < results["player_cutoff"]["match_seconds"] < results["tuvok"]["match_seconds"] < results["player_ack"]["match_seconds"] < results["player_tertiary"]["match_seconds"]
log = a.capture.with_name(a.capture.stem + "_xblog_profiles.log").read_text()
runtime_log = a.capture.with_suffix('.runtime.log')
if runtime_log.exists():
    log += '\n' + runtime_log.read_text()
assert "STEFX_ZONE_INVALID_FREE" not in log, "Invalid memory free during campaign"
assert "FS_HandleForFile: none free" not in log, "File handle leak during campaign"
assert "EFALLOC_FATAL:" not in log, "Fatal allocation during campaign"
if a.archived:
    for line_name in ['ivebeencutoff', 'acknowledged', 'tertiarywhat']:
        assert re.search(r"STEFX_VOICE_SPATIAL: mode=speaker-pan .*" + line_name + r"\.mp3'", log), line_name
assert "mode=speaker-pan listeners=1" in log
assert "mode=hrtf listeners=1" in log
assert re.search(r"STEFX_VOICE_SPATIAL: pos=.*hr=0x00000000", log)
assert not re.search(r"STEFX_VOICE_SPATIAL: .*hr=0x(?!00000000)[0-9a-fA-F]{8}", log)
times = [int(t) for t in re.findall(r"serverTime=(\d+)", log)]
assert times and max(times) >= 130000
flare_results = re.findall(r"STEFX_FLARE_QUERY: submit end id=(\d+) hr=0x([0-9a-fA-F]{8}) retry=(\d+)", log)
for query_id, hr, retry in flare_results:
    assert bool(int(hr, 16) & 0x80000000) == bool(int(retry)), query_id
report = {"status": "PASS", "xbe_sha256": variant["xbe_sha256"],
          "asset_path": "archived" if a.archived else "packaged",
          "logged_flare_failures_retried": sorted({int(query_id) for query_id, hr, retry in flare_results if int(retry)}),
          "max_server_time": max(times), "results": results,
          "scope": "Borg1 opening SP, XEMU GP/EP with HRTF on; no retail hardware claim"}
a.capture.with_suffix(".acceptance.json").write_text(json.dumps(report, indent=2))
print("PASS: exact Release XBE, DSP output, normal HRTF, five dialogue matches, <2 dB coloration, zero clipping, effects routing restored, game progress >130 seconds, ISO restored")
