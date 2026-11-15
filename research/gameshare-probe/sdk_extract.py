#!/usr/bin/env python3
"""Extract an owned cartridge's SDK through GameShare Probe; never write console files."""
import argparse,hashlib,http.client,json,os,struct,subprocess,time
from datetime import datetime,timezone
from pathlib import Path
from agent import Agent,ROOT,compile_payload,packet
from nso_verify import verify_nso
from toolchain import default_sdk

def request(op,program=0,attributes=1,offset=0,length=0,filename='sdk'):
    if filename not in ('sdk','main'):raise ValueError('Only sdk or explicit main extraction is supported')
    return struct.pack('<6I2Q2I16s',0x314b4453,1,op,2,attributes,0,program,offset,length,0,filename.encode())

def inventory(data):
    if len(data)<8:raise ValueError('Truncated cartridge inventory')
    total,count=struct.unpack_from('<ii',data)
    if not 0<=count<=total or count>512 or len(data)!=8+16*count:raise ValueError('Malformed cartridge inventory')
    return {'total':total,'entries':[dict(zip(('program_id','version','type','install_type'),
        (f'{tid:016x}',version,kind,install))) for tid,version,kind,install,_ in
        struct.iter_unpack('<QIBBH',data[8:])]}

class Runner:
    def __init__(self,agent,folder,code,entry):
        self.agent,self.folder,self.code,self.entry=agent,folder,code,entry
        state=agent.status()
        if state['busy'] or state.get('poisoned'):raise RuntimeError('Probe is busy or poisoned')
        self.session=state['session_id'];self.last_job=state['job_id'];self.uncertain=False
    def run(self,data,allow_existing=False,code=None,entry=None):
        before=self.agent.status()
        if before['session_id']!=self.session or before['job_id']!=self.last_job or before['busy'] or before.get('poisoned'):
            raise RuntimeError('Probe restarted or another client changed it; stopping')
        self.uncertain=True
        response=self.agent.submit(packet(code if code is not None else self.code,
            self.entry if entry is None else entry,data))
        expected=before['job_id']+1
        deadline=time.monotonic()+30
        while True:
            if response['session_id']!=self.session or response['job_id']!=expected:
                raise RuntimeError('Unexpected job/session after POST; not retrying')
            if not response['busy']:break
            if time.monotonic()>deadline:raise RuntimeError('Job is still running; not cancelled or retried')
            time.sleep(.05);response=self.agent.status()
        self.last_job=expected;self.uncertain=False
        output=bytes.fromhex(response.get('output_hex',''))
        # Journal metadata and hashes, not duplicated 64 KiB binary responses.
        record={k:v for k,v in response.items() if k!='output_hex'}
        record.update(input_hex=data.hex(),output_size=len(output),output_sha256=hashlib.sha256(output).hexdigest())
        with (self.folder/'jobs.jsonl').open('a',encoding='utf-8') as file:file.write(json.dumps(record)+'\n')
        if response['result']!='00000000' or response.get('poisoned'):raise RuntimeError('Native publication/execution failed')
        value=int(response['return_value'],16)
        if value and not (allow_existing and value==7 and 'HEN handles already retained' in response.get('log','')):
            if 'research_backend_info' in response.get('log',''):
                raise RuntimeError(f'The running HEN lacks the research CodeFS bridge (result 0x{value:x}). No core files were changed.')
            raise RuntimeError(f'Console request failed: 0x{value:x}\n'+response.get('log',''))
        return output

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host',default='192.168.1.182')
    parser.add_argument('--sdk',type=Path,default=default_sdk())
    parser.add_argument('--layout',type=Path,default=ROOT/'build/agent-layout-hen.bin',help='Guarded layout matching the installed probe; generate with qualify.py')
    sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('inventory');sub.add_parser('status');sub.add_parser('close')
    extract=sub.add_parser('extract')
    extract.add_argument('--program-id',required=True,type=lambda x:int(x,16))
    extract.add_argument('--attributes',type=int,choices=(0,1),default=1)
    extract.add_argument('--file',choices=('sdk','main'),default='sdk')
    extract.add_argument('--output',type=Path)
    args=parser.parse_args()
    try:
        folder=ROOT/'evidence/sdk-extract'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
        folder.mkdir(parents=True)
        code,entry=compile_payload(ROOT/'examples/sdk_extract.c',folder/'payload',args.sdk)
        agent=Agent(args.host,(ROOT/'evidence/agent-key.txt').read_text().strip())
        run=Runner(agent,folder,code,entry)
        # Existing helper has exact runtime function-byte guards and uses only
        # the documented RO/HEN protocol. No console binary is rebuilt.
        init,init_entry=compile_payload(ROOT/'examples/hen_readonly.c',folder/'init',args.sdk)
        layout=args.layout
        if not layout.is_file():raise RuntimeError('Missing layout qualification input for the installed probe')
        run.run(layout.read_bytes(),allow_existing=True,code=init,entry=init_entry)
        if args.command=='inventory':
            result=inventory(run.run(request(0)))
            (folder/'inventory.json').write_text(json.dumps(result,indent=2)+'\n')
            print(json.dumps(result,indent=2))
        elif args.command=='status':
            out=run.run(request(1))
            if len(out)!=16:raise ValueError('Invalid backend capability reply')
            magic,abi,available,reserved=struct.unpack('<4I',out)
            if (magic,abi,reserved)!=(0x4f525347,1,0):raise ValueError('Unknown backend')
            print('Research CodeFS bridge ABI 1; retained loader:',bool(available))
        elif args.command=='close':run.run(request(4));print('Closed adapter-owned extraction handles')
        else:
            destination=(args.output or folder/args.file).resolve()
            destination.parent.mkdir(parents=True,exist_ok=True)
            partial=destination.with_name(destination.name+'.partial')
            if destination.exists() or partial.exists():raise ValueError('Output or partial file already exists; refusing to overwrite')
            opened=False
            try:
                data=run.run(request(2,args.program_id,args.attributes,filename=args.file))
                if len(data)!=8:raise ValueError('Invalid file-size reply')
                size,=struct.unpack('<Q',data)
                if not 0<size<=256*1024*1024:raise ValueError('Unexpected executable size')
                opened=True
                with partial.open('xb') as file:
                    offset=0
                    while offset<size:
                        count=min(65536,size-offset)
                        data=run.run(request(3,offset=offset,length=count))
                        if len(data)!=count:raise ValueError('Short or oversized file chunk')
                        file.write(data);offset+=count
                        if offset==size or offset%(1024*1024)==0:print(f'Read {offset:,}/{size:,} bytes',flush=True)
                    file.flush();os.fsync(file.fileno())
                verified=verify_nso(partial.read_bytes())
                verified.update(program_id=f'{args.program_id:016x}',attributes=args.attributes,
                    storage=2,source='owned cartridge via paired console probe',session_id=run.session,
                    output=str(destination))
                os.link(partial,destination);partial.unlink() # Atomic no-clobber publication.
                (folder/'verified.json').write_text(json.dumps(verified,indent=2)+'\n')
                print('Verified NSO:',destination,'SHA-256',verified['file_sha256'])
            finally:
                if opened and not run.uncertain:
                    try:run.run(request(4))
                    except (OSError,ValueError,RuntimeError) as error:print('Cleanup could not be confirmed:',error)
        print('Evidence:',folder)
    except (OSError,ValueError,RuntimeError,http.client.HTTPException,subprocess.CalledProcessError) as error:
        parser.exit(1,str(error)+'\nNo POST is retried automatically.\n')
if __name__=='__main__':main()
