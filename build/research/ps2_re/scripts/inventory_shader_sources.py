"""Package-only shader byte accounting; runtime enumeration/order remains unverified."""
from pathlib import Path
import json,zipfile
base=Path('build/research/ps2_re')
resolved={}
# Model known package precedence only; loose files and runtime MAX_SHADER_FILES are not applied.
for name in ['PAK0.PK3','PAK1.PK3','PAK2.PK3','PAK3.PK3','xbox0.pk3']:
    with zipfile.ZipFile(Path('build/release/BaseEF')/name) as z:
        for entry in z.infolist():
            n=entry.filename.lower()
            if n.startswith(('scripts/','shaders/')) and n.endswith('.shader'):
                resolved[n]={'path':n,'package':name,'bytes':entry.file_size}
rows=list(resolved.values())
report={'scope':'Package union with later listed packages overriding duplicate paths; runtime and loose-file precedence unverified','files':len(rows),'raw_bytes':sum(r['bytes'] for r in rows),'largest_file_bytes':max(r['bytes'] for r in rows),'rows':sorted(rows,key=lambda r:r['path'])}
(base/'xbox_shader_source_inventory.json').write_text(json.dumps(report,indent=2))
print({k:v for k,v in report.items() if k!='rows'})
