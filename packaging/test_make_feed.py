"""python3 -m unittest discover -s packaging -p "test_*.py"   (needs the "cryptography" package)"""
import base64
import json
import os
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path
from unittest import mock

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

sys.path.insert(0, str(Path(__file__).resolve().parent))
import make_feed  # noqa: E402

NS = {"sparkle": make_feed.SPARKLE}
INSTALLER = b"MZ sample installer for the otter tests " * 100


def new_key():
    key = Ed25519PrivateKey.generate()
    pem = key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption())
    public = base64.b64encode(key.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)).decode()
    return pem, public


class SignTest(unittest.TestCase):
    def test_signature_is_what_the_updater_checks(self):
        pem, public = new_key()
        signature = base64.b64decode(make_feed.sign(INSTALLER, pem, public))
        self.assertEqual(len(signature), 64)
        Ed25519PublicKey.from_public_bytes(base64.b64decode(public)).verify(signature, INSTALLER)  # raises if wrong

    def test_a_key_that_is_not_the_programs_is_refused(self):
        pem, _ = new_key()
        _, other_public = new_key()
        with self.assertRaisesRegex(make_feed.FeedError, "does not belong"):
            make_feed.sign(INSTALLER, pem, other_public)

    def test_things_that_are_not_keys_are_refused(self):
        _, public = new_key()
        with self.assertRaisesRegex(make_feed.FeedError, "not a private key"):
            make_feed.sign(INSTALLER, b"otter", public)


class FeedTest(unittest.TestCase):
    NOTES = {"en": "First line <b>&\nSecond line", "ja": "見本の変更"}

    def test_both_files_say_the_same_version(self):
        latest, appcast = make_feed.make("0.2.0", INSTALLER, "https://dl.example.invalid/a b.exe?x=1&y=2", "U0lH", self.NOTES, 1_800_000_000)
        self.assertEqual(json.loads(latest), {"version": "0.2.0", "notes": self.NOTES})
        root = ET.fromstring(appcast)
        enclosure = root.find("./channel/item/enclosure")
        self.assertEqual(enclosure.get("url"), "https://dl.example.invalid/a b.exe?x=1&y=2")
        self.assertEqual(enclosure.get(f"{{{make_feed.SPARKLE}}}version"), "0.2.0")
        self.assertEqual(enclosure.get(f"{{{make_feed.SPARKLE}}}edSignature"), "U0lH")
        self.assertEqual(enclosure.get(f"{{{make_feed.SPARKLE}}}installerArguments"), "/SILENT /SP- /relaunch=1")
        self.assertEqual(enclosure.get("length"), str(len(INSTALLER)))
        # The notes are text, never markup of their own.
        self.assertEqual(root.find("./channel/item/description").text, "<p>First line &lt;b&gt;&amp;</p><p>Second line</p>")

    def test_refuses_what_would_break_the_updater(self):
        for version in ("0.2", "v0.2.0", "0.2.0-beta", "latest", ""):
            with self.assertRaises(make_feed.FeedError):
                make_feed.make(version, INSTALLER, "https://dl.example.invalid/a.exe", "U0lH", self.NOTES, 0)
        with self.assertRaisesRegex(make_feed.FeedError, "https"):
            make_feed.make("0.2.0", INSTALLER, "http://dl.example.invalid/a.exe", "U0lH", self.NOTES, 0)


class CommandTest(unittest.TestCase):
    def run_main(self, folder: Path, env: dict, notes=True):
        installer = folder / "setup.exe"
        installer.write_bytes(INSTALLER)
        if notes:
            (folder / "notes").mkdir(exist_ok=True)
            (folder / "notes" / "0.2.0.en.txt").write_text("Sample change\n", encoding="utf-8")
        argv = ["--version", "0.2.0", "--installer", str(installer), "--url", "https://dl.example.invalid/setup.exe",
                "--out", str(folder / "feed"), "--notes", str(folder / "notes")]
        with mock.patch.dict(os.environ, env, clear=False):
            return make_feed.main(argv)

    def test_writes_nothing_without_the_key_or_with_a_foreign_one(self):
        pem, _ = new_key()  # not the key of packaging/update_public_key.txt
        for env in ({"UPDATE_SIGNING_KEY": ""}, {"UPDATE_SIGNING_KEY": pem.decode()}):
            with tempfile.TemporaryDirectory() as tmp:
                self.assertEqual(self.run_main(Path(tmp), env), 1)
                self.assertFalse((Path(tmp) / "feed").exists())

    def test_writes_nothing_without_release_notes(self):
        pem, public = new_key()
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(make_feed, "HERE", Path(tmp)):
            (Path(tmp) / "update_public_key.txt").write_text(public + "\n", encoding="ascii")
            self.assertEqual(self.run_main(Path(tmp), {"UPDATE_SIGNING_KEY": pem.decode()}, notes=False), 1)
            self.assertFalse((Path(tmp) / "feed").exists())

    def test_writes_both_files_with_the_programs_key(self):
        pem, public = new_key()
        with tempfile.TemporaryDirectory() as tmp, mock.patch.object(make_feed, "HERE", Path(tmp)):
            (Path(tmp) / "update_public_key.txt").write_text(public + "\n", encoding="ascii")
            self.assertEqual(self.run_main(Path(tmp), {"UPDATE_SIGNING_KEY": pem.decode()}), 0)
            latest = json.loads((Path(tmp) / "feed" / "latest.json").read_text(encoding="utf-8"))
            self.assertEqual(latest, {"version": "0.2.0", "notes": {"en": "Sample change"}})
            enclosure = ET.parse(Path(tmp) / "feed" / "appcast.xml").getroot().find("./channel/item/enclosure")
            signature = base64.b64decode(enclosure.get(f"{{{make_feed.SPARKLE}}}edSignature"))
            Ed25519PublicKey.from_public_bytes(base64.b64decode(public)).verify(signature, INSTALLER)


if __name__ == "__main__":
    unittest.main()
