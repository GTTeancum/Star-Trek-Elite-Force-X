"""Recover active renderer counters from a retained final XEMU RAM snapshot.

Validate address translation against independent native memsave outputs before
adding the recovered view evidence to a probe summary.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
from run_sp_perf_probe import verify_mode

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--name', required=True)
p.add_argument('--symbols', type=Path, required=True)
a=p.parse_args()
if not re.fullmatch('[A-Za-z0-9_]+', a.name):
    p.error('Invalid probe name')
out=Path(__file__).resolve().parents[1]/'build/research/letterbox_perf'
ram_path=out/(a.name+'_retained_ram.bin')
ram=ram_path.read_bytes()
registers=sorted(out.glob(a.name+'_*_final_registers.txt'))[-1]
reg_text=registers.read_text()
cr3=int(re.search(r'CR3=([0-9a-fA-F]+)',reg_text)[1],16)
cr4=int(re.search(r'CR4=([0-9a-fA-F]+)',reg_text)[1],16)
assert not cr4 & 0x20, 'PAE translation is not implemented'
symbols=a.symbols.read_text()

def u32(offset):
    return struct.unpack_from('<I',ram,offset)[0]

def read_symbol(name, count):
    match=re.search(re.escape(name)+r'\s+([0-9A-Fa-f]{8})\s', symbols)
    assert match, name
    address=int(match[1],16)-0x3f0000
    result=bytearray()
    while len(result)<count:
        va=address+len(result)
        pde=u32((cr3&0xfffff000)+4*(va>>22))
        assert pde&1, 'Missing page directory entry'
        if pde&0x80:
            assert cr4&0x10
            phys=(pde&0xffc00000)+(va&0x3fffff)
        else:
            pte=u32((pde&0xfffff000)+4*((va>>12)&1023))
            assert pte&1, 'Missing page table entry'
            phys=(pte&0xfffff000)+(va&4095)
        size=min(count-len(result),4096-(va&4095))
        assert phys+size<=len(ram)
        result.extend(ram[phys:phys+size])
    return bytes(result)

anchors=[]
for symbol,label,count in [('_g_SPXBMapLast','loaded_map',64),
        ('?g_SPXBLoadTimes@@3PAIA','load_times',32),
        ('_g_SPXBSplitP2RefdefValid','p2_refdef_valid',4)]:
    native=sorted(out.glob(a.name+'_*_final_'+label+'.bin'))[-1]
    assert read_symbol(symbol,count)==native.read_bytes()[:count], label+' anchor mismatch'
    anchors.append(native.name)
evidence={}
for symbol,label in [('_g_SPXBHMSplitRenderDrawDelta','hm_draws'),
                     ('_g_SPXBHMSplitRenderDoneSerial','hm_done')]:
    evidence[label]=list(struct.unpack('<4I',read_symbol(symbol,16)))
proof={'ram_sha256':hashlib.sha256(ram).hexdigest(), 'ram_bytes':len(ram),
       'registers':registers.name, 'cr3':cr3, 'anchors_matched':anchors,
       'symbols_sha256':hashlib.sha256(a.symbols.read_bytes()).hexdigest(), 'evidence':evidence}
proof_path=out/(a.name+'.view_recovery.json')
proof_path.write_text(json.dumps(proof,indent=2))
summary_path=out/(a.name+'.summary.json')
summary=json.loads(summary_path.read_text())
summary['original_mode_verified']=summary.get('mode_verified')
summary['split_evidence'].update(evidence)
summary['mode_verified']=verify_mode(summary['mode'],set(summary['observed_players']),summary['split_evidence'])
summary['view_evidence_recovery']=proof_path.name
summary['measured_map_window_pass']=bool(summary['mode_verified']
    and not summary.get('diagnostic_only') and not summary['visual_only']
    and summary['loaded_map']==summary['map'] and len(summary['fps'])>=2
    and min(summary['fps'])>=summary['target_fps'])
summary_path.write_text(json.dumps(summary,indent=2))
print(json.dumps(proof,indent=2))
