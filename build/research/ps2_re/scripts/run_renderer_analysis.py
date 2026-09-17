"""Run the focused renderer export against the saved R5900 project."""
from pathlib import Path
import json
import subprocess


root = Path(__file__).resolve().parents[4]
base = root / "build/research/ps2_re"
launch = json.loads((base / "receiver_launch.json").read_text())
args = launch["args"]
args.insert(2, "-Duser.name=smmel")
args = args[:args.index("-processor")] + [
    "-process", "SLUS_202.27",
    "-analysisTimeoutPerFile", "900",
    "-scriptPath", str(base / "scripts"),
    "-postScript", "ExtractRenderer.py", str(base / "ps2_renderer_decompilation.json")
]
with (base / "renderer_analysis.log").open("w") as log:
    result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT)
raise SystemExit(result.returncode)
