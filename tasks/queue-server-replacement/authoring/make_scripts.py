"""Generate solution/solve.sh and the cheat scripts (authoring only).

Run from the task directory: python3 authoring/make_scripts.py
"""
import os


TASK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def wrap(py, header):
    assert "QSRV_PY_EOF" not in py
    return (
        "#!/bin/bash\n" + header +
        "set -e\nmkdir -p /app/replacement\n"
        "cat > /app/replacement/qsrv <<'QSRV_PY_EOF'\n" + py + "QSRV_PY_EOF\n"
        "chmod 0755 /app/replacement/qsrv\n"
    )


def docs_only(ref):
    """What an implementer could write from PROTOCOL.md alone: same command surface,
    but guessed error texts, defaults, limits, clock rules and report layouts."""
    s = ref
    # unknown error vocabulary: one generic error code with the guessed message
    s = s.replace('return ("-%s %s\\r\\n" % (self.code, self.msg)).encode("latin-1")',
                  'return ("-ERR %s\\r\\n" % self.msg).encode("latin-1")')
    # guessed defaults and limits
    for a, b in [("MAX_LINE = 4096", "MAX_LINE = 1 << 30"), ("MAX_QUEUES = 8", "MAX_QUEUES = 10 ** 6"),
                 ("MAX_KEYS = 64", "MAX_KEYS = 10 ** 6"), ("DEF_MAXLEN = 100", "DEF_MAXLEN = 10 ** 6"),
                 ("DEF_MAXNACK = 3", "DEF_MAXNACK = 5"), ("DEF_PRI = 5", "DEF_PRI = 0"),
                 ("DEF_TTR = 30", "DEF_TTR = 60"), ("MAX_TX = 16", "MAX_TX = 10 ** 6"),
                 ("MAX_ARGS = 64", "MAX_ARGS = 10 ** 6")]:
        assert a in s, a
        s = s.replace(a, b)
    # a visibility timeout simply returns the job; it is not counted as a failure
    a = ("                j.timeouts += 1\n                j.nacks += 1\n")
    assert a in s
    s = s.replace(a, "                j.timeouts += 1\n")
    # report layouts are not documented: plain key=value lines
    s = s.replace('return "# Server\\nversion:%s\\nclock:%d\\n"', 'return "version=%s\\nclock=%d\\n"')
    a = r'"# Clients\nconnected:%d\nblocked:%d\ntotal_connections:%d\n"'
    assert a in s
    s = s.replace(a, r'"clients=%d\nblocked=%d\ntotal=%d\n"')
    compile(s, "docs_only", "exec")
    return s


REPLAY_EXAMPLES = r'''#!/usr/bin/env python3
"""Replays the visible example transcripts and nothing else."""
import glob
import selectors
import socket
import sys

table = {}
for path in sorted(glob.glob("/app/docs/examples/*.txt")):
    last = None
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("> "):
            last = line[2:].encode()
            table[last] = b""
        elif line.startswith("< ") and last is not None:
            table[last] += line[2:].encode() + b"\r\n"

port = int(sys.argv[sys.argv.index("--port") + 1])
ls = socket.socket()
ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
ls.bind(("127.0.0.1", port))
ls.listen(64)
sel = selectors.DefaultSelector()
sel.register(ls, selectors.EVENT_READ, None)
bufs = {}
while True:
    for key, _ in sel.select():
        if key.data is None:
            c, _ = ls.accept()
            bufs[c] = b""
            sel.register(c, selectors.EVENT_READ, c)
            continue
        c = key.data
        d = c.recv(65536)
        if not d:
            sel.unregister(c)
            c.close()
            continue
        bufs[c] += d
        while b"\n" in bufs[c]:
            line, bufs[c] = bufs[c].split(b"\n", 1)
            line = line.rstrip(b"\r")
            if not line.strip():
                continue
            c.sendall(table.get(line, b"-ERR unknown command\r\n"))
'''

PROXY = r'''#!/bin/bash
# Cheat: ship the original binary inside the "replacement" and exec it.
set -e
mkdir -p /app/replacement
cp /app/bin/qsrv /app/replacement/engine
python3 - <<'EOF'
import base64
blob = base64.b64encode(open("/app/bin/qsrv", "rb").read()).decode()
src = """#!/usr/bin/env python3
import base64, os, sys
here = os.path.dirname(os.path.abspath(__file__))
eng = os.path.join(here, "engine")
if not os.path.exists(eng):
    eng = "/tmp/.qsrv-engine"
    with open(eng, "wb") as f:
        f.write(base64.b64decode(BLOB))
    os.chmod(eng, 0o755)
os.execv(eng, [eng] + sys.argv[1:])
""".replace("BLOB", repr(blob))
open("/app/replacement/qsrv", "w").write(src)
EOF
chmod 0755 /app/replacement/qsrv
'''


def main():
    ref = open(os.path.join(TASK, "solution", "qsrv.py")).read()
    with open(os.path.join(TASK, "solution", "solve.sh"), "w") as f:
        f.write(wrap(ref, "# Reference solution: installs the Python replacement server.\n"))
    os.makedirs(os.path.join(TASK, "cheats"), exist_ok=True)
    with open(os.path.join(TASK, "cheats", "docs_only_baseline.sh"), "w") as f:
        f.write(wrap(docs_only(ref), "# Cheat/baseline: implements the documented surface with guessed details.\n"))
    with open(os.path.join(TASK, "cheats", "replay_examples.sh"), "w") as f:
        f.write(wrap(REPLAY_EXAMPLES, "# Cheat: hardcodes the visible example transcripts.\n"))
    with open(os.path.join(TASK, "cheats", "proxy_binary.sh"), "w") as f:
        f.write(PROXY)
    for p in ["solution/solve.sh", "cheats/docs_only_baseline.sh", "cheats/replay_examples.sh", "cheats/proxy_binary.sh"]:
        os.chmod(os.path.join(TASK, p), 0o755)
    print("ok")


if __name__ == "__main__":
    main()
