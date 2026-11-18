"""Record sessions from a reference server, or check a server against recordings.

record: python3 record.py record SERVER_ARGV_JSON IN.json OUT.json [workers]
check:  python3 record.py check  SERVER_ARGV_JSON RECORDED.json [workers]
"""
import json
import os
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests"))
import replay  # noqa: E402

BASE = 23000


def run_one(args):
    mode, argv, sess, idx = args
    port = BASE + idx
    proc = replay.start_server(argv, port)
    if proc is None:
        return idx, None, "server did not start"
    try:
        if mode == "record":
            return idx, replay.record_session(sess, port), "ok"
        ok, msg = replay.check_session(sess, port)
        return idx, ok, msg
    finally:
        replay.stop_server(proc)


def main():
    mode = sys.argv[1]
    argv = json.loads(sys.argv[2])
    sessions = json.load(open(sys.argv[3]))
    workers = int(sys.argv[5] if mode == "record" and len(sys.argv) > 5 else
                  (sys.argv[4] if mode == "check" and len(sys.argv) > 4 else 8))
    jobs = [(mode, argv, s, i) for i, s in enumerate(sessions)]
    with ThreadPoolExecutor(workers) as ex:
        res = list(ex.map(run_one, jobs))
    if mode == "record":
        out = [r[1] for r in res]
        if any(o is None for o in out):
            print("FAILED to record some sessions")
            sys.exit(1)
        json.dump(out, open(sys.argv[4], "w"))
        print("recorded", len(out))
    else:
        bad = [(sessions[i]["id"], m) for i, ok, m in res if not ok]
        print("passed %d/%d" % (len(res) - len(bad), len(res)))
        for sid, m in bad[:15]:
            print(" FAIL", sid, m[:400])
        sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
