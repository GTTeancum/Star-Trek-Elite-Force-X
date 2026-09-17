"""Ensure fast intro samples and fallback heartbeats cannot inflate gameplay FPS."""
from pathlib import Path
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from analyze_hardware_profile import analyze

with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)/'sample.log'
    path.write_text('STEFX_HW_FPS_SAMPLE: sample=2 fps=120.0 gameplay=0 excludedChecks=40\n'
                    'STEFX_HW_FPS_SAMPLE: sample=3 fps=42.0 gameplay=1 excludedChecks=0\n')
    result = analyze(path)
    assert result['fpsProfileSamples'] == 1 and result['fps']['mean'] == 42
    path.write_text('STEFX_HW_FPS_SAMPLE: sample=2 fps=120.0 gameplay=0 excludedChecks=40\n'
                    'FRAME_HEARTBEAT fps=120.0\n')
    result = analyze(path)
    assert result['fpsProfileSamples'] == 0 and result['fps']['samples'] == 0
print('PASS: intro excluded; intro-only run has no gameplay FPS')
