"""Measure a process-local SDL S16LE/48k/stereo capture against a source WAV.

This reports signal evidence, not a subjective or retail-hardware qualification.
The reference is decoded with ffmpeg (including Xbox ADPCM). Alignment uses
normalized waveform correlation, and low-confidence matches are rejected.
"""
import argparse
import json
import subprocess
from pathlib import Path

import numpy as np
from scipy import signal


def measure(capture, reference, rate=48000):
    decoded = subprocess.run([
        "ffmpeg", "-v", "error", "-i", str(reference), "-f", "f32le",
        "-ac", "1", "-ar", str(rate), "pipe:1"],
        check=True, capture_output=True).stdout
    ref = np.frombuffer(decoded, dtype="<f4").astype(np.float64)
    raw = np.fromfile(capture, dtype="<i2")
    if len(raw) % 2 or len(raw) < 2 * len(ref):
        raise ValueError("Capture is truncated or shorter than the reference")
    stereo = raw.reshape(-1, 2).astype(np.float64) / 32768.0
    # Search each ear separately; averaging first can hide phase cancellation.
    candidates = []
    for channel in range(2):
        x = signal.resample_poly(stereo[:, channel], 1, 6)
        y = signal.resample_poly(ref, 1, 6)
        y -= y.mean()
        corr = signal.correlate(x, y, mode="valid", method="fft")
        energy = np.r_[0.0, np.cumsum(x * x)]
        energy = energy[len(y):] - energy[:-len(y)]
        norm = np.sqrt(np.maximum(energy, 0) * np.dot(y, y))
        scores = np.divide(corr, norm, out=np.zeros_like(corr), where=norm > 1e-12)
        at = int(np.argmax(np.abs(scores)))
        candidates.append((abs(float(scores[at])), at * 6, channel))
    confidence, offset, channel = max(candidates)
    matched = confidence >= 0.35
    result = {"capture": str(capture), "reference": str(reference),
              "seconds": len(stereo) / rate, "peak": float(np.max(np.abs(stereo))),
              "rms": float(np.sqrt(np.mean(stereo ** 2))),
              "clipped_fraction": float(np.mean(np.abs(stereo) >= 32767 / 32768)),
              "match_accepted": matched, "correlation": confidence,
              "match_seconds": offset / rate, "match_channel": channel}
    if not matched:
        return result
    segment = stereo[offset:offset + len(ref), channel]
    n = min(len(segment), len(ref))
    result["matched_segment_peak"] = float(np.max(np.abs(segment[:n])))
    result["matched_segment_clipped_fraction"] = float(np.mean(np.abs(segment[:n]) >= 32767 / 32768))
    freqs, ref_psd = signal.welch(ref[:n], rate, nperseg=2048)
    _, out_psd = signal.welch(segment[:n], rate, nperseg=2048)
    bands = {"low_200_1000": (200, 1000), "presence_2000_6000": (2000, 6000),
             "high_6000_12000": (6000, 12000)}
    gains = {}
    for label, (lo, hi) in bands.items():
        mask = (freqs >= lo) & (freqs < hi)
        gains[label] = float(10 * np.log10((out_psd[mask].sum() + 1e-20) /
                                         (ref_psd[mask].sum() + 1e-20)))
    result["band_gain_db"] = gains
    result["presence_relative_to_low_db"] = gains["presence_2000_6000"] - gains["low_200_1000"]
    result["caveat"] = "Mixed scene audio can bias band measurements; correlation is an alignment check."
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = measure(args.capture, args.reference)
    text = json.dumps(result, indent=2)
    print(text)
    if args.output:
        args.output.write_text(text + "\n", encoding="utf-8")
