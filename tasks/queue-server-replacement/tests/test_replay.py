"""Grader: replays recorded qsrv sessions against /app/replacement/qsrv."""
import collections
import json
import os
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
REPLACEMENT = "/app/replacement/qsrv"
ARGV = [sys.executable, "-I", os.path.join(HERE, "run_audited.py"), REPLACEMENT]
BASE_PORT = 31000
WORKERS = 3


def _run(item):
    idx, sess = item
    port = BASE_PORT + idx
    proc = replay.start_server(ARGV, port)
    if proc is None:
        return sess, False, "server did not start listening on port %d" % port
    try:
        ok, msg = replay.check_session(sess, port)
    except Exception as e:  # any client-side surprise is a failure of the session
        ok, msg = False, "error: %r" % (e,)
    finally:
        replay.stop_server(proc)
    return sess, ok, msg


def test_original_binary_absent():
    assert not os.path.exists("/app/bin/qsrv")


def test_replacement_exists():
    assert os.path.isfile(REPLACEMENT), "missing " + REPLACEMENT


def test_recorded_sessions():
    assert os.path.isfile(REPLACEMENT), "missing " + REPLACEMENT
    sessions = json.load(open(os.path.join(HERE, "sessions.json")))
    with ThreadPoolExecutor(WORKERS) as ex:
        results = list(ex.map(_run, enumerate(sessions)))
    per = collections.OrderedDict()
    fails = []
    for sess, ok, msg in results:
        p = per.setdefault(sess["category"], [0, 0])
        p[1] += 1
        if ok:
            p[0] += 1
        else:
            fails.append((sess["id"], msg))
    lines = ["%-14s %5s %5s" % ("category", "pass", "total")]
    for cat, (a, b) in per.items():
        lines.append("%-14s %5d %5d" % (cat, a, b))
    total = sum(b for _, b in per.values())
    passed = sum(a for a, _ in per.values())
    lines.append("%-14s %5d %5d" % ("ALL", passed, total))
    report = "\n".join(lines)
    print(report)
    for sid, msg in fails[:25]:
        print("FAIL", sid, msg[:500])
    try:
        os.makedirs("/logs/verifier", exist_ok=True)
        with open("/logs/verifier/sessions_report.txt", "w") as f:
            f.write(report + "\n")
            for sid, msg in fails:
                f.write("FAIL %s %s\n" % (sid, msg[:1000]))
    except OSError:
        pass
    assert not fails, "%d of %d sessions differ from the original server" % (len(fails), total)
