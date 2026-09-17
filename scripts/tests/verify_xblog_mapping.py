"""Check live mapping validation in the actual native telemetry attachment helper."""
import ast,os,re,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
tree=ast.parse((ROOT/'scripts/ja_xemu_smoke.py').read_text(encoding='utf-8'))
selected=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in ('parse_monitor_words','probe_xblog_physical_addr')]
class MappingTests(unittest.TestCase):
 def run_probe(self,gpa,words=None,boot=0x442804,fail=False):
  calls=[];logs=[]
  def command(sock,cmd,delay):
   calls.append(cmd)
   if fail:raise OSError('unavailable')
   if gpa is not None and cmd=='xp/8wx 0x%08x'%(gpa-4) and words is not None:
    return '0x%08x: '%(gpa-4)+' '.join('0x%08x'%w for w in words)
   return ''
  with tempfile.TemporaryDirectory() as d:
   env=dict(os=os,re=re,OUT_DIR=d,monitor_gva_to_gpa=lambda sock,addr:gpa,monitor_cmd=command,monitor_pmemsave=lambda *args:False)
   exec(compile(ast.Module(body=selected,type_ignores=[]),'<actual probe>','exec'),env)
   result=env['probe_xblog_physical_addr'](None,boot,0x284000,logs.append)
  return result,calls,logs
 def test_current_mapping_avoids_scans(self):
  r,c,l=self.run_probe(0x220804,[0x53504546,5,0,0,0x48424653]);self.assertEqual(r,0x220804);self.assertEqual(len(c),1);self.assertIn('mapped va=',l[0])
 def test_nontraditional_mapping_above_virtual(self):
  r,c,l=self.run_probe(0x1200804,[0x53504546,5,0,0,0x48424653]);self.assertEqual(r,0x1200804);self.assertEqual(len(c),1)
 def test_wrong_first_sentinel_rejected(self):self.assertIsNone(self.run_probe(0x220804,[0,5,0,0,0x48424653])[0])
 def test_wrong_second_sentinel_rejected(self):self.assertIsNone(self.run_probe(0x220804,[0x53504546,5,0,0,0])[0])
 def test_short_read_rejected(self):self.assertIsNone(self.run_probe(0x220804,[0x53504546,5])[0])
 def test_missing_mapping_falls_back(self):self.assertIsNone(self.run_probe(None)[0])
 def test_outside_ram_rejected(self):self.assertIsNone(self.run_probe(0x4000000,[0x53504546,5,0,0,0x48424653])[0])
 def test_monitor_failure_rejected(self):self.assertIsNone(self.run_probe(0x220804,fail=True)[0])
 def test_cross_page_header_not_flat_read(self):
  r,c,l=self.run_probe(0x220ffc,[0x53504546,5,0,0,0x48424653],boot=0x442ffc);self.assertIsNone(r);self.assertNotIn('xp/8wx 0x00220ff8',c)
if __name__=='__main__':unittest.main()
