import copy
import sys
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from analyze_coop_combat import analyze


def row(t,seq,events,client=100):
    player=dict(client=client,entity=200,command_time=t*100,event_sequence=seq,
                events=events,ammo=[0,20,0,0],weapon=2)
    other=dict(player,client=300,entity=400,events=[1,2])
    return dict(kind='coop_players',t=t,camera=False,players=[player,other])


class Combat(unittest.TestCase):
    def test_new_events_overrun_and_respawn(self):
        rows=[row(1,0,[0,0]),row(2,1,[21,0]),row(3,1,[21,0]),
              row(4,4,[22,21]),row(5,5,[21,21],client=101)]
        result=analyze(rows,[dict(wall_start=0,wall_end=6)])
        a,b=result['players']
        self.assertEqual((a['primary'],a['alternate'],a['missed_events'],a['resets']),(2,1,1,1))
        self.assertEqual(b['observed_firing_events'],0)
        self.assertFalse(result['both_players_observed_firing'])
        self.assertFalse(result['full_workload_qualified'])

    def test_wrap_and_no_history_leak(self):
        rows=[row(1,0xffffffff,[0,0]),row(2,0,[0,21])]
        self.assertEqual(analyze(rows,[dict(wall_start=1,wall_end=3)])['players'][0]['primary'],1)
        self.assertEqual(analyze(rows,[dict(wall_start=2,wall_end=3)])['players'][0]['primary'],0)
        for r in rows:r['camera']=True
        with self.assertRaises(ValueError):analyze(rows,[dict(wall_start=1,wall_end=3)])

    def test_legacy_and_bad_layout_rejected(self):
        rows=[row(1,0,[0,0])]
        for mutation in ('missing','wrong'):
            changed=copy.deepcopy(rows)
            if mutation=='missing':del changed[0]['players'][0]['ammo']
            else:changed[0]['players'][0]['events']=[21]
            with self.assertRaises(ValueError):analyze(changed,[dict(wall_start=0,wall_end=2)])


if __name__=='__main__':unittest.main()
