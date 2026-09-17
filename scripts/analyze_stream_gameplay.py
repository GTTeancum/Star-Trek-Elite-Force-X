"""Conservative gameplay intervals anchored by observed complete FPS publications.

Requires one following complete gameplay record to cover publication/poll lag
at each interval end. Stalled time between good publications remains included.
This classifies scene intervals only; FPS comes from independent native counters.
"""
import argparse,json,re
from pathlib import Path


def analyze(rows):
    records={};anchors=[]
    for event in rows:
        if event.get('kind')!='fps_profiles':continue
        for entry in event['records']:
            line=entry['line']
            if not line.startswith('STEFX_HW_FPS_SAMPLE:'):raise ValueError('Invalid FPS publication')
            fields={k:int(v) for k,v in re.findall(r'\b(sample|frame|realtime|serverTime|gameplay|excludedChecks)=(\d+)\b',line)}
            if len(fields)!=6 or fields['sample']!=entry['publication']:raise ValueError('Incomplete or mismatched publication')
            n=fields['sample']
            if n in records and records[n]!=fields:raise ValueError('Conflicting publication')
            records[n]=fields
        # Older records recovered together have no separate host-time anchors.
        if event['records']:
            n=event['records'][-1]['publication']
            if not anchors or n>anchors[-1]['serial']:
                anchors.append({'serial':n,'wall':event['t']})
    intervals=[]
    for a,b in zip(anchors,anchors[1:]):
        if b['wall']<=a['wall']:continue
        covered=[records.get(n) for n in range(a['serial'],b['serial']+2)]
        if any(r is None or r['gameplay']!=1 or r['excludedChecks']!=0 for r in covered):continue
        before,after=records[a['serial']],records[b['serial']]
        if after['frame']<=before['frame'] or after['serverTime']<before['serverTime']:continue
        intervals.append(dict(wall_start=a['wall'],wall_end=b['wall'],
            server_start=before['serverTime'],server_end=after['serverTime'],
            first_publication=a['serial'],last_publication=b['serial'],
            following_publication=b['serial']+1))
    return {'gameplay_intervals':intervals,'publications':len(records),'anchors':len(anchors),
            'method':'Observed complete FPS publications with preceding-window and following-publication exclusion guards. No simulation FPS acceptance; real stalled time remains included.'}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('flight',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=analyze([json.loads(l) for l in a.flight.read_text().splitlines()]);result['source']=str(a.flight);a.output.write_text(json.dumps(result,indent=2));print('Confirmed gameplay intervals:',len(result['gameplay_intervals']))
