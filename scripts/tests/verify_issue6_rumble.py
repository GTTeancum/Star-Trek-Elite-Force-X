"""Host regression for the campaign's physical motor clock and asynchronous Xbox output.

Uses the game's actual CG_FireWeapon, FF_Play, rumble mixer, controller lifecycle
and IN_Frame functions. The unavailable Xbox driver, device clocks, UI input state
and unrelated input/sound work are stand-ins. Run with: python3 scripts/tests/verify_issue6_rumble.py
"""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(text, name):
    start = re.search(r"^(?:static )?(?:void|int|qboolean)\s+" + name + r"\s*\(", text, re.M).start()
    return text[start:text.index("\n}", start) + 2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT, help="use another source tree, e.g. v0.9.4")
    parser.add_argument("--case", choices=("clock", "expiry", "off", "menu", "pause", "options",
                                           "coalesce", "retry", "hotplug", "split"))
    parser.add_argument("--sanitize", action="store_true", help="also instrument the host simulation")
    args = parser.parse_args()
    root = args.source.resolve()
    read = lambda path: (root / path).read_text(encoding="latin-1")
    compiler = os.environ.get("CXX", "c++")

    xbox = read("code/win32/win_input_xbox.cpp")
    host = xbox[xbox.index("struct inputstate_t"):xbox.index("static inline float _joyAxisConvert")]
    declarations = re.findall(r'^extern "C" volatile unsigned int (g_SPXB\w+);', xbox, re.M)
    host = "\n".join(f"volatile unsigned int {name};" for name in declarations) + "\n" + host
    host += "\n" + function(xbox, "IN_Frame")
    mixer = re.sub(r'^#include[^\n]*', '', read("code/win32/win_input_rumble.cpp"), flags=re.M)
    effects = function(read("code/ff/ff_console.cpp"), "FF_Play")
    weapon = function(read("SP-Mod-Source-Code-master/cgame/cg_weapons.cpp"), "CG_FireWeapon")
    split = function(read("code/client/cl_input.cpp"), "STEFX_SplitHolomatchAttackRumble")
    frontend = read("code/ui/ui_ef_frontend.cpp")
    options = "\n".join(function(frontend, "EFFe_" + name) for name in (
        "IsAcceptKey", "IsBackKey", "IsLeftKey", "IsRightKey", "LoadControllerState", "HandleControllerKey"))
    options += "\n" + function(read("code/win32/win_input_console.cpp"), "IN_STEFX_IsFrontendButton")
    syscalls = read("SP-Mod-Source-Code-master/cgame/cg_syscalls.cpp")
    ff_dispatch = [function(syscalls, f"cgi_FF_{verb}FX") for verb in ("Start", "Ensure")]
    client = read("code/client/cl_cgame.cpp")
    cases = client[client.index("\tcase STEFX_CG_FF_STARTFX:"):client.index("\tcase STEFX_CG_FF_STOPFX:")]
    # Run the original syscall cases; unrelated entries in the VM table are not needed here.
    bridge = ('void FF_EnsurePlaying(ffFX_e) {}\nint mockSyscall(int op, int arg) {'
              'int args[2] = {op, arg}; switch (op) {\n' + cases +
              '\ndefault: assert(false); return 0; }}\n#define syscall mockSyscall\n' +
              '\n'.join(ff_dispatch) + '\n#undef syscall\n')
    cpp = (ROOT / "scripts/tests/fixtures/issue6_rumble.cpp").read_text()
    cpp = cpp.replace("// GAME_UNDER_TEST", host + "\n" + effects +
                      "\n#define cg hostCg\n" + mixer + "\n#undef cg\n" + bridge +
                      "\n#define cg efCg\n" + weapon + "\n#undef cg\n" + split + "\n" + options)

    # Check the source-level ABI diagnosis on an actual 32-bit target, not with
    # the host's 64-bit pointers. Only prefixes are needed to locate byte 44.
    def cg_prefix(path, last):
        text = read(path)
        block = text[text.index("// all cg.stepTime"):]
        block = block.split("typedef struct {", 1)[1]
        return block[:re.search(r"^\s*" + last + ";", block, re.M).end()]
    ja = cg_prefix("code/cgame/cg_local.h", r"int\s+time\s*")
    ef = cg_prefix("SP-Mod-Source-Code-master/cgame/cg_local.h", r"snapshot_t\s+activeSnapshots\[2\]")
    snapshot = read("SP-Mod-Source-Code-master/cgame/cg_public.h").split("typedef struct {", 1)[1]
    snapshot = snapshot[:snapshot.index("areamask[MAX_MAP_AREA_BYTES];") + len("areamask[MAX_MAP_AREA_BYTES];")]
    area_size = re.search(r"#define\s+MAX_MAP_AREA_BYTES\s+\d+", read("SP-Mod-Source-Code-master/game/q_shared.h")).group()
    layout = ("typedef int qboolean; typedef unsigned char byte;\n" + area_size +
              "\nstruct snapshot_t {" + snapshot + "\n};\nstruct JA {" + ja +
              "\n};\nstruct EF {" + ef + "\n};\n" +
              'static_assert(sizeof(void*) == 4, "Xbox pointer size");\n' +
              'static_assert(__builtin_offsetof(JA,time) == 44, "JA time offset");\n' +
              'static_assert(__builtin_offsetof(EF, activeSnapshots[0].areamask[4]) == 44, "EF fields at 44");\n')

    with tempfile.TemporaryDirectory(prefix="ef-rumble-") as out:
        out = Path(out)
        (out / "layout.cpp").write_text(layout)
        subprocess.run([compiler, "-m32", "-ffreestanding", "-std=c++11", "-c", str(out / "layout.cpp"),
                        "-o", str(out / "layout.o")], check=True)
        print("PASS Xbox 32-bit source layout: JA cg.time aliases EF snapshot[0].areamask[4..7].", flush=True)
        (out / "test.cpp").write_text(cpp)
        for mode in ("sp", "spmp"):
            group = ("split",) if mode == "spmp" else ("clock", "expiry", "off", "menu", "pause", "options",
                                                         "coalesce", "retry", "hotplug")
            tests = [case for case in group if not args.case or case == args.case]
            if not tests:
                continue
            cmd = [compiler, "-std=c++98", "-g", "-O1", "-I", str(root)]
            if mode == "spmp":
                cmd.append("-DSTEFX_SP_HOSTED_MP")
            if args.sanitize:
                cmd += ["-fno-omit-frame-pointer", "-fsanitize=address,undefined", "-no-pie"]
            subprocess.run(cmd + [str(out / "test.cpp"), "-o", str(out / mode)], check=True)
            for case in tests:
                subprocess.run([str(out / mode), case], check=True)


if __name__ == "__main__":
    main()
