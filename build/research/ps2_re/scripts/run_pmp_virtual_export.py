from pathlib import Path
import json,subprocess
root=Path(__file__).resolve().parents[4]
base=root/'build/research/ps2_re'
args=json.loads((base/'receiver_launch.json').read_text())['args']
args=args[:args.index('-processor')]+['-process','SLUS_202.27','-noanalysis','-scriptPath',str(base/'scripts'),'-postScript','ExtractPMPVirtuals.py',str(base/'pmp_virtual_decompilation.json')]
with (base/'pmp_virtual_export.log').open('w') as log:
    result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
raise SystemExit(result.returncode)
