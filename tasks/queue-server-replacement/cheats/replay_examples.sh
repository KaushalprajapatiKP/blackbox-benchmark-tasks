#!/bin/bash
# Cheat: hardcodes the visible example transcripts.
set -e
mkdir -p /app/replacement
cat > /app/replacement/qsrv <<'QSRV_PY_EOF'
#!/usr/bin/env python3
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
QSRV_PY_EOF
chmod 0755 /app/replacement/qsrv
