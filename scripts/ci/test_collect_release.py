"""Release packaging checks; run with python -m unittest discover -s scripts/ci."""

import tempfile
import unittest
from pathlib import Path
from zipfile import ZipFile

from collect_release import ANDROID_ASSETS, ASSETS, collect


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.artifacts = self.root / "artifacts"
        self.output = self.root / "dist"
        for name, (_, extension, bundle) in ASSETS.items():
            if name == "Moonlight-android-release-apk":
                continue
            directory = self.artifacts / name
            directory.mkdir(parents=True)
            if bundle:
                filename = "Moonlight.exe" if "windows" in name else "Moonlight.elf"
            elif name == "Debug.elf":
                filename = "Debug.elf"
            else:
                filename = "Moonlight" + extension
            (directory / filename).write_bytes(name.encode())

        windows = self.artifacts / "Moonlight-windows-ucrt64"
        (windows / "resources").mkdir()
        (windows / "resources" / "icon.png").write_bytes(b"icon")
        switch2 = self.artifacts / "Moonlight-Switch2" / "Moonlight.zip"
        with ZipFile(switch2, "w") as archive:
            archive.writestr("switch/Moonlight-Switch/Moonlight-Switch.nro", b"nro")
        (self.artifacts / "Moonlight-android-build-logs").mkdir()

    def test_all_platforms_and_debug_symbols(self):
        assets = collect("v1.6.0", self.artifacts, self.output)
        self.assertEqual(len(assets), 19)
        self.assertEqual(len({asset.name for asset in assets}), 19)
        self.assertEqual(
            (self.output / "Moonlight-Switch1-v1.6.0-Debug.elf").read_bytes(),
            b"Debug.elf",
        )
        self.assertTrue(
            (self.output / "Moonlight-Android-v1.6.0-unsigned.apk").is_file()
        )
        for architecture in ("x86_64", "arm64"):
            for extension in (".AppImage", ".tar.gz", ".deb", ".rpm"):
                self.assertTrue(
                    (
                        self.output
                        / f"Moonlight-Linux-{architecture}-v1.6.0{extension}"
                    ).is_file()
                )
        with ZipFile(self.output / "Moonlight-Windows-x64-v1.6.0.zip") as archive:
            self.assertEqual(archive.read("resources/icon.png"), b"icon")
            self.assertIn("Moonlight.exe", archive.namelist())
        with ZipFile(self.output / "Moonlight-Switch2-v1.6.0.zip") as archive:
            self.assertEqual(
                archive.read("switch/Moonlight-Switch/Moonlight-Switch.nro"), b"nro"
            )
        with ZipFile(self.output / "Moonlight-Switch2-v1.6.0-Debug.zip") as archive:
            self.assertIn("Moonlight.elf", archive.namelist())

    def test_signed_android(self):
        unsigned = self.artifacts / "Moonlight-android-release-unsigned-apk"
        (unsigned / "Moonlight-unsigned.apk").unlink()
        unsigned.rename(self.artifacts / "Moonlight-android-release-apk")
        (self.artifacts / "Moonlight-android-release-apk" / "signed.apk").touch()
        collect("v12.30.456", self.artifacts, self.output)
        self.assertTrue((self.output / "Moonlight-Android-v12.30.456.apk").is_file())

    def test_rejects_non_release_tags(self):
        for tag in ("v1", "1.6.0", "v1.6.0-rc1", "v1.6.0/other"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                collect(tag, self.artifacts, self.output)
        self.assertFalse(self.output.exists())

    def test_requires_switch_debug_symbols(self):
        (self.artifacts / "Debug.elf" / "Debug.elf").unlink()
        with self.assertRaisesRegex(ValueError, "file count in Debug.elf"):
            collect("v1.6.0", self.artifacts, self.output)
        self.assertFalse(self.output.exists())

    def test_requires_complete_build_set(self):
        (self.artifacts / "Debug.elf").rename(self.root / "missing")
        with self.assertRaisesRegex(ValueError, "missing artifacts:.*Debug.elf"):
            collect("v1.6.0", self.artifacts, self.output)

    def test_refuses_to_overwrite_assets(self):
        self.output.mkdir()
        asset = self.output / "Moonlight-Switch1-v1.6.0-Debug.elf"
        asset.write_bytes(b"keep")
        with self.assertRaisesRegex(ValueError, "already exists"):
            collect("v1.6.0", self.artifacts, self.output)
        self.assertEqual(list(self.output.iterdir()), [asset])
        self.assertEqual(asset.read_bytes(), b"keep")

    def test_rejects_ambiguous_android_artifacts(self):
        for name in ANDROID_ASSETS:
            (self.artifacts / name).mkdir(exist_ok=True)
        with self.assertRaisesRegex(ValueError, "exactly one"):
            collect("v1.6.0", self.artifacts, self.output)


if __name__ == "__main__":
    unittest.main()
