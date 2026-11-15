import importlib.util
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import struct
import threading
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("research_agent",ROOT/"agent.py")
agent=importlib.util.module_from_spec(spec)
spec.loader.exec_module(agent)


class Handler(BaseHTTPRequestHandler):
    posts=0
    job=0
    last_packet=None
    error=False

    def send(self,status,value):
        body=json.dumps(value).encode()
        self.send_response(status)
        self.send_header("Content-Length",str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def authorized(self):
        if self.headers.get("X-Probe-Key")!="0123456789abcdef":
            self.send(403,{"error":"pairing_key_required"})
            return False
        return True

    def do_GET(self):
        if not self.authorized():
            return
        if self.error:
            self.send(200,{"schema":"wrong"})
        else:
            self.send(200,{"schema":"kaihen-research-agent-v1","api_abi":1,"job_id":type(self).job,
                           "busy":False,"finished":True,"result":"00000000","return_value":"0x2a"})

    def do_POST(self):
        if not self.authorized():
            return
        type(self).posts+=1
        type(self).job+=1
        type(self).last_packet=self.rfile.read(int(self.headers["Content-Length"]))
        self.send(202,{"schema":"kaihen-research-agent-v1","api_abi":1,"job_id":type(self).job,
                       "busy":True,"finished":False})

    def log_message(self,*args):
        pass


class AgentTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server=ThreadingHTTPServer(("127.0.0.1",0),Handler)
        cls.worker=threading.Thread(target=cls.server.serve_forever)
        cls.worker.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.worker.join()

    def connection(self,key="0123456789abcdef"):
        return agent.Agent("127.0.0.1",key,self.server.server_port)

    def test_submit_one_command_then_collect_result(self):
        before=Handler.posts
        payload=agent.packet(bytes.fromhex("40058052c0035fd6"),0,b"input")
        connection=self.connection()
        accepted=connection.submit(payload)
        self.assertTrue(accepted["busy"])
        done=connection.status()
        self.assertEqual(done["job_id"],accepted["job_id"])
        self.assertFalse(done["busy"])
        self.assertEqual(Handler.posts,before+1)
        self.assertEqual(Handler.last_packet,payload)
        self.assertEqual(struct.unpack("<4s5I",payload[:24]),(b"KJIT",1,0,8,5,0))

    def test_auth_failure_does_not_retry(self):
        before=Handler.posts
        with self.assertRaisesRegex(RuntimeError,"HTTP 403"):
            self.connection("0000000000000000").submit(agent.packet(b"\0"*4))
        self.assertEqual(Handler.posts,before)

    def test_rejects_wrong_schema(self):
        Handler.error=True
        try:
            with self.assertRaisesRegex(ValueError,"protocol"):
                self.connection().status()
        finally:
            Handler.error=False

    def test_packet_bounds(self):
        cases=[(b"",0,b""),(b"\0"*3,0,b""),(b"\0"*8,1,b""),
               (b"\0"*8,8,b""),(b"\0"*8,-4,b""),
               (b"\0"*(agent.CODE_LIMIT+4),0,b""),(b"\0"*4,0,b"x"*(agent.INPUT_LIMIT+1))]
        for args in cases:
            with self.subTest(args=(len(args[0]),args[1],len(args[2]))),self.assertRaises(ValueError):
                agent.packet(*args)

    def test_host_and_key_validation(self):
        for host,key in [("8.8.8.8","0"*16),("0.0.0.0","0"*16),("http://127.0.0.1","0"*16),("127.0.0.1","bad")]:
            with self.subTest(host=host),self.assertRaises(ValueError):
                agent.Agent(host,key)


if __name__=="__main__":
    unittest.main()
