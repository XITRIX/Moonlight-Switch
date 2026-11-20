"""Verify actual Switch machine code, beyond host-only functional fixtures."""
from pathlib import Path
import struct, subprocess, tempfile, re
ROOT=Path(__file__).resolve().parents[1]
SDK=Path('C:/devkitPro/devkitA64/bin')
elf=ROOT/'build/switch-local/Moonlight.elf'
data=elf.read_bytes()
assert data[:5]==b'\x7fELF\x02'
section_offset=struct.unpack_from('<Q',data,40)[0]
section_size,section_count=struct.unpack_from('<HH',data,58)
unsafe=[]
for i in range(section_count):
    section=struct.unpack_from('<IIQQQQIIQQ',data,section_offset+i*section_size)
    if not section[2]&4:continue # SHF_EXECINSTR
    code=data[section[4]:section[4]+section[5]]
    for offset in range(0,len(code)-3,4):
        word=struct.unpack_from('<I',code,offset)[0]
        if word&0xffffffe0==0xd53bd040:unsafe.append(hex(section[3]+offset))
assert not unsafe,'Direct TPIDR_EL0 reads in Switch executable: '+str(unsafe[:12])
stable = b'Build: LL27 Stable' in data
if stable:
    symbols = subprocess.check_output([str(SDK/'aarch64-none-elf-nm.exe'),'-C',str(elf)],text=True)
    for removed in ['AudioDiagnostics::','LiAudioTraceEvent','LiAudioTraceCurrentSequence','audioTraceSequence']:
        assert removed not in symbols, 'Stable binary retains detailed tracing: '+removed
    assert b'RAW submit frame' not in data and b'RAW acquire_begin' not in data
else:
    disassembly=subprocess.check_output([str(SDK/'aarch64-none-elf-objdump.exe'),'-d','--disassemble=LiAudioTraceCurrentSequence',str(elf)],text=True)
    assert '__aarch64_read_tp' in disassembly
    assert '[x0, x0]' not in disassembly, 'Invalid self-indexed TLS load (LL25 PIC regression)'
    assert 'add\tx0, x0,' in disassembly, 'TLS accessor must add the local-exec offset after obtaining the thread pointer'
    # Verify the actual address calculation, not merely the helper's presence.
    tls_value=None
    for i in range(section_count):
        section=struct.unpack_from('<IIQQQQIIQQ',data,section_offset+i*section_size)
        if section[1]!=2 or section[9]!=24:continue
        strings=struct.unpack_from('<IIQQQQIIQQ',data,section_offset+section[6]*section_size)
        names=data[strings[4]:strings[4]+strings[5]]
        for name,info,_,_,value,_ in struct.iter_unpack('<IBBHQQ',data[section[4]:section[4]+section[5]]):
            if info&15==6 and names[name:names.find(b'\0',name)]==b'audioTraceSequence':tls_value=value
    assert tls_value is not None,'Audio TLS symbol was incorrectly optimized away'
    offset=sum(int(value,16) << int(shift or 0) for value,shift in re.findall(r'\badd\s+x0,\s*x0,\s*#0x([0-9a-f]+)(?:,\s*lsl\s*#(\d+))?',disassembly))
    assert offset==tls_value+16,(offset,tls_value,'Incorrect AArch64 TLS/TCB address')
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory)
    for language,extension in [('gcc','c'),('g++','cpp')]:
        p=d/('probe.'+extension);assembly=d/'probe.s'
        keyword='_Thread_local' if extension=='c' else 'thread_local'
        p.write_text(f'{keyword} int slot; int read_slot(void){{return slot;}}\n')
        for pic in ['-fPIC','-fPIE']:
            subprocess.run([str(SDK/f'aarch64-none-elf-{language}.exe'),'-mtp=soft','-mtls-dialect=trad','-ftls-model=local-exec',pic,'-O2','-S',str(p),'-o',str(assembly)],check=True)
            text=assembly.read_text()
            assert '__aarch64_read_tp' in text and 'tpidr_el0' not in text
            assert ':tprel_hi12:' in text and ':tprel_lo12_nc:' in text
            assert ':tlsdesc:' not in text and '[x0, x0]' not in text
print('PASS: native C/C++ TLS probes, stable diagnostic removal / linked audio accessor and executable instruction scan use libnx-compatible TLS')
