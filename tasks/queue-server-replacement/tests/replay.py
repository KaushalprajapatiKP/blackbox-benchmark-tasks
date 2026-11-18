"""Session replay client shared by the recorder (authoring) and the grader.

A session is {"id", "category", "conns": N, "steps": [...]}. Each step is
{"c": i, "send": <latin-1 str>} or {"c": i, "close": true}; recorded sessions add
"out": {conn_index: <latin-1 str>} and "eof": [conn_index, ...].
"""
import os
import select
import socket
import subprocess
import time

HOST = "127.0.0.1"


def listening(port):
    want = ":%04X" % port
    for path in ("/proc/net/tcp", "/proc/net/tcp6"):
        try:
            with open(path) as f:
                next(f)
                for line in f:
                    parts = line.split()
                    if parts[1].endswith(want) and parts[3] == "0A":
                        return True
        except OSError:
            continue
    return False


def wait_listening(port, proc, timeout=10.0):
    end = time.time() + timeout
    while time.time() < end:
        if proc.poll() is not None:
            return False
        if listening(port):
            return True
        time.sleep(0.01)
    return False


def start_server(argv, port):
    proc = subprocess.Popen(argv + ["--port", str(port)], stdin=subprocess.DEVNULL,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not wait_listening(port, proc):
        stop_server(proc)
        return None
    return proc


def stop_server(proc):
    if proc is None:
        return
    try:
        proc.kill()
    except OSError:
        pass
    try:
        proc.wait(timeout=5)
    except Exception:
        pass


def open_conns(port, n):
    socks = []
    for _ in range(n):
        s = socket.create_connection((HOST, port), timeout=5)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.setblocking(False)
        socks.append(s)
        time.sleep(0.005)
    return socks


def _send(s, data):
    s.setblocking(True)
    s.settimeout(5)
    try:
        s.sendall(data)
    finally:
        s.setblocking(False)


def record_session(sess, port, idle=0.04):
    """Run a session against a live server and capture output per step."""
    socks = open_conns(port, sess["conns"])
    closed = set()
    for st in sess["steps"]:
        c = st["c"]
        if st.get("close"):
            if c not in closed:
                try:
                    socks[c].shutdown(socket.SHUT_WR)
                except OSError:
                    pass
        elif c not in closed:
            try:
                _send(socks[c], st["send"].encode("latin-1"))
            except OSError:
                pass
        out = {}
        eof = []
        last = time.time()
        while True:
            live = [i for i in range(len(socks)) if i not in closed]
            if not live:
                break
            r, _, _ = select.select([socks[i] for i in live], [], [], idle)
            if not r:
                if time.time() - last >= idle:
                    break
                continue
            for i in live:
                if socks[i] in r:
                    try:
                        d = socks[i].recv(65536)
                    except (BlockingIOError, InterruptedError):
                        continue
                    except OSError:
                        d = b""
                    if d:
                        out[i] = out.get(i, b"") + d
                    else:
                        eof.append(i)
                        closed.add(i)
                        socks[i].close()
            last = time.time()
        st["out"] = {str(k): v.decode("latin-1") for k, v in out.items()}
        st["eof"] = sorted(eof)
    for i, s in enumerate(socks):
        if i not in closed:
            s.close()
    return sess


def _read_exact(s, want, timeout):
    """Read len(want) bytes; stop early on mismatch. Returns (ok, got)."""
    got = b""
    end = time.time() + timeout
    while len(got) < len(want):
        rem = end - time.time()
        if rem <= 0:
            return False, got
        r, _, _ = select.select([s], [], [], rem)
        if not r:
            return False, got
        try:
            d = s.recv(len(want) - len(got))
        except (BlockingIOError, InterruptedError):
            continue
        except OSError:
            d = b""
        if not d:
            return False, got + b"<EOF>"
        got += d
        if not want.startswith(got):
            return False, got
    return True, got


def _read_eof(s, timeout):
    end = time.time() + timeout
    while True:
        rem = end - time.time()
        if rem <= 0:
            return False, b"<no EOF>"
        r, _, _ = select.select([s], [], [], rem)
        if not r:
            return False, b"<no EOF>"
        try:
            d = s.recv(4096)
        except (BlockingIOError, InterruptedError):
            continue
        except OSError:
            return True, b""
        if not d:
            return True, b""
        return False, d


def check_session(sess, port, timeout=2.0, settle=0.1):
    """Replay a recorded session; returns (ok, message)."""
    try:
        socks = open_conns(port, sess["conns"])
    except OSError as e:
        return False, "connect failed: %s" % e
    closed = set()
    try:
        for n, st in enumerate(sess["steps"]):
            c = st["c"]
            if st.get("close"):
                if c not in closed:
                    try:
                        socks[c].shutdown(socket.SHUT_WR)
                    except OSError:
                        pass
            elif c not in closed:
                try:
                    _send(socks[c], st["send"].encode("latin-1"))
                except OSError as e:
                    return False, "step %d: send failed: %s" % (n, e)
            outs = {int(k): v.encode("latin-1") for k, v in st["out"].items()}
            if not outs and not st["eof"]:
                time.sleep(settle)
            for i in sorted(set(outs) | set(st["eof"])):
                if i in closed:
                    return False, "step %d: conn %d already closed" % (n, i)
                want = outs.get(i, b"")
                ok, got = _read_exact(socks[i], want, timeout)
                if not ok:
                    return False, "step %d conn %d: expected %r got %r" % (n, i, want[:300], got[:300])
                if i in st["eof"]:
                    ok, extra = _read_eof(socks[i], timeout)
                    if not ok:
                        return False, "step %d conn %d: expected close, got %r" % (n, i, extra[:200])
                    closed.add(i)
                    socks[i].close()
        # nothing else may arrive
        time.sleep(settle)
        for i, s in enumerate(socks):
            if i in closed:
                continue
            r, _, _ = select.select([s], [], [], 0)
            if r:
                try:
                    d = s.recv(4096)
                except OSError:
                    d = b""
                if d:
                    return False, "unexpected trailing output on conn %d: %r" % (i, d[:200])
        return True, "ok"
    finally:
        for i, s in enumerate(socks):
            if i not in closed:
                try:
                    s.close()
                except OSError:
                    pass


def free_port(base, idx):
    return base + (idx % 2000)
