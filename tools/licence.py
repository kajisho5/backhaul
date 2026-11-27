#!/usr/bin/env python3
"""Issue, list and stop backhaul licences (the owner's tool).

  BACKHAUL_GATE_ADMIN_TOKEN=... python3 tools/licence.py issue --seats 5 --days 365 --note "Sample Hall Co."
  BACKHAUL_GATE_ADMIN_TOKEN=... python3 tools/licence.py list
  BACKHAUL_GATE_ADMIN_TOKEN=... python3 tools/licence.py revoke 3
  BACKHAUL_GATE_ADMIN_TOKEN=... python3 tools/licence.py restore 3

The admin token is the Worker secret ADMIN_TOKEN. A new key is printed once and is not
stored anywhere: copy it right away. Only the standard library is used.
"""
import argparse
import datetime
import json
import os
import sys
import urllib.error
import urllib.request

DEFAULT_URL = "https://backhaul-gate.seventh-well.com"


def call(method, path, body=None):
    token = os.environ.get("BACKHAUL_GATE_ADMIN_TOKEN", "")
    if not token:
        sys.exit("BACKHAUL_GATE_ADMIN_TOKEN is not set")
    base = os.environ.get("BACKHAUL_GATE_URL", DEFAULT_URL).rstrip("/")
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(
        base + path,
        data=data,
        method=method,
        headers={"authorization": "Bearer " + token, "content-type": "application/json"},
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except urllib.error.HTTPError as error:
        detail = error.read().decode(errors="replace")
        sys.exit(f"the server answered {error.code}: {detail}")
    except urllib.error.URLError as error:
        sys.exit(f"the server could not be reached: {error.reason}")


def day(seconds):
    if seconds is None:
        return "no end"
    return datetime.datetime.fromtimestamp(seconds, datetime.timezone.utc).strftime("%Y-%m-%d")


def main():
    parser = argparse.ArgumentParser(description="backhaul licences")
    commands = parser.add_subparsers(dest="command", required=True)
    issue = commands.add_parser("issue", help="make a new licence key")
    issue.add_argument("--seats", type=int, required=True, help="how many PCs may use the key at the same time")
    issue.add_argument("--days", type=int, help="how long it lasts; without this it has no end")
    issue.add_argument("--note", default="", help="who it is for (only you see this)")
    commands.add_parser("list", help="show all licences and the PCs that use them")
    for name, text in (("revoke", "stop a licence"), ("restore", "let a stopped licence work again")):
        one = commands.add_parser(name, help=text)
        one.add_argument("id", type=int)
    args = parser.parse_args()

    if args.command == "issue":
        made = call("POST", "/admin/licences", {"seats": args.seats, "days": args.days, "note": args.note})
        print(f"key    {made['key']}")
        print(f"id     {made['id']}")
        print(f"PCs    {made['seats']}")
        print(f"until  {day(made['expiresAt'])}")
    elif args.command == "list":
        for licence in call("GET", "/admin/licences")["licences"]:
            state = licence["problem"] or "ok"
            print(f"#{licence['id']}  ...{licence['keyTail']}  {state:8}  PCs {len(licence['devices'])}/{licence['seats']}"
                  f"  until {day(licence['expiresAt'])}  {licence['note']}")
            for device in licence["devices"]:
                print(f"      {device['name']}  (last used {day(device['lastSeenAt'])})")
    else:
        call("POST", f"/admin/licences/{args.id}/{args.command}")
        print("done")


if __name__ == "__main__":
    main()
