"""Check actual settings validation and audio-section placement."""
from pathlib import Path
import re, json, subprocess, tempfile, xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
settings=(ROOT/'app/src/utils/Settings.hpp').read_bytes().decode('utf-8')
setter=re.search(r'void set_audio_reserve_ms\(int value\) \{[^}]+\}',settings)[0]
code='''#include <cassert>
#include <climits>
struct Settings {int m_audio_reserve_ms=10;
'''+setter+'''};
int main(){Settings s;
for(int value=0;value<=100;value+=10){s.set_audio_reserve_ms(value);assert(s.m_audio_reserve_ms==value);}
for(int value=-150;value<=150;++value){s.set_audio_reserve_ms(value);assert(s.m_audio_reserve_ms==(value>=0&&value<=100&&value%10==0?value:10));}
s.set_audio_reserve_ms(INT_MAX);assert(s.m_audio_reserve_ms==10);
s.set_audio_reserve_ms(INT_MIN);assert(s.m_audio_reserve_ms==10);
}
'''
with tempfile.TemporaryDirectory() as temporary:
    source=Path(temporary)/'settings.cpp';exe=Path(temporary)/'settings.exe'
    source.write_text(code)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
xml=(ROOT/'resources/xml/tabs/settings.xml').read_text()
root=ET.fromstring(xml.replace('brls:',''))
content=root.find('.//Box[@id="settings_content"]')
children=list(content)
backend=next(i for i,node in enumerate(children) if node.get('id')=='audio_backend')
assert children[backend-1].get('title')=='@i18n/settings/audio_settings'
assert children[backend+1].get('id')=='audio_reserve'
assert children[backend+2].get('id')=='pcAudio'
ids=[node.get('id') for node in root.iter() if node.get('id')]
assert len(ids)==len(set(ids))
ui=(ROOT/'app/src/settings_tab.cpp').read_bytes().decode('utf-8')
options=re.search(r'audioReserve->init\(.*?\{(.*?)\}',ui,re.S)[1]
assert re.findall(r'"(\d+) ms"',options)==[str(i) for i in range(0,101,10)]
for lang in ['en-US','pt-BR']:
    values=json.loads((ROOT/f'resources/i18n/{lang}/main.json').read_bytes())['settings']
    assert values['audio_settings'] and values['audio_reserve']
print('PASS: production 0..100 ms validator, all 11 selector choices, unique rows, audio header/driver/buffer placement and UTF-8 labels')
