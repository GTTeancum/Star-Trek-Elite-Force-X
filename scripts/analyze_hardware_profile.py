#!/usr/bin/env python3
"""Summarize STEFX retail-Xbox frame profiling from an SP or MP log."""

from __future__ import annotations

import argparse
import json
import math
import re
import statistics
from pathlib import Path
from typing import Iterable


PROFILE_MARKER = "STEFX_HW_FRAME_PROFILE:"
FPS_PROFILE_MARKER = "STEFX_HW_FPS_SAMPLE:"
ALIGNED_PROFILE_MARKER = "STEFX_HW_RENDER_SAMPLE:"
HEARTBEAT_MARKER = "FRAME_HEARTBEAT"
TEXTURE_STAGE_CACHE_MARKER = "STEFX_HW_STAGE_CACHE:"
VERTEX_SHADER_CACHE_MARKER = "STEFX_HW_VERTEX_SHADER_CACHE:"
STREAM_SOURCE_CACHE_MARKER = "STEFX_HW_STREAM_SOURCE_CACHE:"
MDR_PALETTE_CACHE_MARKER = "STEFX_HW_MDR_PALETTE_CACHE:"
PAIR_RE = re.compile(r"([A-Za-z][A-Za-z0-9]*)=([^\s]+)")
FPS_RE = re.compile(r"^(\d+)\.(\d+)$")


def parse_scalar(value: str) -> int | float | str | list[int]:
    fps_match = FPS_RE.match(value)
    if fps_match:
        fraction = fps_match.group(2)
        return int(fps_match.group(1)) + int(fraction) / (10 ** len(fraction))
    if "/" in value:
        fields = value.split("/")
        if all(field.isdigit() for field in fields):
            return [int(field) for field in fields]
    try:
        return int(value, 0)
    except ValueError:
        return value


def parse_records(lines: Iterable[str], marker: str) -> list[dict[str, object]]:
    records: list[dict[str, object]] = []
    for line_number, line in enumerate(lines, 1):
        if marker not in line:
            continue
        payload = line.split(marker, 1)[1]
        record: dict[str, object] = {"line": line_number}
        for key, value in PAIR_RE.findall(payload):
            record[key] = parse_scalar(value.rstrip(",;"))
        records.append(record)
    return records


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return 0.0
    index = (len(ordered) - 1) * fraction
    lower = math.floor(index)
    upper = math.ceil(index)
    if lower == upper:
        return ordered[lower]
    weight = index - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def numeric_series(records: list[dict[str, object]], key: str) -> list[float]:
    values: list[float] = []
    for record in records:
        value = record.get(key)
        if isinstance(value, (int, float)):
            values.append(float(value))
    return values


def ratio_series(
    records: list[dict[str, object]], numerator: str, denominator: str, scale: float = 1.0
) -> list[float]:
    values: list[float] = []
    for record in records:
        top = record.get(numerator)
        bottom = record.get(denominator)
        if not isinstance(top, (int, float)) or not isinstance(bottom, (int, float)):
            continue
        if float(bottom) <= 0.0:
            continue
        values.append(float(top) * scale / float(bottom))
    return values


def settled_profile_records(
    profiles: list[dict[str, object]],
) -> tuple[list[dict[str, object]], str]:
    if len(profiles) <= 1:
        return profiles, "all records (insufficient samples to separate startup)"

    aligned = [
        record
        for record in profiles
        if isinstance(record.get("sample"), (int, float))
        and isinstance(record.get("total"), (int, float))
        and float(record["total"]) < 1000.0
    ]
    if aligned:
        return aligned, "aligned end-of-frame samples with total<1000ms"

    # The first ten-second sample often represents one multi-second loading
    # frame. Preserve it in raw statistics, but do not let it dominate the
    # gameplay diagnosis when later completed frames are available.
    settled = [
        record
        for record in profiles
        if isinstance(record.get("frame"), (int, float))
        and float(record["frame"]) >= 10.0
        and isinstance(record.get("total"), (int, float))
        and float(record["total"]) < 1000.0
    ]
    if settled:
        return settled, "frame>=10 and total<1000ms"

    return profiles[-1:], "latest record fallback (no clearly settled sample)"


def list_index_series(records: list[dict[str, object]], key: str, index: int) -> list[float]:
    values: list[float] = []
    for record in records:
        value = record.get(key)
        if isinstance(value, list) and len(value) > index:
            values.append(float(value[index]))
    return values


def list_sum_series(records: list[dict[str, object]], key: str) -> list[float]:
    values: list[float] = []
    for record in records:
        value = record.get(key)
        if isinstance(value, list):
            values.append(float(sum(value)))
    return values


def summarize_series(values: list[float]) -> dict[str, float | int]:
    if not values:
        return {"samples": 0}
    return {
        "samples": len(values),
        "min": min(values),
        "mean": statistics.fmean(values),
        "median": statistics.median(values),
        "p95": percentile(values, 0.95),
        "max": max(values),
    }


def frame_progress(records: list[dict[str, object]]) -> dict[str, object]:
    frames = [int(value) for value in numeric_series(records, "frame")]
    if not frames:
        frames = [int(value) for value in numeric_series(records, "completedFrame")]
    if not frames:
        return {"samples": 0}
    nonadvancing = sum(1 for previous, current in zip(frames, frames[1:]) if current <= previous)
    return {
        "samples": len(frames),
        "first": frames[0],
        "last": frames[-1],
        "advanced": frames[-1] > frames[0],
        "nonadvancingIntervals": nonadvancing,
    }


def memory_summary(heartbeats: list[dict[str, object]]) -> dict[str, object]:
    samples = [record["mem"] for record in heartbeats if isinstance(record.get("mem"), list)]
    samples = [sample for sample in samples if len(sample) >= 4]
    if not samples:
        return {"samples": 0}
    used = [sample[0] for sample in samples]
    free = [sample[1] for sample in samples]
    largest = [sample[2] for sample in samples]
    return {
        "samples": len(samples),
        "usedFirst": used[0],
        "usedLast": used[-1],
        "usedDelta": used[-1] - used[0],
        "usedPeak": max(used),
        "freeMinimum": min(free),
        "largestFreeMinimum": min(largest),
    }


def cumulative_summary(records: list[dict[str, object]], key: str) -> dict[str, object]:
    values = numeric_series(records, key)
    if not values:
        return {"samples": 0}
    return {
        "samples": len(values),
        "first": values[0],
        "last": values[-1],
        "delta": values[-1] - values[0],
        "maximum": max(values),
        "decreasedIntervals": sum(
            1 for previous, current in zip(values, values[1:]) if current < previous
        ),
    }


def state_cache_summary(records: list[dict[str, object]]) -> dict[str, object]:
    if not records:
        return {"samples": 0}
    latest = records[-1]
    summary: dict[str, object] = {"samples": len(records)}
    for key in (
        "sample", "players", "requests", "emitted", "skipped", "skipPct", "bypass", "slots"
    ):
        value = latest.get(key)
        if isinstance(value, (int, float)):
            summary[key] = value
    requests = summary.get("requests")
    emitted = summary.get("emitted")
    skipped = summary.get("skipped")
    if all(isinstance(value, (int, float)) for value in (requests, emitted, skipped)):
        summary["accountingBalanced"] = requests == emitted + skipped
    return summary


def topology_summary(records: list[dict[str, object]]) -> dict[str, object]:
    if not records:
        return {"samples": 0}
    latest = records[-1]
    summary: dict[str, object] = {"samples": len(records)}
    for key in ("players", "humans", "bots", "source", "virtual"):
        value = latest.get(key)
        if isinstance(value, (int, float, str, list)):
            summary[key] = value
    players = summary.get("players")
    humans = summary.get("humans")
    bots = summary.get("bots")
    if all(isinstance(value, (int, float)) for value in (players, humans, bots)):
        summary["accountingBalanced"] = players == humans + bots
    return summary


def median_from_summary(summary: object) -> float | None:
    if not isinstance(summary, dict):
        return None
    value = summary.get("median")
    return float(value) if isinstance(value, (int, float)) else None


def dominant_phase_diagnosis(
    phases: dict[str, object], cycle_phases: dict[str, object]
) -> dict[str, object]:
    frontend = median_from_summary(phases.get("frontend"))
    backend = median_from_summary(phases.get("backend"))
    server = median_from_summary(phases.get("server"))
    client = median_from_summary(phases.get("client"))
    detailed_renderer_available = bool((frontend or 0.0) > 0.0 or (backend or 0.0) > 0.0)
    top_level = {
        key: value
        for key, value in (
            ("frontend", frontend),
            ("backend", backend),
            ("server", server),
            ("client", client if not detailed_renderer_available else None),
        )
        if value is not None
    }
    if not top_level:
        return {"available": False}

    dominant = max(top_level, key=top_level.get)
    result: dict[str, object] = {
        "available": True,
        "dominant": dominant,
        "medianMsec": top_level[dominant],
    }

    if dominant == "frontend":
        child_names = (
            "frontendSetup",
            "frontendMarkLeaves",
            "frontendWorld",
            "frontendPolys",
            "frontendProjection",
            "frontendEntities",
            "frontendSort",
            "frontendDebug",
        )
        recommendation = "Profile the dominant frontend owner before changing D3D submission."
    elif dominant == "backend":
        child_names = ("backendDrawSurfs", "backendSwap", "backendOther")
        recommendation = "Use the backend child and draw-cycle split to choose the next renderer target."
    elif dominant == "client":
        child_names = ()
        recommendation = (
            "The production log exposes only the inclusive client/render boundary; "
            "use a bounded profiled-renderer A/B to separate frontend, submission, and GPU waits."
        )
    else:
        child_names = ("gamePre", "gameEntities", "gamePost")
        recommendation = "Renderer work is not the largest measured boundary; inspect server/game phases."

    children = {
        key: value
        for key in child_names
        if (value := median_from_summary(phases.get(key))) is not None
    }
    if children:
        child = max(children, key=children.get)
        result["dominantChild"] = child
        result["childMedianMsec"] = children[child]

        if child == "backendDrawSurfs" and cycle_phases:
            cycle_medians = {
                key: value
                for key, summary in cycle_phases.items()
                if key != "drawCycles"
                if (value := median_from_summary(summary)) is not None
            }
            if cycle_medians:
                cycle_child = max(cycle_medians, key=cycle_medians.get)
                result["dominantDrawCycle"] = cycle_child
                result["drawCycleMedian"] = cycle_medians[cycle_child]
        elif child == "backendSwap":
            finish = median_from_summary(phases.get("finish"))
            present = median_from_summary(phases.get("present"))
            if finish is not None or present is not None:
                waits = {"finish": finish or 0.0, "present": present or 0.0}
                wait = max(waits, key=waits.get)
                result["dominantSwapWait"] = wait
                result["swapWaitMedianMsec"] = waits[wait]

    result["recommendation"] = recommendation
    return result


def analyze(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    legacy_profiles = parse_records(lines, PROFILE_MARKER)
    fps_profiles_all = parse_records(lines, FPS_PROFILE_MARKER)
    context_tagged = any("gameplay" in record for record in fps_profiles_all)
    fps_profiles = [
        record
        for record in fps_profiles_all
        if isinstance(record.get("sample"), (int, float))
        and int(record["sample"]) > 1
        and (not context_tagged or
             (record.get("gameplay") == 1 and record.get("excludedChecks") == 0))
    ]
    aligned_profiles = parse_records(lines, ALIGNED_PROFILE_MARKER)
    profiles = aligned_profiles or legacy_profiles
    profile_kind = "aligned" if aligned_profiles else ("legacy" if legacy_profiles else "none")
    heartbeats = parse_records(lines, HEARTBEAT_MARKER)
    texture_stage_cache = parse_records(lines, TEXTURE_STAGE_CACHE_MARKER)
    vertex_shader_cache = parse_records(lines, VERTEX_SHADER_CACHE_MARKER)
    stream_source_cache = parse_records(lines, STREAM_SOURCE_CACHE_MARKER)
    mdr_palette_cache = parse_records(lines, MDR_PALETTE_CACHE_MARKER)
    settled_profiles, settled_selection = settled_profile_records(profiles)
    profile_fps = numeric_series(fps_profiles, "fps")
    fps_source = ("gameplay-only-fps-ring" if context_tagged else
                  "unclassified-fps-ring") if profile_fps else "none"
    if not profile_fps and not context_tagged:
        profile_fps = numeric_series(settled_profiles, "fps")
        if profile_fps:
            fps_source = "profile"
    if not profile_fps and not context_tagged and aligned_profiles and legacy_profiles:
        # Production logs can retain detailed end-of-frame samples separately
        # from the periodic frame/FPS record. Match their monotonically
        # increasing sample IDs so the loading-only sample 0 is not folded
        # into settled gameplay FPS.
        settled_sample_ids = {
            int(record["sample"])
            for record in settled_profiles
            if isinstance(record.get("sample"), (int, float))
        }
        paired_fps_profiles = [
            record
            for record in legacy_profiles
            if isinstance(record.get("sample"), (int, float))
            and int(record["sample"]) in settled_sample_ids
            and isinstance(record.get("total"), (int, float))
            and float(record["total"]) < 1000.0
        ]
        profile_fps = numeric_series(paired_fps_profiles, "fps")
        if profile_fps:
            fps_source = "sample-paired-profile"
    heartbeat_fps = [] if context_tagged else numeric_series(heartbeats, "fps")
    fps = profile_fps or heartbeat_fps
    if not profile_fps and heartbeat_fps:
        fps_source = "heartbeat"
    totals = (
        numeric_series(fps_profiles, "total")
        or numeric_series(settled_profiles, "total")
        or list_index_series(heartbeats, "perf", 0)
    )
    raw_profile_fps = numeric_series(profiles, "fps")
    raw_profile_totals = numeric_series(profiles, "total")

    phases: dict[str, object] = {}
    for key in (
        "server", "client", "game", "frontend", "backend", "audio", "screen",
        "endFrame", "finish", "present"
    ):
        values = numeric_series(fps_profiles, key) or numeric_series(settled_profiles, key)
        if values:
            phases[key] = summarize_series(values)
    for tuple_key, names in (
        ("gamePhases", ("gamePre", "gameEntities", "gamePost")),
        (
            "frontendPhases",
            (
                "frontendSetup",
                "frontendMarkLeaves",
                "frontendWorld",
                "frontendPolys",
                "frontendProjection",
                "frontendEntities",
                "frontendSort",
                "frontendDebug",
            ),
        ),
    ):
        total = list_sum_series(settled_profiles, tuple_key)
        if total:
            if tuple_key == "gamePhases":
                phases["game"] = summarize_series(total)
            elif "frontend" not in phases:
                phases["frontend"] = summarize_series(total)
            else:
                phases["frontendChildren"] = summarize_series(total)
        for index, key in enumerate(names):
            values = list_index_series(settled_profiles, tuple_key, index)
            if values:
                phases[key] = summarize_series(values)
    for index, key in enumerate(("backendDrawSurfs", "backendSwap", "backendOther")):
        values = list_index_series(settled_profiles, "backendPhases", index)
        if values:
            phases[key] = summarize_series(values)
    cycle_phases = {
        key: summarize_series(values)
        for key in ("drawCycles", "state", "reserve", "pack", "index", "submitCycles")
        if (values := numeric_series(settled_profiles, key))
    }
    for index, key in enumerate(("setStream", "beginPush", "pushPointer")):
        values = list_index_series(settled_profiles, "reserveParts", index)
        if values:
            cycle_phases[key] = summarize_series(values)
    for index, key in enumerate(
        (
            "beginPushMax",
            "beginPushMaxDwords",
            "beginPushOver100K",
            "beginPushOver1Msec",
            "beginPushOver10Msec",
        )
    ):
        values = list_index_series(settled_profiles, "beginPushDetail", index)
        if values:
            cycle_phases[key] = summarize_series(values)
    frontend_cycle_phases = {}
    for index, key in enumerate(
        (
            "frontendSetupCycles",
            "frontendMarkLeavesCycles",
            "frontendWorldCycles",
            "frontendPolysCycles",
            "frontendProjectionCycles",
            "frontendEntitiesCycles",
            "frontendSortCycles",
            "frontendDebugCycles",
        )
    ):
        values = list_index_series(settled_profiles, "frontendCycles", index)
        if values:
            frontend_cycle_phases[key] = summarize_series(values)
    workload = {
        key: summarize_series(values)
        for key in (
            "views", "portals", "leaves", "inputSurfs", "refEntities", "batches",
            "submits", "indexed", "immediate", "tex1", "verts", "indexes", "totalIndexes"
        )
        if (values := numeric_series(settled_profiles, key))
    }
    for index, key in enumerate(("reserveVertexDwords", "reserveIndexDwords")):
        values = list_index_series(settled_profiles, "reserveDwords", index)
        if values:
            workload[key] = summarize_series(values)
    for index, key in enumerate(
        (
            "worldNodes",
            "worldLeafs",
            "worldMarkSurfaces",
            "worldDuplicateSurfaces",
            "worldCulledSurfaces",
            "worldAddedSurfaces",
            "worldDlightSurfaces",
        )
    ):
        values = list_index_series(settled_profiles, "worldWork", index)
        if values:
            workload[key] = summarize_series(values)
    normalized_costs = {
        label: summarize_series(values)
        for label, numerator, denominator, scale in (
            ("drawCyclesPerSubmit", "drawCycles", "submits", 1.0),
            ("stateCyclesPerSubmit", "state", "submits", 1.0),
            ("reserveCyclesPerSubmit", "reserve", "submits", 1.0),
            ("packCyclesPerVertex", "pack", "verts", 1.0),
            ("indexCyclesPerIndex", "index", "indexes", 1.0),
            ("submitCyclesPerSubmit", "submitCycles", "submits", 1.0),
            ("backendUsecPerBatch", "backend", "batches", 1000.0),
            ("frontendUsecPerInputSurf", "frontend", "inputSurfs", 1000.0),
        )
        if (values := ratio_series(settled_profiles, numerator, denominator, scale))
    }
    world_ratios = {
        label: summarize_series(values)
        for label, numerator_index, denominator_index, scale in (
            ("worldMarksPerLeaf", 2, 1, 1.0),
            ("worldDuplicateRatePercent", 3, 2, 100.0),
            ("worldCullRatePercent", 4, 2, 100.0),
            ("worldAddRatePercent", 5, 2, 100.0),
            ("worldDlightRatePercent", 6, 2, 100.0),
        )
        if (
            values := [
                numerator * scale / denominator
                for record in settled_profiles
                if isinstance(record.get("worldWork"), list)
                and len(record["worldWork"]) > max(numerator_index, denominator_index)
                and (denominator := float(record["worldWork"][denominator_index])) > 0.0
                and (numerator := float(record["worldWork"][numerator_index])) >= 0.0
            ]
        )
    }
    normalized_costs.update(world_ratios)
    if not profiles:
        for index, key in enumerate(("frame", "server", "client", "game", "frontend", "backend")):
            values = list_index_series(heartbeats, "perf", index)
            if values:
                phases[key] = summarize_series(values)
        audio = numeric_series(heartbeats, "audio")
        if audio:
            phases["audio"] = summarize_series(audio)
        for index, key in enumerate(("screen", "endFrame")):
            values = list_index_series(heartbeats, "screen", index)
            if values:
                phases[key] = summarize_series(values)

    result: dict[str, object] = {
        "path": str(path.resolve()),
        "profileKind": profile_kind,
        "profileSamples": len(profiles),
        "alignedProfileSamples": len(aligned_profiles),
        "legacyProfileSamples": len(legacy_profiles),
        "settledProfileSamples": len(settled_profiles),
        "settledSelection": settled_selection,
        "heartbeatSamples": len(heartbeats),
        "fpsProfileSamples": len(fps_profiles),
        "fpsProfileSamplesIncludingStartup": len(fps_profiles_all),
        "fpsSource": fps_source,
        "fps": summarize_series(fps),
        "frameTimeMsec": summarize_series(totals),
        "rawProfileFps": summarize_series(raw_profile_fps),
        "rawProfileFrameTimeMsec": summarize_series(raw_profile_totals),
        "phases": phases,
        "drawCyclePhases": cycle_phases,
        "frontendCyclePhases": frontend_cycle_phases,
        "workload": workload,
        "normalizedCosts": normalized_costs,
        "diagnosis": dominant_phase_diagnosis(phases, cycle_phases),
        "frameProgress": frame_progress(
            fps_profiles if fps_profiles else
            (profiles if any("frame" in record for record in profiles) else heartbeats)
        ),
        "memory": memory_summary(fps_profiles or heartbeats),
        "topology": topology_summary(fps_profiles),
        "textureStageCache": state_cache_summary(texture_stage_cache),
        "vertexShaderCache": state_cache_summary(vertex_shader_cache),
        "streamSourceCache": state_cache_summary(stream_source_cache),
        "mdrPaletteCache": state_cache_summary(mdr_palette_cache),
        "skinTextureSwap": {
            key: cumulative_summary(settled_profiles, key)
            for key in ("skinSwap", "skinFetch", "skinWait", "skinWriteKB", "skinReadKB")
        },
        "texturePools": {
            key: summarize_series(numeric_series(settled_profiles, key))
            for key in ("staticTexKB", "staticTexCapKB", "skinTexKB", "skinTexCapKB")
        },
        "lowFpsSamplesBelow12": sum(1 for value in fps if value < 12.0),
        "longFrameSamplesAtLeast200Msec": sum(1 for value in totals if value >= 200.0),
    }
    return result


def summary_value(result: dict[str, object], section: str, statistic: str) -> float | None:
    values = result.get(section)
    if not isinstance(values, dict):
        return None
    value = values.get(statistic)
    return float(value) if isinstance(value, (int, float)) else None


def percent_change(candidate: float | None, baseline: float | None, lower_is_better: bool) -> float | None:
    if candidate is None or baseline is None or baseline == 0.0:
        return None
    if lower_is_better:
        return (1.0 - candidate / baseline) * 100.0
    return (candidate / baseline - 1.0) * 100.0


def compare(candidate: dict[str, object], baseline: dict[str, object]) -> dict[str, object]:
    comparison: dict[str, object] = {
        "fpsMeanGainPercent": percent_change(
            summary_value(candidate, "fps", "mean"), summary_value(baseline, "fps", "mean"), False
        ),
        "fpsMedianGainPercent": percent_change(
            summary_value(candidate, "fps", "median"), summary_value(baseline, "fps", "median"), False
        ),
        "frameMedianReductionPercent": percent_change(
            summary_value(candidate, "frameTimeMsec", "median"),
            summary_value(baseline, "frameTimeMsec", "median"),
            True,
        ),
    }
    phase_changes: dict[str, float] = {}
    candidate_phases = candidate.get("phases", {})
    baseline_phases = baseline.get("phases", {})
    if isinstance(candidate_phases, dict) and isinstance(baseline_phases, dict):
        for key in sorted(set(candidate_phases) & set(baseline_phases)):
            candidate_value = candidate_phases[key].get("median")
            baseline_value = baseline_phases[key].get("median")
            change = percent_change(
                float(candidate_value) if isinstance(candidate_value, (int, float)) else None,
                float(baseline_value) if isinstance(baseline_value, (int, float)) else None,
                True,
            )
            if change is not None:
                phase_changes[key] = change
    comparison["phaseMedianReductionPercent"] = phase_changes
    return comparison


def format_number(value: object) -> str:
    if isinstance(value, float):
        return f"{value:.2f}"
    return str(value)


def print_summary(result: dict[str, object], title: str | None = None) -> None:
    if title:
        print(title)
    print(f"Log: {result['path']}")
    print(
        f"Samples: profile={result['profileSamples']} ({result['profileKind']}) "
        f"fpsProfile={result['fpsProfileSamples']} heartbeat={result['heartbeatSamples']} "
        f"fpsSource={result['fpsSource']}"
    )
    if result["profileSamples"]:
        print(
            f"Settled selection: {result['settledProfileSamples']}/{result['profileSamples']} "
            f"({result['settledSelection']})"
        )
    for label, key in (("FPS", "fps"), ("Frame ms", "frameTimeMsec")):
        summary = result[key]
        if summary["samples"]:
            print(
                f"{label}: min={format_number(summary['min'])} "
                f"mean={format_number(summary['mean'])} median={format_number(summary['median'])} "
                f"p95={format_number(summary['p95'])} max={format_number(summary['max'])}"
            )
        else:
            print(f"{label}: no samples")
    progress = result["frameProgress"]
    if progress["samples"]:
        print(
            f"Frames: {progress['first']} -> {progress['last']}; "
            f"advanced={progress['advanced']} nonadvancing={progress['nonadvancingIntervals']}"
        )
    memory = result["memory"]
    if memory["samples"]:
        print(
            f"Zone bytes: used {memory['usedFirst']} -> {memory['usedLast']} "
            f"(delta {memory['usedDelta']}), peak={memory['usedPeak']}, "
            f"min free={memory['freeMinimum']}, min largest={memory['largestFreeMinimum']}"
        )
    topology = result["topology"]
    if topology["samples"]:
        print(
            f"Topology: players={topology.get('players', 'unknown')} "
            f"humans={topology.get('humans', 'unknown')} bots={topology.get('bots', 'unknown')} "
            f"source={topology.get('source', 'unknown')} "
            f"virtual={topology.get('virtual', 'unknown')} "
            f"balanced={topology.get('accountingBalanced', 'unknown')}"
        )
    for label, key in (
        ("Texture-stage cache", "textureStageCache"),
        ("Vertex-shader/FVF cache", "vertexShaderCache"),
        ("Stream-source cache", "streamSourceCache"),
        ("MDR bone-palette cache", "mdrPaletteCache"),
    ):
        cache = result[key]
        if cache["samples"]:
            print(
                f"{label}: samples={cache['samples']} latestSample={cache.get('sample', 'unknown')} "
                f"players={cache.get('players', 'unknown')} requests={cache.get('requests', 'unknown')} "
                f"emitted={cache.get('emitted', 'unknown')} skipped={cache.get('skipped', 'unknown')} "
                f"skipPct={cache.get('skipPct', 'unknown')} "
                f"balanced={cache.get('accountingBalanced', 'unknown')}"
            )
    skin_texture_swap = result["skinTextureSwap"]
    populated_swap_counters = {
        key: value for key, value in skin_texture_swap.items() if value["samples"]
    }
    if populated_swap_counters:
        print(
            "Skin texture cumulative first->last (delta): "
            + ", ".join(
                f"{key}={format_number(value['first'])}->{format_number(value['last'])} "
                f"({format_number(value['delta']):s})"
                for key, value in populated_swap_counters.items()
            )
        )
    print(
        f"Flags: fps<12={result['lowFpsSamplesBelow12']} "
        f"frame>=200ms={result['longFrameSamplesAtLeast200Msec']}"
    )
    phases = result["phases"]
    if phases:
        print(
            "Phase mean/median: "
            + ", ".join(
                f"{key}={value['mean']:.2f}/{value['median']:.2f}" for key, value in phases.items()
            )
        )
    cycle_phases = result["drawCyclePhases"]
    if cycle_phases:
        print(
            "Draw cycles mean/median: "
            + ", ".join(
                f"{key}={value['mean']:.0f}/{value['median']:.0f}"
                for key, value in cycle_phases.items()
            )
        )
    frontend_cycle_phases = result["frontendCyclePhases"]
    if frontend_cycle_phases:
        print(
            "Frontend cycles mean/median: "
            + ", ".join(
                f"{key}={value['mean']:.0f}/{value['median']:.0f}"
                for key, value in frontend_cycle_phases.items()
            )
        )
    workload = result["workload"]
    if workload:
        print(
            "Workload mean/median: "
            + ", ".join(
                f"{key}={value['mean']:.1f}/{value['median']:.1f}"
                for key, value in workload.items()
            )
        )
    normalized_costs = result["normalizedCosts"]
    if normalized_costs:
        print(
            "Normalized cost mean/median: "
            + ", ".join(
                f"{key}={value['mean']:.2f}/{value['median']:.2f}"
                for key, value in normalized_costs.items()
            )
        )
    diagnosis = result["diagnosis"]
    if diagnosis.get("available"):
        detail = (
            f"Dominant measured boundary: {diagnosis['dominant']} "
            f"({diagnosis['medianMsec']:.2f} ms median)"
        )
        if "dominantChild" in diagnosis:
            detail += (
                f"; child={diagnosis['dominantChild']} "
                f"({diagnosis['childMedianMsec']:.2f} ms)"
            )
        if "dominantDrawCycle" in diagnosis:
            detail += (
                f"; drawCycle={diagnosis['dominantDrawCycle']} "
                f"({diagnosis['drawCycleMedian']:.0f} cycles)"
            )
        if "dominantSwapWait" in diagnosis:
            detail += (
                f"; swapWait={diagnosis['dominantSwapWait']} "
                f"({diagnosis['swapWaitMedianMsec']:.2f} ms)"
            )
        print(detail)
        print(f"Next evidence action: {diagnosis['recommendation']}")


def print_comparison(comparison: dict[str, object]) -> None:
    print("Comparison (positive is better):")
    for label, key in (
        ("FPS mean gain", "fpsMeanGainPercent"),
        ("FPS median gain", "fpsMedianGainPercent"),
        ("Frame median reduction", "frameMedianReductionPercent"),
    ):
        value = comparison.get(key)
        print(f"  {label}: {value:+.2f}%" if isinstance(value, (int, float)) else f"  {label}: unavailable")
    phases = comparison.get("phaseMedianReductionPercent", {})
    if phases:
        print("  Phase median reductions: " + ", ".join(f"{key}={value:+.2f}%" for key, value in phases.items()))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="Path to ef_sp_log.txt or ef_mp_log.txt")
    parser.add_argument("--baseline", type=Path, help="Older log to compare against the candidate")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON")
    args = parser.parse_args()
    if not args.log.is_file():
        parser.error(f"log does not exist: {args.log}")
    result = analyze(args.log)
    baseline = None
    comparison = None
    if args.baseline:
        if not args.baseline.is_file():
            parser.error(f"baseline log does not exist: {args.baseline}")
        baseline = analyze(args.baseline)
        comparison = compare(result, baseline)
    if args.json:
        output = {"candidate": result}
        if baseline is not None:
            output["baseline"] = baseline
            output["comparison"] = comparison
        print(json.dumps(output, indent=2, sort_keys=True))
    else:
        if baseline is not None:
            print_summary(baseline, "BASELINE")
            print()
        print_summary(result, "CANDIDATE" if baseline is not None else None)
        if comparison is not None:
            print()
            print_comparison(comparison)
    return 0 if (
        result["profileSamples"] or result["fpsProfileSamples"] or result["heartbeatSamples"]
    ) else 2


if __name__ == "__main__":
    raise SystemExit(main())
