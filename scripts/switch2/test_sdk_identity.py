"""SDK identity must support archive and devkitPro package installations."""

import tempfile
import unittest
from pathlib import Path

from sdk_identity import sdk_identity


class SDKIdentityTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.sdk = Path(self.directory.name)

    def package(self, name, version):
        desc = self.sdk / "pacman/var/lib/pacman/local" / f"{name}-{version}/desc"
        desc.parent.mkdir(parents=True)
        desc.write_text(f"%NAME%\n{name}\n\n%VERSION%\n{version}\n\n")

    def test_archive_names_and_precedence(self):
        (self.sdk / "ounce-sdk-version.txt").write_text("legacy\n")
        self.assertEqual(sdk_identity(self.sdk), "legacy")
        (self.sdk / "ounce-devkit-version.txt").write_text("current\n")
        self.assertEqual(sdk_identity(self.sdk), "current")

    def test_package_installation(self):
        self.package("libounce", "20261117.1-1")
        self.package("ouncevk", "20261117.2-1")
        self.assertEqual(
            sdk_identity(self.sdk),
            "devkitPro packages: libounce=20261117.1-1, ouncevk=20261117.2-1",
        )

    def test_missing_or_partial_installation(self):
        with self.assertRaises(SystemExit):
            sdk_identity(self.sdk)
        self.package("libounce", "1-1")
        with self.assertRaises(SystemExit):
            sdk_identity(self.sdk)

    def test_ambiguous_metadata(self):
        self.package("libounce", "1-1")
        self.package("libounce", "2-1")
        self.package("ouncevk", "1-1")
        with self.assertRaises(SystemExit):
            sdk_identity(self.sdk)


if __name__ == "__main__":
    unittest.main()
