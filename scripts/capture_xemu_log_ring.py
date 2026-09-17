"""Read the guest's native log ring through QEMU's monitor, without input."""
import argparse
from pathlib import Path
import re
import ja_xemu_smoke as smoke


def snapshot(map_path, output, port=4475):
    mapping = Path(map_path).read_text(errors='replace')
    match = re.search(r'\s_g_SPXBLogMirror\s+([0-9a-fA-F]{8})\s', mapping)
    if not match:
        raise ValueError('Log mirror symbol missing')
    address = int(match[1], 16) - 0x3f0000
    scratch = Path(output).with_suffix('.ring.tmp').resolve()
    sock = smoke.monitor_connect(port)
    try:
        reply = smoke.monitor_cmd(sock, 'memsave 0x%x 0x8000 "%s"' % (address, scratch.as_posix()))
        if not scratch.exists() or scratch.stat().st_size != 32768:
            raise RuntimeError('Guest log snapshot failed: ' + reply[-500:])
        raw = scratch.read_bytes()
        # Retain complete lines only. The circular buffer can split one line
        # at either edge; later snapshots recover lines at those boundaries.
        lines = raw.replace(b'\0', b'\n').decode('ascii', 'replace').splitlines()[1:-1]
        existing = Path(output).read_text().splitlines() if Path(output).exists() else []
        seen = set(existing)
        with Path(output).open('a') as stream:
            for line in lines:
                if line and line not in seen:
                    stream.write(line + '\n')
                    seen.add(line)
    finally:
        sock.close()
        scratch.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--map', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--port', type=int, default=4475)
    args = parser.parse_args()
    snapshot(args.map, args.output, args.port)
