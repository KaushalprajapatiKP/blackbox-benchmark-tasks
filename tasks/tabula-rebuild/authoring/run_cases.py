#!/usr/bin/env python3
"""Run case specs against a command. MIT.

record:  run_cases.py record CASES.json OUT.json CMD...
compare: run_cases.py compare FIXTURE.json CMD...
"""
import base64
import json
import os
import subprocess
import sys
import tempfile


def d(s):
    return base64.b64decode(s)


def run_case(cs, cmd):
    with tempfile.TemporaryDirectory() as td:
        for name, data in cs["files"].items():
            p = os.path.join(td, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as fh:
                fh.write(d(data))
        args = [os.fsencode(a) for a in cs["args"]]
        env = {"PATH": "/usr/local/bin:/usr/bin:/bin", "LC_ALL": "C.UTF-8"}
        p = subprocess.run(cmd + args, input=d(cs["stdin"]), cwd=td, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
        return p.stdout, p.stderr, p.returncode


def main():
    mode = sys.argv[1]
    cases = json.load(open(sys.argv[2]))
    if mode == "record":
        out = sys.argv[3]
        cmd = [os.fsencode(c) for c in sys.argv[4:]]
        for cs in cases:
            o, e, c = run_case(cs, cmd)
            cs["stdout"], cs["stderr"], cs["code"] = (base64.b64encode(o).decode(),
                                                      base64.b64encode(e).decode(), c)
        json.dump(cases, open(out, "w"), indent=0)
        print("recorded", len(cases))
        return
    cmd = [os.fsencode(c) for c in sys.argv[3:]]
    bad = 0
    per = {}
    for cs in cases:
        o, e, c = run_case(cs, cmd)
        ok = o == d(cs["stdout"]) and e == d(cs["stderr"]) and c == cs["code"]
        t = per.setdefault(cs["cat"], [0, 0])
        t[1] += 1
        if ok:
            t[0] += 1
        else:
            bad += 1
            if bad <= 15:
                print("FAIL", cs["id"], cs["args"])
                print("  exp", c if False else cs["code"], d(cs["stdout"])[:300], d(cs["stderr"])[:300])
                print("  got", c, o[:300], e[:300])
    for k, v in per.items():
        print("%-10s %d/%d" % (k, v[0], v[1]))
    print("TOTAL fails", bad, "of", len(cases))


if __name__ == "__main__":
    main()
