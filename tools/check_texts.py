#!/usr/bin/env python3
"""Fails when a Japanese text of the app has no English in src/app/dictionary_en.cpp,
or when a Japanese text in the screens is not marked for translation.

Run from the repository root: python3 tools/check_texts.py
"""
import glob
import re
import sys

JAPANESE = re.compile(r"[ぁ-んァ-ヶ一-龠々ー]")
LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
# Shown the same in every language.
NOT_TRANSLATED = {"日本語"}


def code_part(line: str) -> str:
    """The line without a trailing // comment (a // inside a text stays)."""
    inside = False
    for i, ch in enumerate(line):
        if ch == '"' and (i == 0 or line[i - 1] != "\\"):
            inside = not inside
        if not inside and line[i : i + 2] == "//":
            return line[:i]
    return line


def main() -> int:
    table = open("src/app/dictionary_en.cpp", encoding="utf-8").read()
    known = set(re.findall(r'^\s*\{"((?:[^"\\\n]|\\.)*)", "', table, flags=re.M))
    problems = []

    for path in sorted(glob.glob("src/app/qml/*.qml")):
        for number, line in enumerate(open(path, encoding="utf-8"), 1):
            code = code_part(line)
            for match in LITERAL.finditer(code):
                text = match.group(1)
                if not JAPANESE.search(text) or text in NOT_TRANSLATED:
                    continue
                if not code[: match.start()].rstrip().endswith("qsTr("):
                    problems.append(f"{path}:{number}: not inside qsTr(): {text}")
                elif text not in known:
                    problems.append(f"{path}:{number}: no English for: {text}")

    sources = [p for p in sorted(glob.glob("src/app/*.cpp") + glob.glob("src/app/*.h")) if "dictionary_en" not in p]
    for path in sources:
        for number, line in enumerate(open(path, encoding="utf-8"), 1):
            code = code_part(line)
            for match in LITERAL.finditer(code):
                text = match.group(1)
                if not JAPANESE.search(text):
                    continue
                if not code[: match.start()].rstrip().endswith("T("):
                    problems.append(f"{path}:{number}: not inside T(): {text}")
                elif text not in known:
                    problems.append(f"{path}:{number}: no English for: {text}")

    # What the parts without Qt say reaches the screen through T(std::string).
    lower = sorted(sum((glob.glob(f"src/{part}/*.cpp") for part in ("zoom", "audio", "video", "plugins", "core")), []))
    for path in lower:
        for number, line in enumerate(open(path, encoding="utf-8"), 1):
            for match in LITERAL.finditer(code_part(line)):
                text = match.group(1)
                if JAPANESE.search(text) and text not in known:
                    problems.append(f"{path}:{number}: no English for: {text}")

    for problem in problems:
        print(problem)
    print(f"{len(known)} texts have English; {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
