#!/usr/bin/env python3
"""Exercise the SP death, server think and client animation paths; optional Raven demo PAK geometry."""
import argparse
from pathlib import Path
import os
import posixpath
import re
import struct
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def function(source, signature):
    a = source.index(signature)
    return source[a:source.index("\n}", a) + 2]


def headers(src, out):
    # The XDK resolves mixed-case relative includes; Linux does not. Copy only
    # headers and normalize their include spelling in the temporary build.
    files = {p.as_posix().lower(): p for p in src.rglob("*") if p.suffix.lower() == ".h"}
    inc = re.compile(r'(?m)^(\s*#\s*include\s*")([^"]+)(")')
    for p in files.values():
        def norm(m):
            name = posixpath.normpath((p.parent / m[2].replace("\\", "/")).as_posix()).lower()
            return m[1] + posixpath.relpath(files[name], p.parent) + m[3] if name in files else m[0]
        dest = out / p.relative_to(src)
        dest.parent.mkdir(exist_ok=True, parents=True)
        dest.write_bytes(inc.sub(norm, p.read_bytes().decode("latin1")).encode("latin1"))
    # Windows typedefs needed by the included, unused ICARUS tokenizer.
    (out / "windows.h").write_text("""
#pragma once
typedef const char *LPCTSTR; typedef char *LPTSTR; typedef unsigned long DWORD, COLORREF;
typedef unsigned int UINT; typedef int BOOL; typedef void *HANDLE;
typedef unsigned char BYTE; typedef unsigned short WORD;
""")


SUFFIXES = [str(i+1) for i in range(7)] + ["FORWARD1", "FORWARD2", "BACKWARD1", "BACKWARD2"]


def samples(pak):
    if not pak:
        return [[(100 + n * 20, 5 + n, 125) for n in range(11)]]
    rows = []
    with zipfile.ZipFile(pak) as z:
        for model in ("klingon", "hirogen"):
            base = f"models/players/{model}/"
            cfg = {s[0].upper():s[1:5] for line in z.read(base + "animation.cfg").decode("latin1").splitlines()
                   if len(s := line.split()) >= 5 and not s[0].startswith("//")}
            m = z.read(base + "lower.mdr")
            frames, bones, ofs, _, _, tags, tags_at, end = struct.unpack_from("<8i", m, 72)
            assert end == len(m) and ofs < 0
            joints = {name.rstrip(b"\0"): bone for bone, name in
                      (struct.unpack_from("<i32s", m, tags_at + j*36) for j in range(tags))}
            bone = joints[b"tag_torso"]
            def zheight(frame):
                assert 0 <= frame < frames
                offset = -ofs + frame * (40+24*bones) + 40 + 24*bone + 4
                return (struct.unpack_from("<H", m, offset)[0] - 32768)/64.0
            model_rows = []
            for i, suffix in enumerate(SUFFIXES):
                first, count, loop, fps = map(int, cfg["BOTH_DEATH" + suffix])
                last = first+count-1
                assert loop == -1 and tuple(map(int, cfg["BOTH_DEAD" + suffix])) == (last,1,-1,fps)
                if i < 7:
                    assert zheight(first) - zheight(last) > 15 # lower-model torso attachment
                model_rows.append((first, count, (1000+fps-1)//fps))
            a, b, _ = model_rows[0]
            print(f"{model}: 11 nonlooping falls and exact rest frames; DEATH1 torso Z "
                  f"{zheight(a):.2f} -> {zheight(a+b-1):.2f}", flush=True)
            rows.append(model_rows)
    return rows


def author_samples(archive):
    """Raven's model build instructions are a separate source, not compiled model proof."""
    if not archive:
        return []
    rows = []
    with zipfile.ZipFile(archive) as z:
        for member in ("hirogen/hirogen.car", "klingon/klingon.car", "klingonfem/klingonfemale.car", "malon/malon.car"):
            lines = z.read("StarTrek/quake/baseEF/models/players/"+member).decode("latin1").splitlines()
            model_rows = []
            for suffix in SUFFIXES:
                line, = (s.split() for s in lines if "-enum BOTH_DEATH"+suffix+" " in s)
                ix, j = line.index("-frames"), line.index("BOTH_DEAD"+suffix)-5
                assert line[j] == "-additional"
                start, first, count = map(int, line[ix+1:ix+4])
                off, length, loop, fps = map(int, line[j+1:j+5])
                assert start == 0 and line[line.index("-loop")+1] == "-1"
                assert (off,length,loop) == (count-1,1,-1) and line[j+5] == "BOTH_DEAD"+suffix
                model_rows.append((first,count,(1000+fps-1)//fps))
            print(f"{member}: 11 authored falls with explicit matching final frame",flush=True)
            rows.append(model_rows)
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--cxx", default=os.getenv("CXX", "c++"))
    ap.add_argument("--pak", type=Path, help="optional local Raven demo pak0.pk3; no game data redistributed")
    ap.add_argument("--authoring", type=Path, help="optional local Raven PlayerModelAnimationSource.zip")
    ap.add_argument("--source", type=Path, default=ROOT, help="source checkout to exercise")
    ap.add_argument("--expect-stock", action="store_true", help="negative control: require the pre-fix failures")
    ap.add_argument("--sanitize", action="store_true")
    args = ap.parse_args()
    src = args.source.resolve() / "SP-Mod-Source-Code-master"
    def read(name): return (src/name).read_bytes().decode("latin1")
    npc, combat, bg, cg = map(read, ("game/NPC.cpp", "game/g_combat.cpp", "game/bg_panimate.cpp", "cgame/cg_players.cpp"))
    add = function(read("cgame/cg_ents.cpp"), "static void CG_AddCEntity(")
    guard = add[add.index("if ( cg_stefxCullHiddenPlayers.integer"):].split("#endif", 1)[0]
    assert "CG_Player( cent );" in add and add.index(guard) < add.index("CG_Player( cent );")
    assert "cent->gent->client->renderInfo.legsFrame = cent->pe.legs.frame;" in cg
    think = function(npc, "void NPC_Think (")
    assert "if (self->health <= 0)" in think and "DeadThink();" in think
    parts = [function(cg, "qboolean ValidAnimFileIndex (")]
    parts += [function(bg, s) for s in ("qboolean PM_HasAnimation(", "int PM_PickAnim(",
              "void PM_SetLegsAnimTimer(", "void PM_SetTorsoAnimTimer(", "void PM_SetAnimFinal(",
              "qboolean PM_InOnGroundAnim (", "int PM_GetTurnAnim(")]
    parts += [function(npc, "void NPC_SetAnim(gentity_t\t*ent,int setAnimParts,int anim,int setAnimFlags)")]
    parts += [function(combat, s) for s in ("static qboolean STEFX_SplitCoopPlayerPair(", "static void STEFX_TraceDeathPose(",
              "void AddScore(", "static void DeathFX(", "void player_die(", "int CheckArmor (", "void G_Damage(") if s in combat]
    icarus = read("game/g_ICARUS.cpp")
    parts += [function(icarus, s) for s in ("void ICARUS_AssociateEnt(", "void ICARUS_FreeEnt(")]
    init = function(icarus, "void ICARUS_InitEnt(")
    assert "iICARUS->GetSequencer( ent->s.number )" in init and "ent->sequencer->GetTaskManager()" in init
    assert "ICARUS_InitEnt( ent );" in read("game/NPC_spawn.cpp")
    parts.append(function(read("game/g_functions.cpp"), "void GEntity_DieFunc("))
    assert "ent->e_DieFunc = dieF_player_die" in read("game/NPC_spawn.cpp")
    weapon = read("game/g_weapon.cpp")
    parts.append(weapon[weapon.index("#define\tPHASER_WEAK_DAMAGE"):].split("\n\n",1)[0])
    parts += [function(weapon, sig) for sig in ("qboolean LogAccuracyHit( gentity_t *target, gentity_t *attacker ) {", "void WP_FirePhaser(")]
    # Stock is compiled too, so a negative control does not reimplement its bug.
    for s in ("static bool NPC_IsScavengerBody(", "static int NPC_SettledScavengerPose(", "static bool NPC_DeathAnimFinished("):
        if s in npc:
            parts.append(function(npc, s))
    parts += [npc[npc.index("#define REMOVE_DISTANCE"):].split("\n\n", 1)[0]]
    parts += [function(npc, s) for s in ("int BodyRemovalPadTime(", "static void NPC_RemoveBodyEffect(",
              "static void DeadThink (", "void NPC_RemoveBody(", "void SetNPCGlobals(", "void NPC_Think (")]
    parts += [function(cg, s) for s in ("void CG_SetLerpFrameAnimation(", "qboolean CG_RunLerpFrame(",
              "void CG_ClearLerpFrame(", "void CG_ResetPlayerEntity(", "void CG_PlayerAnimation(")]
    snapshot = read("cgame/cg_snapshot.cpp")
    parts += [function(snapshot, s) for s in ("void CG_ResetEntity(", "void CG_SetEntityNextState(", "void CG_TransitionEntity(")]
    parts += ["static bool present(centity_t *cent) { do {", guard, "draw(cent); return true; } while(0); return false; }"]
    with tempfile.TemporaryDirectory() as td:
        out = Path(td)
        headers(src, out)
        (out/"paths.inc").write_text("\n".join(parts))
        exe = out/"probe"
        flags = ["-std=gnu++98", "-fpermissive", "-w", "-I"+td]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-no-pie"]
        subprocess.run([args.cxx, *flags, str(HERE/"campaign_corpse_pose_host.cpp"), "-o", str(exe)], check=True)
        for rows in samples(args.pak) + author_samples(args.authoring):
            subprocess.run([str(exe), str(int(args.expect_stock)), *(str(v) for row in rows for v in row)], check=True)
    print("PASS:", "stock negative control" if args.expect_stock else "settled corpse, script/special and visibility guards")


if __name__ == "__main__":
    main()
