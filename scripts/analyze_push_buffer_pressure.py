"""Relate completed-frame command reservations to Xbox BeginPush waits."""
import argparse
import json
import math
import statistics
from pathlib import Path


def percentile(values, fraction):
    ordered = sorted(values)
    if not ordered:
        return None
    position = (len(ordered) - 1) * fraction
    lower = int(math.floor(position))
    upper = int(math.ceil(position))
    if lower == upper:
        return ordered[lower]
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def summarize(rows, start=None, end=None):
    if start is not None:
        rows = [row for row in rows if row.get("t", float("-inf")) >= start]
    if end is not None:
        rows = [row for row in rows if row.get("t", float("inf")) <= end]
    costs = {row["v"]["sample"]: row["v"] for row in rows if row.get("kind") == "frame_cost"}
    paired = []
    for row in rows:
        if row.get("kind") != "draw_kinds":
            continue
        kinds = row["v"]
        cost = costs.get(kinds["sample"])
        if not cost or kinds["MainLoopCount"] != cost["MainLoopCount"]:
            continue
        reserved = sum(kind["reserved_dwords"] for kind in kinds["kinds"].values())
        paired.append({
            "sample": kinds["sample"],
            "guest_ms": kinds["frame_guest_ms"],
            "reserved_dwords": reserved,
            "reserved_bytes": reserved * 4,
            "begin_push_cycles": cost["PerfDrawBeginPushCycles"],
            "draw_cycles": cost["PerfDrawCycles"],
            "calls": sum(kind["calls"] for kind in kinds["kinds"].values()),
        })
    if len(paired) < 8:
        raise ValueError("Insufficient coherent paired frame samples")

    def group(items):
        return {
            "samples": len(items),
            "guest_ms_median": statistics.median(item["guest_ms"] for item in items),
            "reserved_bytes_median": statistics.median(item["reserved_bytes"] for item in items),
            "begin_push_cycles_median": statistics.median(item["begin_push_cycles"] for item in items),
            "draw_cycles_median": statistics.median(item["draw_cycles"] for item in items),
            "calls_median": statistics.median(item["calls"] for item in items),
        }

    reservations = [item["reserved_bytes"] for item in paired]
    thresholds = {}
    # The kickoff threshold is reserved from the nominal primary allocation.
    for nominal_kib in (1024, 1152, 1280, 1536):
        usable = (nominal_kib - 128) * 1024
        over = [item for item in paired if item["reserved_bytes"] > usable]
        thresholds[str(nominal_kib)] = {
            "nominal_bytes": nominal_kib * 1024,
            "usable_before_kickoff_bytes": usable,
            "frames_over_usable": len(over),
            "fraction_over_usable": len(over) / len(paired),
            "over_group": group(over) if over else None,
        }
    ordered = sorted(paired, key=lambda item: item["guest_ms"])
    quarter = max(1, len(ordered) // 4)
    return {
        "paired_samples": len(paired),
        "host_time_range": {"start": start, "end": end},
        "reservation_percentiles_bytes": {
            "p50": percentile(reservations, 0.50),
            "p75": percentile(reservations, 0.75),
            "p90": percentile(reservations, 0.90),
            "p95": percentile(reservations, 0.95),
            "max": max(reservations),
        },
        "all": group(paired),
        "fast_quarter": group(ordered[:quarter]),
        "slow_quarter": group(ordered[-quarter:]),
        "thresholds_kib": thresholds,
        "limitations": [
            "Reservations are requested command/index/inline-payload dwords, not direct GPU time.",
            "Crossing a usable threshold predicts a kickoff; it does not prove that every kickoff blocks.",
            "Diagnostic sampled frames are used for attribution, not FPS acceptance."
        ],
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("flight", type=Path)
    parser.add_argument("--start", type=float)
    parser.add_argument("--end", type=float)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    records = [json.loads(line) for line in args.flight.read_text().splitlines()]
    result = summarize(records, args.start, args.end)
    encoded = json.dumps(result, indent=2)
    if args.output:
        args.output.write_text(encoded)
    print(encoded)
