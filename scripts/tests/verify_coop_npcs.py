import struct
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from xemu_coop_npcs import read_npcs

class CensusTests(unittest.TestCase):
    def setUp(self):
        self.mem=bytearray(0x30000)
        self.base=0x11000; self.client=0x16000; self.npc=0x18000
        def put(address,fmt,*values):struct.pack_into('<'+fmt,self.mem,address,*values)
        self.put=put
        put(0x10000,'I',self.base);put(0x10140,'i',3);put(0x1020c,'i',10000)
        for num in range(3):put(self.base+num*1056,'i',num)
        self.addr=self.base+2*1056
        for off,val in [(232,self.client),(236,1),(540,100),(848,self.npc),(584,self.base)]:put(self.addr+off,'I',val)
        put(self.addr+304,'3f',1,2,3)
        put(self.client+1812,'2i',2,1);put(self.client+112,'I',5);put(self.client+116,'2i',21,22)
        put(self.npc+128,'3i',0,1,2);put(self.npc+556,'I',1);put(self.npc+576,'2b',-72,20)
    def read(self,a,n):return bytes(self.mem[a:a+n])
    def census(self,read=None):return read_npcs(read or self.read,0x10000,0x10100,0x10200)
    def test_live_borg_target_and_events(self):
        c=self.census();self.assertEqual(len(c['npcs']),1);n=c['npcs'][0]
        self.assertTrue(n['borg']);self.assertEqual(n['enemy'],0);self.assertEqual(n['health'],100)
        self.assertEqual(n['events'],(21,22));self.assertEqual(n['move'],(-72,20))
        self.assertEqual(c['rejected'],0)
    def test_dead_idle_and_missing_target_are_retained(self):
        self.put(self.addr+540,'i',-10);self.put(self.addr+584,'I',0);self.put(self.npc+556,'I',0)
        n=self.census()['npcs'][0];self.assertEqual(n['health'],-10);self.assertIsNone(n['enemy']);self.assertEqual(n['buttons'],0)
    def test_non_borg_is_not_classified_as_borg(self):
        self.put(self.client+1812,'i',1);self.assertFalse(self.census()['npcs'][0]['borg'])
    def test_missing_and_invalid_layout(self):
        self.assertIsNone(read_npcs(self.read,None,0x10100,0x10200))
        self.put(0x10140,'i',1025);self.assertIsNone(self.census())
        self.put(0x10140,'i',3);self.put(self.addr+848,'I',3)
        self.assertEqual(self.census()['rejected'],1)
    def test_event_race_and_entity_replacement_rejected(self):
        for target in [self.client+112,self.addr+232,self.addr+848]:
            def read(a,n):return b'X'*n if a==target else self.read(a,n)
            self.assertEqual(self.census(read)['rejected'],1)
    def test_short_memory_read(self):
        self.assertIsNone(self.census(lambda a,n:b'' if a==self.base else self.read(a,n)))
    def test_nonfinite_origin_rejected(self):
        self.put(self.addr+304,'f',float('nan'));self.assertEqual(self.census()['rejected'],1)

if __name__=='__main__':unittest.main()
