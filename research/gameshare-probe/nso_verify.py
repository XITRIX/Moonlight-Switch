"""Bounded NSO0 integrity checks, including raw LZ4 segment decompression."""
import hashlib,struct
LIMIT=256*1024*1024

def lz4_block(source,expected):
    if not 0<=expected<=LIMIT: raise ValueError('LZ4 output size exceeds limit')
    out=bytearray();at=0
    def length(base):
        nonlocal at
        if base!=15:return base
        while True:
            if at>=len(source):raise ValueError('Truncated LZ4 length')
            value=source[at];at+=1;base+=value
            if base>expected:raise ValueError('Oversized LZ4 length')
            if value!=255:return base
    while at<len(source):
        token=source[at];at+=1;literal=length(token>>4)
        if at+literal>len(source) or len(out)+literal>expected:raise ValueError('Truncated LZ4 literals')
        out.extend(source[at:at+literal]);at+=literal
        if at==len(source):break
        if at+2>len(source):raise ValueError('Truncated LZ4 offset')
        distance=int.from_bytes(source[at:at+2],'little');at+=2
        count=length(token&15)+4
        if not distance or distance>len(out) or len(out)+count>expected:raise ValueError('Invalid LZ4 match')
        for _ in range(count):out.append(out[-distance])
    if len(out)!=expected:raise ValueError('LZ4 output size mismatch')
    return bytes(out)

def verify_nso(data):
    if len(data)<0x100 or len(data)>LIMIT or data[:4]!=b'NSO0':raise ValueError('Not a complete NSO0 file')
    flags=struct.unpack_from('<I',data,12)[0]
    if flags&~0x3f:raise ValueError('Unsupported NSO flags')
    records=[];ranges=[]
    for i,name in enumerate(('text','rodata','data')):
        offset,address,size=struct.unpack_from('<III',data,0x10+16*i)
        packed=struct.unpack_from('<I',data,0x60+4*i)[0]
        length=packed if flags&(1<<i) else size
        if size>LIMIT or (length and offset<0x100) or offset+length>len(data):raise ValueError('Invalid NSO segment bounds')
        for start,end in ranges:
            if length and offset<end and offset+length>start:raise ValueError('Overlapping NSO segments')
        if length:ranges.append((offset,offset+length))
        segment=data[offset:offset+length]
        if flags&(1<<i):segment=lz4_block(segment,size)
        digest=hashlib.sha256(segment).digest()
        has_hash=bool(flags&(8<<i))
        if has_hash and digest!=data[0xa0+32*i:0xc0+32*i]:raise ValueError('NSO '+name+' SHA-256 mismatch')
        records.append({'name':name,'address':address,'size':size,'stored_size':length,
                        'sha256':digest.hex(),'header_hash_verified':has_hash})
    return {'format':'NSO0','build_id':data[0x40:0x60].hex(),
            'file_sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data),'segments':records}
