#!/usr/bin/env python3
"""Render and validate the Elite Force LCARS loading screen.

This is an offline parity aid. It renders the small EF loading LCARS widget from
runtime assets and validates the active pre-cgame draw path against the original
Elite Force cgame load-bar geometry, colors, text, and pulse order.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from PIL import Image

SCREEN_W = 640
SCREEN_H = 480
BASE_X = 10
BASE_Y = 244
TEXT_RIGHT = 0x0001
TEXT_SCALE = 0.65
FONT_HEIGHT = 27
FONT_GAP = 1
FONT_SPACE = 4

STAGED_PIECES = (
    (18, 102, 128, 64, "piece[0]", "smpiece1.tga", 1, "CT_VDKPURPLE3", "CT_VLTPURPLE3"),
    (0, 37, 64, 64, "piece[1]", "smpiece2.tga", 2, "CT_VDKBLUE1", "CT_VLTBLUE1"),
    (17, 0, 128, 64, "piece[2]", "smpiece3.tga", 3, "CT_VDKPURPLE1", "CT_LTPURPLE1"),
    (99, 0, 128, 128, "piece[3]", "smpiece4.tga", 4, "CT_VDKPURPLE2", "CT_LTPURPLE2"),
    (137, 81, 64, 64, "piece[4]", "smpiece5.tga", 5, "CT_VDKBLUE2", "CT_VLTBLUE2"),
    (45, 99, 128, 64, "piece[5]", "smpiece6.tga", 6, "CT_VDKORANGE", "CT_LTORANGE"),
    (38, 24, 64, 128, "piece[6]", "smpiece7.tga", 7, "CT_VDKBLUE2", "CT_LTBLUE2"),
    (78, 20, 128, 64, "piece[7]", "smpiece8.tga", 8, "CT_VDKPURPLE1", "CT_LTPURPLE1"),
    (112, 66, 64, 128, "piece[8]", "smpiece9.tga", 9, "CT_VDKBROWN1", "CT_VLTBROWN1"),
    (62, 44, 128, 128, "circle", "arrowpiece.tga", 9, "CT_DKBLUE2", "CT_LTBLUE2"),
)

QUARTERS = (
    (61, 43, 32, 32),
    (135, 43, -32, 32),
    (135, 117, -32, -32),
    (61, 117, 32, -32),
)

LOAD_TEXT = (
    (21, 150, "0987", 0),
    (3, 90, "18", 0),
    (24, 20, "7", 0),
    (93, 5, "51", TEXT_RIGHT),
    (103, 5, "35", 0),
    (165, 83, "21", 0),
    (101, 149, "67", 0),
    (123, 36, "8", 0),
    (90, 65, "1", TEXT_RIGHT),
    (105, 65, "2", 0),
    (105, 87, "3", 0),
    (91, 87, "4", TEXT_RIGHT),
)

COLOR_RE = re.compile(r"^\s*\{([^{}]+)\}\s*,\s*//\s*(CT_[A-Z0-9_]+)", re.MULTILINE)
FONT_TRIPLE_RE = re.compile(r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*\}")


def squash(text: str) -> str:
    return re.sub(r"\s+", "", text)


def parse_float_token(token: str) -> float:
    token = token.strip().replace("f", "")
    if token.startswith("."):
        token = "0" + token
    if token.startswith("-."):
        token = token.replace("-.", "-0.", 1)
    return float(token)


def parse_color_table(path: Path) -> dict[str, tuple[int, int, int, int]]:
    text = path.read_text(errors="ignore")
    colors: dict[str, tuple[int, int, int, int]] = {}
    for values, name in COLOR_RE.findall(text):
        parts = [parse_float_token(part) for part in values.split(",")[:4]]
        rgba = tuple(max(0, min(255, int(round(value * 255)))) for value in parts)
        colors[name] = rgba  # type: ignore[assignment]
    required = {
        "CT_BLACK", "CT_VDKPURPLE3", "CT_VLTPURPLE3", "CT_VDKBLUE1", "CT_VLTBLUE1",
        "CT_VDKPURPLE1", "CT_LTPURPLE1", "CT_VDKPURPLE2", "CT_LTPURPLE2",
        "CT_VDKBLUE2", "CT_VLTBLUE2", "CT_VDKORANGE", "CT_LTORANGE", "CT_LTBLUE2",
        "CT_VDKBROWN1", "CT_VLTBROWN1", "CT_DKBLUE2", "CT_DKPURPLE2",
    }
    missing = sorted(required.difference(colors))
    if missing:
        raise ValueError(f"missing colors in {path}: {', '.join(missing)}")
    return colors


def load_font_map(path: Path) -> list[tuple[int, int, int]]:
    triples = [(int(a), int(b), int(c)) for a, b, c in FONT_TRIPLE_RE.findall(path.read_text(errors="ignore"))]
    if len(triples) < 256:
        raise ValueError(f"expected font map triples in {path}, got {len(triples)}")
    return triples[:256]


def tint_image(img: Image.Image, color: tuple[int, int, int, int]) -> Image.Image:
    rgba = img.convert("RGBA")
    r, g, b, a = rgba.split()
    cr, cg, cb, ca = color
    r = r.point(lambda value: value * cr // 255)
    g = g.point(lambda value: value * cg // 255)
    b = b.point(lambda value: value * cb // 255)
    if ca != 255:
        a = a.point(lambda value: value * ca // 255)
    return Image.merge("RGBA", (r, g, b, a))


class LoadScreenRenderer:
    def __init__(self, repo_root: Path, base_dir: Path):
        self.repo_root = repo_root
        self.base_dir = base_dir
        self.colors = parse_color_table(repo_root / "code" / "game" / "q_math.cpp")
        self.font_map = load_font_map(base_dir / "ext_data" / "fonts.dat")
        self.font = Image.open(base_dir / "gfx" / "2d" / "chars_tiny.tga").convert("RGBA")
        loading = base_dir / "menu" / "loading"
        self.assets = {
            asset_name: Image.open(loading / file_name).convert("RGBA")
            for *_prefix, asset_name, file_name, _threshold, _dark, _lit in STAGED_PIECES
        }
        self.assets["quarter"] = Image.open(loading / "quarter.tga").convert("RGBA")

    def draw_asset(
        self,
        canvas: Image.Image,
        asset_name: str,
        x: int,
        y: int,
        w: int,
        h: int,
        color_name: str,
    ) -> None:
        img = self.assets[asset_name]
        if w < 0:
            x += w
            img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            w = -w
        if h < 0:
            y += h
            img = img.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
            h = -h
        if img.size != (w, h):
            img = img.resize((w, h), Image.Resampling.BILINEAR)
        tinted = tint_image(img, self.colors[color_name])
        canvas.alpha_composite(tinted, (x, y))

    def text_width(self, text: str) -> float:
        width = 0.0
        for ch in text:
            code = ord(ch) & 0xFF
            sw = FONT_SPACE if ch == " " else self.font_map[code][2]
            if sw != -1:
                width += (sw + FONT_GAP) * TEXT_SCALE
        return max(0.0, width - FONT_GAP * TEXT_SCALE)

    def draw_text(self, canvas: Image.Image, x: int, y: int, text: str, style: int) -> None:
        draw_x = float(x)
        if style & TEXT_RIGHT:
            draw_x -= self.text_width(text)
        for ch in text:
            code = ord(ch) & 0xFF
            if ch == " ":
                draw_x += (FONT_SPACE + FONT_GAP) * TEXT_SCALE
                continue
            sx, sy, sw = self.font_map[code]
            if sw == -1:
                continue
            glyph = self.font.crop((sx, sy, sx + sw, sy + FONT_HEIGHT))
            w = max(1, int(round(sw * TEXT_SCALE)))
            h = max(1, int(round(FONT_HEIGHT * TEXT_SCALE)))
            glyph = glyph.resize((w, h), Image.Resampling.BILINEAR)
            glyph = tint_image(glyph, self.colors["CT_BLACK"])
            canvas.alpha_composite(glyph, (int(round(draw_x)), y))
            draw_x += (sw + FONT_GAP) * TEXT_SCALE

    def render(self, stage: int, pulse_before: int) -> tuple[Image.Image, int]:
        canvas = Image.new("RGBA", (SCREEN_W, SCREEN_H), self.colors["CT_BLACK"])
        for dx, dy, w, h, asset_name, _file_name, threshold, dark, lit in STAGED_PIECES:
            color = dark if stage < threshold else lit
            self.draw_asset(canvas, asset_name, BASE_X + dx, BASE_Y + dy, w, h, color)

        for dx, dy, w, h in QUARTERS:
            self.draw_asset(canvas, "quarter", BASE_X + dx, BASE_Y + dy, w, h, "CT_DKPURPLE2")

        pulse = pulse_before + 1
        if pulse > 3:
            pulse = 0
        qx, qy, qw, qh = QUARTERS[pulse]
        self.draw_asset(canvas, "quarter", BASE_X + qx, BASE_Y + qy, qw, qh, "CT_LTPURPLE2")

        for dx, dy, text, style in LOAD_TEXT:
            self.draw_text(canvas, BASE_X + dx, BASE_Y + dy, text, style)
        return canvas, pulse


def validate_source(repo_root: Path, base_dir: Path) -> list[str]:
    ui = (repo_root / "code" / "ui" / "ui_splash.cpp").read_text(errors="ignore")
    cg = (repo_root / "code" / "cgame" / "cg_info.cpp").read_text(errors="ignore")
    orig = (repo_root / "SP-Mod-Source-Code-master" / "cgame" / "cg_info.cpp").read_text(errors="ignore")
    ui_sq = squash(ui)
    cg_sq = squash(cg)
    orig_sq = squash(orig)
    errors: list[str] = []

    for dx, dy, w, h, asset_name, _file_name, threshold, dark, lit in STAGED_PIECES:
        x_expr = "x" if dx == 0 else f"x+{dx}"
        y_expr = "y" if dy == 0 else f"y+{dy}"
        expected = squash(
            f"SP_DrawEFLoadingPicStage({x_expr}, {y_expr}, {w}, {h}, "
            f"s_spEfLoadingAssets.{asset_name}, stage, {threshold}, {dark}, {lit});"
        )
        if expected not in ui_sq:
            errors.append(f"active ui_splash missing staged draw {asset_name} threshold {threshold}")

    style_names = {0: "0", TEXT_RIGHT: "SP_EF_LOAD_TEXT_RIGHT"}
    for dx, dy, text, style in LOAD_TEXT:
        expected = squash(f'SP_DrawEFLoadingText(x+{dx}, y+{dy}, "{text}", {style_names[style]});')
        if expected not in ui_sq:
            errors.append(f"active ui_splash missing load text {text}")

    pulse_inc = ui_sq.find("s_spEfLoadingPulse++;")
    pulse_switch = ui_sq.find("switch(s_spEfLoadingPulse)")
    if pulse_inc < 0 or pulse_switch < 0 or pulse_inc > pulse_switch:
        errors.append("active ui_splash pulse order does not match EF loadLCARScnt order")

    for token in ("cg.loadLCARScnt++;", "switch(cg.loadLCARScnt)"):
        if squash(token) not in cg_sq:
            errors.append(f"active cgame load bar missing {token}")
        if squash(token) not in orig_sq:
            errors.append(f"original EF load bar missing {token}")

    forbidden = ("CIN_PlayAllFrames(\"logos\"", "CIN_PlayAllFrames(\"jedi\"", "LicenseScreen")
    for token in forbidden:
        if squash(token) in ui_sq:
            errors.append(f"legacy JA splash token still present: {token}")

    loading = base_dir / "menu" / "loading"
    for *_prefix, file_name, _threshold, _dark, _lit in STAGED_PIECES:
        if not (loading / file_name).is_file():
            errors.append(f"missing loading asset {file_name}")
    if not (loading / "quarter.tga").is_file():
        errors.append("missing loading asset quarter.tga")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--base-dir", type=Path, default=Path("build/release/BaseEF"))
    parser.add_argument("--output", type=Path, default=Path("scripts/output/ef_loadscreen_stage0_pulse1.png"))
    parser.add_argument("--json", type=Path, default=Path("scripts/output/ef_loadscreen_stage0_pulse1.json"))
    parser.add_argument("--stage", type=int, default=0)
    parser.add_argument("--pulse-before", type=int, default=0)
    parser.add_argument("--skip-source-check", action="store_true")
    args = parser.parse_args()

    repo_root = args.repo_root.resolve()
    base_dir = (repo_root / args.base_dir).resolve() if not args.base_dir.is_absolute() else args.base_dir
    output = (repo_root / args.output).resolve() if not args.output.is_absolute() else args.output
    json_path = (repo_root / args.json).resolve() if not args.json.is_absolute() else args.json

    source_errors: list[str] = [] if args.skip_source_check else validate_source(repo_root, base_dir)
    if source_errors:
        print("FAIL EF loadscreen source parity")
        for error in source_errors:
            print(error)
        return 1

    output.parent.mkdir(parents=True, exist_ok=True)
    json_path.parent.mkdir(parents=True, exist_ok=True)

    renderer = LoadScreenRenderer(repo_root, base_dir)
    image, pulse_after = renderer.render(args.stage, args.pulse_before)
    image.save(output)

    report = {
        "output": str(output),
        "baseDir": str(base_dir),
        "stage": args.stage,
        "pulseBefore": args.pulse_before,
        "pulseAfter": pulse_after,
        "size": [SCREEN_W, SCREEN_H],
        "stageDraws": len(STAGED_PIECES),
        "texts": len(LOAD_TEXT),
        "sourceParity": "skipped" if args.skip_source_check else "pass",
    }
    json_path.write_text(json.dumps(report, indent=2) + "\n")

    print("PASS EF loadscreen render/source parity")
    print(f"wrote {output} {SCREEN_W}x{SCREEN_H}")
    print(f"stage={args.stage} pulseBefore={args.pulse_before} pulseAfter={pulse_after}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
