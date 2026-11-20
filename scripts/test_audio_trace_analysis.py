from analyze_audio_trace import analyze

text='''# LL24 AUDIO session_begin
100,2,1,97,100,0,0
120,7,1,100,0,0,0
130,10,1,0,100,0,0
140,13,1,100,0,0,0
141,20,1,100,0,0,0
145,21,1,0,0,0,0
146,22,1,100,240,0,0
160,23,1,240,14,0,0
165,24,1,480,480,0,2
170,14,1,0,0,0,0
200,34,1,0,480,1,0
50100,2,2,97,100,0,0
50200,35,0,0,0,50060,10
# session_end recorded=13 lost_diagnostic_records=3
'''
r=analyze(text)
assert r['sessions']==1 and r['lost_diagnostic_records']==3
for name,expected in {'receive_to_lbq_offer':20,'packet_lbq_residence':10,
                      'whole_callback':30,'audio_lock_wait':4,'opus':14,
                      'pcm_to_driver_submit':35,'audio_recv_gap':50000}.items():
    assert r['durations'][name]['max_us']==expected,(name,r)
assert r['underruns'][0]['ages_us']['recv_audio']==100
assert r['recv_gaps_over_ms']['30']==1 and r['recv_gaps_over_ms']['50']==0
assert analyze(text+text)['sessions']==2
assert analyze(text+text.replace('LL24','LL25'))['sessions']==2
assert analyze('not,a,csv')['malformed_lines']==1
print('PASS: trace analyzer pairing, asynchronous timestamps, underrun context, sessions and missing diagnostic records')
