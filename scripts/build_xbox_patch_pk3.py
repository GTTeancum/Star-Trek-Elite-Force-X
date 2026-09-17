#!/usr/bin/env python3
"""Build the Elite Force Xbox patch PK3.

The pack intentionally uses normal PK3/ZIP storage. Runtime code can choose
Xbox-specific alternates such as maps/xbox/<map>.bsp and <texture>.dds, but
those overrides are opt-in so unproven conversions cannot perturb gameplay.
"""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import warnings
import zipfile
from pathlib import Path

warnings.filterwarnings(
    "ignore",
    category=DeprecationWarning,
    message=r"Image\.Image\.getdata is deprecated.*",
)

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - this is an operator error path
    raise SystemExit("Pillow is required to build XBOX0.PK3") from exc


EF_BSP_IDENT = b"IBSP"
EF_BSP_VERSION = 46
EF_LUMP_SHADERS = 1
EF_LUMP_LIGHTMAPS = 14
EF_HEADER_LUMPS = 17
EF_SHADER_ENTRY_SIZE = 72
AAS_IDENT = b"EAAS"
AAS_HEADER_SIZE = 12 + 14 * 8
MAX_QPATH = 64
LIGHTMAP_SIZE = 128
LIGHTMAP_RGB_BYTES = LIGHTMAP_SIZE * LIGHTMAP_SIZE * 3
LIGHTMAP_RGB565_DDS_BYTES = 128 + LIGHTMAP_SIZE * LIGHTMAP_SIZE * 2
DEFAULT_LIGHTMAP_BOOST = 2.5
MULTIPLAYER_MAP_PREFIXES = ("ctf_", "hm_", "dm_", "team_")
PACKED_BSP_LUMP_NAMES = (
    "brushes",
    "brushsides",
    "entities",
    "faces",
    "flares",
    "indexes",
    "leafbrushes",
    "leafs",
    "leafsurfaces",
    "lightarray",
    "lightgrid",
    "misc",
    "models",
    "nodes",
    "patches",
    "planes",
    "shaders",
    "trisurfs",
    "verts",
    "visibility",
)

IMAGE_EXTS = (".jpg", ".jpeg", ".tga", ".png")
SKYBOX_SUFFIXES = ("rt", "lf", "bk", "ft", "up", "dn")
DIRECTORY_TEXTURE_SEEDS = (
    "textures/borg",
    "textures/detail",
)
PLAYER_TEXTURE_SEEDS = (
    "models/players/avatar",
    "models/players/biessman",
    "models/players/borgbig",
    "models/players/borgbig2",
    "models/players/borgbig3",
    "models/players/borgbig4",
    "models/players/borgthin",
    "models/players/borgthin2",
    "models/players/borgthin3",
    "models/players/borgthin4",
    "models/players/crewthin",
    "models/players/hazard",
    "models/players/hazardfemale",
    "models/players/munro",
    "models/players/tuvok",
    "models/players/tuvok_h",
    "models/players2",
)
HUD_TEXTURE_SEEDS = (
    "gfx/2d",
    "gfx/hud",
    "gfx/interface",
)
HIGH_FIDELITY_TEXTURES = (
    # Borg wall panels use thin pipe/detail silhouettes against black backing.
    # A 128px cap makes these read as large black slabs, so keep their source
    # resolution while still packaging them as Xbox DDS overrides.
    "textures/borg/xpanelb",
    # The distribution-node model reuses this alpha atlas for its animated
    # disnode2 shader layer.  The 128px cap collapses its small panels into the
    # atlas's intentionally black regions, leaving a conspicuous missing slab.
    "models/mapobjects/borg/disnode",
    # dn1 uses textures/common/junk_sky, whose skyParms reference env/junk.
    # These faces are very dark/detail-heavy; the normal 128px cap loses the
    # junk/star signal and makes the openings look like flat black voids.
    "env/junk_rt",
    "env/junk_lf",
    "env/junk_bk",
    "env/junk_ft",
    "env/junk_up",
    "env/junk_dn",
)
RGB565_TEXTURES = (
    # DXT1 block compression crushes the thin dark/bright traces in these
    # structural wall panels. RGB565 is still an Xbox-native DDS path and keeps
    # the source signal intact enough for the lightmap multiply pass.
    "textures/borg/xpanelb",
    "env/junk_rt",
    "env/junk_lf",
    "env/junk_bk",
    "env/junk_ft",
    "env/junk_up",
    "env/junk_dn",
)
NO_MIPMAP_TEXTURES = (
    # The scavenger recharge-station shader explicitly blends these source
    # alpha channels.  RGB565 turns their transparent atlas into a rectangle,
    # while generated alpha mips erase the thin station features at distance.
    "models/mapobjects/scavenger/power_up",
    "models/mapobjects/scavenger/power_up_copy",
    # disnode2 blends the same alpha atlas over the base model.  Generated
    # alpha mips make pieces disappear until the player is close enough for
    # mip zero, matching the reported distance-dependent black panel.
    "models/mapobjects/borg/disnode",
)
ALWAYS_TEXTURES = (
    # These atlases are registered directly by the shared SP frontend/pause
    # code, so shader-script discovery cannot find them. They require alpha
    # fidelity and are therefore emitted through the retail-style DXT5 path.
    "gfx/2d/chars_big",
    "gfx/2d/chars_medium",
    "gfx/2d/chars_tiny",
    # The cgame indexes this atlas with authored 256x256 coordinates. Applying
    # the ordinary HUD cap changes every glyph rectangle.
    "gfx/2d/charsgrid_med",
)
FULLSCREEN_TEXTURE_SEEDS = (
    "textures/common/70yearjourney",
    "textures/common/enemyspace",
    "textures/common/sevenspace",
    "textures/common/tuvokhazard",
)
HOLOMATCH_LOADSCREEN_OVERRIDES = (
    # Loading-screen assets are already shipped by the SP Xbox package.
    # Rebuilding them into xbox1.pk3 shadows the proven SP copies and has
    # produced corrupted loading-wheel draws in Holomatch.
    "menu/common/corner_lr_8_16.dds",
)
HOLOMATCH_SHARED_SP_DDS = (
    # The SP interface HUD owns Holomatch HUD drawing, so xbox1.pk3 carries the
    # same proven DDS assets from xbox0.pk3. This keeps Holomatch DDS-only even
    # after loose original image fallbacks are removed from the staged tree.
    "gfx/interface/ammobar.dds",
    "gfx/interface/ammolowercap1.dds",
    "gfx/interface/ammolowercap2.dds",
    "gfx/interface/ammouppercap1.dds",
    "gfx/interface/ammouppercap2.dds",
    "gfx/interface/armorcap1.dds",
    "gfx/interface/armorcap2.dds",
    "gfx/interface/healthcap1.dds",
    "gfx/interface/healthcap2.dds",
)
SEEDED_SHARED_SP_DDS = HOLOMATCH_SHARED_SP_DDS + HOLOMATCH_LOADSCREEN_OVERRIDES
SEEDED_UI_DDS_PREFIXES = (
    "gfx/",
    "icons/",
    "menu/",
    "sprites/",
)
ORIGINAL_FORMAT_TEXTURES = (
    # Dark/detail-heavy sky backing loses too much signal in the current DXT1
    # conversion, so keep the stock JPG until the Xbox-native path is proven.
    "textures/borg/borgsky",
    # These Borg materials rely on the stock EF shader scripts and authored
    # alpha/additive masks. DDS overrides have produced black panel/field
    # artifacts in CXBX-R/XEMU, so keep the original assets as the authority.
    "textures/borg/bars",
    "textures/borg/bars2",
    "textures/borg/basic1",
    "textures/borg/bigborg",
    "textures/borg/forceborder",
    "textures/borg/forceborder2",
    "textures/borg/forceborder3",
    "textures/borg/green1",
    "textures/borg/green1_dos",
    "textures/borg/oddlight1",
    "textures/borg/oddlight1dam",
    "textures/borg/oddlight1mult",
    "textures/borg/static",
    "textures/borg/static2",
    "textures/borg/static_yellow",
    "textures/common/70yearjourney",
    "textures/common/enemyspace",
    "textures/common/sevenspace",
    "textures/common/tuvokhazard",
)

XBOX_PATCH_SHADER_PATH = "scripts/borg.shader"
XBOX_STOCK_SHADER_PATHS = (
    XBOX_PATCH_SHADER_PATH,
    "scripts/voyager.shader",
)
XBOX_PREMULTIPLIED_ALPHA_MAPS = (
    "textures/borg/bigborg.tga",
    "textures/borg/oddlight1.tga",
)

REFERENCE_RE = re.compile(r"\b(qer_editorimage|map|clampmap|animmap|skyparms)\s+(.+)", re.IGNORECASE)
PAK_NAME_RE = re.compile(r"^pak(\d+)\.pk3$", re.IGNORECASE)
FIXED_ZIP_TIME = (2026, 1, 1, 0, 0, 0)
XBOX_EFFECTS_IMAGE_REL = "sound/dsstdfx.bin"
XBOX_EFFECTS_IMAGE_CANDIDATES = (
    Path(r"C:\XDK\source\dsound\dsp\dsstdfx.bin"),
    Path(r"C:\XDK\Samples\Xbox\Sound\BackgroundMusic\Media\dsstdfx.bin"),
)
HOLOMATCH_BOTFILE_ALIASES = {
    "botfiles/bots/long_i.c": "botfiles/bots/biessman_i.c",
}


def normalized_rel(path: str) -> str:
    return path.replace("\\", "/").strip().lower()


def _rol32(value: int, bits: int) -> int:
    value &= 0xFFFFFFFF
    return ((value << bits) | (value >> (32 - bits))) & 0xFFFFFFFF


def md4_digest(data: bytes) -> bytes:
    mask = 0xFFFFFFFF
    a = 0x67452301
    b = 0xEFCDAB89
    c = 0x98BADCFE
    d = 0x10325476
    bit_length = (len(data) * 8) & 0xFFFFFFFFFFFFFFFF
    padded = data + b"\x80"
    padded += b"\x00" * ((56 - len(padded) % 64) % 64)
    padded += struct.pack("<Q", bit_length)

    def f(x: int, y: int, z: int) -> int:
        return ((x & y) | (~x & z)) & mask

    def g(x: int, y: int, z: int) -> int:
        return ((x & y) | (x & z) | (y & z)) & mask

    def h(x: int, y: int, z: int) -> int:
        return (x ^ y ^ z) & mask

    for offset in range(0, len(padded), 64):
        x = list(struct.unpack_from("<16I", padded, offset))
        aa, bb, cc, dd = a, b, c, d

        shifts = (3, 7, 11, 19)
        order = range(16)
        for i, k in enumerate(order):
            s = shifts[i & 3]
            if i & 3 == 0:
                a = _rol32((a + f(b, c, d) + x[k]) & mask, s)
            elif i & 3 == 1:
                d = _rol32((d + f(a, b, c) + x[k]) & mask, s)
            elif i & 3 == 2:
                c = _rol32((c + f(d, a, b) + x[k]) & mask, s)
            else:
                b = _rol32((b + f(c, d, a) + x[k]) & mask, s)

        shifts = (3, 5, 9, 13)
        order = (0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15)
        for i, k in enumerate(order):
            s = shifts[i & 3]
            if i & 3 == 0:
                a = _rol32((a + g(b, c, d) + x[k] + 0x5A827999) & mask, s)
            elif i & 3 == 1:
                d = _rol32((d + g(a, b, c) + x[k] + 0x5A827999) & mask, s)
            elif i & 3 == 2:
                c = _rol32((c + g(d, a, b) + x[k] + 0x5A827999) & mask, s)
            else:
                b = _rol32((b + g(c, d, a) + x[k] + 0x5A827999) & mask, s)

        shifts = (3, 9, 11, 15)
        order = (0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15)
        for i, k in enumerate(order):
            s = shifts[i & 3]
            if i & 3 == 0:
                a = _rol32((a + h(b, c, d) + x[k] + 0x6ED9EBA1) & mask, s)
            elif i & 3 == 1:
                d = _rol32((d + h(a, b, c) + x[k] + 0x6ED9EBA1) & mask, s)
            elif i & 3 == 2:
                c = _rol32((c + h(d, a, b) + x[k] + 0x6ED9EBA1) & mask, s)
            else:
                b = _rol32((b + h(c, d, a) + x[k] + 0x6ED9EBA1) & mask, s)

        a = (a + aa) & mask
        b = (b + bb) & mask
        c = (c + cc) & mask
        d = (d + dd) & mask

    return struct.pack("<4I", a, b, c, d)


def com_block_checksum(data: bytes) -> int:
    digest = struct.unpack("<4I", md4_digest(data))
    return (digest[0] ^ digest[1] ^ digest[2] ^ digest[3]) & 0xFFFFFFFF


def patch_aas_checksum(data: bytes, checksum: int) -> bytes:
    if len(data) < AAS_HEADER_SIZE:
        raise ValueError("AAS file is too small to patch")
    ident, version = struct.unpack_from("<4sI", data, 0)
    if ident != AAS_IDENT or version not in (4, 5):
        raise ValueError(f"not an Elite Force AAS file ident={ident!r} version={version}")
    output = bytearray(data)
    checksum_bytes = bytearray(struct.pack("<I", checksum & 0xFFFFFFFF))
    if version == 5:
        for index in range(len(checksum_bytes)):
            checksum_bytes[index] ^= (index * 119) & 0xFF
    output[8:12] = checksum_bytes
    return bytes(output)


def strip_line_comment(line: str) -> str:
    return line.split("//", 1)[0].strip()


def read_bsp_shader_names(bsp_path: Path) -> list[str]:
    data = bsp_path.read_bytes()
    if len(data) < 8 + EF_HEADER_LUMPS * 8:
        raise ValueError(f"{bsp_path} is too small to be an EF BSP")

    ident, version = struct.unpack_from("<4sI", data, 0)
    if ident != EF_BSP_IDENT or version != EF_BSP_VERSION:
        raise ValueError(f"{bsp_path} is not an Elite Force IBSP v46 map")

    lump_ofs = 8 + EF_LUMP_SHADERS * 8
    shader_ofs, shader_len = struct.unpack_from("<II", data, lump_ofs)
    if shader_ofs < 0 or shader_len < 0 or shader_ofs + shader_len > len(data):
        raise ValueError(f"{bsp_path} has an invalid shader lump")
    if shader_len % EF_SHADER_ENTRY_SIZE:
        raise ValueError(f"{bsp_path} shader lump is not dshader_t aligned")

    names: set[str] = set()
    for offset in range(shader_ofs, shader_ofs + shader_len, EF_SHADER_ENTRY_SIZE):
        raw = data[offset : offset + MAX_QPATH].split(b"\0", 1)[0]
        name = normalized_rel(raw.decode("latin1"))
        if name and name != "noshader":
            names.add(name)
    return sorted(names)


def parse_shader_references(base_dir: Path, shader_names: set[str]) -> set[str]:
    refs: set[str] = set()
    scripts_dir = base_dir / "scripts"
    if not scripts_dir.exists():
        return refs

    for shader_file in sorted(scripts_dir.glob("*.shader")):
        pending_header: str | None = None
        current_header: str | None = None
        active = False
        depth = 0

        for raw_line in shader_file.read_text(errors="ignore").splitlines():
            line = strip_line_comment(raw_line)
            if not line:
                continue

            if depth == 0:
                if "{" not in line:
                    pending_header = normalized_rel(line.split()[0])
                    continue

                before_brace = line.split("{", 1)[0].strip()
                current_header = normalized_rel(before_brace.split()[0]) if before_brace else pending_header
                active = bool(current_header and current_header in shader_names)
                pending_header = None

            if active:
                refs.update(extract_texture_references(line))

            depth += line.count("{")
            depth -= line.count("}")
            if depth <= 0:
                depth = 0
                current_header = None
                active = False

    return refs


def shader_archive_sort_key(path: Path) -> tuple[int, str]:
    match = PAK_NAME_RE.match(path.name)
    return (int(match.group(1)) if match else -1, path.name.lower())


def shader_source_texts(base_dir: Path, archive_dir: Path | None) -> dict[str, str]:
    texts: dict[str, str] = {}
    if archive_dir and archive_dir.is_dir():
        archives = sorted(
            (
                path
                for path in archive_dir.iterdir()
                if path.is_file() and PAK_NAME_RE.match(path.name)
            ),
            key=shader_archive_sort_key,
        )
        for archive in archives:
            with zipfile.ZipFile(archive, "r") as source:
                for entry in source.infolist():
                    rel = normalized_rel(entry.filename)
                    if entry.is_dir() or not rel.endswith(".shader"):
                        continue
                    texts[rel] = source.read(entry).decode("latin1", "ignore")

    for shader_path in shader_script_files(base_dir):
        rel = normalized_rel(shader_path.relative_to(base_dir).as_posix())
        texts[rel] = shader_path.read_text(errors="ignore")
    return texts


def xbox_stock_shader_bytes(base_dir: Path, archive_dir: Path | None) -> dict[str, bytes]:
    sources = shader_source_texts(base_dir, archive_dir)
    missing = [path for path in XBOX_STOCK_SHADER_PATHS if path not in sources]
    if missing:
        raise FileNotFoundError(
            f"missing stock shader scripts {', '.join(missing)}; "
            "provide --shader-archive-dir"
        )

    patched = sources[XBOX_PATCH_SHADER_PATH]
    for texture_map in XBOX_PREMULTIPLIED_ALPHA_MAPS:
        pattern = re.compile(
            r"(?im)(^\s*map\s+" + re.escape(texture_map) +
            r"\s*\r?\n\s*blendFunc\s+)GL_ONE\s+GL_SRC_ALPHA(\s*(?://.*)?$)"
        )
        patched, count = pattern.subn(r"\1GL_ONE GL_ONE_MINUS_SRC_ALPHA\2", patched)
        if count == 0:
            corrected = re.compile(
                r"(?im)^\s*map\s+" + re.escape(texture_map) +
                r"\s*\r?\n\s*blendFunc\s+GL_ONE\s+GL_ONE_MINUS_SRC_ALPHA\s*(?://.*)?$"
            )
            count = len(corrected.findall(patched))
        if count != 1:
            raise ValueError(
                f"expected one blend correction for {texture_map} in "
                f"{XBOX_PATCH_SHADER_PATH}, found {count}"
            )
    packaged = {
        path: sources[path].encode("latin1")
        for path in XBOX_STOCK_SHADER_PATHS
    }
    packaged[XBOX_PATCH_SHADER_PATH] = patched.encode("latin1")
    return packaged


def extract_all_texture_references(line: str) -> set[str]:
    match = REFERENCE_RE.search(line)
    if not match:
        return set()

    keyword = match.group(1).lower()
    tokens = [token.strip('"') for token in match.group(2).split()]
    if keyword == "skyparms":
        tokens = [f"{tokens[0]}_{suffix}" for suffix in SKYBOX_SUFFIXES] if tokens else []
    elif keyword == "animmap":
        tokens = tokens[1:]
    else:
        tokens = tokens[:1]

    refs: set[str] = set()
    for token in tokens:
        token = normalized_rel(token)
        if not token or token.startswith("$") or token == "-" or "/" not in token:
            continue
        path = Path(*token.split("/"))
        if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
            token = normalized_rel(path.with_suffix("").as_posix())
        refs.add(token)
    return refs


def no_mipmap_texture_references(base_dir: Path, archive_dir: Path | None) -> set[str]:
    refs: set[str] = set()
    for text in shader_source_texts(base_dir, archive_dir).values():
        pending_header: str | None = None
        current_header: str | None = None
        block_lines: list[str] = []
        depth = 0

        for raw_line in text.splitlines():
            line = strip_line_comment(raw_line)
            if not line:
                continue

            if depth == 0:
                if "{" not in line:
                    pending_header = normalized_rel(line.split()[0])
                    continue
                before_brace = line.split("{", 1)[0].strip()
                current_header = normalized_rel(before_brace.split()[0]) if before_brace else pending_header
                pending_header = None
                block_lines = []

            if current_header:
                block_lines.append(line)

            depth += line.count("{")
            depth -= line.count("}")
            if current_header and depth <= 0:
                if any(re.search(r"\bnomipmaps\b", block_line, re.IGNORECASE) for block_line in block_lines):
                    refs.add(current_header)
                    for block_line in block_lines:
                        refs.update(extract_all_texture_references(block_line))
                current_header = None
                block_lines = []
                depth = 0
    return refs


def extract_texture_references(line: str) -> set[str]:
    match = REFERENCE_RE.search(line)
    if not match:
        return set()

    keyword = match.group(1).lower()
    tokens = [token.strip('"') for token in match.group(2).split()]
    if keyword == "skyparms":
        tokens = [f"{tokens[0]}_{suffix}" for suffix in SKYBOX_SUFFIXES] if tokens else []
    elif keyword == "animmap":
        tokens = tokens[1:]
    else:
        tokens = tokens[:1]

    refs: set[str] = set()
    for token in tokens:
        token = normalized_rel(token)
        if not (token.startswith("textures/") or token.startswith("env/")):
            continue
        if token.startswith("textures/common/") and not is_always_texture(token):
            continue
        refs.add(token)
    return refs


def is_always_texture(candidate: str) -> bool:
    candidate = normalized_rel(candidate)
    path = Path(*candidate.split("/"))
    if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
        candidate = normalized_rel(path.with_suffix("").as_posix())
    return candidate in ALWAYS_TEXTURES


def is_fullscreen_texture(candidate: str) -> bool:
    candidate = normalized_rel(candidate)
    path = Path(*candidate.split("/"))
    if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
        candidate = normalized_rel(path.with_suffix("").as_posix())
    return candidate in FULLSCREEN_TEXTURE_SEEDS


def should_preserve_original_texture(candidate: str) -> bool:
    candidate = normalized_rel(candidate)
    path = Path(*candidate.split("/"))
    if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
        candidate = normalized_rel(path.with_suffix("").as_posix())
    return candidate in ORIGINAL_FORMAT_TEXTURES


def should_use_rgb565_texture(candidate: str) -> bool:
    candidate = normalized_rel(candidate)
    path = Path(*candidate.split("/"))
    if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
        candidate = normalized_rel(path.with_suffix("").as_posix())
    return candidate in RGB565_TEXTURES


def should_force_bgra32_texture(candidate: str) -> bool:
    # Retail JA stores its alpha-bearing gfx assets as DXT5. Its DDS loader
    # copies 32-bit payloads directly into swizzled texture memory, which only
    # works for the single pre-swizzled BGRA32 asset in the retail data set.
    # Our source images are conventional linear pixels, so never select that
    # private asset-pipeline contract here.
    return False


def should_generate_mipmaps(candidate: str, no_mipmap_refs: set[str]) -> bool:
    candidate = normalized_rel(candidate)
    path = Path(*candidate.split("/"))
    if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
        candidate = normalized_rel(path.with_suffix("").as_posix())

    if candidate in no_mipmap_refs:
        return False
    if candidate in NO_MIPMAP_TEXTURES:
        return False
    if is_always_texture(candidate) or is_fullscreen_texture(candidate):
        return False
    if candidate.startswith("levelshots/") or candidate.startswith("fonts/"):
        return False
    if Path(candidate).name.endswith("_spec"):
        return False
    return candidate.startswith(("textures/", "models/", "env/"))


def directory_texture_candidates(base_dir: Path, rel_dirs: tuple[str, ...]) -> set[str]:
    candidates: set[str] = set()
    for rel_dir in rel_dirs:
        root = base_dir / rel_dir
        if not root.exists():
            continue
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in IMAGE_EXTS:
                candidates.add(normalized_rel(path.relative_to(base_dir).as_posix()))
    return candidates


def all_image_candidates(base_dir: Path) -> set[str]:
    candidates: set[str] = set()
    for path in base_dir.rglob("*"):
        if path.is_file() and path.suffix.lower() in IMAGE_EXTS:
            rel = normalized_rel(path.relative_to(base_dir).as_posix())
            candidates.add(rel)
    return candidates


def resolve_texture_source(base_dir: Path, candidate: str, allow_all: bool = False) -> tuple[Path, str] | None:
    candidate = normalized_rel(candidate)
    if allow_all:
        allowed = True
    else:
        allowed = (
            candidate.startswith("textures/")
            or candidate.startswith("models/players/")
            or candidate.startswith("models/players2/")
            or candidate.startswith("gfx/")
            or candidate.startswith("env/")
        )
    if not allowed:
        return None
    if not allow_all and candidate.startswith("textures/common/") and not is_always_texture(candidate):
        return None
    rel_path = Path(*candidate.split("/"))
    if rel_path.suffix.lower() in IMAGE_EXTS:
        exact = base_dir / rel_path
        if exact.exists():
            return exact, rel_path.with_suffix(".dds").as_posix().lower()
        rel_path = rel_path.with_suffix("")

    for ext in IMAGE_EXTS:
        source = base_dir / rel_path.with_suffix(ext)
        if source.exists():
            return source, rel_path.with_suffix(".dds").as_posix().lower()
    return None


def texture_size_for_path(out_rel: str, args: argparse.Namespace) -> int:
    out_rel = normalized_rel(out_rel)
    if is_always_texture(out_rel):
        # fonts.dat stores coordinates in the authored 256x256 atlases.
        # Resizing these like ordinary HUD art makes every glyph sample the
        # wrong rectangle even though the DDS itself loads successfully.
        return max(args.max_hud_texture_size, 256)
    if out_rel.startswith("env/"):
        return args.max_loadscreen_texture_size
    if out_rel.startswith("levelshots/"):
        return max(args.max_texture_size, 256)
    if Path(out_rel).with_suffix("").as_posix() in HIGH_FIDELITY_TEXTURES:
        return max(args.max_texture_size, 256)
    # Preserve authored detail on Voyager corridor surfaces seen close up.
    # Keep the rest of the package on its existing memory policy.
    if out_rel.startswith("textures/voyager/") and any(
        word in Path(out_rel).stem for word in ("door", "floor", "wall")
    ):
        return max(args.max_texture_size, 256)
    if out_rel.startswith("models/players/") or out_rel.startswith("models/players2/"):
        return args.max_player_texture_size
    if out_rel.startswith("gfx/"):
        return args.max_hud_texture_size
    if is_fullscreen_texture(out_rel):
        return args.max_loadscreen_texture_size
    return args.max_texture_size


def next_power_of_two(value: int) -> int:
    out = 1
    while out < value:
        out <<= 1
    return out


def xbox_dimension(value: int, max_size: int) -> int:
    return max(1, min(max_size, next_power_of_two(value)))


def image_has_alpha(image: Image.Image) -> bool:
    if image.mode in ("RGBA", "LA"):
        alpha = image.getchannel("A")
        return alpha.getextrema()[0] < 250
    if image.mode == "P" and "transparency" in image.info:
        converted = image.convert("RGBA")
        alpha = converted.getchannel("A")
        return alpha.getextrema()[0] < 250
    return False


def resize_for_xbox(image: Image.Image, max_size: int) -> Image.Image:
    target = (
        xbox_dimension(image.width, max_size),
        xbox_dimension(image.height, max_size),
    )
    if target == image.size:
        return image
    resample = getattr(Image, "Resampling", Image).LANCZOS
    return image.resize(target, resample)


def dds_header(
    width: int,
    height: int,
    pitch: int,
    rgb_bits: int,
    masks: tuple[int, int, int, int],
    alpha: bool,
    mip_count: int = 1,
) -> bytes:
    DDSD_CAPS = 0x00000001
    DDSD_HEIGHT = 0x00000002
    DDSD_WIDTH = 0x00000004
    DDSD_PITCH = 0x00000008
    DDSD_PIXELFORMAT = 0x00001000
    DDSD_MIPMAPCOUNT = 0x00020000
    DDPF_ALPHAPIXELS = 0x00000001
    DDPF_RGB = 0x00000040
    DDSCAPS_TEXTURE = 0x00001000
    DDSCAPS_COMPLEX = 0x00000008
    DDSCAPS_MIPMAP = 0x00400000

    mip_count = max(1, mip_count)
    pf_flags = DDPF_RGB | (DDPF_ALPHAPIXELS if alpha else 0)
    header_flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT
    caps = DDSCAPS_TEXTURE
    if mip_count > 1:
        header_flags |= DDSD_MIPMAPCOUNT
        caps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    fields = [
        124,
        header_flags,
        height,
        width,
        pitch,
        0,
        mip_count,
        *([0] * 11),
        32,
        pf_flags,
        0,
        rgb_bits,
        masks[0],
        masks[1],
        masks[2],
        masks[3],
        caps,
        0,
        0,
        0,
        0,
    ]
    return b"DDS " + struct.pack("<31I", *fields)


def fourcc(value: bytes) -> int:
    return struct.unpack("<I", value)[0]


def dds_dxt1_header(width: int, height: int, linear_size: int, mip_count: int = 1) -> bytes:
    DDSD_CAPS = 0x00000001
    DDSD_HEIGHT = 0x00000002
    DDSD_WIDTH = 0x00000004
    DDSD_PIXELFORMAT = 0x00001000
    DDSD_LINEARSIZE = 0x00080000
    DDSD_MIPMAPCOUNT = 0x00020000
    DDPF_FOURCC = 0x00000004
    DDSCAPS_TEXTURE = 0x00001000
    DDSCAPS_COMPLEX = 0x00000008
    DDSCAPS_MIPMAP = 0x00400000

    mip_count = max(1, mip_count)
    header_flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    caps = DDSCAPS_TEXTURE
    if mip_count > 1:
        header_flags |= DDSD_MIPMAPCOUNT
        caps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    fields = [
        124,
        header_flags,
        height,
        width,
        linear_size,
        0,
        mip_count,
        *([0] * 11),
        32,
        DDPF_FOURCC,
        fourcc(b"DXT1"),
        0,
        0,
        0,
        0,
        0,
        caps,
        0,
        0,
        0,
        0,
    ]
    return b"DDS " + struct.pack("<31I", *fields)


def dds_dxt5_header(width: int, height: int, linear_size: int, mip_count: int = 1) -> bytes:
    DDSD_CAPS = 0x00000001
    DDSD_HEIGHT = 0x00000002
    DDSD_WIDTH = 0x00000004
    DDSD_PIXELFORMAT = 0x00001000
    DDSD_LINEARSIZE = 0x00080000
    DDSD_MIPMAPCOUNT = 0x00020000
    DDPF_FOURCC = 0x00000004
    DDSCAPS_TEXTURE = 0x00001000
    DDSCAPS_COMPLEX = 0x00000008
    DDSCAPS_MIPMAP = 0x00400000

    mip_count = max(1, mip_count)
    header_flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    caps = DDSCAPS_TEXTURE
    if mip_count > 1:
        header_flags |= DDSD_MIPMAPCOUNT
        caps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    fields = [
        124,
        header_flags,
        height,
        width,
        linear_size,
        0,
        mip_count,
        *([0] * 11),
        32,
        DDPF_FOURCC,
        fourcc(b"DXT5"),
        0,
        0,
        0,
        0,
        0,
        caps,
        0,
        0,
        0,
        0,
    ]
    return b"DDS " + struct.pack("<31I", *fields)


def dds_bgra32_header(width: int, height: int, pitch: int, mip_count: int = 1) -> bytes:
    DDSD_CAPS = 0x00000001
    DDSD_HEIGHT = 0x00000002
    DDSD_WIDTH = 0x00000004
    DDSD_PITCH = 0x00000008
    DDSD_PIXELFORMAT = 0x00001000
    DDSD_MIPMAPCOUNT = 0x00020000
    DDPF_ALPHAPIXELS = 0x00000001
    DDPF_RGB = 0x00000040
    DDSCAPS_TEXTURE = 0x00001000
    DDSCAPS_COMPLEX = 0x00000008
    DDSCAPS_MIPMAP = 0x00400000

    mip_count = max(1, mip_count)
    header_flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PITCH | DDSD_PIXELFORMAT
    caps = DDSCAPS_TEXTURE
    if mip_count > 1:
        header_flags |= DDSD_MIPMAPCOUNT
        caps |= DDSCAPS_COMPLEX | DDSCAPS_MIPMAP
    fields = [
        124,
        header_flags,
        height,
        width,
        pitch,
        0,
        mip_count,
        *([0] * 11),
        32,
        DDPF_RGB | DDPF_ALPHAPIXELS,
        0,
        32,
        0x00FF0000,
        0x0000FF00,
        0x000000FF,
        0xFF000000,
        caps,
        0,
        0,
        0,
        0,
    ]
    return b"DDS " + struct.pack("<31I", *fields)


def rgb_to_565(color: tuple[int, int, int]) -> int:
    r, g, b = color
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def rgb_from_565(value: int) -> tuple[int, int, int]:
    r = (value >> 11) & 0x1F
    g = (value >> 5) & 0x3F
    b = value & 0x1F
    return ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))


def color_distance_sq(a: tuple[int, int, int], b: tuple[int, int, int]) -> int:
    return (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2])


def encode_dxt1(image: Image.Image) -> bytes:
    rgb = image.convert("RGB")
    pixels = list(rgb.getdata())
    payload = bytearray()

    for by in range(0, rgb.height, 4):
        for bx in range(0, rgb.width, 4):
            block: list[tuple[int, int, int]] = []
            for y in range(4):
                sy = min(by + y, rgb.height - 1)
                for x in range(4):
                    sx = min(bx + x, rgb.width - 1)
                    block.append(pixels[sy * rgb.width + sx])

            darkest = min(block, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
            brightest = max(block, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
            c0 = rgb_to_565(brightest)
            c1 = rgb_to_565(darkest)
            if c0 == c1:
                c0 = rgb_to_565(brightest)
                c1 = max(0, c0 - 1)
            elif c0 < c1:
                c0, c1 = c1, c0

            p0 = rgb_from_565(c0)
            p1 = rgb_from_565(c1)
            palette = (
                p0,
                p1,
                (
                    (2 * p0[0] + p1[0]) // 3,
                    (2 * p0[1] + p1[1]) // 3,
                    (2 * p0[2] + p1[2]) // 3,
                ),
                (
                    (p0[0] + 2 * p1[0]) // 3,
                    (p0[1] + 2 * p1[1]) // 3,
                    (p0[2] + 2 * p1[2]) // 3,
                ),
            )

            indices = 0
            for i, color in enumerate(block):
                best = min(range(4), key=lambda idx: color_distance_sq(color, palette[idx]))
                indices |= best << (2 * i)

            payload.extend(struct.pack("<HHI", c0, c1, indices))

    return bytes(payload)


def encode_dxt5(image: Image.Image) -> bytes:
    rgba = image.convert("RGBA")
    pixels = list(rgba.getdata())
    payload = bytearray()

    for by in range(0, rgba.height, 4):
        for bx in range(0, rgba.width, 4):
            block: list[tuple[int, int, int, int]] = []
            for y in range(4):
                sy = min(by + y, rgba.height - 1)
                for x in range(4):
                    sx = min(bx + x, rgba.width - 1)
                    block.append(pixels[sy * rgba.width + sx])

            alphas = [px[3] for px in block]
            a0 = max(alphas)
            a1 = min(alphas)
            alpha_palette = [
                a0,
                a1,
                (6 * a0 + 1 * a1) // 7,
                (5 * a0 + 2 * a1) // 7,
                (4 * a0 + 3 * a1) // 7,
                (3 * a0 + 4 * a1) // 7,
                (2 * a0 + 5 * a1) // 7,
                (1 * a0 + 6 * a1) // 7,
            ]
            alpha_indices = 0
            for i, alpha in enumerate(alphas):
                best = min(range(8), key=lambda idx: abs(alpha - alpha_palette[idx]))
                alpha_indices |= best << (3 * i)

            payload.extend(struct.pack("<BB", a0, a1))
            payload.extend(alpha_indices.to_bytes(6, "little"))

            rgb_block = [(r, g, b) for r, g, b, _a in block]
            darkest = min(rgb_block, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
            brightest = max(rgb_block, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
            c0 = rgb_to_565(brightest)
            c1 = rgb_to_565(darkest)
            if c0 == c1:
                c1 = max(0, c0 - 1)
            elif c0 < c1:
                c0, c1 = c1, c0

            p0 = rgb_from_565(c0)
            p1 = rgb_from_565(c1)
            palette = (
                p0,
                p1,
                (
                    (2 * p0[0] + p1[0]) // 3,
                    (2 * p0[1] + p1[1]) // 3,
                    (2 * p0[2] + p1[2]) // 3,
                ),
                (
                    (p0[0] + 2 * p1[0]) // 3,
                    (p0[1] + 2 * p1[1]) // 3,
                    (p0[2] + 2 * p1[2]) // 3,
                ),
            )

            color_indices = 0
            for i, color in enumerate(rgb_block):
                best = min(range(4), key=lambda idx: color_distance_sq(color, palette[idx]))
                color_indices |= best << (2 * i)

            payload.extend(struct.pack("<HHI", c0, c1, color_indices))

    return bytes(payload)


def encode_rgb565(image: Image.Image) -> bytes:
    image = image.convert("RGB")
    payload = bytearray(image.width * image.height * 2)
    pos = 0
    for r, g, b in image.getdata():
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        payload[pos] = value & 0xFF
        payload[pos + 1] = (value >> 8) & 0xFF
        pos += 2
    return bytes(payload)


def clamp_byte(value: float) -> int:
    if value <= 0:
        return 0
    if value >= 255:
        return 255
    return int(value + 0.5)


def encode_lightmap_rgb565_dds(raw_rgb: bytes, boost: float) -> bytes:
    if len(raw_rgb) != LIGHTMAP_RGB_BYTES:
        raise ValueError(f"lightmap record is {len(raw_rgb)} bytes, expected {LIGHTMAP_RGB_BYTES}")

    payload = bytearray(LIGHTMAP_SIZE * LIGHTMAP_SIZE * 2)
    out_pos = 0
    for in_pos in range(0, len(raw_rgb), 3):
        r = clamp_byte(raw_rgb[in_pos + 0] * boost)
        g = clamp_byte(raw_rgb[in_pos + 1] * boost)
        b = clamp_byte(raw_rgb[in_pos + 2] * boost)
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        payload[out_pos] = value & 0xFF
        payload[out_pos + 1] = (value >> 8) & 0xFF
        out_pos += 2

    return dds_header(
        LIGHTMAP_SIZE,
        LIGHTMAP_SIZE,
        LIGHTMAP_SIZE * 2,
        16,
        (0x0000F800, 0x000007E0, 0x0000001F, 0),
        False,
    ) + bytes(payload)


def encode_bgra32(image: Image.Image) -> bytes:
    image = image.convert("RGBA")
    payload = bytearray(image.width * image.height * 4)
    pos = 0
    for r, g, b, a in image.getdata():
        payload[pos] = b
        payload[pos + 1] = g
        payload[pos + 2] = r
        payload[pos + 3] = a
        pos += 4
    return bytes(payload)


def build_mip_levels(image: Image.Image, generate_mipmaps: bool) -> list[Image.Image]:
    levels = [image.copy()]
    if not generate_mipmaps:
        return levels

    resample = getattr(Image, "Resampling", Image).BOX
    while levels[-1].width > 1 or levels[-1].height > 1:
        previous = levels[-1]
        levels.append(
            previous.resize(
                (max(1, previous.width >> 1), max(1, previous.height >> 1)),
                resample,
            )
        )
    return levels


def build_dds(
    source: Path,
    max_size: int,
    force_bgra32: bool = False,
    force_rgb565: bool = False,
    alpha_format: str = "bgra32",
    generate_mipmaps: bool = False,
) -> tuple[bytes, dict[str, object]] | None:
    with Image.open(source) as opened:
        has_alpha = image_has_alpha(opened)
        if force_rgb565:
            # RGB565 is an explicitly opaque path.  Drop any source alpha
            # before the first resize so Pillow cannot premultiply away RGB
            # that the material still needs.  Converting after resize is too
            # late for sparse-alpha atlases such as the scavenger power unit.
            image = resize_for_xbox(opened.convert("RGB"), max_size)
        else:
            image = resize_for_xbox(opened, max_size)
        levels = build_mip_levels(image, generate_mipmaps)
        mip_count = len(levels)
        if force_rgb565:
            payloads = [encode_rgb565(level) for level in levels]
            header = dds_header(
                image.width,
                image.height,
                image.width * 2,
                16,
                (0x0000F800, 0x000007E0, 0x0000001F, 0),
                False,
                mip_count,
            )
            fmt = "rgb565"
        elif force_bgra32:
            payloads = [encode_bgra32(level) for level in levels]
            header = dds_bgra32_header(image.width, image.height, image.width * 4, mip_count)
            fmt = "bgra32"
        elif has_alpha and alpha_format == "bgra32":
            payloads = [encode_bgra32(level) for level in levels]
            header = dds_bgra32_header(image.width, image.height, image.width * 4, mip_count)
            fmt = "bgra32"
        elif has_alpha:
            payloads = [encode_dxt5(level) for level in levels]
            header = dds_dxt5_header(image.width, image.height, len(payloads[0]), mip_count)
            fmt = "dxt5"
        else:
            payloads = [encode_dxt1(level) for level in levels]
            header = dds_dxt1_header(image.width, image.height, len(payloads[0]), mip_count)
            fmt = "dxt1"

        payload = b"".join(payloads)

        info = {
            "source": source.as_posix(),
            "format": fmt,
            "sourceWidth": opened.width,
            "sourceHeight": opened.height,
            "width": image.width,
            "height": image.height,
            "mipCount": mip_count,
            "bytes": len(header) + len(payload),
        }
        return header + payload, info


def build_dxt5_dds_from_bytes(source_name: str, source_bytes: bytes, max_size: int) -> tuple[bytes, dict[str, object]]:
    with Image.open(io.BytesIO(source_bytes)) as opened:
        image = resize_for_xbox(opened, max_size)
        payload = encode_dxt5(image)
        header = dds_dxt5_header(image.width, image.height, len(payload))
        info = {
            "source": source_name,
            "format": "dxt5",
            "sourceWidth": opened.width,
            "sourceHeight": opened.height,
            "width": image.width,
            "height": image.height,
            "mipCount": 1,
            "bytes": len(header) + len(payload),
            "sha1": hashlib.sha1(header + payload).hexdigest(),
        }
        return header + payload, info


def build_seeded_ui_dds_from_bytes(
    source_name: str,
    source_bytes: bytes,
    max_size: int,
) -> tuple[bytes, dict[str, object]]:
    with Image.open(io.BytesIO(source_bytes)) as opened:
        has_alpha = image_has_alpha(opened)
        image = resize_for_xbox(opened, max_size)
        if has_alpha:
            payload = encode_dxt5(image)
            header = dds_dxt5_header(image.width, image.height, len(payload))
            fmt = "dxt5"
        else:
            payload = encode_dxt1(image)
            header = dds_dxt1_header(image.width, image.height, len(payload))
            fmt = "dxt1"
        info = {
            "source": source_name,
            "format": fmt,
            "sourceWidth": opened.width,
            "sourceHeight": opened.height,
            "width": image.width,
            "height": image.height,
            "mipCount": 1,
            "bytes": len(header) + len(payload),
            "sha1": hashlib.sha1(header + payload).hexdigest(),
        }
        return header + payload, info


def add_holomatch_loadscreen_overrides(
    zip_out: zipfile.ZipFile,
    base_dir: Path,
    max_size: int,
    textures: list[dict[str, object]],
) -> None:
    source_pk3 = base_dir / "xbox0.pk3"
    if not source_pk3.is_file():
        raise FileNotFoundError(f"missing xbox0.pk3 for Holomatch loadscreen overrides: {source_pk3}")

    with zipfile.ZipFile(source_pk3, "r") as source_zip:
        entries = {name.lower(): name for name in source_zip.namelist()}
        for rel in HOLOMATCH_LOADSCREEN_OVERRIDES:
            source_name = entries.get(rel.lower())
            if not source_name:
                raise FileNotFoundError(f"missing Holomatch loadscreen source in xbox0.pk3: {rel}")
            data, info = build_dxt5_dds_from_bytes(
                f"xbox0.pk3:{source_name}",
                source_zip.read(source_name),
                max_size,
            )
            zip_write_bytes(zip_out, rel, data)
            info["path"] = rel
            info["source"] = f"xbox0.pk3:{source_name}"
            info["maxTextureSize"] = max_size
            textures.append(info)


def add_holomatch_shared_sp_dds(
    zip_out: zipfile.ZipFile,
    base_dir: Path,
    max_size: int,
    textures: list[dict[str, object]],
    support_files: list[str],
) -> None:
    source_pk3 = base_dir / "xbox0.pk3"
    if not source_pk3.is_file():
        raise FileNotFoundError(f"missing xbox0.pk3 for Holomatch shared SP DDS assets: {source_pk3}")

    with zipfile.ZipFile(source_pk3, "r") as source_zip:
        entries = {name.lower(): name for name in source_zip.namelist()}
        for rel in HOLOMATCH_SHARED_SP_DDS:
            source_name = entries.get(rel.lower())
            if not source_name:
                raise FileNotFoundError(f"missing shared SP DDS asset in xbox0.pk3: {rel}")
            data, info = build_dxt5_dds_from_bytes(
                f"xbox0.pk3:{source_name}",
                source_zip.read(source_name),
                max_size,
            )
            zip_write_bytes(zip_out, rel, data)
            info["path"] = rel
            info["source"] = f"xbox0.pk3:{source_name}"
            info["maxTextureSize"] = max_size
            textures.append(info)
            support_files.append(rel)


def add_seeded_shared_sp_dds(
    zip_out: zipfile.ZipFile,
    seed_pk3: Path,
    max_size: int,
    textures: list[dict[str, object]],
) -> None:
    if not seed_pk3.is_file():
        raise FileNotFoundError(f"missing shared SP DDS seed package: {seed_pk3}")

    written_names = {normalized_rel(name) for name in zip_out.namelist()}
    with zipfile.ZipFile(seed_pk3, "r") as source_zip:
        entries = {name.lower(): name for name in source_zip.namelist()}
        seeded_ui = sorted(
            rel
            for rel in entries
            if rel.endswith(".dds") and rel.startswith(SEEDED_UI_DDS_PREFIXES)
        )
        for rel in sorted(set(SEEDED_SHARED_SP_DDS).union(seeded_ui)):
            if normalized_rel(rel) in written_names:
                continue
            source_name = entries.get(rel.lower())
            if not source_name:
                raise FileNotFoundError(f"missing shared SP DDS asset in seed package: {rel}")
            data, info = build_seeded_ui_dds_from_bytes(
                f"{seed_pk3.name}:{source_name}",
                source_zip.read(source_name),
                max_size,
            )
            zip_write_bytes(zip_out, rel, data)
            written_names.add(normalized_rel(rel))
            info["path"] = rel
            info["source"] = f"{seed_pk3.name}:{source_name}"
            info["maxTextureSize"] = max_size
            textures.append(info)


def parse_bsp_lumps(data: bytes, bsp_path: Path) -> list[tuple[int, int]]:
    if len(data) < 8 + EF_HEADER_LUMPS * 8:
        raise ValueError(f"{bsp_path} is too small to be an EF BSP")

    ident, version = struct.unpack_from("<4sI", data, 0)
    if ident != EF_BSP_IDENT or version != EF_BSP_VERSION:
        raise ValueError(f"{bsp_path} is not an Elite Force IBSP v46 map")

    lumps: list[tuple[int, int]] = []
    for lump in range(EF_HEADER_LUMPS):
        ofs, length = struct.unpack_from("<II", data, 8 + lump * 8)
        if ofs < 0 or length < 0 or ofs > len(data) or length > len(data) - ofs:
            raise ValueError(f"{bsp_path} has invalid lump {lump} ofs={ofs} len={length}")
        lumps.append((ofs, length))
    return lumps


def campaign_bsp_paths(base_dir: Path) -> list[Path]:
    maps_dir = base_dir / "maps"
    paths: list[Path] = []
    for path in sorted(maps_dir.rglob("*.bsp")):
        name = path.name.lower()
        if name.startswith(MULTIPLAYER_MAP_PREFIXES):
            continue
        paths.append(path)
    return paths


def multiplayer_bsp_paths(base_dir: Path) -> list[Path]:
    maps_dir = base_dir / "maps"
    return sorted(
        path
        for path in maps_dir.glob("*.bsp")
        if path.name.lower().startswith(MULTIPLAYER_MAP_PREFIXES)
    )


def selected_bsp_paths(base_dir: Path, mode: str, map_name: str) -> list[Path]:
    maps_dir = base_dir / "maps"
    if mode == "none":
        return []
    if mode == "campaign":
        return campaign_bsp_paths(base_dir)
    if mode == "multiplayer":
        return multiplayer_bsp_paths(base_dir)
    if mode == "all":
        return sorted(maps_dir.rglob("*.bsp"))
    if mode == "map":
        path = maps_dir / f"{map_name}.bsp"
        if not path.exists():
            raise FileNotFoundError(path)
        return [path]
    raise ValueError(f"unknown BSP map mode {mode}")


def locate_bspthing(repo_root: Path) -> Path:
    tool = repo_root / "build" / "tools" / "bspthing.exe"
    dependencies = (
        repo_root / "code" / "bspthing" / "main.cpp",
        repo_root / "code" / "bspthing" / "bsp.h",
        repo_root / "code" / "bspthing" / "pbsp.h",
        repo_root / "code" / "qcommon" / "sparc.h",
    )
    if tool.is_file() and all(tool.stat().st_mtime_ns >= path.stat().st_mtime_ns for path in dependencies):
        return tool

    source = dependencies[0]
    cl = Path(r"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\bin\cl.exe")
    vc_include = Path(r"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\include")
    sdk_include = Path(r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Include")
    vc_lib = Path(r"C:\Program Files (x86)\Microsoft Visual Studio 8\VC\lib")
    sdk_lib = Path(r"C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0A\Lib")
    if not source.is_file() or not cl.is_file():
        raise FileNotFoundError("Elite Force Xbox BSP converter source/toolchain is unavailable")

    tool.parent.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ)
    env["PATH"] = ";".join(
        (
            str(cl.parent),
            r"C:\Program Files (x86)\Microsoft Visual Studio 8\Common7\IDE",
            env.get("PATH", ""),
        )
    )
    env["INCLUDE"] = f"{vc_include};{sdk_include}"
    env["LIB"] = f"{vc_lib};{sdk_lib}"
    subprocess.run(
        (
            str(cl),
            "/nologo",
            "/EHsc",
            "/O2",
            "/DWIN32",
            f"/Fo{tool.parent / 'bspthing.obj'}",
            f"/Fe{tool}",
            str(source),
        ),
        cwd=repo_root,
        env=env,
        check=True,
    )
    return tool


def packed_bsp_lumps(repo_root: Path, bsp_path: Path) -> tuple[dict[str, bytes], dict[str, object]]:
    tool = locate_bspthing(repo_root)
    with tempfile.TemporaryDirectory(prefix="stefx-bspthing-") as temp_name:
        temp_dir = Path(temp_name)
        input_path = temp_dir / bsp_path.name
        shutil.copy2(bsp_path, input_path)
        conversion = subprocess.run(
            (str(tool), str(temp_dir)),
            cwd=repo_root,
            capture_output=True,
            text=True,
        )
        if conversion.returncode:
            raise RuntimeError(
                f"Xbox BSP conversion failed for {bsp_path}:\n"
                f"{conversion.stdout}{conversion.stderr}"
            )

        output_dir = temp_dir / bsp_path.stem
        outputs: dict[str, bytes] = {}
        missing: list[str] = []
        for name in PACKED_BSP_LUMP_NAMES:
            path = output_dir / f"{name}.mle"
            if not path.is_file():
                if name in {"faces", "flares", "lightarray", "patches", "trisurfs"}:
                    outputs[name] = b""
                    continue
                missing.append(name)
                continue
            outputs[name] = path.read_bytes()
        if missing:
            raise FileNotFoundError(
                f"{bsp_path} converter omitted required Xbox lump(s): {', '.join(missing)}"
            )

    sizes = {name: len(data) for name, data in outputs.items()}
    return outputs, {
        "packedLumpCount": len(outputs),
        "packedLumpBytes": sum(sizes.values()),
        "largestPackedLump": max(sizes, key=sizes.get),
        "largestPackedLumpBytes": max(sizes.values()),
        "packedLumpSizes": sizes,
    }


def optimized_bsp_and_lightmaps(bsp_path: Path, boost: float) -> tuple[bytes, bytes, dict[str, object]]:
    data = bsp_path.read_bytes()
    lumps = parse_bsp_lumps(data, bsp_path)
    lightmap_ofs, lightmap_len = lumps[EF_LUMP_LIGHTMAPS]

    if lightmap_len <= 0:
        raise ValueError(f"{bsp_path} has no lightmap lump to optimize")
    if lightmap_len % LIGHTMAP_RGB_BYTES:
        raise ValueError(f"{bsp_path} lightmap lump is not 128x128 RGB aligned")

    lightmap_count = lightmap_len // LIGHTMAP_RGB_BYTES
    sidecar = bytearray(struct.pack("<I", LIGHTMAP_RGB565_DDS_BYTES))
    for index in range(lightmap_count):
        start = lightmap_ofs + index * LIGHTMAP_RGB_BYTES
        sidecar.extend(encode_lightmap_rgb565_dds(data[start : start + LIGHTMAP_RGB_BYTES], boost))

    new_lumps = [(0, 0)] * EF_HEADER_LUMPS
    output = bytearray(8 + EF_HEADER_LUMPS * 8)
    write_items = sorted(
        (ofs, lump, length)
        for lump, (ofs, length) in enumerate(lumps)
        if lump != EF_LUMP_LIGHTMAPS and length > 0
    )

    for ofs, lump, length in write_items:
        while len(output) & 3:
            output.append(0)
        new_ofs = len(output)
        output.extend(data[ofs : ofs + length])
        new_lumps[lump] = (new_ofs, length)

    new_lumps[EF_LUMP_LIGHTMAPS] = (8 + EF_HEADER_LUMPS * 8, 0)
    struct.pack_into("<4sI", output, 0, EF_BSP_IDENT, EF_BSP_VERSION)
    for lump, (ofs, length) in enumerate(new_lumps):
        struct.pack_into("<II", output, 8 + lump * 8, ofs, length)

    optimized = bytes(output)
    optimized_lumps = parse_bsp_lumps(optimized, bsp_path)

    preserved_hash_ok = True
    for lump in range(EF_HEADER_LUMPS):
        if lump == EF_LUMP_LIGHTMAPS:
            continue
        old_ofs, old_len = lumps[lump]
        new_ofs, new_len = optimized_lumps[lump]
        if old_len != new_len:
            preserved_hash_ok = False
            break
        if hashlib.sha1(data[old_ofs : old_ofs + old_len]).digest() != hashlib.sha1(optimized[new_ofs : new_ofs + new_len]).digest():
            preserved_hash_ok = False
            break

    sidecar_bytes = len(sidecar)
    stream_peak_before = len(data) + LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4
    stream_peak_after = len(optimized) + LIGHTMAP_RGB565_DDS_BYTES + (LIGHTMAP_SIZE * LIGHTMAP_SIZE * 4)
    report = {
        "map": bsp_path.stem,
        "source": normalized_rel(bsp_path.as_posix()),
        "originalBspBytes": len(data),
        "optimizedBspBytes": len(optimized),
        "rawLightmapBytesRemoved": lightmap_len,
        "optimizedLightmapSidecarBytes": sidecar_bytes,
        "lightmapCount": lightmap_count,
        "lightmapRecordBytes": LIGHTMAP_RGB565_DDS_BYTES,
        "bspReadBufferBytesSaved": len(data) - len(optimized),
        "estimatedPeakLoadBytesBefore": stream_peak_before,
        "estimatedPeakLoadBytesAfter": stream_peak_after,
        "estimatedPeakLoadBytesSaved": stream_peak_before - stream_peak_after,
        "nonLightmapLumpsPreserved": preserved_hash_ok,
        "lightmapBoostBaked": boost,
    }
    if not preserved_hash_ok:
        raise ValueError(f"{bsp_path} optimized BSP changed a non-lightmap lump")
    return optimized, bytes(sidecar), report


def zip_write_bytes(zip_out: zipfile.ZipFile, rel: str, data: bytes) -> None:
    info = zipfile.ZipInfo(normalized_rel(rel), FIXED_ZIP_TIME)
    info.compress_type = zipfile.ZIP_STORED
    info.external_attr = 0o644 << 16
    zip_out.writestr(info, data)


def existing_dds_entries(out_path: Path) -> dict[str, bytes]:
    carried: dict[str, bytes] = {}
    if not out_path.is_file():
        return carried

    try:
        with zipfile.ZipFile(out_path, "r") as existing:
            for source_name in existing.namelist():
                rel = normalized_rel(source_name)
                if Path(rel).suffix.lower() != ".dds":
                    continue
                data = existing.read(source_name)
                if len(data) >= 128 and data[:4] == b"DDS ":
                    carried[rel] = data
    except (OSError, zipfile.BadZipFile):
        return {}

    return carried


def carried_dds_info(source_name: str, data: bytes) -> dict[str, object]:
    height = struct.unpack_from("<I", data, 12)[0]
    width = struct.unpack_from("<I", data, 16)[0]
    mip_count = struct.unpack_from("<I", data, 28)[0] or 1
    fourcc_code = data[84:88]
    bits_per_pixel = struct.unpack_from("<I", data, 88)[0]
    if fourcc_code == b"DXT1":
        fmt = "dxt1"
    elif fourcc_code == b"DXT5":
        fmt = "dxt5"
    elif bits_per_pixel == 32:
        fmt = "bgra32"
    elif bits_per_pixel == 16:
        fmt = "rgb565"
    else:
        fmt = "unknown"
    return {
        "source": source_name,
        "format": fmt,
        "sourceWidth": width,
        "sourceHeight": height,
        "width": width,
        "height": height,
        "mipCount": mip_count,
        "bytes": len(data),
        "sha1": hashlib.sha1(data).hexdigest(),
        "carriedForward": True,
    }


def ui_script_files(base_dir: Path) -> list[Path]:
    ui_dir = base_dir / "ui"
    if not ui_dir.is_dir():
        return []

    deprecated_mp_menu_scripts = {"jahud.txt", "jampmenus.txt", "jampingame.txt", "testhud.menu"}
    scripts: list[Path] = []
    for path in ui_dir.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() not in {".txt", ".menu"}:
            continue
        if path.name.lower() == "vssver.scc":
            continue

        rel = normalized_rel(path.relative_to(ui_dir).as_posix()).lower()
        if rel in deprecated_mp_menu_scripts or rel.startswith("jamp/"):
            continue

        scripts.append(path)

    return sorted(scripts)


def holomatch_support_files(base_dir: Path, map_names: list[str]) -> list[Path]:
    direct_files = [
        *(f"maps/{map_name}.aas" for map_name in map_names),
        "scripts/bots.txt",
        "scripts/arenas.txt",
        "scripts/xpack.arena",
        "botfiles/bots/chars.h",
        "botfiles/bots/seven_c.c",
        "botfiles/bots/seven_i.c",
        "botfiles/bots/seven_t.c",
        "botfiles/bots/seven_w.c",
        "botfiles/bots/reaver_c.c",
        "botfiles/bots/reaver_i.c",
        "botfiles/bots/reaver_t.c",
        "botfiles/bots/reaver_w.c",
    ]
    support_dirs = (
        "botfiles",
        "models/players/munro",
        "models/players/seven",
        "models/players/reaver",
        "models/players2",
        "models/powerups/trek",
        "models/weapons2",
    )

    seen: set[str] = set()
    files: list[Path] = []

    def add_path(path: Path) -> None:
        if not path.is_file():
            return
        rel = normalized_rel(path.relative_to(base_dir).as_posix())
        if rel in seen:
            return
        seen.add(rel)
        files.append(path)

    for rel in direct_files:
        add_path(base_dir / Path(*rel.split("/")))

    for rel_dir in support_dirs:
        root = base_dir / Path(*rel_dir.split("/"))
        if not root.is_dir():
            continue
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            if path.name.lower() == "vssver.scc":
                continue
            if path.suffix.lower() in IMAGE_EXTS or path.suffix.lower() == ".dds":
                continue
            add_path(path)

    return sorted(files, key=lambda path: normalized_rel(path.relative_to(base_dir).as_posix()))


def shader_script_files(base_dir: Path) -> list[Path]:
    scripts_dir = base_dir / "scripts"
    if not scripts_dir.is_dir():
        return []

    return sorted(path for path in scripts_dir.glob("*.shader") if path.is_file())


def console_shader_list_bytes(shader_scripts: list[str]) -> bytes:
    names = sorted({rel.rsplit("/", 1)[-1] for rel in shader_scripts})
    return (("\r\n".join(names) + "\r\n") if names else "").encode("ascii")


def console_file_list_bytes(names: list[str]) -> bytes:
    unique = sorted({name for name in names if name})
    return (("\r\n".join(unique) + "\r\n") if unique else "").encode("ascii")


def resolve_xbox_effects_image(explicit_path: Path | None) -> Path:
    candidates = [explicit_path] if explicit_path else list(XBOX_EFFECTS_IMAGE_CANDIDATES)
    for candidate in candidates:
        if candidate and candidate.is_file():
            return candidate.resolve()

    checked = ", ".join(str(path) for path in candidates if path)
    raise FileNotFoundError(
        f"missing Xbox effects image for {XBOX_EFFECTS_IMAGE_REL}; checked: {checked}"
    )


def build_patch(args: argparse.Namespace) -> dict[str, object]:
    base_dir = args.base_dir.resolve()
    map_path = base_dir / "maps" / f"{args.map}.bsp"
    out_path = args.output.resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)
    carried_dds = existing_dds_entries(out_path)
    shader_archive_dir = args.shader_archive_dir.resolve() if args.shader_archive_dir else None
    no_mipmap_refs = no_mipmap_texture_references(base_dir, shader_archive_dir)

    shader_names = set(read_bsp_shader_names(map_path))
    resolved: dict[str, Path] = {}
    skipped: list[str] = []
    if args.texture_mode != "none":
        candidates = set(shader_names)
        candidates.update(parse_shader_references(base_dir, shader_names))
        candidates.update(directory_texture_candidates(base_dir, DIRECTORY_TEXTURE_SEEDS))
        candidates.update(directory_texture_candidates(base_dir, PLAYER_TEXTURE_SEEDS))
        candidates.update(directory_texture_candidates(base_dir, HUD_TEXTURE_SEEDS))
        candidates.update(ALWAYS_TEXTURES)
        candidates.update(FULLSCREEN_TEXTURE_SEEDS)

        if args.texture_mode == "all":
            candidates.update(all_image_candidates(base_dir))

        for candidate in sorted(candidates):
            found = resolve_texture_source(base_dir, candidate, args.texture_mode == "all")
            if found:
                source, out_rel = found
                resolved[out_rel] = source
            elif candidate.startswith("textures/") and not candidate.startswith("textures/common/"):
                skipped.append(candidate)

    textures: list[dict[str, object]] = []
    skipped_alpha: list[str] = []
    preserved_original: list[str] = []
    preserved_original_sources: list[dict[str, str]] = []
    preserved_original_written: set[str] = set()
    bsp_optimizations: list[dict[str, object]] = []
    bsp_outputs: list[tuple[str, str, bytes, str, bytes, dict[str, bytes]]] = []
    bsp_checksums: dict[str, int] = {}
    patched_aas_checksums: dict[str, int] = {}
    ui_scripts: list[str] = []
    support_files: list[str] = []
    effects_image_info: dict[str, object] | None = None
    shader_scripts: list[str] = []
    shader_script_set: set[str] = set()
    stock_shader_bytes = xbox_stock_shader_bytes(base_dir, shader_archive_dir)

    if args.include_bsp and args.bsp_mode != "none":
        raise ValueError("--include-bsp cannot be combined with --bsp-mode")

    if args.bsp_mode != "none":
        for bsp_path in selected_bsp_paths(base_dir, args.bsp_maps, args.map):
            optimized_bsp, optimized_lightmaps, report = optimized_bsp_and_lightmaps(
                bsp_path, args.lightmap_boost
            )
            packed_lumps, packed_report = packed_bsp_lumps(
                Path(__file__).resolve().parents[1], bsp_path
            )
            map_name = bsp_path.relative_to(base_dir / "maps").with_suffix("").as_posix().lower()
            bsp_out_rel = f"maps/xbox/{map_name}.bsp"
            lightmap_out_rel = f"maps/xbox/{map_name}.lmpdds"
            checksum = com_block_checksum(optimized_bsp)
            packed_lumps["checksum"] = struct.pack("<I", checksum)
            report["map"] = map_name
            report["bspPath"] = None
            report["packedMapPath"] = f"maps/{map_name}/"
            report["lightmapPath"] = lightmap_out_rel
            report["xboxBspChecksum"] = checksum
            report.update(packed_report)
            bsp_optimizations.append(report)
            bsp_outputs.append(
                (
                    bsp_out_rel,
                    map_name,
                    optimized_bsp,
                    lightmap_out_rel,
                    optimized_lightmaps,
                    packed_lumps,
                )
            )
            bsp_checksums[map_name] = checksum

    with zipfile.ZipFile(out_path, "w") as zip_out:
        for stock_shader_path in XBOX_STOCK_SHADER_PATHS:
            stock_shader_rel = normalized_rel(stock_shader_path)
            zip_write_bytes(zip_out, stock_shader_rel, stock_shader_bytes[stock_shader_path])
            shader_scripts.append(stock_shader_rel)
            shader_script_set.add(stock_shader_rel)

        for shader_path in shader_script_files(base_dir):
            shader_rel = normalized_rel(shader_path.relative_to(base_dir).as_posix())
            if shader_rel in shader_script_set:
                continue
            zip_write_bytes(zip_out, shader_rel, shader_path.read_bytes())
            shader_scripts.append(shader_rel)
            shader_script_set.add(shader_rel)

        shader_list_path = "scripts/_console_shader_list_"
        zip_write_bytes(zip_out, shader_list_path, console_shader_list_bytes(shader_scripts))

        if not args.no_ui_scripts:
            for ui_path in ui_script_files(base_dir):
                ui_rel = normalized_rel(ui_path.relative_to(base_dir).as_posix())
                zip_write_bytes(zip_out, ui_rel, ui_path.read_bytes())
                ui_scripts.append(ui_rel)

        if args.holomatch_support_assets:
            support_map_names = sorted(bsp_checksums) if bsp_checksums else [args.map.lower()]
            for support_path in holomatch_support_files(base_dir, support_map_names):
                support_rel = normalized_rel(support_path.relative_to(base_dir).as_posix())
                support_data = support_path.read_bytes()
                support_suffix = Path(support_rel).suffix.lower()
                support_map_name = Path(support_rel).stem.lower()
                if support_suffix == ".aas" and support_map_name in bsp_checksums:
                    checksum = bsp_checksums[support_map_name]
                    support_data = patch_aas_checksum(support_data, checksum)
                    patched_aas_checksums[support_rel] = checksum
                zip_write_bytes(zip_out, support_rel, support_data)
                support_files.append(support_rel)
            for alias_rel, source_rel in sorted(HOLOMATCH_BOTFILE_ALIASES.items()):
                if alias_rel in support_files:
                    continue
                source_path = base_dir / Path(*source_rel.split("/"))
                if not source_path.is_file():
                    raise FileNotFoundError(
                        f"missing official EF botfile alias source: {source_path}"
                    )
                zip_write_bytes(zip_out, alias_rel, source_path.read_bytes())
                support_files.append(alias_rel)
            arena_names = [
                name
                for name in ("arenas.txt", "xpack.arena")
                if (base_dir / "scripts" / name).is_file()
            ]
            holomatch_lists = {
                "scripts/_console_bot_list_": ["bots.txt"],
                "scripts/_console_arena_list_": arena_names,
            }
            for list_rel, names in holomatch_lists.items():
                zip_write_bytes(zip_out, list_rel, console_file_list_bytes(names))
                support_files.append(list_rel)

            if args.effects_image is not None:
                effects_image_path = resolve_xbox_effects_image(args.effects_image)
                effects_image_data = effects_image_path.read_bytes()
                zip_write_bytes(zip_out, XBOX_EFFECTS_IMAGE_REL, effects_image_data)
                support_files.append(XBOX_EFFECTS_IMAGE_REL)
                effects_image_info = {
                    "path": XBOX_EFFECTS_IMAGE_REL,
                    "source": str(effects_image_path),
                    "bytes": len(effects_image_data),
                    "sha1": hashlib.sha1(effects_image_data).hexdigest(),
                }
            add_holomatch_shared_sp_dds(
                zip_out,
                base_dir,
                args.max_hud_texture_size,
                textures,
                support_files,
            )
            add_holomatch_loadscreen_overrides(
                zip_out,
                base_dir,
                args.max_loadscreen_texture_size,
                textures,
            )

        bsp_rel: str | None = None
        if args.include_bsp:
            bsp_rel = f"maps/xbox/{args.map}.bsp"
            zip_write_bytes(zip_out, bsp_rel, map_path.read_bytes())

        if args.bsp_mode != "none":
            for bsp_out_rel, map_name, optimized_bsp, lightmap_out_rel, optimized_lightmaps, packed_lumps in bsp_outputs:
                zip_write_bytes(zip_out, lightmap_out_rel, optimized_lightmaps)
                for lump_name, lump_data in sorted(packed_lumps.items()):
                    zip_write_bytes(zip_out, f"maps/{map_name}/{lump_name}.mle", lump_data)

        written_names = {normalized_rel(name) for name in zip_out.namelist()}
        for out_rel, source in sorted(resolved.items()):
            out_rel = normalized_rel(out_rel)
            if out_rel in written_names:
                continue
            if not args.dds_only and should_preserve_original_texture(out_rel):
                source_rel = normalized_rel(source.relative_to(base_dir).as_posix())
                if source_rel not in preserved_original_written:
                    zip_write_bytes(zip_out, source_rel, source.read_bytes())
                    preserved_original_written.add(source_rel)
                    written_names.add(source_rel)
                preserved_original.append(out_rel)
                preserved_original_sources.append(
                    {
                        "path": source_rel,
                        "source": source_rel,
                        "skippedOverride": out_rel,
                    }
                )
                continue
            max_size = texture_size_for_path(out_rel, args)
            built = build_dds(
                source,
                max_size,
                force_bgra32=should_force_bgra32_texture(out_rel),
                force_rgb565=should_use_rgb565_texture(out_rel),
                alpha_format=args.alpha_texture_format,
                generate_mipmaps=(
                    args.generate_mipmaps
                    and should_generate_mipmaps(out_rel, no_mipmap_refs)
                ),
            )
            if built is None:
                skipped_alpha.append(out_rel)
                continue
            dds, info = built
            if is_always_texture(out_rel) and (
                info["width"] != info["sourceWidth"]
                or info["height"] != info["sourceHeight"]
            ):
                raise ValueError(
                    f"fixed font atlas was resized: {out_rel} "
                    f"{info['sourceWidth']}x{info['sourceHeight']} -> "
                    f"{info['width']}x{info['height']}"
                )
            zip_write_bytes(zip_out, out_rel, dds)
            written_names.add(out_rel)
            info["path"] = out_rel
            info["source"] = normalized_rel(source.relative_to(base_dir).as_posix())
            info["maxTextureSize"] = max_size
            textures.append(info)

        if args.dds_seed is not None:
            add_seeded_shared_sp_dds(
                zip_out,
                args.dds_seed.resolve(),
                args.max_hud_texture_size,
                textures,
            )

        written_names = {normalized_rel(name) for name in zip_out.namelist()}
        for out_rel, data in sorted(carried_dds.items()):
            if out_rel in written_names:
                continue
            carried_info = carried_dds_info(f"{out_path.name}:{out_rel}", data)
            if carried_info["format"] == "bgra32" and args.alpha_texture_format == "dxt5":
                data, carried_info = build_dxt5_dds_from_bytes(
                    f"{out_path.name}:{out_rel}",
                    data,
                    texture_size_for_path(out_rel, args),
                )
                carried_info["transcodedFrom"] = "bgra32"
            zip_write_bytes(zip_out, out_rel, data)
            carried_info["path"] = out_rel
            carried_info["maxTextureSize"] = texture_size_for_path(out_rel, args)
            textures.append(carried_info)

        manifest = {
            "name": "Star Trek: Elite Force Xbox patch pack",
            "map": args.map,
            "bsp": bsp_rel,
            "bspIncluded": bool(bsp_rel),
            "maxTextureSize": args.max_texture_size,
            "maxPlayerTextureSize": args.max_player_texture_size,
            "maxHudTextureSize": args.max_hud_texture_size,
            "maxLoadscreenTextureSize": args.max_loadscreen_texture_size,
            "textureMode": args.texture_mode,
            "ddsOnly": args.dds_only,
            "generateMipmaps": args.generate_mipmaps,
            "alphaTextureFormat": args.alpha_texture_format,
            "textureCount": len(textures),
            "mipmappedTextureCount": sum(int(item.get("mipCount", 1)) > 1 for item in textures),
            "singleLevelTextureCount": sum(int(item.get("mipCount", 1)) <= 1 for item in textures),
            "noMipmapsShaderReferenceCount": len(no_mipmap_refs),
            "shaderArchiveDir": str(shader_archive_dir) if shader_archive_dir else None,
            "textures": textures,
            "skippedTextureCandidates": skipped,
            "skippedAlphaTextures": skipped_alpha,
            "preservedOriginalTextures": preserved_original,
            "preservedOriginalTextureSources": preserved_original_sources,
            "patchShaders": list(XBOX_STOCK_SHADER_PATHS),
            "shaderScriptCount": len(shader_scripts),
            "shaderScripts": shader_scripts,
            "shaderListPath": shader_list_path,
            "shaderListCount": len(shader_scripts),
            "uiScriptCount": len(ui_scripts),
            "uiScripts": ui_scripts,
            "supportFileCount": len(support_files),
            "supportFiles": support_files,
            "effectsImage": effects_image_info,
            "patchedAasChecksums": patched_aas_checksums,
            "bspMode": args.bsp_mode,
            "bspMaps": args.bsp_maps,
            "bspOptimizationCount": len(bsp_optimizations),
            "bspOptimizations": bsp_optimizations,
            "bspOptimizationTotals": {
                "originalBspBytes": sum(int(item["originalBspBytes"]) for item in bsp_optimizations),
                "optimizedBspBytes": sum(int(item["optimizedBspBytes"]) for item in bsp_optimizations),
                "rawLightmapBytesRemoved": sum(int(item["rawLightmapBytesRemoved"]) for item in bsp_optimizations),
                "optimizedLightmapSidecarBytes": sum(int(item["optimizedLightmapSidecarBytes"]) for item in bsp_optimizations),
                "bspReadBufferBytesSaved": sum(int(item["bspReadBufferBytesSaved"]) for item in bsp_optimizations),
                "estimatedPeakLoadBytesSaved": sum(int(item["estimatedPeakLoadBytesSaved"]) for item in bsp_optimizations),
                "packedLumpBytes": sum(int(item["packedLumpBytes"]) for item in bsp_optimizations),
            },
        }
        runtime_manifest = dict(manifest)
        for diagnostic_key in (
            "shaderArchiveDir",
            "textures",
            "skippedTextureCandidates",
            "preservedOriginalTextureSources",
            "supportFiles",
        ):
            runtime_manifest.pop(diagnostic_key, None)
        zip_write_bytes(
            zip_out,
            "xbox_patch_manifest.json",
            json.dumps(runtime_manifest, indent=2, sort_keys=True).encode("ascii"),
        )

    manifest_path = out_path.with_suffix(out_path.suffix + ".manifest.json")
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True), encoding="ascii")

    return {
        "output": str(out_path),
        "manifest": str(manifest_path),
        "bytes": out_path.stat().st_size,
        "map": args.map,
        "bspIncluded": args.include_bsp,
        "shaderNames": len(shader_names),
        "textureMode": args.texture_mode,
        "ddsOnly": args.dds_only,
        "generateMipmaps": args.generate_mipmaps,
        "alphaTextureFormat": args.alpha_texture_format,
        "textures": len(textures),
        "mipmappedTextures": sum(int(item.get("mipCount", 1)) > 1 for item in textures),
        "singleLevelTextures": sum(int(item.get("mipCount", 1)) <= 1 for item in textures),
        "noMipmapsShaderReferences": len(no_mipmap_refs),
        "skipped": len(skipped),
        "skippedAlpha": len(skipped_alpha),
        "preservedOriginal": len(preserved_original),
        "preservedOriginalSources": len(preserved_original_written),
        "uiScripts": len(ui_scripts),
        "supportFiles": len(support_files),
        "bspMode": args.bsp_mode,
        "bspMaps": args.bsp_maps,
        "bspOptimizations": len(bsp_optimizations),
        "bspOptimizationTotals": manifest["bspOptimizationTotals"],
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build an Elite Force Xbox patch PK3")
    parser.add_argument("--base-dir", type=Path, default=Path("build/release/BaseEF"))
    parser.add_argument(
        "--shader-archive-dir",
        type=Path,
        help="Optional canonical retail PAK directory used to honor nomipmaps shader metadata.",
    )
    parser.add_argument(
        "--generate-mipmaps",
        action="store_true",
        help="Emit full DDS mip chains for eligible world/model/environment textures.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/release/BaseEF/xbox0.pk3"),
        help="Patch PK3 output.",
    )
    parser.add_argument(
        "--dds-seed",
        type=Path,
        help="Optional licensed DDS seed used for direct-registration SP HUD atlases.",
    )
    parser.add_argument("--map", default="borg1")
    parser.add_argument("--max-texture-size", type=int, default=128)
    parser.add_argument("--max-player-texture-size", type=int, default=64)
    parser.add_argument("--max-hud-texture-size", type=int, default=128)
    parser.add_argument("--max-loadscreen-texture-size", type=int, default=512)
    parser.add_argument(
        "--include-bsp",
        action="store_true",
        help="Pack maps/xbox/<map>.bsp so the runtime BSP override becomes active.",
    )
    parser.add_argument(
        "--no-ui-scripts",
        action="store_true",
        help="Do not pack UI .menu/.txt scripts into this patch PK3.",
    )
    parser.add_argument(
        "--holomatch-support-assets",
        action="store_true",
        help="Pack non-texture map/bot/model support files needed by the direct Holomatch smoke target.",
    )
    parser.add_argument(
        "--effects-image",
        type=Path,
        help=(
            "Optional Xbox DirectSound effects image to pack as sound/dsstdfx.bin. "
            "Holomatch omits it by default to match the SP dry-audio setup."
        ),
    )
    parser.add_argument(
        "--bsp-mode",
        choices=("none", "optimized-lightmaps"),
        default="none",
        help=(
            "Build Xbox BSP overrides that preserve all geometry/collision lumps "
            "and move raw RGB lightmaps to streamed RGB565 DDS sidecars."
        ),
    )
    parser.add_argument(
        "--bsp-maps",
        choices=("map", "campaign", "multiplayer", "all"),
        default="map",
        help="Which BSPs to optimize when --bsp-mode is active.",
    )
    parser.add_argument(
        "--lightmap-boost",
        type=float,
        default=DEFAULT_LIGHTMAP_BOOST,
        help="Brightness multiplier baked into optimized RGB565 lightmaps.",
    )
    parser.add_argument(
        "--texture-mode",
        choices=("none", "borg1", "all"),
        default="none",
        help=(
            "DDS texture substitution mode. Disabled by default because the "
            "current mode only converts opaque textures to DXT1; alpha textures stay loose."
        ),
    )
    parser.add_argument(
        "--dds-only",
        action="store_true",
        help="Convert all selected source textures to DDS instead of preserving original JPG/TGA overrides.",
    )
    parser.add_argument(
        "--alpha-texture-format",
        choices=("dxt5", "bgra32"),
        default="dxt5",
        help="DDS format to use for selected source textures with alpha.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    summary = build_patch(parse_args(argv))
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
