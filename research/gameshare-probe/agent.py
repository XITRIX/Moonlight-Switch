#!/usr/bin/env python3
"""Compile and run native research commands on the persistent kaiHEN NRO."""
import argparse
from datetime import datetime, timezone
import hashlib
import http.client
import ipaddress
import json
import os
from pathlib import Path
import re
import shlex
import struct
import subprocess
import time
from toolchain import default_sdk

ROOT=Path(__file__).resolve().parent
ABI=1
CODE_LIMIT=256*1024
INPUT_LIMIT=65536


def packet(code, entry=0, input_data=b""):
    if not code or len(code)>CODE_LIMIT or len(code)%4 or entry<0 or entry%4 or entry>=len(code):
        raise ValueError("Invalid native image size/entry alignment")
    if len(input_data)>INPUT_LIMIT:
        raise ValueError("Input exceeds 64 KiB")
    return struct.pack("<4s5I",b"KJIT",ABI,entry,len(code),len(input_data),0)+code+input_data


class Agent:
    def __init__(self, host, key, port=39381, timeout=10):
        address=ipaddress.IPv4Address(host)
        if not address.is_private or address.is_unspecified or address.is_multicast:
            raise ValueError("Use the console's literal LAN IPv4 address")
        if not re.fullmatch("[0-9a-f]{16}",key):
            raise ValueError("Pairing key must have 16 lowercase hexadecimal characters")
        self.host=str(address)
        self.key=key
        self.port=port
        self.timeout=timeout

    def request(self, method, endpoint, data=None):
        connection=http.client.HTTPConnection(self.host,self.port,timeout=self.timeout)
        try:
            connection.request(method,endpoint,body=data,headers={"X-Probe-Key":self.key,
                               "Content-Type":"application/octet-stream"})
            response=connection.getresponse()
            raw=response.read(512*1024+1)
            if len(raw)>512*1024:
                raise ValueError("Oversized agent reply")
            result=json.loads(raw)
            if response.status not in (200,202):
                raise RuntimeError(f"HTTP {response.status}: {result}")
            if result.get("schema")!="kaihen-research-agent-v1" or result.get("api_abi")!=ABI:
                raise ValueError("Unexpected agent protocol; launch probe 0.3 or newer")
            return result
        finally:
            connection.close()

    def status(self):
        return self.request("GET","/agent")

    def submit(self, payload):
        # Never retry a POST automatically: a lost reply leaves execution uncertain.
        return self.request("POST","/run",payload)


def compile_payload(source, output_directory, sdk):
    source=Path(source).resolve()
    directory=Path(output_directory).resolve()
    sdk=Path(sdk).resolve()
    directory.mkdir(parents=True,exist_ok=True)
    suffix=".exe" if os.name=="nt" else ""
    prefix=sdk/"devkitA64/bin"
    gcc=prefix/("aarch64-none-elf-gcc"+suffix)
    env=dict(os.environ)
    env["PATH"]=str(prefix)+os.pathsep+env["PATH"]
    match=re.search(r"^SWITCH2_CPU\s*:=\s*(.+)$",(sdk/"libounce/switch2_cpu.mk").read_text(),re.MULTILINE)
    if not match:
        raise ValueError("SDK CPU flags unavailable")
    flags=[*shlex.split(match.group(1)),"-mtp=soft","-std=gnu11","-O2","-Wall","-Wextra","-Werror",
           "-ffreestanding","-fno-builtin","-fno-stack-protector","-fno-tree-loop-distribute-patterns",
           "-fno-unwind-tables","-fno-asynchronous-unwind-tables","-fPIE","-fvisibility=hidden",
           "-ffunction-sections","-fdata-sections","-D__SWITCH__","-D__SWITCH2__",
           "-I"+str(ROOT/"source"),"-I"+str(sdk/"libounce/include")]
    commands=[]

    def run(command):
        subprocess.run([str(x) for x in command],env=env,check=True,cwd=ROOT)
        commands.append([str(x) for x in command])

    elf=directory/"payload.elf"
    run([gcc,*flags,"-nostdlib","-nostartfiles","-nodefaultlibs","-no-pie",
         "-Wl,--no-undefined,--build-id=none,--gc-sections,-z,max-page-size=0x1000",
         "-Wl,-T,"+str(ROOT/"payload.ld"),source,ROOT/"payload_runtime.c","-o",elf])
    binary=directory/"payload.bin"
    run([prefix/("aarch64-none-elf-objcopy"+suffix),"-O","binary",elf,binary])
    names=subprocess.check_output([prefix/("aarch64-none-elf-nm"+suffix),"--defined-only",elf],env=env,text=True)
    symbols={}
    for line in names.splitlines():
        row=line.split()
        if len(row)==3:
            symbols[row[2]]=int(row[0],16)
    entry=symbols.get("probe_entry")
    image_end=symbols.get("__image_end")
    if entry is None or image_end is None or image_end>CODE_LIMIT:
        raise ValueError("Missing entry/image bounds or payload too large")
    code=binary.read_bytes()
    if len(code)>image_end:
        raise ValueError("Binary exceeds linked image")
    code+=b"\0"*(image_end-len(code))
    packet(code,entry)
    binary.write_bytes(code)
    record={"source":str(source),"source_sha256":hashlib.sha256(source.read_bytes()).hexdigest(),
            "api_sha256":hashlib.sha256((ROOT/"source/research_api.h").read_bytes()).hexdigest(),
            "code_sha256":hashlib.sha256(code).hexdigest(),"code_size":len(code),"entry":entry,"commands":commands}
    (directory/"payload-build.json").write_text(json.dumps(record,indent=2)+"\n")
    (directory/"source.c").write_bytes(source.read_bytes())
    return code,entry


def show(result):
    print(f"Job {result['job_id']}: {result['stage']} | {result['elapsed_ms']} ms | result {result['result']} | return {result['return_value']}")
    if result.get("log"):
        print(result["log"],end="" if result["log"].endswith("\n") else "\n")
    if result.get("resources"):
        print("Retained service objects:",json.dumps(result["resources"]))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host",default="192.168.1.182")
    parser.add_argument("--port",type=int,default=39381)
    parser.add_argument("--key",help="Defaults to the private key saved by deploy.py")
    parser.add_argument("--sdk",type=Path,default=default_sdk())
    sub=parser.add_subparsers(dest="command",required=True)
    sub.add_parser("status")
    compile_cmd=sub.add_parser("compile")
    compile_cmd.add_argument("source",type=Path)
    compile_cmd.add_argument("--output",type=Path,default=ROOT/"build/payload")
    for name in ("run","run-bin"):
        command=sub.add_parser(name)
        command.add_argument("source",type=Path)
        command.add_argument("--input",type=Path)
        command.add_argument("--timeout",type=float,default=30)
        if name=="run-bin":
            command.add_argument("--entry",type=lambda s:int(s,0),default=0)
    args=parser.parse_args()
    try:
        if args.command=="compile":
            code,entry=compile_payload(args.source,args.output,args.sdk)
            print(f"Compiled {len(code)} bytes; entry 0x{entry:x}; {args.output.resolve()}")
            return
        key=args.key or (ROOT/"evidence/agent-key.txt").read_text().strip()
        agent=Agent(args.host,key,args.port)
        if args.command=="status":
            print(json.dumps(agent.status(),indent=2))
            return
        if not 0<args.timeout<=300:
            raise ValueError("Timeout must be between 0 and 300 seconds")
        current=agent.status()
        if current["busy"] or current.get("poisoned"):
            raise RuntimeError("Agent is busy or needs a restart after JIT cleanup failure")
        stamp=datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        folder=ROOT/"evidence/jobs"/stamp
        folder.mkdir(parents=True)
        if args.command=="run":
            code,entry=compile_payload(args.source,folder,args.sdk)
        else:
            code=args.source.read_bytes()
            entry=args.entry
        input_data=args.input.read_bytes() if args.input else b""
        payload=packet(code,entry,input_data)
        (folder/"request.bin").write_bytes(payload)
        (folder/"request.json").write_text(json.dumps({"host":args.host,"port":args.port,
            "code_sha256":hashlib.sha256(code).hexdigest(),"packet_sha256":hashlib.sha256(payload).hexdigest(),
            "entry":entry,"input_size":len(input_data),"previous_job_id":current["job_id"],"session_id":current.get("session_id")},indent=2)+"\n")
        result=agent.submit(payload)
        if result.get("session_id")!=current.get("session_id"):
            raise RuntimeError("Agent restarted during submission; inspect status before taking another action")
        expected=result["job_id"]
        deadline=time.monotonic()+args.timeout
        previous=None
        with (folder/"responses.jsonl").open("w",encoding="utf-8") as records:
            while True:
                records.write(json.dumps(result)+"\n")
                records.flush()
                if result["job_id"]!=expected or result.get("session_id")!=current.get("session_id"):
                    raise RuntimeError("Agent restarted or another client replaced this job; inspect the saved responses")
                state=(result["stage"],result.get("log"),result["busy"])
                if state!=previous:
                    show(result)
                    previous=state
                if not result["busy"]:
                    break
                if time.monotonic()>=deadline:
                    print("Command is still running. It was not cancelled or resubmitted; use status.")
                    print(f"Evidence: {folder}")
                    return
                time.sleep(.25)
                result=agent.status()
        (folder/"result.json").write_text(json.dumps(result,indent=2)+"\n")
        (folder/"output.bin").write_bytes(bytes.fromhex(result.get("output_hex","")))
        print(f"Evidence: {folder}")
        if result["result"]!="00000000" or result.get("poisoned"):
            raise RuntimeError("Native job/publication failed; inspect the recorded stage and result")
    except (OSError,ValueError,RuntimeError,http.client.HTTPException,subprocess.CalledProcessError) as error:
        parser.exit(1,f"Agent command failed: {error}\nNo command is retried automatically.\n")


if __name__=="__main__":
    main()
