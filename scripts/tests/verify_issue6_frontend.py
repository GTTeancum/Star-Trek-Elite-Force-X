"""Run the production campaign frontend and button routes with host draw/save spies.

Synthetic glyph metrics vary. This is not Xbox/XDK, an emulator, actual device
feedback, or validation of a platform signature. No game assets required.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
FIX = ROOT / 'scripts/tests/fixtures'


def read(name):
    return (ROOT / name).read_text(encoding='latin-1')


def body(s, name):
    match = re.search(r'^(?:static )?(?:void|qboolean) ' + name + r'\(', s, re.M)
    return s[match.start():s.index('\n}', match.start()) + 2]


def without_includes(s):
    return re.sub(r'^#include [^\n]+', '', s, flags=re.M)


def main():
    pause = read('code/ui/ui_ef_pause.cpp')
    pause_types = pause[pause.index('enum {\n\tEF_MBT_SAVEGAME'):pause.index('} efPauseButton_t;') + len('} efPauseButton_t;')]
    buttons = pause[pause.index('static efPauseButton_t s_buttons'):].split('\n};', 1)[0] + '\n};'
    pause_path = '\n'.join(body(pause, k) for k in (
        'EFPause_OpenEFQmenu', 'EFPause_ResumeGame', 'EFPause_ActivateButton',
        'UI_EFPauseMenu_IsActive', 'UI_EFPauseMenu_Deactivate', 'UI_EFPauseMenu_Open', 'UI_EFPauseMenu_KeyEvent'))
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        includes = {
            'platform.inc': (FIX / 'issue1_preferences.cpp').read_text().split('// SETTINGS_CODE')[0],
            'settings.inc': without_includes(read('code/qcommon/xb_settings.h') + '\n' + read('code/qcommon/xb_settings.cpp')),
            'frontend.inc': without_includes(read('code/ui/ui_ef_frontend.cpp')),
            'pause_route.inc': pause_types + '\n' + buttons + '\n' + pause_path,
            'qmenu_command.inc': body(read('code/ui/ui_ef_qmenu.cpp'), 'UI_EFQmenu_ConsoleCommand'),
            'key_route.inc': body(read('code/ui/ui_main.cpp'), '_UI_KeyEvent'),
        }
        for name, data in includes.items():
            (out / name).write_text(data)
        cxx = os.environ.get('CXX', 'c++')
        # Header-only ILP32 check: no 32-bit host runtime/XDK installation is needed.
        (out / 'xtl.h').write_text('struct XCALCSIG_SIGNATURE;\n')
        layout = out / 'layout.cpp'
        layout.write_text('#include "qcommon/xb_settings.h"\n'
                          'static_assert(sizeof(XBSettings)==152, "v0.9.4 settings payload");\n'
                          'static_assert(__builtin_offsetof(XBSettings,rumble)==32, "rumble offset");\n'
                          'static_assert(__builtin_offsetof(XBSettings,textureFilter)==148, "calibration prefix");\n')
        subprocess.run([cxx, '-std=c++11', '-m32', '-ffreestanding', '-fsyntax-only',
                        '-I', str(out), '-I', str(ROOT / 'code'), str(layout)], check=True)
        for personality in ('campaign', 'holomatch'):
            exe = out / personality
            flags = ['-DSTEFX_SP_HOSTED_MP'] if personality == 'holomatch' else []
            subprocess.run([cxx, '-std=c++11', '-O1',
                            '-fsanitize=undefined', '-fno-sanitize-recover=all',
                            '-D_XBOX', '-DSTEFX_ELITE_FORCE_SP', *flags,
                            '-I', str(out), '-I', str(ROOT / 'code'), str(FIX / 'issue6_frontend.cpp'), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    main()
