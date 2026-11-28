#!/usr/bin/env python3
"""Signs an installer and writes the two small files the program reads to find updates.

  latest.json   read by backhaul itself: which version is the newest, and what is new
  appcast.xml   read by the updater (WinSparkle): where the installer is, and its signature

The signature is Ed25519 over the installer's bytes, the way WinSparkle checks it. The
private key comes from the environment (UPDATE_SIGNING_KEY, PEM) and is never written
anywhere; before anything is written, the signature is checked against the public key
that is built into the program (packaging/update_public_key.txt), so that a wrong key
cannot produce a feed no installed program would accept.

  python3 packaging/make_feed.py --version 0.2.0 --installer out/backhaul-0.2.0-setup.exe \
      --url https://…/backhaul-0.2.0-setup.exe --out out/feed
"""
import argparse
import base64
import json
import os
import re
import sys
import time
from email.utils import formatdate
from pathlib import Path
from xml.sax.saxutils import escape, quoteattr

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.hazmat.primitives.serialization import load_pem_private_key

HERE = Path(__file__).resolve().parent
SPARKLE = "http://www.andymatuschak.org/xml-namespaces/sparkle"
# How the updater runs the installer: no questions, and the program starts again afterwards.
INSTALLER_ARGUMENTS = "/SILENT /SP- /relaunch=1"


class FeedError(Exception):
    pass


def sign(installer: bytes, private_pem: bytes, public_b64: str) -> str:
    """The signature as the appcast carries it (base64). Fails when the key is not the program's."""
    try:
        key = load_pem_private_key(private_pem, password=None)
    except (ValueError, TypeError) as error:
        raise FeedError("UPDATE_SIGNING_KEY is not a private key in PEM form") from error
    if not isinstance(key, Ed25519PrivateKey):
        raise FeedError("UPDATE_SIGNING_KEY is not an Ed25519 key")
    signature = key.sign(installer)
    try:
        public = Ed25519PublicKey.from_public_bytes(base64.b64decode(public_b64, validate=True))
        public.verify(signature, installer)
    except (ValueError, InvalidSignature) as error:
        raise FeedError("the signing key does not belong to packaging/update_public_key.txt") from error
    return base64.b64encode(signature).decode("ascii")


def read_notes(folder: Path, version: str) -> dict:
    notes = {}
    for lang in ("en", "ja"):
        path = folder / f"{version}.{lang}.txt"
        if path.is_file():
            notes[lang] = path.read_text(encoding="utf-8").strip()
    if not notes.get("en"):
        raise FeedError(f"no release notes: {folder / (version + '.en.txt')}")
    return notes


def make(version: str, installer: bytes, url: str, signature: str, notes: dict, now: float):
    if not re.fullmatch(r"\d{1,6}\.\d{1,6}\.\d{1,6}", version):
        raise FeedError(f"not a version number: {version!r}")
    if not url.startswith("https://"):
        raise FeedError("the installer must be fetched over https")
    latest = json.dumps({"version": version, "notes": notes}, ensure_ascii=False, indent=2) + "\n"
    html = "".join(f"<p>{escape(line)}</p>" for line in notes["en"].splitlines() if line.strip())
    appcast = f"""<?xml version="1.0" encoding="utf-8"?>
<rss version="2.0" xmlns:sparkle="{SPARKLE}">
  <channel>
    <title>backhaul</title>
    <link>https://backhaul.seventh-well.com/</link>
    <description>Updates of backhaul</description>
    <language>en</language>
    <item>
      <title>Version {version}</title>
      <description><![CDATA[{html.replace("]]>", "]]&gt;")}]]></description>
      <pubDate>{formatdate(now, usegmt=True)}</pubDate>
      <enclosure url={quoteattr(url)} sparkle:version="{version}" sparkle:os="windows"
                 length="{len(installer)}" type="application/octet-stream"
                 sparkle:edSignature="{signature}" sparkle:installerArguments="{INSTALLER_ARGUMENTS}"/>
    </item>
  </channel>
</rss>
"""
    return latest, appcast


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--version", required=True)
    parser.add_argument("--installer", required=True, type=Path)
    parser.add_argument("--url", required=True, help="where the installer will be downloaded from")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--notes", type=Path, default=HERE / "notes")
    args = parser.parse_args(argv)
    try:
        private_pem = os.environ.get("UPDATE_SIGNING_KEY", "").encode()
        if not private_pem.strip():
            raise FeedError("UPDATE_SIGNING_KEY is not set")
        public_b64 = (HERE / "update_public_key.txt").read_text(encoding="ascii").strip()
        installer = args.installer.read_bytes()
        if len(installer) < 1024:
            raise FeedError(f"{args.installer} is too small to be an installer")
        latest, appcast = make(args.version, installer, args.url, sign(installer, private_pem, public_b64),
                               read_notes(args.notes, args.version), time.time())
    except (FeedError, OSError) as error:
        print(f"make_feed: {error}", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "latest.json").write_text(latest, encoding="utf-8", newline="\n")
    (args.out / "appcast.xml").write_text(appcast, encoding="utf-8", newline="\n")
    print(f"wrote {args.out / 'latest.json'} and {args.out / 'appcast.xml'} for version {args.version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
