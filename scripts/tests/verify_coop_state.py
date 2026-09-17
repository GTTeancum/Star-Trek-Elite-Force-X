import struct
import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from xemu_coop_state import read_players
ram=bytearray(0x30000)
struct.pack_into('<I',ram,0x100,0x10000)
struct.pack_into('<I',ram,0x104,7)
for number,client in ((0,0x20000),(7,0x22000)):
    entity=0x10000+number*1056
    struct.pack_into('<i',ram,entity,number)
    struct.pack_into('<I',ram,entity+232,client)
    struct.pack_into('<i',ram,entity+540,100)
    struct.pack_into('<i',ram,client,5000)
    struct.pack_into('<3f',ram,client+20,number,10,20)
    struct.pack_into('<3f',ram,client+156,0,90,0)
    struct.pack_into('<i',ram,client+188,100)
    struct.pack_into('<I2i',ram,client+112,3,21,22)
    struct.pack_into('<2i',ram,client+148,2,1)
    struct.pack_into('<4i',ram,client+380,10,20,30,40)
def read(a,n):return bytes(ram[a:a+n])
result=read_players(read,0x100,0x104)
assert [p['number'] for p in result]==[0,7]
assert result[1]['origin']==(7,10,20) and result[0]['command_time']==5000
assert result[1]['event_sequence']==3 and result[1]['events']==(21,22)
assert result[1]['weapon']==2 and result[1]['ammo']==(10,20,30,40)
assert read_players(lambda a,n:struct.pack('<I',4) if a==0x20000+112 else read(a,n),0x100,0x104) is None
assert read_players(lambda a,n:read(a,192) if n==396 else read(a,n),0x100,0x104) is None
assert read_players(read,None,0x104) is None
assert read_players(lambda a,n:b'',0x100,0x104) is None
struct.pack_into('<I',ram,0x104,1024)
assert read_players(read,0x100,0x104) is None
struct.pack_into('<I',ram,0x104,7)
struct.pack_into('<i',ram,0x10000+7*1056,8)
assert read_players(read,0x100,0x104) is None
struct.pack_into('<i',ram,0x10000+7*1056,7)
struct.pack_into('<f',ram,0x22000+20,float('nan'))
assert read_players(read,0x100,0x104) is None
struct.pack_into('<f',ram,0x22000+20,7)
assert read_players(lambda a,n:b'\0'*n if a==0x10000+232 else read(a,n),0x100,0x104) is None
print('Co-op state: both player identities/positions, missing reads, invalid slot, nonfinite position and replaced pointer passed')
