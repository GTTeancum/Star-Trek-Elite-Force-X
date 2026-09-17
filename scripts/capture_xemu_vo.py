"""Capture this task's XEMU APU output without host audio or UI control.

The hook is specific to the verified local XEMU executable. It reads the
48-kHz stereo S16LE monitor frame immediately before XEMU clears it. It does
not alter guest state, input, DSP output, or ICARUS execution.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

import frida
import pefile
import ja_xemu_smoke as smoke
from capture_xemu_log_ring import snapshot as snapshot_log_ring


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--exe", type=Path, required=True)
    p.add_argument("--iso", type=Path, required=True)
    p.add_argument("--config", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--seconds", type=int, default=340)
    p.add_argument("--port", type=int, default=4475)
    p.add_argument("--extract-log", action="store_true")
    p.add_argument("--map", type=Path, help="Matching XBE map for periodic native log-ring capture")
    a = p.parse_args()
    exe = a.exe.resolve()
    exe_hash = hashlib.sha256(exe.read_bytes()).hexdigest()
    if exe_hash != "7da537938ea2ac09f894186ba793c9ae51dff37c95903b0002273b8d363818b7":
        raise RuntimeError("Unsupported XEMU executable hash; revalidate the capture hook")
    image = pefile.PE(str(exe))
    # Disassembly matched mcpx_apu_monitor_frame from commit fc24584c:
    # SDL_PutAudioStreamData(stream, frame_buf, 0x400); memset(frame_buf,0,0x400).
    hook_rva = 0x292077
    expected = bytes.fromhex("41 b8 00 04 00 00 31 d2 48 89 f1")
    if image.get_data(hook_rva, len(expected)) != expected:
        raise RuntimeError("Unsupported XEMU: APU capture instruction signature differs")
    a.output.parent.mkdir(parents=True, exist_ok=True)
    log_path = a.output.with_suffix(".xemu.log")
    metadata = {"exe": str(exe), "exe_sha256": exe_hash,
                "hook_rva": hex(hook_rva), "config": a.config.read_text(),
                "format": "s16le", "rate": 48000,
                "channels": 2, "bytes": 0, "monitor_point_frames": {}, "errors": []}
    if a.map:
        metadata['map_sha256'] = hashlib.sha256(a.map.read_bytes()).hexdigest()
        a.output.with_suffix('.runtime.log').write_text('')
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    environment = os.environ.copy()
    for key in ["SDL_AUDIO_DRIVER", "SDL_AUDIO_DISK_OUTPUT_FILE", "QEMU_AUDIO_DRV", "QEMU_WAV_PATH"]:
        environment.pop(key, None)
    proc = None
    session = None
    started = time.monotonic()
    with log_path.open("w", encoding="utf-8") as log, a.output.open("wb") as output:
        try:
            proc = subprocess.Popen([str(exe), "-dvd_path", str(a.iso.resolve()),
                "-config_path", str(a.config.resolve()), "-monitor",
                f"tcp:127.0.0.1:{a.port},server,nowait"], cwd=exe.parent,
                stdout=log, stderr=subprocess.STDOUT, env=environment,
                startupinfo=startup, creationflags=subprocess.BELOW_NORMAL_PRIORITY_CLASS)
            metadata["pid"] = proc.pid
            session = frida.attach(proc.pid)
            js = """
                const base = Process.mainModule.base;
                Interceptor.attach(base.add(0x292077), {
                    onEnter() {
                        send({kind: 'audio', point: this.context.rsi.sub(4).readU32()}, this.context.rsi.readByteArray(1024));
                    }
                });
                send({kind: 'ready'});
            """
            script = session.create_script(js)
            def receive(message, data):
                if message.get("type") == "send" and message["payload"].get("kind") == "audio":
                    output.write(data)
                    point = str(message["payload"]["point"])
                    counts = metadata["monitor_point_frames"]
                    counts[point] = counts.get(point, 0) + 1
                    metadata["bytes"] += len(data)
                elif message.get("type") == "error":
                    metadata["errors"].append(message)
            script.on("message", receive)
            script.load()
            print(f"capture pid={proc.pid} output={a.output}", flush=True)
            next_report = time.monotonic() + 30
            while proc.poll() is None and time.monotonic() - started < a.seconds:
                time.sleep(0.25)
                boot_log = log_path.read_text(encoding="utf-8", errors="replace")
                if "Could not open" in boot_log or "Failed to open DVD" in boot_log:
                    metadata["errors"].append("Emulator failed to open a boot resource; capture invalid")
                    break
                if time.monotonic() >= next_report:
                    print(f"elapsed={time.monotonic()-started:.1f} audio_seconds={metadata['bytes']/192000:.1f}", flush=True)
                    if a.map:
                        snapshot_log_ring(a.map, a.output.with_suffix('.runtime.log'), a.port)
                    next_report += 30
            metadata["exit_before_stop"] = proc.poll()
            if proc.poll() is not None:
                metadata["errors"].append("Emulator exited before capture duration completed")
            if a.extract_log and proc.poll() is None and not metadata["errors"]:
                sock = smoke.monitor_connect(a.port)
                try:
                    smoke.monitor_cmd(sock, "stop")
                    registers = smoke.monitor_cmd(sock, "info registers")
                    a.output.with_suffix(".registers.txt").write_text(registers)
                    smoke.extract_xblog_profiles_from_physical_memory(
                        sock, str(a.output.with_suffix("")), print)
                finally:
                    sock.close()
        finally:
            if proc and proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=15)
            if session:
                try:
                    session.detach()
                except frida.InvalidOperationError:
                    pass  # The emulator may already have exited.
    metadata["wall_seconds"] = time.monotonic() - started
    a.output.with_suffix(".json").write_text(json.dumps(metadata, indent=2)+"\n")
    if not metadata["bytes"] or metadata["errors"]:
        raise RuntimeError("Audio capture failed; inspect capture metadata")
    print(json.dumps(metadata), flush=True)


if __name__ == "__main__":
    main()
