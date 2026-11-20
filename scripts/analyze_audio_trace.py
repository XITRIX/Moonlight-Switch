"""Summarize LL24 audio CSV; timestamps are client observations, not wire time."""
from collections import Counter, defaultdict, deque
from pathlib import Path
import argparse, json, re

def distribution(values):
    values=sorted(values)
    if not values:return {'count':0}
    return {'count':len(values),'min_us':values[0],
            'median_us':values[len(values)//2],
            'p95_us':values[min(len(values)-1,int(len(values)*.95))],
            'max_us':values[-1]}

def analyze(text):
    sessions=[]; current=[]; lost=0; malformed=0
    for line in text.splitlines():
        if re.match(r'# LL\d+ AUDIO session_begin',line):
            if current:sessions.append(current)
            current=[]
        if line.startswith('# session_end'):
            match=re.search(r'lost_diagnostic_records=(\d+)',line)
            if match:lost+=int(match[1])
        if not line or line.startswith('#'):continue
        try:
            row=tuple(map(int,line.split(',')))
            if len(row)!=7:raise ValueError()
            current.append(row)
        except ValueError:malformed+=1
    if current:sessions.append(current)
    delays=defaultdict(list); counts=Counter(); underruns=[]; startup=0
    pairs={12:(11,'decryption'),14:(13,'whole_callback'),21:(20,'audio_lock_wait'),
           23:(22,'opus'),32:(31,'driver_update'),41:(40,'summary_log_write')}
    for rows in sessions:
        starts=defaultdict(deque); received={}; offered={}; pcm={}; last={}; previous_rx=None
        for row in sorted(rows,key=lambda r:r[0]):
            time,stage,seq,a,b,c,d=row;counts[stage]+=1
            if stage==3:startup+=1
            if stage==2 and a==97 and b>0:
                if previous_rx is not None:delays['audio_recv_gap'].append(time-previous_rx)
                previous_rx=time;received[seq]=time;last['recv_audio']=time
            if stage==7:
                if seq in received:delays['receive_to_lbq_offer'].append(time-received.pop(seq))
                offered[seq]=time
            if stage==10 and a==0 and seq in offered:
                delays['packet_lbq_residence'].append(time-offered.pop(seq))
            if stage==24:pcm[seq]=time;last['pcm_queue']=time
            if stage==34 and c:
                if seq in pcm:delays['pcm_to_driver_submit'].append(time-pcm.pop(seq))
                last['driver_submit']=time
            if stage==13:last['callback_begin']=time
            if stage==23:last['opus_end']=time
            if stage in pairs:
                begin,name=pairs[stage]
                if starts[(begin,seq)]:delays[name].append(time-starts[(begin,seq)].popleft())
            if stage in {11,13,20,22,31,40}:starts[(stage,seq)].append(time)
            if stage==35:
                underruns.append({'time_us':time,'software_samples':a,'staged_samples':b,
                                 'callback_gap_us':c,'played_raw':d,
                                 'ages_us':{name:time-value for name,value in last.items()}})
    gaps=delays['audio_recv_gap']
    return {'sessions':len(sessions),'events':sum(counts.values()),
            'lost_diagnostic_records':lost,'malformed_lines':malformed,
            'startup_drops':startup,'stage_counts':dict(counts),
            'durations':{k:distribution(v) for k,v in delays.items()},
            'recv_gaps_over_ms':{str(ms):sum(v>ms*1000 for v in gaps) for ms in (10,20,30,50,100)},
            'underruns':underruns,
            'limitations':'Client-side observations include scheduling. Lost trace rows make paired durations incomplete; packet sequence wraps and recovered packets need raw context.'}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('file',type=Path);args=parser.parse_args()
    print(json.dumps(analyze(args.file.read_text(encoding='utf-8',errors='replace')),indent=2))
