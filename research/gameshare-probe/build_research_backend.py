#!/usr/bin/env python3
"""Build an isolated research HEN; never deploy or alter the probe's ELF/NRO."""
import argparse,hashlib,json,os,re,shlex,subprocess
from pathlib import Path
from toolchain import default_sdk, executable

ROOT=Path(__file__).resolve().parent
SRC=ROOT/'research-backend/kaihen'
SDK=default_sdk()
OUT=ROOT/'build/research-hen'

def main():
    global SDK
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk',type=Path,default=SDK)
    SDK=parser.parse_args().sdk.resolve()
    OUT.mkdir(parents=True,exist_ok=True)
    env=dict(os.environ,DEVKITPRO=SDK.as_posix())
    env['PATH']=str(SDK/'devkitA64/bin')+os.pathsep+env['PATH']
    cpu=shlex.split(re.search(r'^SWITCH2_CPU\s*:=\s*(.+)$',(SDK/'libounce/switch2_cpu.mk').read_text(),re.M)[1])
    flags=[*cpu,'-mtp=soft','-fPIE','-std=gnu++20','-O2','-Wall','-Wextra','-Werror',
        '-Wno-missing-field-initializers','-fno-exceptions','-fno-rtti',
        '-ffunction-sections','-fdata-sections','-D__SWITCH__','-D__SWITCH2__',
        '-I'+str(SDK/'libounce/include'),'-I'+str(SRC/'libkaihen/include'),'-I'+str(SRC/'hen/source')]
    specs=(SDK/'libounce/switch.specs').read_text().replace('%:getenv(DEVKITPRO /libounce/switch.ld)',(SDK/'libounce/switch.ld').as_posix())
    (OUT/'switch.specs').write_text(specs)
    cxx=executable(SDK,'devkitA64/bin/aarch64-none-elf-g++')
    commands=[]
    def run(command):
        cmd=[str(x) for x in command]; subprocess.run(cmd,env=env,cwd=ROOT,check=True);commands.append(cmd)
    objects=[]
    sources=sorted((SRC/'hen/source').glob('*.cpp'))+[SRC/'common/policy_package.cpp',SRC/'common/sd_writer.cpp']
    # Preserve the older installed kit's SD prefix in generated build copies.
    copies=OUT/'compatible-source'; copies.mkdir(exist_ok=True)
    for s in sources:
        # Relative includes depend on the original source directory. Quote paths
        # with -iquote while compiling a copy containing only this path change.
        copied=copies/s.name
        copied.write_text(s.read_text().replace('/kaihen/','/ouncehen/'))
        obj=OUT/(s.stem+'.o')
        run([cxx,*flags,'-iquote',s.parent,'-c',copied,'-o',obj]); objects.append(obj)
    elf=OUT/'hen.elf'
    run([cxx,*cpu,'-mtp=soft','-fPIE','-specs='+str(OUT/'switch.specs'),'-Wl,--gc-sections',
        '-Wl,-Map,'+str(OUT/'hen.map'),*objects,'-L'+str(SDK/'libounce/lib'),'-lounce_core','-o',elf])
    run([executable(SDK,'tools/bin/elf2nso'),elf,OUT/'main'])
    all_sources=[p for p in SRC.rglob('*') if p.is_file() and p.suffix in ('.c','.cpp','.h','.hpp')]
    hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in all_sources}
    record={'upstream':json.loads((SRC/'UPSTREAM.json').read_text()),'source_hashes':hashes,
        'commands':commands,'binary_sha256':hashlib.sha256((OUT/'main').read_bytes()).hexdigest(),
        'deployed':False,'device_tested':False,'sd_prefix':'/ouncehen/'}
    (OUT/'build.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Built isolated HEN:',OUT/'main',record['binary_sha256'])
if __name__=='__main__':main()
