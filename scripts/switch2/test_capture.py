#!/usr/bin/env python3
"""Reject console evidence that only records commands or uses a copy fallback."""

import unittest

from capture import presentation_evidence

SOURCE = "SWITCH2_VULKAN_VIDEO: source=direct-decoder-buffer sampling=texel-buffer"
PRESENT = (
    "SWITCH2_VULKAN_PRESENT: presents=60 newVideo=60 repeatedVideo=0 "
    "directBuffer=60 copyBuffer=0 texel=60 storage=0 skipped=58 uniqueFPS=60 frameDriven=true"
)


class CaptureTests(unittest.TestCase):
    def test_submitted_direct_texel_frames(self):
        result = presentation_evidence(SOURCE + "\n" + PRESENT)
        self.assertTrue(result["directBufferPathActive"])
        self.assertTrue(result["directTexelSamplingActive"])
        self.assertTrue(result["frameDrivenPresentationActive"])
        self.assertEqual(result["directBufferPresents"], 60)

    def test_recording_commands_does_not_prove_submission(self):
        result = presentation_evidence(
            SOURCE + "\ndirectBufferRecords=9999 copyRecords=0"
        )
        self.assertFalse(result["directBufferPathActive"])

    def test_copy_fallback_is_not_direct(self):
        result = presentation_evidence(
            SOURCE + "\n" + PRESENT.replace("copyBuffer=0", "copyBuffer=2")
        )
        self.assertFalse(result["directBufferPathActive"])
        self.assertFalse(result["directTexelSamplingActive"])

    def test_storage_sampling_is_not_texel_sampling(self):
        result = presentation_evidence(
            SOURCE + "\n" + PRESENT.replace("texel=60 storage=0", "texel=0 storage=60")
        )
        self.assertTrue(result["directBufferPathActive"])
        self.assertFalse(result["directTexelSamplingActive"])

    def test_repeated_picture_is_not_a_decoded_frame(self):
        result = presentation_evidence(
            SOURCE + "\n" + PRESENT.replace("newVideo=60", "newVideo=0")
        )
        self.assertFalse(result["directBufferPathActive"])


if __name__ == "__main__":
    unittest.main()
