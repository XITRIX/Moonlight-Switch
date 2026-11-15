from pathlib import Path
import struct,sys
from capstone import Cs,CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN

def lz4(src,size):
 out=bytearray();i=0
 while i<len(src):
  token=src[i];i+=1;n=token>>4
  if n==15:
   while True:
    v=src[i];i+=1;n+=v
    if v!=255:break
  out+=src[i:i+n];i+=n
  if i==len(src):break
  off=int.from_bytes(src[i:i+2],'little');i+=2;n=token&15
  if n==15:
   while True:
    v=src[i];i+=1;n+=v
    if v!=255:break
  n+=4
  if not off or off>len(out):raise ValueError('bad offset')
  for _ in range(n):out.append(out[-off])
  if len(out)>size:raise ValueError('oversize')
 assert len(out)==size
 return bytes(out)

def segments(filename):
 b=Path(filename).read_bytes();flags=struct.unpack_from('<I',b,12)[0];result=[]
 for seg in range(3):
  off,va,size=struct.unpack_from('<III',b,16+seg*16);packed=struct.unpack_from('<I',b,96+seg*4)[0]
  data=lz4(b[off:off+packed],size) if flags&(1<<seg) else b[off:off+size]
  result.append((va,data))
 return result

if __name__=='__main__':
 filename=sys.argv[1];segs=segments(filename);md=Cs(CS_ARCH_ARM64,CS_MODE_LITTLE_ENDIAN)
 md.skipdata=True
 text=segs[0][1];instructions=list(md.disasm(text,segs[0][0]))
 if len(sys.argv)>2:
  begin=int(sys.argv[2],0);end=int(sys.argv[3],0)
  for ins in md.disasm(text[begin:end],begin):print(f'{ins.address:06x}: {ins.mnemonic:8} {ins.op_str}')
 else:
  for i,ins in enumerate(instructions):
   if any(token in ins.op_str for token in ['#0x1759','#0xfdec','#0xfde8','#0xfde9','#0xfdea','#0xfdeb']):
    print('---')
    for row in instructions[max(0,i-8):i+10]:print(f'{row.address:06x}: {row.mnemonic:8} {row.op_str}')
