"""Host regression: exercise the SP save screen, command routing, metadata and wait policy.
The Xbox search, filesystem, signatures and game-world data are in-memory test doubles.
Run with a C++11 compiler in CXX, or c++ on PATH; --sanitize enables ASan/UBSan.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_text(encoding="latin-1")


def function(source, name):
    match = re.search(r"^(?:static )?(?:void|qboolean|int|bool|const char \*)\s*" +
                      re.escape(name) + r"\s*\([^;{}]*?\)(?:\s*/\*.*?\*/)?\s*\{",
                      source, re.MULTILINE)
    if not match:
        raise ValueError("Function not found: " + name)
    return source[match.start():source.index("\n}", match.end()) + 2]


def main():
    args = argparse.ArgumentParser()
    args.add_argument("--sanitize", action="store_true")
    sanitize = args.parse_args().sanitize
    menu = read("code/ui/ui_main.cpp")
    front = read("code/ui/ui_ef_frontend.cpp")
    server = read("code/server/sv_savegame.cpp")
    shared = read("code/game/q_shared.h")
    fixture = read("scripts/tests/fixtures/issue6_save_menu.cpp")
    savedata = menu[menu.rindex("typedef struct", 0, menu.index("} savedata_t;")):
                    menu.index("} savedata_t;") + len("} savedata_t;")]
    defs = []
    for source, names in [(shared, ("MAX_QPATH", "iSG_COMMENT_SIZE")),
                          (server, ("filepathlength", "iSG_MAPCMD_SIZE")),
                          (menu, ("MAX_SAVELOADFILES", "MAX_SAVELOADNAME", "LISTBUFSIZE"))]:
        for name in names:
            defs.append(re.search(r"^#define\s+" + name + r"\s+[^\s]+", source, re.M)[0])
    body = fixture.replace("// GAME_SIZES", "\n".join(defs) + "\n" + savedata)
    body = body.replace("// GAME_SAVE_COMMAND_HEADER", read("code/qcommon/stefx_save_menu.h"))
    ui_local = read("code/ui/ui_local.h")
    declarations = re.findall(r"^extern qboolean UI_EFSave_(?:CreateNew|Overwrite)\([^;]+;", ui_local, re.M)
    assert len(declarations) == 2
    body = body.replace("// GAME_SAVE_MENU_DECLARATIONS", "\n".join(declarations))
    groups = [
        ("// GAME_STRING_COPY", read("code/game/q_shared.cpp"), ["Q_strncpyz"]),
        ("// GAME_LEGACY_SCRIPT", read("code/ui/ui_shared.cpp"), ["Script_RunDeferred"]),
        ("// GAME_WAIT_POLICY", menu, ["UI_EFSave_HasLegacyWait"]),
        ("// GAME_SAVE_CODE", server, ["SG_XboxIsCheckpointAlias", "SG_Create", "SG_Open",
             "SG_StoreSaveGameComment", "SG_WriteComment", "SG_GetSaveGameComment", "SG_WriteSavegameInternal",
             "SG_WriteSavegame", "SV_SaveGameCommand", "SV_SaveGame_f", "SV_EFMenuSaveGame_f"]),
        ("// GAME_LEVEL_ENTRY", read("code/server/sv_client.cpp"), ["SV_ClientEnterWorld"]),
        ("// GAME_CHECKPOINT_TRIGGER", read("SP-Mod-Source-Code-master/game/g_target.cpp"), ["target_autosave_use"]),
        ("// GAME_REGISTER_COMMANDS", read("code/server/sv_ccmds.cpp"), ["SV_AddOperatorCommands"]),
        ("// GAME_MENU_CODE", menu, ["UI_SortSaveGames", "setBlockDisplayCvar", "ReadSaveDirectoryInternal",
             "UI_EFSave_ValidIndex", "UI_EFSave_Refresh", "UI_EFSave_Count", "UI_EFSave_Name",
             "UI_EFSave_Map", "UI_EFSave_Comment", "UI_EFSave_QueueWrite", "UI_EFSave_CreateNew", "UI_EFSave_Overwrite"]),
        ("// GAME_FRONTEND_CODE", front, ["EFFe_SetScreen", "EFFe_HandleSaveGameKey"]),
    ]
    for marker, source, names in groups:
        body = body.replace(marker, "\n".join(function(source, name) for name in names))
    with tempfile.TemporaryDirectory(prefix="ef-save-menu-") as temp:
        cpp = Path(temp, "save.cpp")
        cpp.write_text(body)
        binary = Path(temp, "save")
        cmd = [os.environ.get("CXX", "c++"), "-std=c++11", "-Wno-multichar", "-g"]
        if sanitize:
            cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(cmd + [str(cpp), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    main()
