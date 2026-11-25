#!/usr/bin/env python3
"""Offline geometry regression for #6; bring your own unmodified 0.9.4 xbox0.pk3.

Optional: supply the second original issue attachment to check sampled artifact
coverage. Requires NumPy, and only for image comparison, Pillow. No game data,
screenshot, or console state is written.
"""
import argparse
import hashlib
import math
import struct
import zipfile
from pathlib import Path

import numpy as np

PAK_SHA = "a7df7e2a2a606915830f0bd19ec9e80bc73746ce2ebc3e40e3c7d9c369d12657"
JPEG_SHA = "745f8ba6ce8f9ff0b937471f8ca38ea32b90c3fc40cfe36fe9f17ee84ed8a862"
BLACK = "textures/common/black"
EYE = np.array((1083.71105, -1965.17494, 251.70125))
YAW, ELEVATION, ROLL = np.radians((-5.51244, -69.55829, 0.16435))
FOCAL = (480 / math.tan(math.radians(40))) * 0.993034  # fitted square pixels at 1280x720
CAP = 8485
LUMPS = (("faces", "<IHbII4B4B3h"), ("trisurfs", "<IHbII4B"), ("patches", "<IHbI4B4B6h2B"))


def sha(path):
    with path.open("rb") as fp:
        return hashlib.file_digest(fp, "sha256").hexdigest()


def eye_ray(u, v):
    y, e, r = YAW, ELEVATION, ROLL
    view = np.array((math.cos(e)*math.cos(y), math.cos(e)*math.sin(y), math.sin(e)))
    right = np.array((math.sin(y), -math.cos(y), 0))
    down = np.cross(view, right)
    return (view + (right*math.cos(r) + down*math.sin(r)) * ((u+.5-640)/FOCAL)
            + (down*math.cos(r) - right*math.sin(r)) * ((v+.5-360)/FOCAL))


class PatchUncertain(Exception):
    """A patch must be tessellated to decide this sample."""


class World:
    """Static drawable world only. No PVS rejection, doors, post-processing or live camera."""

    def __init__(self, archive, map_name):
        self.read = lambda lump: archive.read(f"maps/{map_name}/{lump}.mle")
        self.rows = lambda lump, fmt: list(struct.iter_unpack(fmt, self.read(lump)))
        mats = self.rows("shaders", "<64sII")
        materials = [(n.split(b"\0", 1)[0].decode("ascii").lower(), fl, co) for n, fl, co in mats]
        end = self.rows("models", "<6fiHiH")[0][7]
        verts = np.array([r[10:13] for r in self.rows("verts", "<10f6h16B")], dtype=float)
        source_indexes = np.frombuffer(self.read("indexes"), "<i2").astype(np.int32)
        tri, codes, self.material = [], [], {}
        self.patch_bounds = []
        for kind, fmt in LUMPS:
            for row in self.rows(kind, fmt):
                code, mat, _, packed = row[:4]
                if code >= end:
                    continue
                name, flags, contents = materials[mat]
                self.material[code] = name, flags, contents
                if flags & 0x80 or contents & 0x40 or name == "noshader":  # BSP NODRAW / fog volume
                    continue
                base = packed >> 12
                if kind == "patches":
                    # Bound the entire control hull later. A checked ray must not
                    # intersect a patch hull on its way to the first face: this
                    # avoids pretending an untessellated patch is a planar mesh.
                    nverts = packed & 4095
                    pts = verts[base:base + nverts]
                    self.patch_bounds.append((pts.min(axis=0), pts.max(axis=0)))
                    continue
                off, count = row[4] >> 12, row[4] & 4095
                indices = source_indexes[off:off+count].reshape(-1, 3)
                tri.append(verts[base+indices])
                codes.extend([code] * len(indices))
        self.xyz = np.concatenate(tri)
        self.codes = np.array(codes)
        self.black = np.array([self.material[c][0] == BLACK for c in self.codes])
        self.pmin = self.xyz.min(axis=1) - 1e-5
        self.pmax = self.xyz.max(axis=1) + 1e-5
        self.patch_min, self.patch_max = (np.array(x) for x in zip(*self.patch_bounds))

    @staticmethod
    def slab(lo, hi, eye, d):
        # Positive ray/AABB intervals; tiny substitute for an exactly parallel axis.
        inv = 1.0 / np.where(np.abs(d) < 1e-12, 1e-12, d)
        a, b = (lo-eye)*inv, (hi-eye)*inv
        near = np.maximum(np.minimum(a, b).max(axis=1), 0)
        far = np.maximum(a, b).min(axis=1)
        return near, far >= near

    def hit_pair(self, eye, direction):
        _, possible = self.slab(self.pmin, self.pmax, eye, direction)
        ids = np.flatnonzero(possible)
        faces = self.xyz[ids]
        a, b = faces[:, 1] - faces[:, 0], faces[:, 2] - faces[:, 0]
        p = np.cross(direction, b)
        det = np.einsum("ij,ij->i", a, p)
        valid = np.abs(det) > 1e-8
        denom = np.where(valid, det, 1.0)
        disp = eye - faces[:, 0]
        q = np.cross(disp, a)
        u = np.einsum("ij,ij->i", disp, p)/denom
        v = np.einsum("j,ij->i", direction, q)/denom
        t = np.einsum("ij,ij->i", b, q)/denom
        valid &= (u >= -1e-6) & (v >= -1e-6) & (u+v <= 1.000001) & (t > 0.01)
        hits = ids[valid][np.argsort(t[valid])]
        near, hit = self.slab(self.patch_min, self.patch_max, eye, direction)
        without = hits[~self.black[hits]]
        old_valid = valid & ~self.black[ids]
        # Check even the old path: a patch behind a removed cap could otherwise
        # supply background coverage when no planar draw remains.
        dt = np.min(t[old_valid]) if np.any(old_valid) else np.inf
        if np.any(hit & (near <= dt)):
            raise PatchUncertain("a patch hull intersects the checked face ray")
        return (int(self.codes[hits[0]]) if len(hits) else None,
                int(self.codes[without[0]]) if len(without) else None)

    def check_pvs(self):
        nodes = self.rows("nodes", "<i8h")
        planes = self.rows("planes", "<4f")
        leaves = self.rows("leafs", "<hb6h4H")
        marks = np.frombuffer(self.read("leafsurfaces"), "<u4")
        n = 0
        while n >= 0:
            plane, front, back = nodes[n][:3]
            normal_and_d = planes[plane]
            n = front if EYE @ normal_and_d[:3] > normal_and_d[3] else back
        eye_leaf = leaves[-n-1]
        targets = {(leaf[0], leaf[1]) for leaf in leaves
                   if CAP in marks[leaf[8]:leaf[8]+leaf[9]]}
        assert eye_leaf[:2] == (480, 1) and targets == {(593, 1), (594, 1), (630, 1)}
        packed = self.read("visibility")
        clusters, stride, offsize, compressed, default, size, count = struct.unpack_from("<IIBBBII", packed)
        assert (offsize, compressed, size) == (3, 1, clusters*stride)
        needed = {eye_leaf[0]*stride + (c >> 3): default for c, _ in targets}
        for i in range(19, len(packed)-3, 1+offsize):
            off = int.from_bytes(packed[i+1:i+4], "little")
            if off > max(needed):
                break
            if off in needed:
                needed[off] = packed[i]
        assert all(needed[eye_leaf[0]*stride + (c >> 3)] & (1 << (c & 7)) for c, _ in targets)
        print("Borg2 camera cluster 480 can see all three cap clusters, all in area 1.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--v094-xbox0", required=True, type=Path)
    parser.add_argument("--issue6-second-image", type=Path)
    args = parser.parse_args()
    if sha(args.v094_xbox0) != PAK_SHA:
        raise SystemExit("The map corpus does not match the unmodified 0.9.4 xbox0.pk3.")
    with zipfile.ZipFile(args.v094_xbox0) as archive:
        pit = World(archive, "borg2")
        pit.check_pvs()
        interior = [(637,360), (602,366), (684,366), (618,500), (675,530), (598,457)]
        borders = [(550,513,10035), (532,469,10029), (690,295,8482), (813,412,10099),
                   (1000,265,10118), (445,442,9995), (241,132,9989), (875,301,10101),
                   (696,599,10152), (742,490,10066), (627,125,8482), (777,45,10090)]
        for u, v in interior:
            assert pit.hit_pair(EYE, eye_ray(u, v)) == (CAP, None), (u, v)
        for u, v, face in borders:
            assert pit.hit_pair(EYE, eye_ray(u, v)) == (face, face), (u, v, face)
        assert pit.material[CAP] == (BLACK, 0x430, 1)
        print("6 artifact rays now hit opaque face 8485 rather than empty space; 12 controls unchanged.")

        if args.issue6_second_image:
            from PIL import Image
            if sha(args.issue6_second_image) != JPEG_SHA:
                raise SystemExit("Supply the original second attachment from issue comment 7133922903.")
            with Image.open(args.issue6_second_image) as jpeg:
                img = np.array(jpeg.convert("RGB"))
            assert img.shape == (720, 1280, 3)
            good = total = 0
            for v in range(305, 590, 13):
                for u in range(553, 750, 13):
                    r, _, b = img[v, u]
                    if r <= 150 or b <= 150:
                        continue
                    total += 1
                    try:
                        hit = pit.hit_pair(EYE, eye_ray(u, v))[0]
                    except PatchUncertain:
                        hit = None  # conservative: keep uncertainty in the denominator
                    good += hit == CAP
            assert total > 150 and good / total > .97, (good, total)
            print(f"{good}/{total} sampled bright screenshot pixels fall on the restored pit floor.")

        field = World(archive, "borg6")
        assert {i for i,(n,fl,_) in field.material.items() if fl & 0x40000} == {1218,1219,1220,1221}
        assert {i for i,(n,fl,_) in field.material.items() if n == BLACK} == {2426,3910}
        for side, y, direction, face in [("south",1725,1,1218), ("north",1891,-1,1219)]:
            origin = np.array((3648, y, 24))
            for dx in (-.2, 0, .2):
                for dz in (-.3, 0, .3):
                    assert field.hit_pair(origin, np.array((dx,direction,dz))) == (face, face), side
        print("Borg6: both sides of the static2 forcefield remain in front on all 18 depth probes.")


if __name__ == "__main__":
    main()
