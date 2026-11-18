#!/bin/bash
# Cheat/baseline: implements the documented surface with guessed details.
set -e
mkdir -p /app/replacement
cat > /app/replacement/qsrv <<'QSRV_PY_EOF'
#!/usr/bin/env python3
"""Reference drop-in replacement for qsrv (Python 3 standard library only).

MIT License, Copyright (c) 2026 The qsrv author.
"""
import selectors
import socket
import sys

VERSION = "2.3.1"
MAX_LINE = 1 << 30
MAX_ARGS = 10 ** 6
MAX_CONNS = 32
MAX_TX = 10 ** 6
MAX_KEYLEN = 64
MAX_VALLEN = 512
MAX_QNAME = 32
MAX_NSNAME = 16
MAX_QUEUES = 10 ** 6
MAX_KEYS = 10 ** 6
DEF_MAXLEN = 10 ** 6
DEF_MAXNACK = 5
DEF_PRI = 0
DEF_TTR = 60
INT_LIMIT = 10 ** 15

READY, DELAYED, RESERVED, DEAD = 1, 2, 3, 4
STATE_NAMES = {READY: "ready", DELAYED: "delayed", RESERVED: "reserved", DEAD: "dead"}


class Err(Exception):
    def __init__(self, code, msg):
        self.code = code
        self.msg = msg

    def reply(self):
        return ("-ERR %s\r\n" % self.msg).encode("latin-1")


def r_simple(s):
    return b"+" + s.encode() + b"\r\n"


def r_int(v):
    return b":" + str(v).encode() + b"\r\n"


def r_bulk(b):
    return b"$" + str(len(b)).encode() + b"\r\n" + b + b"\r\n"


NILBULK = b"$-1\r\n"
NILARR = b"*-1\r\n"


def r_arr(n):
    return b"*" + str(n).encode() + b"\r\n"


def sanitize(word):
    out = []
    for c in word[:24]:
        if c < 0x20 or c >= 0x7F:
            out.append("?")
        elif 0x61 <= c <= 0x7A:
            out.append(chr(c - 32))
        else:
            out.append(chr(c))
    return "".join(out)


NAMECH = set(b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")


def valid_name(t, maxlen, allow_dot):
    if not (1 <= len(t) <= maxlen):
        return False
    for c in t:
        if c in NAMECH or (allow_dot and c == 0x2E):
            continue
        return False
    return True


def parse_num(t, lo, hi, what):
    i = 0
    neg = False
    if t[:1] == b"-":
        neg = True
        i = 1
    digits = t[i:]
    if not digits or any(c < 0x30 or c > 0x39 for c in digits):
        raise Err("E_NUM", "value for '%s' is not an integer" % what)
    if len(digits) > 18:
        raise Err("E_RANGE", "value for '%s' must be between %d and %d" % (what, lo, hi))
    v = int(digits)
    if neg:
        v = -v
    if v < lo or v > hi:
        raise Err("E_RANGE", "value for '%s' must be between %d and %d" % (what, lo, hi))
    return v


def ieq(t, s):
    return t.upper() == s.encode() if all(c < 0x80 for c in t) else False


HEX = b"0123456789abcdefABCDEF"
ESC = {ord("n"): 10, ord("t"): 9, ord("r"): 13, ord("0"): 0, ord("\\"): 92, ord('"'): 34}


def lex(s):
    """Returns list of byte tokens; raises Err."""
    for c in s:
        if (c < 0x20 and c != 9) or c == 0x7F:
            raise Err("E_CTRL", "control character in command")
    toks = []
    n = len(s)
    i = 0
    while True:
        while i < n and s[i] in (32, 9):
            i += 1
        if i >= n:
            break
        if len(toks) == MAX_ARGS:
            raise Err("E_ARGS", "too many arguments")
        t = bytearray()
        c = s[i]
        if c == 0x22:
            i += 1
            closed = False
            while i < n:
                ch = s[i]
                if ch == 0x22:
                    closed = True
                    i += 1
                    break
                if ch == 0x5C:
                    if i + 1 >= n:
                        raise Err("E_QUOTE", "unbalanced quotes")
                    e = s[i + 1]
                    if e in ESC:
                        t.append(ESC[e])
                        i += 2
                        continue
                    if e == ord("x"):
                        if i + 3 < n and s[i + 2] in HEX and s[i + 3] in HEX:
                            t.append(int(bytes(s[i + 2:i + 4]), 16))
                            i += 4
                            continue
                    raise Err("E_ESCAPE", "invalid escape sequence")
                t.append(ch)
                i += 1
            if not closed:
                raise Err("E_QUOTE", "unbalanced quotes")
            if i < n and s[i] not in (32, 9):
                raise Err("E_QUOTE", "closing quote must be followed by a space")
        elif c == 0x27:
            i += 1
            closed = False
            while i < n:
                if s[i] == 0x27:
                    closed = True
                    i += 1
                    break
                t.append(s[i])
                i += 1
            if not closed:
                raise Err("E_QUOTE", "unbalanced quotes")
            if i < n and s[i] not in (32, 9):
                raise Err("E_QUOTE", "closing quote must be followed by a space")
        else:
            while i < n and s[i] not in (32, 9):
                t.append(s[i])
                i += 1
        toks.append(bytes(t))
    return toks


class Job:
    __slots__ = ("id", "ns", "q", "payload", "pri", "ttr", "state", "ready_at",
                 "deadline", "owner", "nacks", "reserves", "timeouts", "seq")


class Queue:
    def __init__(self, ns, name, maxlen, maxnack):
        self.ns, self.name, self.maxlen, self.maxnack = ns, name, maxlen, maxnack
        self.pushed = self.acked = self.nacked = self.timeouts = 0


class Key:
    def __init__(self, ns, k):
        self.ns, self.k = ns, k
        self.v = b""
        self.version = 0
        self.expire_at = 0


class Conn:
    def __init__(self, sock, cid):
        self.sock = sock
        self.id = cid
        self.ns = "default"
        self.inbuf = bytearray()
        self.blocked = False
        self.woken = False
        self.wq = None
        self.wns = None
        self.wdeadline = -1
        self.wseq = 0
        self.inmulti = False
        self.txabort = False
        self.tx = []
        self.closing = False
        self.used = True


def parse_opts(args, start, spec):
    """spec: list of (NAME, what, has_value, lo, hi). returns dict NAME->value (True for flags)."""
    seen = {}
    i = start
    while i < len(args):
        t = args[i]
        found = None
        for sp in spec:
            if ieq(t, sp[0]):
                found = sp
                break
        if not found:
            raise Err("E_SYNTAX", "unknown option '%s'" % sanitize(t))
        name = found[0]
        if name in seen:
            raise Err("E_DUPOPT", "duplicate option '%s'" % name)
        if found[2]:
            if i + 1 >= len(args):
                raise Err("E_SYNTAX", "option '%s' requires a value" % name)
            seen[name] = parse_num(args[i + 1], found[3], found[4], found[1])
            i += 2
        else:
            seen[name] = True
            i += 1
    return seen


def glob_match(p, s):
    pi = si = 0
    star_p = -1
    star_s = 0
    while si < len(s):
        if pi < len(p) and (p[pi] == 0x3F or p[pi] == s[si]):
            pi += 1
            si += 1
        elif pi < len(p) and p[pi] == 0x2A:
            star_p = pi
            pi += 1
            star_s = si
        elif star_p != -1:
            pi = star_p + 1
            star_s += 1
            si = star_s
        else:
            return False
    while pi < len(p) and p[pi] == 0x2A:
        pi += 1
    return pi == len(p)


def stored_int(v):
    i = 1 if v[:1] == b"-" else 0
    d = v[i:]
    if not d or len(d) > 18 or any(c < 0x30 or c > 0x39 for c in d):
        return None
    x = int(d)
    return -x if i else x


ARITY = {
    "PING": (0, 1), "ECHO": (1, 1), "USE": (1, 1), "WHOAMI": (0, 0), "QUIT": (0, 0),
    "TICK": (1, 1), "TIME": (0, 0), "QDECLARE": (1, 5), "QDROP": (1, 1), "QLIST": (0, 0),
    "PUSH": (2, 8), "RESERVE": (1, 1), "BRESERVE": (2, 2), "ACK": (1, 1), "NACK": (1, 3),
    "RELEASE": (1, 5), "TOUCH": (1, 1), "PEEK": (1, 1), "QSTAT": (1, 1), "DEADLIST": (1, 2),
    "KICK": (1, 2), "JOBINFO": (1, 1), "SET": (2, 6), "GET": (1, 1), "GETV": (1, 1),
    "CAS": (3, 3), "DEL": (1, -1), "INCR": (1, 2), "KEYS": (0, 1), "TTL": (1, 1),
    "EXPIRE": (2, 2), "INFO": (0, 1), "FLUSH": (0, 0), "MULTI": (0, 0), "EXEC": (0, 0),
    "DISCARD": (0, 0),
}

BLOCK = object()
CLOSE = object()


class Server:
    def __init__(self):
        self.jobs = {}      # id -> Job (insertion order = creation order)
        self.queues = []    # list of Queue
        self.keys = []      # list of Key
        self.conns = []     # live Conn objects
        self.clock = 0
        self.next_job = 1
        self.seq = 0
        self.wseq = 0
        self.next_conn = 1
        self.commands = 0
        self.total_conns = 0
        self.pushed = self.acked = self.expired = 0

    # ---------------- helpers
    def find_queue(self, ns, name):
        for q in self.queues:
            if q.ns == ns and q.name == name:
                return q
        return None

    def qjobs(self, ns, name):
        return [j for j in self.jobs.values() if j.ns == ns and j.q == name]

    def live_count(self, q):
        return sum(1 for j in self.qjobs(q.ns, q.name) if j.state != DEAD)

    def best_ready(self, ns, name):
        best = None
        for j in self.jobs.values():
            if j.state != READY or j.ns != ns or j.q != name:
                continue
            if best is None or (j.pri, j.seq) < (best.pri, best.seq):
                best = j
        return best

    def find_key(self, ns, k):
        for x in self.keys:
            if x.ns == ns and x.k == k:
                return x
        return None

    def count_keys(self, ns):
        return sum(1 for x in self.keys if x.ns == ns)

    def make_ready(self, j):
        j.state = READY
        j.owner = 0
        self.seq += 1
        j.seq = self.seq

    def make_dead(self, j):
        j.state = DEAD
        j.owner = 0
        self.seq += 1
        j.seq = self.seq

    def send(self, c, data):
        if c.closing or not c.used or not data:
            return
        try:
            c.sock.sendall(data)
        except OSError:
            c.closing = True

    def blocked_sorted(self):
        return sorted((c for c in self.conns if c.used and c.blocked), key=lambda c: c.wseq)

    def reserve(self, j, c):
        j.state = RESERVED
        j.owner = c.id
        j.reserves += 1
        j.deadline = self.clock + j.ttr
        return r_arr(3) + r_int(j.id) + r_int(j.reserves) + r_bulk(j.payload)

    def serve_waiters(self):
        while True:
            pick = None
            for w in self.blocked_sorted():
                if self.best_ready(w.wns, w.wq):
                    pick = w
                    break
            if not pick:
                return
            j = self.best_ready(pick.wns, pick.wq)
            out = self.reserve(j, pick)
            pick.blocked = False
            pick.woken = True
            self.send(pick, out)

    def fail_waiters(self, ns, qn):
        for w in self.blocked_sorted():
            if w.wns != ns or (qn is not None and w.wq != qn):
                continue
            w.blocked = False
            w.woken = True
            self.send(w, Err("E_NOQUEUE", "queue dropped").reply())

    def advance(self, n):
        self.clock += n
        for x in list(self.keys):
            if x.expire_at and x.expire_at <= self.clock:
                self.keys.remove(x)
                self.expired += 1
        ev = []
        for j in self.jobs.values():
            if j.state == DELAYED and j.ready_at <= self.clock:
                ev.append((j.ready_at, j.id, j))
            elif j.state == RESERVED and j.deadline <= self.clock:
                ev.append((j.deadline, j.id, j))
        ev.sort(key=lambda e: (e[0], e[1]))
        for _, _, j in ev:
            if j.state == DELAYED:
                self.make_ready(j)
            else:
                q = self.find_queue(j.ns, j.q)
                j.timeouts += 1
                q.timeouts += 1
                if j.nacks >= q.maxnack:
                    self.make_dead(j)
                else:
                    self.make_ready(j)
        self.serve_waiters()
        for w in self.blocked_sorted():
            if w.wdeadline >= 0 and w.wdeadline <= self.clock:
                w.blocked = False
                w.woken = True
                self.send(w, NILARR)

    def qname(self, t):
        if not valid_name(t, MAX_QNAME, True):
            raise Err("E_NAME", "invalid queue name")
        return t.decode()

    def key(self, t):
        if not (1 <= len(t) <= MAX_KEYLEN):
            raise Err("E_KEY", "key length must be between 1 and 64")
        return t

    def owned_job(self, c, t):
        jid = parse_num(t, 1, 10 ** 12, "id")
        j = self.jobs.get(jid)
        if j is None or j.ns != c.ns:
            raise Err("E_NOJOB", "no such job")
        if j.state == DEAD:
            raise Err("E_DEAD", "job is dead")
        if j.state != RESERVED:
            raise Err("E_NOTRESERVED", "job is not reserved")
        if j.owner != c.id:
            raise Err("E_NOTOWNER", "job is reserved by another client")
        return j

    def dead_sorted(self, ns, qn):
        return sorted((j for j in self.qjobs(ns, qn) if j.state == DEAD), key=lambda j: j.seq)

    # ---------------- commands
    def execute(self, c, name, a, in_exec):
        try:
            r = getattr(self, "c_" + name.lower())(c, a, in_exec)
        except Err as e:
            r = e.reply()
        self.serve_waiters()
        return r

    def c_ping(self, c, a, ie):
        return r_simple("PONG") if len(a) == 1 else r_bulk(a[1])

    def c_echo(self, c, a, ie):
        return r_bulk(a[1])

    def c_use(self, c, a, ie):
        if not valid_name(a[1], MAX_NSNAME, False):
            raise Err("E_NAME", "invalid namespace name")
        c.ns = a[1].decode()
        return r_simple("OK")

    def c_whoami(self, c, a, ie):
        return ("+id=%d ns=%s\r\n" % (c.id, c.ns)).encode()

    def c_quit(self, c, a, ie):
        return (r_simple("BYE"), CLOSE)

    def c_tick(self, c, a, ie):
        n = parse_num(a[1], 1, 1000000, "ticks")
        self.advance(n)
        return r_int(self.clock)

    def c_time(self, c, a, ie):
        return r_int(self.clock)

    def c_qdeclare(self, c, a, ie):
        qn = self.qname(a[1])
        o = parse_opts(a, 2, [("MAXLEN", "maxlen", 1, 1, 1000), ("MAXNACK", "maxnack", 1, 1, 100)])
        q = self.find_queue(c.ns, qn)
        if q:
            if ("MAXLEN" in o and o["MAXLEN"] != q.maxlen) or ("MAXNACK" in o and o["MAXNACK"] != q.maxnack):
                raise Err("E_CONFLICT", "queue exists with different settings")
            return r_simple("EXISTS")
        if sum(1 for x in self.queues if x.ns == c.ns) >= MAX_QUEUES:
            raise Err("E_LIMIT", "too many queues in namespace")
        self.queues.append(Queue(c.ns, qn, o.get("MAXLEN", DEF_MAXLEN), o.get("MAXNACK", DEF_MAXNACK)))
        return r_simple("CREATED")

    def c_qdrop(self, c, a, ie):
        qn = self.qname(a[1])
        q = self.find_queue(c.ns, qn)
        if not q:
            raise Err("E_NOQUEUE", "no such queue")
        n = 0
        for j in self.qjobs(c.ns, qn):
            del self.jobs[j.id]
            n += 1
        self.queues.remove(q)
        self.fail_waiters(c.ns, qn)
        return r_int(n)

    def c_qlist(self, c, a, ie):
        names = sorted(q.name.encode() for q in self.queues if q.ns == c.ns)
        return r_arr(len(names)) + b"".join(r_bulk(n) for n in names)

    def c_push(self, c, a, ie):
        qn = self.qname(a[1])
        if len(a[2]) > MAX_VALLEN:
            raise Err("E_VALUE", "payload too long")
        o = parse_opts(a, 3, [("PRI", "pri", 1, 0, 9), ("DELAY", "delay", 1, 0, 86400), ("TTR", "ttr", 1, 1, 3600)])
        q = self.find_queue(c.ns, qn)
        if not q:
            if sum(1 for x in self.queues if x.ns == c.ns) >= MAX_QUEUES:
                raise Err("E_LIMIT", "too many queues in namespace")
            q = Queue(c.ns, qn, DEF_MAXLEN, DEF_MAXNACK)
            self.queues.append(q)
        if self.live_count(q) >= q.maxlen:
            raise Err("E_FULL", "queue is full")
        j = Job()
        j.id = self.next_job
        self.next_job += 1
        j.ns, j.q, j.payload = c.ns, qn, a[2]
        j.pri = o.get("PRI", DEF_PRI)
        j.ttr = o.get("TTR", DEF_TTR)
        j.ready_at = j.deadline = 0
        j.owner = j.nacks = j.reserves = j.timeouts = 0
        j.seq = 0
        if o.get("DELAY", 0) > 0:
            j.state = DELAYED
            j.ready_at = self.clock + o["DELAY"]
        else:
            self.make_ready(j)
        self.jobs[j.id] = j
        q.pushed += 1
        self.pushed += 1
        return r_int(j.id)

    def c_reserve(self, c, a, ie):
        qn = self.qname(a[1])
        if not self.find_queue(c.ns, qn):
            raise Err("E_NOQUEUE", "no such queue")
        j = self.best_ready(c.ns, qn)
        return self.reserve(j, c) if j else NILARR

    def c_breserve(self, c, a, ie):
        qn = self.qname(a[1])
        to = parse_num(a[2], 0, 100000, "timeout")
        if not self.find_queue(c.ns, qn):
            raise Err("E_NOQUEUE", "no such queue")
        j = self.best_ready(c.ns, qn)
        if j:
            return self.reserve(j, c)
        if ie:
            return NILARR
        c.blocked = True
        c.wq, c.wns = qn, c.ns
        c.wdeadline = -1 if to == 0 else self.clock + to
        self.wseq += 1
        c.wseq = self.wseq
        return BLOCK

    def c_ack(self, c, a, ie):
        j = self.owned_job(c, a[1])
        q = self.find_queue(j.ns, j.q)
        q.acked += 1
        self.acked += 1
        del self.jobs[j.id]
        return r_int(1)

    def c_nack(self, c, a, ie):
        j = self.owned_job(c, a[1])
        o = parse_opts(a, 2, [("DELAY", "delay", 1, 0, 86400)])
        q = self.find_queue(j.ns, j.q)
        j.nacks += 1
        q.nacked += 1
        if j.nacks >= q.maxnack:
            self.make_dead(j)
            return r_simple("DEAD")
        if o.get("DELAY", 0) > 0:
            j.state = DELAYED
            j.owner = 0
            j.ready_at = self.clock + o["DELAY"]
            return r_simple("DELAYED")
        self.make_ready(j)
        return r_simple("READY")

    def c_release(self, c, a, ie):
        j = self.owned_job(c, a[1])
        o = parse_opts(a, 2, [("DELAY", "delay", 1, 0, 86400), ("PRI", "pri", 1, 0, 9)])
        if "PRI" in o:
            j.pri = o["PRI"]
        if o.get("DELAY", 0) > 0:
            j.state = DELAYED
            j.owner = 0
            j.ready_at = self.clock + o["DELAY"]
        else:
            self.make_ready(j)
        return r_simple("RELEASED")

    def c_touch(self, c, a, ie):
        j = self.owned_job(c, a[1])
        j.deadline = self.clock + j.ttr
        return r_int(j.deadline)

    def c_peek(self, c, a, ie):
        qn = self.qname(a[1])
        if not self.find_queue(c.ns, qn):
            raise Err("E_NOQUEUE", "no such queue")
        j = self.best_ready(c.ns, qn)
        if not j:
            return NILARR
        return r_arr(3) + r_int(j.id) + r_int(j.pri) + r_bulk(j.payload)

    def c_qstat(self, c, a, ie):
        qn = self.qname(a[1])
        q = self.find_queue(c.ns, qn)
        if not q:
            raise Err("E_NOQUEUE", "no such queue")
        js = self.qjobs(c.ns, qn)
        cnt = lambda st: sum(1 for j in js if j.state == st)
        body = ("name:%s\nready:%d\ndelayed:%d\nreserved:%d\ndead:%d\nmaxlen:%d\nmaxnack:%d\n"
                "pushed:%d\nacked:%d\nnacked:%d\ntimeouts:%d\n" % (
                    q.name, cnt(READY), cnt(DELAYED), cnt(RESERVED), cnt(DEAD), q.maxlen,
                    q.maxnack, q.pushed, q.acked, q.nacked, q.timeouts))
        return r_bulk(body.encode())

    def c_deadlist(self, c, a, ie):
        qn = self.qname(a[1])
        cnt = parse_num(a[2], 1, 1000, "count") if len(a) == 3 else 1000
        if not self.find_queue(c.ns, qn):
            raise Err("E_NOQUEUE", "no such queue")
        d = self.dead_sorted(c.ns, qn)[:cnt]
        return r_arr(len(d)) + b"".join(r_int(j.id) for j in d)

    def c_kick(self, c, a, ie):
        qn = self.qname(a[1])
        cnt = parse_num(a[2], 1, 1000, "count") if len(a) == 3 else 1
        q = self.find_queue(c.ns, qn)
        if not q:
            raise Err("E_NOQUEUE", "no such queue")
        d = self.dead_sorted(c.ns, qn)
        room = max(0, q.maxlen - self.live_count(q))
        moved = 0
        for j in d:
            if moved >= cnt or moved >= room:
                break
            j.nacks = 0
            self.make_ready(j)
            moved += 1
        return r_int(moved)

    def c_jobinfo(self, c, a, ie):
        jid = parse_num(a[1], 1, 10 ** 12, "id")
        j = self.jobs.get(jid)
        if j is None or j.ns != c.ns:
            raise Err("E_NOJOB", "no such job")
        s = "id:%d\nqueue:%s\nstate:%s\npri:%d\nttr:%d\nnacks:%d\nreserves:%d\ntimeouts:%d\n" % (
            j.id, j.q, STATE_NAMES[j.state], j.pri, j.ttr, j.nacks, j.reserves, j.timeouts)
        if j.state == DELAYED:
            s += "ready_at:%d\n" % j.ready_at
        if j.state == RESERVED:
            s += "owner:%d\ndeadline:%d\n" % (j.owner, j.deadline)
        return r_bulk(s.encode())

    def new_key(self, ns, k):
        if self.count_keys(ns) >= MAX_KEYS:
            raise Err("E_LIMIT", "too many keys in namespace")
        x = Key(ns, k)
        self.keys.append(x)
        return x

    def c_set(self, c, a, ie):
        k = self.key(a[1])
        if len(a[2]) > MAX_VALLEN:
            raise Err("E_VALUE", "value too long")
        o = parse_opts(a, 3, [("TTL", "ttl", 1, 1, 1000000), ("NX", "nx", 0, 0, 0), ("XX", "xx", 0, 0, 0)])
        if "NX" in o and "XX" in o:
            raise Err("E_SYNTAX", "NX and XX are mutually exclusive")
        x = self.find_key(c.ns, k)
        if ("NX" in o and x) or ("XX" in o and not x):
            return NILBULK
        if not x:
            x = self.new_key(c.ns, k)
        x.v = a[2]
        x.version += 1
        x.expire_at = self.clock + o["TTL"] if "TTL" in o else 0
        return r_int(x.version)

    def c_get(self, c, a, ie):
        x = self.find_key(c.ns, self.key(a[1]))
        return r_bulk(x.v) if x else NILBULK

    def c_getv(self, c, a, ie):
        x = self.find_key(c.ns, self.key(a[1]))
        return r_arr(2) + r_int(x.version) + r_bulk(x.v) if x else NILARR

    def c_cas(self, c, a, ie):
        k = self.key(a[1])
        ver = parse_num(a[2], 0, 10 ** 12, "version")
        if len(a[3]) > MAX_VALLEN:
            raise Err("E_VALUE", "value too long")
        x = self.find_key(c.ns, k)
        cur = x.version if x else 0
        if cur != ver:
            raise Err("E_VERSION", "version mismatch (current %d)" % cur)
        if not x:
            x = self.new_key(c.ns, k)
        x.v = a[3]
        x.version += 1
        return r_int(x.version)

    def c_del(self, c, a, ie):
        for t in a[1:]:
            self.key(t)
        n = 0
        for t in a[1:]:
            x = self.find_key(c.ns, t)
            if x:
                self.keys.remove(x)
                n += 1
        return r_int(n)

    def c_incr(self, c, a, ie):
        k = self.key(a[1])
        delta = parse_num(a[2], -10 ** 9, 10 ** 9, "delta") if len(a) == 3 else 1
        x = self.find_key(c.ns, k)
        cur = 0
        if x:
            cur = stored_int(x.v)
            if cur is None:
                raise Err("E_NOTINT", "value is not an integer")
        nv = cur + delta
        if nv > INT_LIMIT or nv < -INT_LIMIT:
            raise Err("E_OVERFLOW", "increment would overflow")
        if not x:
            x = self.new_key(c.ns, k)
        x.v = str(nv).encode()
        x.version += 1
        return r_int(nv)

    def c_keys(self, c, a, ie):
        pat = b"*"
        if len(a) == 2:
            if len(a[1]) > MAX_KEYLEN:
                raise Err("E_PATTERN", "pattern too long")
            pat = a[1]
        ks = sorted(x.k for x in self.keys if x.ns == c.ns and glob_match(pat, x.k))
        return r_arr(len(ks)) + b"".join(r_bulk(k) for k in ks)

    def c_ttl(self, c, a, ie):
        x = self.find_key(c.ns, self.key(a[1]))
        if not x:
            return r_int(-2)
        if not x.expire_at:
            return r_int(-1)
        return r_int(x.expire_at - self.clock)

    def c_expire(self, c, a, ie):
        k = self.key(a[1])
        t = parse_num(a[2], 0, 1000000, "ttl")
        x = self.find_key(c.ns, k)
        if not x:
            return r_int(0)
        x.expire_at = 0 if t == 0 else self.clock + t
        return r_int(1)

    def info_section(self, sec):
        if sec == "SERVER":
            return "version=%s\nclock=%d\n" % (VERSION, self.clock)
        if sec == "CLIENTS":
            live = [x for x in self.conns if x.used and not x.closing]
            return "clients=%d\nblocked=%d\ntotal=%d\n" % (
                len(live), sum(1 for x in live if x.blocked), self.total_conns)
        if sec == "DATA":
            nss = set(q.ns for q in self.queues) | set(k.ns for k in self.keys)
            return "# Data\nnamespaces:%d\nqueues:%d\njobs:%d\nkeys:%d\n" % (
                len(nss), len(self.queues), len(self.jobs), len(self.keys))
        return "# Stats\ncommands:%d\njobs_pushed:%d\njobs_acked:%d\nkeys_expired:%d\n" % (
            self.commands, self.pushed, self.acked, self.expired)

    def c_info(self, c, a, ie):
        secs = ["SERVER", "CLIENTS", "DATA", "STATS"]
        if len(a) == 1 or ieq(a[1], "ALL"):
            body = "\n".join(self.info_section(s) for s in secs)
        else:
            sel = [s for s in secs if ieq(a[1], s)]
            if not sel:
                raise Err("E_SYNTAX", "unknown section '%s'" % sanitize(a[1]))
            body = self.info_section(sel[0])
        return r_bulk(body.encode())

    def c_flush(self, c, a, ie):
        n = 0
        for x in [x for x in self.keys if x.ns == c.ns]:
            self.keys.remove(x)
            n += 1
        for j in [j for j in self.jobs.values() if j.ns == c.ns]:
            del self.jobs[j.id]
            n += 1
        self.queues = [q for q in self.queues if q.ns != c.ns]
        self.fail_waiters(c.ns, None)
        return r_int(n)

    # ---------------- dispatch
    def process_line(self, c, line):
        """returns (bytes, flag) with flag in (None, BLOCK, CLOSE)"""
        try:
            a = lex(line)
        except Err as e:
            self.commands += 1
            if c.inmulti:
                c.txabort = True
            return e.reply(), None
        if not a:
            return b"", None
        self.commands += 1
        name = a[0].upper().decode("latin-1") if all(ch < 0x80 for ch in a[0]) else None
        if name not in ARITY:
            if c.inmulti:
                c.txabort = True
            return Err("E_UNKNOWN", "unknown command '%s'" % sanitize(a[0])).reply(), None
        lo, hi = ARITY[name]
        n = len(a) - 1
        if n < lo or (hi >= 0 and n > hi):
            if c.inmulti:
                c.txabort = True
            return Err("E_ARITY", "wrong number of arguments for '%s'" % name.lower()).reply(), None
        if name == "MULTI":
            if c.inmulti:
                return Err("E_NESTED", "MULTI calls can not be nested").reply(), None
            c.inmulti, c.txabort, c.tx = True, False, []
            return r_simple("OK"), None
        if name == "DISCARD":
            if not c.inmulti:
                return Err("E_NOMULTI", "DISCARD without MULTI").reply(), None
            c.inmulti, c.txabort, c.tx = False, False, []
            return r_simple("OK"), None
        if name == "EXEC":
            if not c.inmulti:
                return Err("E_NOMULTI", "EXEC without MULTI").reply(), None
            if c.txabort:
                c.inmulti, c.txabort, c.tx = False, False, []
                return Err("E_ABORTED", "transaction discarded because of previous errors").reply(), None
            q = c.tx
            c.inmulti, c.txabort, c.tx = False, False, []
            out = [r_arr(len(q))]
            for qa in q:
                out.append(self.execute(c, qa[0].upper().decode(), qa, True))
            return b"".join(out), None
        if c.inmulti and name == "QUIT":
            c.inmulti, c.txabort, c.tx = False, False, []
        elif c.inmulti and name == "USE":
            return Err("E_INMULTI", "USE is not allowed inside MULTI").reply(), None
        elif c.inmulti:
            if len(c.tx) >= MAX_TX:
                c.txabort = True
                return Err("E_TXFULL", "transaction queue is full").reply(), None
            c.tx.append(a)
            return r_simple("QUEUED"), None
        r = self.execute(c, name, a, False)
        if r is BLOCK:
            return b"", BLOCK
        if isinstance(r, tuple):
            return r[0], CLOSE
        return r, None


class Net:
    def __init__(self, port):
        self.srv = Server()
        self.sel = selectors.DefaultSelector()
        self.ls = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.ls.bind(("127.0.0.1", port))
        self.ls.listen(64)
        self.sel.register(self.ls, selectors.EVENT_READ, None)

    def close(self, c):
        s = self.srv
        if not c.used:
            return
        try:
            self.sel.unregister(c.sock)
        except (KeyError, ValueError):
            pass
        c.sock.close()
        c.blocked = False
        for j in sorted((j for j in s.jobs.values() if j.state == RESERVED and j.owner == c.id), key=lambda j: j.id):
            s.make_ready(j)
        c.used = False
        c.closing = False
        s.conns.remove(c)
        s.serve_waiters()

    def drain(self, c):
        s = self.srv
        while c.used and not c.blocked and not c.closing:
            nl = c.inbuf.find(b"\n")
            if nl < 0:
                if len(c.inbuf) > MAX_LINE:
                    s.send(c, Err("E_LINE", "line too long").reply())
                    self.close(c)
                break
            if nl > MAX_LINE:
                s.send(c, Err("E_LINE", "line too long").reply())
                self.close(c)
                break
            line = bytes(c.inbuf[:nl])
            del c.inbuf[:nl + 1]
            if line.endswith(b"\r"):
                line = line[:-1]
            out, flag = s.process_line(c, line)
            s.send(c, out)
            if flag is BLOCK:
                break
            if flag is CLOSE or c.closing:
                self.close(c)
                break

    def pump(self):
        s = self.srv
        while True:
            cand = [c for c in s.conns if c.used and c.woken]
            if not cand:
                return
            c = min(cand, key=lambda x: x.id)
            c.woken = False
            self.drain(c)

    def run(self):
        s = self.srv
        while True:
            events = self.sel.select()
            if any(key.data is None for key, _ in events):
                sock, _ = self.ls.accept()
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                if len(s.conns) >= MAX_CONNS:
                    try:
                        sock.sendall(b"-E_LIMIT too many connections\r\n")
                    except OSError:
                        pass
                    sock.close()
                    continue
                c = Conn(sock, s.next_conn)
                s.next_conn += 1
                s.total_conns += 1
                s.conns.append(c)
                self.sel.register(sock, selectors.EVENT_READ, c)
                continue
            for key, _ in sorted(events, key=lambda e: e[0].data.id):
                c = key.data
                if not c.used:
                    continue
                try:
                    data = c.sock.recv(8192)
                except OSError:
                    data = b""
                if not data:
                    self.close(c)
                    self.pump()
                    continue
                c.inbuf += data
                self.drain(c)
                self.pump()


def main():
    port = 7070
    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] in ("--port", "-p") and i + 1 < len(args):
            port = int(args[i + 1])
            i += 2
        elif args[i] == "--version":
            print("qsrv " + VERSION)
            return
        else:
            sys.stderr.write("usage: qsrv [--port PORT]\n")
            sys.exit(2)
    Net(port).run()


if __name__ == "__main__":
    main()
QSRV_PY_EOF
chmod 0755 /app/replacement/qsrv
