"""Verify that final VV packaging replaces assets and both executable payloads."""
from pathlib import Path
import json
import struct
import sys
import tempfile
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build_virtual_voyager_test_iso import build
from verify_beta_iso_components import locate

with tempfile.TemporaryDirectory(prefix='stefx_vv_iso_') as temp:
    d=Path(temp); source=d/'source.iso'; package=d/'xbox0.pk3'
    default=d/'default.xbe'; mp=d/'efmp.xbe'
    data=bytearray(38*2048)
    data[32*2048:32*2048+28]=b'MICROSOFT*XBOX*MEDIA'+struct.pack('<II',33,72)
    root=struct.pack('<HHIIBB',7,12,35,8,0,11)+b'default.xbe'+b'\0'*3
    root+=struct.pack('<HHIIBB',0,0,34,24,16,6)+b'BaseEF'
    root+=struct.pack('<HHIIBB',0,0,36,8,0,8)+b'efmp.xbe'+b'\0'*2
    data[33*2048:33*2048+len(root)]=root
    child=struct.pack('<HHIIBB',0,0,37,7,0,9)+b'xbox0.pk3'+b'\0'
    data[34*2048:34*2048+len(child)]=child
    data[35*2048:35*2048+8]=b'old_sp__'
    data[36*2048:36*2048+8]=b'old_mp__'
    data[37*2048:37*2048+7]=b'old_pak'
    source.write_bytes(data); package.write_bytes(b'new_assets'*777)
    default.write_bytes(b'XBEH'+b'new_sp'*1000); mp.write_bytes(b'XBEH'+b'new_mp'*999)
    output=d/'ready.iso'
    report=build(source,output,package,default,mp)
    assert source.read_bytes()==data
    assert output.stat().st_size%2048==0
    with output.open('rb') as f:
        for path,expected in [('BaseEF/xbox0.pk3',package),('default.xbe',default),('efmp.xbe',mp)]:
            offset,size=locate(f,path); f.seek(offset); assert f.read(size)==expected.read_bytes(),path
    assert all(x['verified'] for x in report['executables'].values())
    assert len(report['executables'])==2
    assert json.loads(output.with_suffix('.manifest.json').read_text())==report
    try: build(source,d/'bad.iso',package,default,None)
    except ValueError: pass
    else: raise AssertionError('Unpaired executable accepted')
print('PASS: asset and paired executable identity, normal lookup, aligned sectors, preserved source, manifest')
