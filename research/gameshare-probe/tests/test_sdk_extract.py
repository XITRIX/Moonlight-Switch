import hashlib,struct,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from sdk_extract import request,inventory,Runner
from nso_verify import lz4_block,verify_nso

class Formats(unittest.TestCase):
    def test_request_and_inventory(self):
        self.assertEqual(len(request(2,0x040077201be50000)),64)
        key=struct.pack('<QIBBH',0x040077201be50000,0,0x80,0,0)
        self.assertEqual(inventory(struct.pack('<ii',1,1)+key)['entries'][0]['program_id'],'040077201be50000')
        with self.assertRaises(ValueError):inventory(struct.pack('<ii',1,2)+key)
        with self.assertRaises(ValueError):request(2,filename='../save')
    def test_lz4_overlap_and_truncation(self):
        self.assertEqual(lz4_block(b'\x10a\x01\x00',5),b'aaaaa')
        for data,n in [(b'\x10',1),(b'\x00\x00\x00',4),(b'\x10a\x01',5)]:
            with self.assertRaises(ValueError):lz4_block(data,n)
    def test_nso_hash_verification(self):
        data=bytearray(0x10c);data[:4]=b'NSO0';struct.pack_into('<I',data,12,0x38)
        for i,value in enumerate((b'code',b'ro!!',b'data')):
            struct.pack_into('<III',data,0x10+16*i,0x100+4*i,0x1000*i,4)
            data[0x100+4*i:0x104+4*i]=value
            data[0xa0+32*i:0xc0+32*i]=hashlib.sha256(value).digest()
        self.assertTrue(all(x['header_hash_verified'] for x in verify_nso(data)['segments']))
        data[-1]^=1
        with self.assertRaisesRegex(ValueError,'SHA-256 mismatch'):verify_nso(data)
    def test_restart_stops_before_post(self):
        class Fake:
            def __init__(self):self.n=0
            def status(self):
                self.n+=1
                return dict(busy=False,session_id=str(self.n),job_id=0)
            def submit(self,_):raise AssertionError('Must not post after restart')
        run=Runner(Fake(),Path('.'),b'1234',0)
        with self.assertRaisesRegex(RuntimeError,'restarted'):run.run(request(0))

if __name__=='__main__':unittest.main()
