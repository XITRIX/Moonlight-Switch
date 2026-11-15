// Finite, read-only inspection of Civ7's loaded Nintendo SDK export table.
// No SDK functions invoked, no filesystem access, no HEN/RO code changes.
(async function () {
    'use strict';
    if (typeof sdk_base !== 'bigint' || typeof read32 !== 'function' ||
        typeof read64 !== 'function' || typeof read8 !== 'function')
        throw Error('Civ7 native read primitives unavailable');
    var base=sdk_base,mod=base+read32(base+4n);
    if(read32(mod)!==0x30444f4dn)throw Error('SDK MOD0 signature missing');
    var dynamic=mod+BigInt.asIntN(32,read32(mod+4n)),tags={},ended=false;
    for(var i=0;i<256;i++){
        var tag=Number(read64(dynamic+BigInt(i*16)));if(!tag){ended=true;break;}
        tags[tag]=read64(dynamic+BigInt(i*16+8));
    }
    if(!ended || !tags[4] || !tags[5] || !tags[6] || !tags[10])throw Error('Incomplete SDK dynamic table');
    function ptr(v){return v<base?base+v:v;}
    var hash=ptr(tags[4]),strings=ptr(tags[5]),symbols=ptr(tags[6]);
    var count=Number(read32(hash+4n)),stringSize=Number(tags[10]);
    if(tags[11]!==24n || count<1 || count>300000 || stringSize<1 || stringSize>32*1024*1024)
        throw Error('SDK table bounds rejected');
    function text(at,max){
        var s='';for(var j=0;j<max;j++){var c=Number(read8(at+BigInt(j)));if(!c)return s;s+=String.fromCharCode(c);}
        throw Error('Unterminated bounded SDK string');
    }
    var selected=[],matches=0,defined=0,imported=0,versionNames=[];
    var pattern=/streamplay|shareplay|gameshare|StreamPlay|SharePlay|GameShare|(?:MountCode|OpenCodeFileSystem|GetSdkVersion|SdkVersion|SdkBuild|NintendoSDK)/;
    for(var n=0;n<count;n++){
        var at=symbols+BigInt(n*24),nameAt=Number(read32(at));
        if(nameAt>=stringSize)throw Error('SDK symbol name offset invalid');
        var name=text(strings+BigInt(nameAt),Math.min(2048,stringSize-nameAt));
        var flags=Number(read32(at+4n)),section=flags>>>16,value=read64(at+8n),size=Number(read64(at+16n));
        if(section)defined++;else imported++;
        if(!pattern.test(name))continue;
        matches++;
        if(selected.length<256){
            var item={name:name,defined:section!==0,type:flags&15,value:'0x'+value.toString(16),size:size};
            if(section && value && (flags&15)===2){
                var address=base+value,hex='';item.address='0x'+address.toString(16);
                for(var k=0;k<Math.min(size,32);k++)hex+=Number(read8(address+BigInt(k))).toString(16).padStart(2,'0');
                item.first_bytes=hex;
            }
            selected.push(item);
        }
        if(/SdkVersion|SdkBuild|NintendoSDK/.test(name))versionNames.push(name);
        if(n && n%4096===0)await new Promise(function(r){setTimeout(r,0);});
    }
    var report={schema:'civ7-sdk-inventory-v1',evidence:'console_runtime',
        scope:'Civ7 Nintendo SDK module only',base:'0x'+base.toString(16),
        dynamic:'0x'+dynamic.toString(16),symbol_count:count,defined:defined,imported:imported,
        string_bytes:stringSize,scanned:count,matches:matches,truncated:matches>selected.length,symbols:selected};
    var encoded=PayloadCore.base64(PayloadCore.encode(JSON.stringify(report))),pieces=Math.ceil(encoded.length/800);
    for(var p=0;p<pieces;p++)await log('CIV7_SDK_DATA '+p+'/'+pieces+' '+encoded.slice(p*800,(p+1)*800));
    await log('CIV7_SDK_COMPLETE');
})();
