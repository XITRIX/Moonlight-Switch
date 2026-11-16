"""A connection can fail after native execution; recovery must never replay it."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock

from agent import packet, recover_result


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder=Path(self.temporary.name)
        payload=packet(b"\0"*8,0,b"configuration")
        (self.folder/"request.bin").write_bytes(payload)
        (self.folder/"request.json").write_text(json.dumps({
            "session_id":"session-a","previous_job_id":6,
            "packet_sha256":hashlib.sha256(payload).hexdigest()}))
        self.result={"session_id":"session-a","job_id":7,"code_size":8,
            "entry_offset":0,"busy":False,"finished":True,"output_hex":"2a"}
        self.agent=Mock()
        self.agent.status.return_value=self.result

    def test_lost_reply_recovers_by_get_without_post(self):
        self.assertEqual(recover_result(self.agent,self.folder),self.result)
        self.agent.status.assert_called_once_with()
        self.agent.submit.assert_not_called()
        self.assertEqual((self.folder/"output.bin").read_bytes(),b"*")

    def test_other_session_or_later_job_cannot_be_misattributed(self):
        for changes in ({"session_id":"session-b"},{"job_id":8},{"code_size":12}):
            with self.subTest(changes=changes):
                self.agent.status.return_value={**self.result,**changes}
                with self.assertRaisesRegex(RuntimeError,"does not match"):
                    recover_result(self.agent,self.folder)
        self.agent.submit.assert_not_called()
        self.assertFalse((self.folder/"recovered-result.json").exists())

    def test_active_job_is_never_cancelled_or_resubmitted(self):
        self.agent.status.return_value={**self.result,"busy":True,"finished":False}
        with self.assertRaisesRegex(RuntimeError,"still active"):
            recover_result(self.agent,self.folder)
        self.agent.submit.assert_not_called()

    def test_tampered_request_does_not_contact_console(self):
        data=(self.folder/"request.bin").read_bytes()
        (self.folder/"request.bin").write_bytes(data[:-1]+b"!")
        with self.assertRaisesRegex(ValueError,"integrity"):
            recover_result(self.agent,self.folder)
        self.agent.status.assert_not_called()
        self.agent.submit.assert_not_called()
