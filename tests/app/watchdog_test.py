"""The watcher, for real, on Windows: python watchdog_test.py <watchdog_probe.exe>

Runs a stand-in program that starts the real watcher and then ends in three ways:
  crash         -> started again once
  clean         -> not started again, no mark left
  crash-always  -> started again twice; the third crash is left alone (the limit)
The folder has a space and Japanese letters in its name on purpose.
"""
import os
import subprocess
import sys
import tempfile
import time


def lines(path):
    try:
        with open(path, encoding="utf-8") as f:
            return [line for line in f.read().splitlines() if line]
    except FileNotFoundError:
        return []


def run(probe, plan, base):
    folder = os.path.join(base, plan + " 架空 フォルダ")
    os.makedirs(folder)
    env = dict(os.environ, BACKHAUL_PROBE_FOLDER=folder, BACKHAUL_PROBE_PLAN=plan)
    subprocess.run([probe], env=env, timeout=60, check=False)
    return folder


def wait_for(folder, count, seconds):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if len(lines(os.path.join(folder, "resumed.txt"))) >= count:
            return True
        time.sleep(0.2)
    return False


def main():
    probe = sys.argv[1]
    problems = []
    with tempfile.TemporaryDirectory(ignore_cleanup_errors=True) as base:
        crash = run(probe, "crash", base)
        clean = run(probe, "clean", base)
        always = run(probe, "crash-always", base)

        if not wait_for(crash, 1, 30):
            problems.append("crash: the program was not started again")
        if len(lines(os.path.join(crash, "restarts.txt"))) != 1:
            problems.append("crash: the restart was not written down once")

        wait_for(always, 2, 60)
        time.sleep(8)  # long enough for a third start, if the limit did not hold
        again = len(lines(os.path.join(always, "resumed.txt")))
        if again != 2:
            problems.append(f"crash-always: started again {again} times, wanted 2")

        if lines(os.path.join(clean, "resumed.txt")):
            problems.append("clean: the program was started again after a normal exit")
        if os.path.exists(os.path.join(clean, "running")):
            problems.append("clean: the mark was left behind")
        if len(lines(os.path.join(crash, "resumed.txt"))) != 1:
            problems.append("crash: started again more than once")

    for problem in problems:
        print("FAILED:", problem)
    if not problems:
        print("watcher: 3 ways of ending, all as wanted")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
