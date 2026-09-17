"""Receiver-only R5900 recovery; preserves the interrupted project and outputs."""
from pathlib import Path
import subprocess
import json
import hashlib

root = Path(__file__).resolve().parents[4]
base = root / 'build/research/ps2_re'
java = base / 'tooling/jdk-21.0.10+7/bin/java.exe'
utility = root.parent.parent / 'ghidra_11.0.1_PUBLIC/Ghidra/Framework/Utility/lib/Utility.jar'
# The repository is under Programming/!archived; Ghidra is under Programming.
project = 'stefx_ps2_r5900_receiver'
output = base / 'receiver_loader_decompilation.json'
if (base / 'ghidra' / (project + '.gpr')).exists() or output.exists():
    raise SystemExit('Receiver project/output exists; inspect before another import')
for path in (java, utility, base / 'SLUS_202.27'):
    if not path.is_file():
        raise SystemExit('Missing dependency: ' + str(path))
elf_hash = hashlib.sha256((base / 'SLUS_202.27').read_bytes()).hexdigest()
assert elf_hash == '44bbcf612c8b90835c8510da8fc4bb7e7e222bc33d5950f93218f8622a4b10af'
args = [str(java), '-Xmx2G', '-Duser.home=' + str(base / 'tool_home'),
        '-XX:ParallelGCThreads=2', '-XX:CICompilerCount=2',
        '-Djava.system.class.loader=ghidra.GhidraClassLoader', '-Dfile.encoding=UTF-8',
        '-cp', str(utility), 'ghidra.Ghidra', 'ghidra.app.util.headless.AnalyzeHeadless',
        str(base / 'ghidra'), project, '-processor', 'r5900:LE:32:default',
        '-cspec', 'default', '-import', str(base / 'SLUS_202.27'),
        '-analysisTimeoutPerFile', '300', '-scriptPath', str(base / 'scripts'),
        '-postScript', 'ExtractLoaderReceiver.py', str(base / 're_targets.json'), str(output)]
(base / 'receiver_launch.json').write_text(json.dumps({'args': args, 'elf_sha256': elf_hash}, indent=2))
with (base / 'receiver_ghidra.log').open('w') as log:
    result = subprocess.run(args, stdout=log, stderr=subprocess.STDOUT)
raise SystemExit(result.returncode)
