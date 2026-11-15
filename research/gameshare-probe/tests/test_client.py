import importlib.util
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import threading
import unittest
from urllib.error import HTTPError

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("probe_client", ROOT / "client.py")
client = importlib.util.module_from_spec(spec)
spec.loader.exec_module(client)


class Handler(BaseHTTPRequestHandler):
    payload = {"schema": client.SCHEMA, "worker_done": False, "services": []}

    def do_GET(self):
        if self.headers.get("X-Probe-Key") != "0123456789abcdef":
            self.send_error(403)
            return
        body = json.dumps(self.payload).encode()
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


class ClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        cls.thread = threading.Thread(target=cls.server.serve_forever)
        cls.thread.start()

    @classmethod
    def tearDownClass(cls):
        cls.server.shutdown()
        cls.server.server_close()
        cls.thread.join()

    def test_roundtrip_and_pending(self):
        report = client.fetch("127.0.0.1", self.server.server_port, "0123456789abcdef")
        self.assertFalse(report["worker_done"])
        self.assertIn("not a failed/absent", client.summary(report))

    def test_bad_key(self):
        with self.assertRaises(HTTPError):
            client.fetch("127.0.0.1", self.server.server_port, "bad")

    def test_creator_result_is_separate_from_handle(self):
        report = {"worker_done": True, "services": [{"name": "stpl:u", "state": "opened", "result":
            {"hex": "00000000", "module": 0, "description": 0}}], "streamplay_creator":
            {"state": "error", "result": {"hex": "0000f601", "module": 1, "description": 123}}}
        text = client.summary(report)
        self.assertIn("stpl:u   opened", text)
        self.assertIn("creator: error 0000f601", text)

    def test_rejects_public_or_url_host(self):
        for host in ["8.8.8.8", "0.0.0.0", "http://127.0.0.1/", "127.0.0.1/path"]:
            with self.subTest(host=host), self.assertRaises(ValueError):
                client.fetch(host, 1, "unused")

    def test_overflow_report_is_failure(self):
        previous = Handler.payload
        try:
            Handler.payload = {"schema": client.SCHEMA, "error": "report_overflow"}
            with self.assertRaisesRegex(ValueError, "report_overflow"):
                client.fetch("127.0.0.1", self.server.server_port, "0123456789abcdef")
        finally:
            Handler.payload = previous


if __name__ == "__main__":
    unittest.main()
