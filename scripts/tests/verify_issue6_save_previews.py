"""Test the native pause/save preview code with in-memory platform calls."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
args = argparse.ArgumentParser()
args.add_argument("--sanitize", action="store_true")
sanitize = args.parse_args().sanitize
fixture = root / "scripts/tests/fixtures/issue6_save_previews.cpp"
with tempfile.TemporaryDirectory(prefix="ef-save-preview-") as temp:
    args = [os.environ.get("CXX", "c++"), "-std=c++11", "-O1", "-g", "-Wall", "-Wextra",
            "-Werror", str(fixture), "-o", str(Path(temp) / "preview")]
    if sanitize:
        args.extend(["-fsanitize=address,undefined", "-fno-omit-frame-pointer"])
    subprocess.run(args, check=True)
    subprocess.run([str(Path(temp) / "preview")], check=True)
