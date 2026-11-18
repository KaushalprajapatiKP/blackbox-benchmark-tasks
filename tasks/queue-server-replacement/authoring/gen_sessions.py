"""Seeded generator for qsrv client sessions (authoring only).

Usage: python3 gen_sessions.py SEED COUNT_PER_CATEGORY OUT.json
"""
import json
import random
import sys

QUEUES = ["jobs", "mail", "a.b", "x-1", "Jobs"]
KEYS = ["k", "k2", "cnt", "user:1", "\"a b\"", "'q'", "K"]
NSS = ["default", "ns1", "t_2", "NS1"]


def num(r, lo, hi):
    """A numeric token biased towards edges and malformed forms."""
    c = r.random()
    if c < 0.45:
        return str(r.randint(lo, hi))
    if c < 0.75:
        return str(r.choice([lo, hi, lo - 1, hi + 1, 0, 1, -1]))
    return r.choice(["+5", "5x", "x", "-", "007", "-0", "0x10", "1e3", "\"12\"", "'3'",
                     "999999999999999999", "1234567890123456789", "00000000000000000001",
                     "3.0", "--1", " 4"]).strip() or "1"


def qn(r):
    c = r.random()
    if c < 0.9:
        return r.choice(QUEUES[:3]) if r.random() < 0.7 else r.choice(QUEUES)
    return r.choice(["bad/name", "q" * 33, "q" * 32, "\"\"", "a..b", ".x", "\"sp ace\"", "\xe9t\xe9"])


def key(r):
    c = r.random()
    if c < 0.92:
        return r.choice(KEYS[:3]) if r.random() < 0.6 else r.choice(KEYS)
    return r.choice(["\"\"", "k" * 64, "k" * 65, "\"\\x00z\"", "'*'", "k?"])


def val(r):
    c = r.random()
    if c < 0.5:
        return r.choice(["v", "hello", "42", "-7", "x y", "007", "1000000000000000"])
    if c < 0.7:
        return str(r.randint(-100, 100))
    if c < 0.8:
        return r.choice(["\"a b\"", "'it''s'", "\"tab\\there\"", "\"\\x41\\x42\"", "\"\"", "''"])
    if c < 0.9:
        return r.choice(["v" * 512, "v" * 513, "\"" + "w" * 511 + "\\n\""])
    return r.choice(["999999999999999", "-999999999999999", "1000000000000001", "0", "-0", "+3"])


def case(r, word):
    c = r.random()
    if c < 0.6:
        return word
    if c < 0.8:
        return word.lower()
    return "".join(ch.upper() if r.random() < 0.5 else ch.lower() for ch in word)


def push_cmd(r):
    parts = [case(r, "PUSH"), qn(r), r.choice(["p1", "p2", "job", "\"pay load\"", "x", "''"])]
    opts = []
    if r.random() < 0.6:
        opts.append(["PRI", num(r, 0, 9) if r.random() < 0.3 else str(r.randint(0, 9))])
    if r.random() < 0.35:
        opts.append(["DELAY", num(r, 0, 86400) if r.random() < 0.3 else str(r.randint(0, 6))])
    if r.random() < 0.35:
        opts.append(["TTR", num(r, 1, 3600) if r.random() < 0.3 else str(r.randint(1, 8))])
    if r.random() < 0.05:
        opts.append(["PRI", "1"])
    if r.random() < 0.05:
        opts.append(["BOGUS"])
    if r.random() < 0.04:
        opts.append(["TTR"])
    r.shuffle(opts)
    for o in opts:
        parts.append(case(r, o[0]))
        parts.extend(o[1:])
    return " ".join(parts)


def jid(r, st):
    if r.random() < 0.85 and st["maxid"] > 0:
        return str(r.randint(max(1, st["maxid"] - 4), st["maxid"] + 1))
    return num(r, 1, 10 ** 12)


def queue_cmd(r, st):
    c = r.random()
    if c < 0.25:
        return push_cmd(r)
    if c < 0.40:
        return "%s %s" % (case(r, "RESERVE"), qn(r))
    if c < 0.48:
        return "%s %s" % (case(r, "ACK"), jid(r, st))
    if c < 0.55:
        s = "%s %s" % (case(r, "NACK"), jid(r, st))
        if r.random() < 0.3:
            s += " DELAY " + str(r.choice([0, 1, 2, 3]))
        return s
    if c < 0.60:
        s = "%s %s" % (case(r, "RELEASE"), jid(r, st))
        if r.random() < 0.4:
            s += " PRI " + str(r.randint(0, 9))
        if r.random() < 0.3:
            s += " DELAY " + str(r.randint(0, 3))
        return s
    if c < 0.64:
        return "%s %s" % (case(r, "TOUCH"), jid(r, st))
    if c < 0.68:
        return "%s %s" % (case(r, "PEEK"), qn(r))
    if c < 0.72:
        return "%s %s" % (case(r, "QSTAT"), qn(r))
    if c < 0.76:
        return "%s %s" % (case(r, "JOBINFO"), jid(r, st))
    if c < 0.80:
        return "%s %s" % (case(r, "TICK"), r.choice(["1", "2", "3", "5", "10", "30", num(r, 1, 1000000)]))
    if c < 0.84:
        s = "%s %s" % (case(r, "QDECLARE"), qn(r))
        if r.random() < 0.6:
            s += " MAXLEN " + r.choice(["2", "3", "5", "100", num(r, 1, 1000)])
        if r.random() < 0.5:
            s += " MAXNACK " + r.choice(["1", "2", "3", num(r, 1, 100)])
        return s
    if c < 0.87:
        return "%s %s%s" % (case(r, "DEADLIST"), qn(r), r.choice(["", "", " 1", " 2", " 0", " 1001"]))
    if c < 0.90:
        return "%s %s%s" % (case(r, "KICK"), qn(r), r.choice(["", " 1", " 3", " 0", " 1000"]))
    if c < 0.92:
        return case(r, "QLIST")
    if c < 0.94:
        return "%s %s" % (case(r, "QDROP"), qn(r))
    if c < 0.96:
        return "%s %s %s" % (case(r, "BRESERVE"), qn(r), r.choice(["1", "2", "0", "5"]))
    return case(r, "TIME")


def kv_cmd(r):
    c = r.random()
    if c < 0.25:
        s = "%s %s %s" % (case(r, "SET"), key(r), val(r))
        opts = []
        if r.random() < 0.3:
            opts.append("TTL " + r.choice(["1", "2", "5", num(r, 1, 1000000)]))
        if r.random() < 0.2:
            opts.append(r.choice(["NX", "XX", "nx", "xx"]))
        if r.random() < 0.04:
            opts.append(r.choice(["NX", "XX", "TTL", "KEEPTTL"]))
        r.shuffle(opts)
        return " ".join([s] + opts)
    if c < 0.40:
        return "%s %s" % (case(r, "GET"), key(r))
    if c < 0.47:
        return "%s %s" % (case(r, "GETV"), key(r))
    if c < 0.57:
        return "%s %s %s %s" % (case(r, "CAS"), key(r), r.choice(["0", "1", "2", "3", num(r, 0, 10 ** 12)]), val(r))
    if c < 0.64:
        return "%s %s" % (case(r, "DEL"), " ".join(key(r) for _ in range(r.randint(1, 3))))
    if c < 0.76:
        s = "%s %s" % (case(r, "INCR"), key(r))
        if r.random() < 0.5:
            s += " " + r.choice(["5", "-3", "1000000000", "-1000000000", "1000000001", num(r, -10 ** 9, 10 ** 9)])
        return s
    if c < 0.82:
        return "%s%s" % (case(r, "KEYS"), r.choice(["", " *", " k*", " ?", " *1", " u*:?", " '*'", " k" * 1, " " + "*" * 65]))
    if c < 0.88:
        return "%s %s" % (case(r, "TTL"), key(r))
    if c < 0.93:
        return "%s %s %s" % (case(r, "EXPIRE"), key(r), r.choice(["0", "1", "3", num(r, 0, 1000000)]))
    return "%s %s" % (case(r, "TICK"), r.choice(["1", "2", "3"]))


def lexer_line(r):
    pieces = [
        "ECHO \"a\\tb\"", "ECHO 'a\\tb'", "ECHO \"\\x4a\\x4B\"", "ECHO \"\\xZZ\"", "ECHO \"\\q\"",
        "ECHO \"unterminated", "ECHO 'unterminated", "ECHO \"a\"b", "ECHO 'a'b", "ECHO a\"b\"",
        "ECHO \"\"", "ECHO ''", "ECHO \"\\\\\"", "ECHO \"\\\"\"", "ECHO \"\\0\"", "ECHO \"x\\\"",
        "ECHO\t\ttabbed", "  ECHO   spaced  ", "ECHO a b", "ECHO", "PING", "ping hello", "PiNg \"\"",
        "ECHO \"\\x4\"", "ECHO \"\\x\"", "ECHO \"a\" \"b\"", "ECHO 'it\"s'", "ECHO \"it's\"",
        "ECHO \"\\n\\r\"", "ECHO \x01", "ECHO a\x7f", "ECHO a\rb", "ECHO \"\xff\xfe\"", "\xc3\xa9cho x",
        "FOO", "foo bar", "\"PING\"", "'ping'", "\"\"", "ECHO#", "#comment", "ECHO \"a\"\t", "ECHO 'a'\t",
        "averyveryverylongcommandnamethatkeepsgoing", "bad\x00", "ECHO " + " ".join(["x"] * 63),
        "ECHO " + " ".join(["x"] * 64), "PING " + " ".join(["x"] * 63), "ECHO \"a\"\"b\"",
    ]
    return r.choice(pieces)


def common_cmd(r):
    c = r.random()
    if c < 0.2:
        return case(r, "PING")
    if c < 0.35:
        return "%s %s" % (case(r, "USE"), r.choice(NSS + ["bad.ns", "n" * 17, "n" * 16, "\"\""]))
    if c < 0.5:
        return case(r, "WHOAMI")
    if c < 0.6:
        return case(r, "TIME")
    if c < 0.75:
        return "%s%s" % (case(r, "INFO"), r.choice(["", " server", " CLIENTS", " data", " stats", " all", " bogus"]))
    if c < 0.8:
        return case(r, "FLUSH")
    return lexer_line(r)


def arity_line(r):
    cmds = ["PING", "ECHO", "USE", "WHOAMI", "QUIT", "TICK", "TIME", "QDECLARE", "QDROP", "QLIST",
            "PUSH", "RESERVE", "BRESERVE", "ACK", "NACK", "RELEASE", "TOUCH", "PEEK", "QSTAT",
            "DEADLIST", "KICK", "JOBINFO", "SET", "GET", "GETV", "CAS", "DEL", "INCR", "KEYS",
            "TTL", "EXPIRE", "INFO", "FLUSH", "MULTI", "EXEC", "DISCARD"]
    name = r.choice(cmds)
    if name == "QUIT":
        name = "QUIT"
        return "QUIT extra"
    n = r.choice([0, 1, 2, 3, 4, 6, 9])
    args = ["1"] * n
    return " ".join([case(r, name)] + args)


class Session:
    def __init__(self, sid, cat, conns):
        self.d = {"id": sid, "category": cat, "conns": conns, "steps": []}
        self.st = {"maxid": 0}

    def line(self, c, s, nl=None):
        nl = nl or "\n"
        self.d["steps"].append({"c": c, "send": s + nl})

    def chunk(self, c, lines, r):
        nl = "\r\n" if r.random() < 0.3 else "\n"
        self.d["steps"].append({"c": c, "send": "".join(x + nl for x in lines)})

    def raw(self, c, s):
        self.d["steps"].append({"c": c, "send": s})

    def close(self, c):
        self.d["steps"].append({"c": c, "close": True})


def push_seed(r, s, c, n=None):
    for _ in range(n or r.randint(1, 4)):
        s.line(c, "PUSH %s %s PRI %d" % (r.choice(QUEUES[:2]), r.choice(["a", "b", "c"]), r.randint(0, 9)))
        s.st["maxid"] += 1


def gen_lexer(r, s):
    for _ in range(r.randint(10, 25)):
        x = r.random()
        if x < 0.7:
            s.line(0, lexer_line(r), r.choice(["\n", "\n", "\r\n"]))
        elif x < 0.8:
            s.raw(0, r.choice(["\n", "   \n", "\t\n", "\r\n", " \r\n"]))
        elif x < 0.9:
            s.line(0, "SET %s %s" % (r.choice(["\"k\\x20\"", "'k k'", "\"\\x00\"", "k"]), val(r)))
            s.line(0, "KEYS")
        else:
            s.line(0, "ECHO \"" + "".join(r.choice(["\\t", "\\x41", "a", "\\\\", " ", "\\\""]) for _ in range(6)) + "\"")


def gen_basic(r, s):
    for _ in range(r.randint(10, 20)):
        x = r.random()
        if x < 0.5:
            s.line(0, arity_line(r))
        elif x < 0.8:
            s.line(0, common_cmd(r))
        else:
            s.line(0, r.choice(["ping", "PING", "Ping x", "ECHO hi", "echo \"hi there\"", "whoami", "time",
                                "HELP", "GETX k", "SETNX k v", "PUBLISH a b", "EXIT", "STATS"]))
    if r.random() < 0.5:
        s.line(0, r.choice(["QUIT", "quit", "QUIT extra"]))
        if r.random() < 0.5:
            s.line(0, "PING")


def gen_numeric(r, s):
    for _ in range(r.randint(12, 25)):
        x = r.random()
        if x < 0.2:
            s.line(0, "TICK " + num(r, 1, 1000000))
        elif x < 0.4:
            s.line(0, "PUSH jobs p PRI %s DELAY %s TTR %s" % (num(r, 0, 9), num(r, 0, 86400), num(r, 1, 3600)))
        elif x < 0.5:
            s.line(0, "QDECLARE %s MAXLEN %s MAXNACK %s" % (qn(r), num(r, 1, 1000), num(r, 1, 100)))
        elif x < 0.6:
            s.line(0, "SET k v TTL " + num(r, 1, 1000000))
        elif x < 0.7:
            s.line(0, "INCR cnt " + num(r, -10 ** 9, 10 ** 9))
        elif x < 0.8:
            s.line(0, "ACK " + num(r, 1, 10 ** 12))
        elif x < 0.9:
            s.line(0, "CAS k %s v" % num(r, 0, 10 ** 12))
        else:
            s.line(0, r.choice(["BRESERVE jobs ", "DEADLIST jobs ", "KICK jobs ", "EXPIRE k "]) + num(r, 0, 100000))


def gen_kv(r, s):
    for _ in range(r.randint(15, 35)):
        s.line(0, kv_cmd(r))
    s.line(0, "KEYS")


def gen_ttl(r, s):
    for _ in range(r.randint(3, 6)):
        s.line(0, "SET %s %s TTL %d" % (key(r), val(r), r.randint(1, 5)))
    for _ in range(r.randint(10, 25)):
        x = r.random()
        if x < 0.3:
            s.line(0, "TICK %d" % r.randint(1, 3))
        elif x < 0.5:
            s.line(0, "TTL " + key(r))
        elif x < 0.6:
            s.line(0, "EXPIRE %s %s" % (key(r), r.choice(["0", "2", "4"])))
        elif x < 0.7:
            s.line(0, "INCR " + key(r))
        elif x < 0.8:
            s.line(0, "CAS %s %d v" % (key(r), r.randint(0, 3)))
        elif x < 0.9:
            s.line(0, "SET %s %s" % (key(r), val(r)))
        else:
            s.line(0, "INFO stats")
    s.line(0, "KEYS")
    s.line(0, "INFO data")


def gen_queue_basic(r, s):
    if r.random() < 0.5:
        s.line(0, "QDECLARE %s MAXLEN %d MAXNACK %d" % (r.choice(QUEUES[:2]), r.randint(2, 6), r.randint(1, 3)))
    for _ in range(r.randint(15, 35)):
        cmd = queue_cmd(r, s.st)
        if cmd.upper().startswith("PUSH"):
            s.st["maxid"] += 1
        if cmd.upper().startswith("BRESERVE"):
            cmd = "RESERVE " + cmd.split()[1]
        s.line(0, cmd)
    s.line(0, "QLIST")
    s.line(0, "QSTAT " + r.choice(QUEUES[:2]))


def gen_order(r, s):
    q = r.choice(QUEUES[:3])
    n = r.randint(4, 9)
    for _ in range(n):
        parts = ["PUSH", q, r.choice(["a", "b", "c", "d"])]
        if r.random() < 0.7:
            parts += ["PRI", str(r.choice([0, 1, 5, 5, 9]))]
        if r.random() < 0.4:
            parts += ["DELAY", str(r.randint(1, 4))]
        s.line(0, " ".join(parts))
        s.st["maxid"] += 1
    for _ in range(r.randint(8, 16)):
        x = r.random()
        if x < 0.45:
            s.line(0, "RESERVE " + q)
        elif x < 0.6:
            s.line(0, "PEEK " + q)
        elif x < 0.75:
            s.line(0, "TICK %d" % r.randint(1, 3))
        elif x < 0.85:
            s.line(0, "RELEASE %d PRI %d" % (r.randint(1, s.st["maxid"]), r.randint(0, 9)))
        else:
            s.line(0, "NACK %d" % r.randint(1, s.st["maxid"]))
    s.line(0, "QSTAT " + q)


def gen_visibility(r, s):
    q = r.choice(QUEUES[:2])
    s.line(0, "QDECLARE %s MAXNACK %d" % (q, r.randint(1, 3)))
    for _ in range(r.randint(2, 4)):
        s.line(0, "PUSH %s job TTR %d" % (q, r.randint(1, 5)))
        s.st["maxid"] += 1
    for _ in range(r.randint(15, 30)):
        x = r.random()
        mid = s.st["maxid"]
        if x < 0.2:
            s.line(0, "RESERVE " + q)
        elif x < 0.35:
            s.line(0, "TICK %d" % r.randint(1, 6))
        elif x < 0.45:
            s.line(0, "NACK %d%s" % (r.randint(1, mid), r.choice(["", "", " DELAY 2"])))
        elif x < 0.52:
            s.line(0, "TOUCH %d" % r.randint(1, mid))
        elif x < 0.6:
            s.line(0, "ACK %d" % r.randint(1, mid))
        elif x < 0.68:
            s.line(0, "JOBINFO %d" % r.randint(1, mid))
        elif x < 0.75:
            s.line(0, "DEADLIST " + q)
        elif x < 0.82:
            s.line(0, "KICK %s %d" % (q, r.randint(1, 3)))
        elif x < 0.9:
            s.line(0, "QSTAT " + q)
        else:
            s.line(0, "RELEASE %d" % r.randint(1, mid))
    s.line(0, "QSTAT " + q)


def gen_multiconn(r, s):
    n = s.d["conns"]
    q = r.choice(QUEUES[:2])
    s.line(0, "QDECLARE " + q)
    for _ in range(r.randint(18, 35)):
        c = r.randrange(n)
        x = r.random()
        mid = max(1, s.st["maxid"])
        if x < 0.18:
            s.line(c, "PUSH %s m%d" % (q, r.randint(1, 9)))
            s.st["maxid"] += 1
        elif x < 0.3:
            s.line(c, "RESERVE " + q)
        elif x < 0.42:
            s.line(c, "BRESERVE %s %d" % (q, r.choice([0, 1, 2, 3])))
        elif x < 0.5:
            s.line(c, "ACK %d" % r.randint(1, mid))
        elif x < 0.56:
            s.line(c, "NACK %d" % r.randint(1, mid))
        elif x < 0.62:
            s.line(c, "TOUCH %d" % r.randint(1, mid))
        elif x < 0.7:
            s.line(c, "TICK %d" % r.randint(1, 3))
        elif x < 0.76:
            s.line(c, "JOBINFO %d" % r.randint(1, mid))
        elif x < 0.8:
            s.line(c, r.choice(["WHOAMI", "INFO clients"]))
        elif x < 0.84:
            s.line(c, "QDROP " + q)
            s.line(c, "QDECLARE " + q)
        elif x < 0.9:
            s.line(c, "RELEASE %d" % r.randint(1, mid))
        elif x < 0.95:
            s.close(c)
        else:
            s.line(c, "QSTAT " + q)
    s.line(n - 1, "QSTAT " + q) if r.random() < 0.5 else None


def gen_tx(r, s):
    for _ in range(r.randint(2, 4)):
        x = r.random()
        if x < 0.1:
            s.line(0, r.choice(["EXEC", "DISCARD"]))
        s.line(0, case(r, "MULTI"))
        for _ in range(r.randint(0, 6) if r.random() < 0.9 else 17):
            y = r.random()
            if y < 0.4:
                s.line(0, kv_cmd(r))
            elif y < 0.6:
                cmd = queue_cmd(r, s.st)
                if cmd.upper().startswith("PUSH"):
                    s.st["maxid"] += 1
                s.line(0, cmd)
            elif y < 0.67:
                s.line(0, r.choice(["MULTI", "USE ns1", "WHOAMI", "PING", "TIME"]))
            elif y < 0.74:
                s.line(0, r.choice(["FOO", "GET", "SET k", "ECHO \"x", "TICK", "INCR a b c"]))
            elif y < 0.8:
                s.line(0, r.choice(["EXEC x", "DISCARD 1", "MULTI x"]))
            else:
                s.line(0, r.choice(["INCR cnt", "SET k v", "GET k", "INCR k", "TICK 1"]))
        s.line(0, r.choice(["EXEC", "EXEC", "EXEC", "DISCARD", "exec"]))
        if r.random() < 0.6:
            s.line(0, r.choice(["GET k", "GET cnt", "INFO stats", "QSTAT jobs", "KEYS"]))
    if r.random() < 0.2:
        s.line(0, "MULTI")
        s.line(0, "SET k zz")
        s.line(0, "QUIT")


def gen_ns(r, s):
    n = s.d["conns"]
    for _ in range(r.randint(15, 30)):
        c = r.randrange(n)
        x = r.random()
        if x < 0.25:
            s.line(c, "%s %s" % (case(r, "USE"), r.choice(NSS + ["bad.ns", "n" * 17])))
        elif x < 0.45:
            s.line(c, kv_cmd(r))
        elif x < 0.6:
            cmd = queue_cmd(r, s.st)
            if cmd.upper().startswith("PUSH"):
                s.st["maxid"] += 1
            if cmd.upper().startswith("BRESERVE"):
                cmd = "RESERVE " + cmd.split()[1]
            s.line(c, cmd)
        elif x < 0.7:
            s.line(c, "WHOAMI")
        elif x < 0.8:
            s.line(c, r.choice(["QLIST", "KEYS", "INFO data"]))
        elif x < 0.88:
            s.line(c, "FLUSH")
        else:
            s.line(c, "JOBINFO %d" % r.randint(1, max(1, s.st["maxid"])))


def gen_pipeline(r, s):
    n = s.d["conns"]
    for _ in range(r.randint(5, 10)):
        c = r.randrange(n)
        x = r.random()
        if x < 0.5:
            lines = []
            for _ in range(r.randint(2, 8)):
                y = r.random()
                if y < 0.4:
                    lines.append(kv_cmd(r))
                elif y < 0.7:
                    cmd = queue_cmd(r, s.st)
                    if cmd.upper().startswith("PUSH"):
                        s.st["maxid"] += 1
                    lines.append(cmd)
                else:
                    lines.append(common_cmd(r))
            s.chunk(c, lines, r)
        elif x < 0.7:
            # a line split across writes
            line = kv_cmd(r) + "\n"
            cut = r.randint(1, len(line) - 1)
            s.raw(c, line[:cut])
            s.raw(c, line[cut:])
        elif x < 0.85 and n > 1:
            # blocked reserve with commands queued behind it
            q = r.choice(QUEUES[:2])
            s.line(c, "QDECLARE " + q)
            s.raw(c, "BRESERVE %s %d\nPING\nWHOAMI\nTIME\n" % (q, r.choice([0, 2])))
            other = (c + 1) % n
            if r.random() < 0.6:
                s.line(other, "PUSH %s wake" % q)
                s.st["maxid"] += 1
            else:
                s.line(other, "TICK 2")
        else:
            s.raw(c, r.choice(["PING\nPING\nPING\n", "MULTI\nINCR a\nINCR a\nEXEC\n", "ECHO a\nQUIT\nECHO b\n",
                               "\n\n\nPING\n", "SET a 1\r\nGET a\r\n", "PING\nECHO \"x\nPING\n"]))


def gen_info(r, s):
    n = s.d["conns"]
    for _ in range(r.randint(10, 22)):
        c = r.randrange(n)
        x = r.random()
        if x < 0.3:
            s.line(c, "INFO" + r.choice(["", " server", " clients", " data", " stats", " ALL", " Stats"]))
        elif x < 0.5:
            s.line(c, kv_cmd(r))
        elif x < 0.7:
            cmd = queue_cmd(r, s.st)
            if cmd.upper().startswith("PUSH"):
                s.st["maxid"] += 1
            if cmd.upper().startswith("BRESERVE"):
                cmd = "RESERVE " + cmd.split()[1]
            s.line(c, cmd)
        elif x < 0.8:
            s.line(c, common_cmd(r))
        elif x < 0.85:
            s.line(c, "")
        elif x < 0.9 and n > 1:
            s.close(c)
        else:
            s.line(c, "BRESERVE mail 3" if r.random() < 0.5 else "USE ns1")
    s.line(n - 1, "INFO")


def gen_limits(r, s):
    x = r.random()
    if x < 0.25:
        for i in range(r.randint(7, 10)):
            s.line(0, "QDECLARE q%d" % i)
        s.line(0, "PUSH qz x")
        s.line(0, "QLIST")
        s.line(0, "USE ns1")
        s.line(0, "PUSH qz x")
    elif x < 0.5:
        for i in range(r.randint(62, 66)):
            s.line(0, "SET key%02d v" % i)
        s.line(0, "INCR newcnt")
        s.line(0, "CAS newk 0 v")
        s.line(0, "SET key01 w")
        s.line(0, "DEL key01")
        s.line(0, "SET fresh v")
    elif x < 0.75:
        q = "jobs"
        s.line(0, "QDECLARE %s MAXLEN %d MAXNACK 1" % (q, r.randint(2, 4)))
        for _ in range(6):
            s.line(0, "PUSH %s p DELAY %d" % (q, r.randint(0, 2)))
            s.st["maxid"] += 1
        for _ in range(r.randint(4, 10)):
            y = r.random()
            if y < 0.3:
                s.line(0, "RESERVE " + q)
            elif y < 0.5:
                s.line(0, "NACK %d" % r.randint(1, 6))
            elif y < 0.7:
                s.line(0, "KICK %s %d" % (q, r.randint(1, 5)))
            elif y < 0.85:
                s.line(0, "PUSH %s more" % q)
                s.st["maxid"] += 1
            else:
                s.line(0, "TICK 3")
        s.line(0, "QSTAT " + q)
    else:
        y = r.random()
        if y < 0.5:
            s.line(0, "ECHO " + "a" * r.choice([4085, 4086, 4087, 4090, 5000]))
            s.line(0, "PING")
        else:
            s.raw(0, "PING\nECHO " + "b" * r.choice([4090, 4092, 4100, 6000]))
            s.raw(0, "\n")
            s.line(0, "PING")


GENS = {
    "lexer": (gen_lexer, 1),
    "commands": (gen_basic, 1),
    "numeric": (gen_numeric, 1),
    "kv": (gen_kv, 1),
    "ttl": (gen_ttl, 1),
    "queue-basic": (gen_queue_basic, 1),
    "queue-order": (gen_order, 1),
    "visibility": (gen_visibility, 1),
    "multi-conn": (gen_multiconn, 3),
    "transactions": (gen_tx, 1),
    "namespaces": (gen_ns, 2),
    "pipelining": (gen_pipeline, 2),
    "info": (gen_info, 2),
    "limits": (gen_limits, 1),
}


def main():
    seed = int(sys.argv[1])
    per = int(sys.argv[2])
    out = sys.argv[3]
    r = random.Random(seed)
    sessions = []
    for cat, (fn, conns) in GENS.items():
        for i in range(per):
            nc = conns if conns == 1 else r.randint(2, conns + 1)
            s = Session("%s-%03d" % (cat, i), cat, nc)
            fn(r, s)
            sessions.append(s.d)
    with open(out, "w") as f:
        json.dump(sessions, f)
    print(len(sessions), "sessions")


if __name__ == "__main__":
    main()
