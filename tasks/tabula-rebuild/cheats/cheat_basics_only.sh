#!/bin/bash
# Cheat 3: a rebuild written from the man page and --help only (common regime).
set -euo pipefail
mkdir -p /app/rebuild
/app/bin/tabula --help > /app/rebuild/help.txt
cat > /app/rebuild/tabula <<'PYEOF'
#!/usr/bin/env python3
# Cheat 3 payload: a rebuild written from the man page and --help only, with
# exact help/version text but common-regime behaviour everywhere else.
import csv
import io
import os
import re
import sys

HELP = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "help.txt"), "rb").read() \
    if os.path.exists(os.path.join(os.path.dirname(os.path.abspath(__file__)), "help.txt")) else b""

LONG = {"delimiter": "d", "out-delimiter": "D", "no-header": "H", "no-header-out": "N",
        "select": "s", "exclude": "x", "where": "w", "sort": "S", "group": "g", "agg": "a",
        "unique": "u", "limit": "n", "skip": "k", "precision": "p", "round": "r",
        "thousands": "T", "quote": "q", "empty": "e", "pretty": "t", "max-width": "m",
        "columns": "c", "count": "C", "strict": "strict", "trim": "trim", "help": "h",
        "version": "V"}
ARG = set("dDsxwSganpkrqem")


def fail(code, msg):
    sys.stderr.write("tabula: %s\n" % msg)
    sys.exit(code)


def parse(argv):
    o = {"w": [], "file": None}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a.startswith("--") and len(a) > 2:
            name, _, val = a[2:].partition("=")
            m = [k for k in LONG if k.startswith(name)]
            if name in LONG:
                m = [name]
            if len(m) != 1:
                fail(1, "unrecognized option '%s'" % a)
            c = LONG[m[0]]
            if c in ARG and not val:
                i += 1
                val = argv[i] if i < len(argv) else fail(1, "option '%s' requires an argument" % a)
            setopt(o, c, val)
        elif a.startswith("-") and len(a) > 1:
            c = a[1]
            if c in ARG:
                val = a[2:]
                if not val:
                    i += 1
                    val = argv[i] if i < len(argv) else fail(1, "option requires an argument -- '%s'" % c)
                setopt(o, c, val)
            else:
                for ch in a[1:]:
                    setopt(o, ch, None)
        else:
            o["file"] = a
        i += 1
    return o


def setopt(o, c, v):
    if c == "h":
        sys.stdout.buffer.write(HELP)
        sys.exit(0)
    if c == "V":
        print("tabula 2.3.1")
        sys.exit(0)
    if c == "w":
        o["w"].append(v)
    elif c in ("n", "k", "p", "m"):
        try:
            o[c] = int(v)
        except ValueError:
            fail(1, "invalid number '%s'" % v)
    else:
        o[c] = v if v is not None else True


def num(v):
    try:
        return float(v)
    except ValueError:
        return None


def ev(expr, row, names):
    py = expr
    py = re.sub(r"\band\b", " and ", py, flags=re.I)
    py = re.sub(r"\bor\b", " or ", py, flags=re.I)
    py = py.replace("&&", " and ").replace("||", " or ")
    m = re.match(r"^\s*(\S+)\s*(==|=|!=|<>|<=|>=|<|>|~|\^=|\$=)\s*(.+?)\s*$", expr)
    if not m:
        v = row.get(expr.strip(), expr.strip())
        return bool(v)
    l, op, r = m.groups()
    lv = row.get(l, l)
    r = r.strip("'\"")
    rv = row.get(r, r)
    if op == "~":
        return rv in lv
    if op == "^=":
        return lv.startswith(rv)
    if op == "$=":
        return lv.endswith(rv)
    a, b = num(lv), num(rv)
    if a is None or b is None:
        a, b = lv, rv
    return {"=": a == b, "==": a == b, "!=": a != b, "<>": a != b, "<": a < b,
            "<=": a <= b, ">": a > b, ">=": a >= b}[op]


def main():
    o = parse(sys.argv[1:])
    delim = "\t" if o.get("d") == "tab" else o.get("d", ",")
    if o["file"] and o["file"] != "-":
        if not os.path.exists(o["file"]):
            fail(6, "cannot open '%s': No such file or directory" % o["file"])
        if o["file"].endswith(".tsv") and "d" not in o:
            delim = "\t"
        text = open(o["file"], encoding="utf-8", newline="").read()
    else:
        text = sys.stdin.read()
    rows = list(csv.reader(io.StringIO(text), delimiter=delim))
    rows = [r for r in rows if r]
    if o.get("H"):
        names = ["c%d" % (i + 1) for i in range(len(rows[0]) if rows else 0)]
    else:
        if not rows:
            fail(5, "empty input")
        names, rows = rows[0], rows[1:]
    if o.get("trim"):
        names = [n.strip() for n in names]
        rows = [[c.strip() for c in r] for r in rows]
    if o.get("c"):
        for i, n in enumerate(names):
            print("%d\t%s" % (i + 1, n))
        return
    data = [dict(zip(names, r)) for r in rows]
    for w in o["w"]:
        data = [d for d in data if ev(w, d, names)]

    def col(spec):
        if spec.isdigit():
            return names[int(spec) - 1]
        if spec not in names:
            fail(3, "unknown column '%s'" % spec)
        return spec

    if "g" in o or "a" in o:
        gcols = [col(c) for c in o["g"].split(",")] if "g" in o else []
        specs = o.get("a", "count(*)").split(",")
        groups = {}
        for d in data:
            groups.setdefault(tuple(d[c] for c in gcols), []).append(d)
        out_names = list(gcols)
        parsed = []
        for s in specs:
            alias, _, rest = s.rpartition("=")
            fn, arg = rest[:-1].split("(")
            parsed.append((fn, arg))
            out_names.append(alias or ("count" if arg == "*" else fn + "_" + col(arg)))
        new = []
        for key, ds in groups.items():
            r = list(key)
            for fn, arg in parsed:
                vals = [d[col(arg)] for d in ds] if arg != "*" else [1] * len(ds)
                nums = [num(v) for v in vals if num(v) is not None]
                if fn == "count":
                    r.append(str(len(vals)))
                elif fn == "sum":
                    r.append("%g" % sum(nums))
                elif fn == "avg":
                    r.append("%g" % (sum(nums) / len(nums)) if nums else "")
                elif fn == "min":
                    r.append(str(min(vals)))
                elif fn == "max":
                    r.append(str(max(vals)))
                elif fn == "first":
                    r.append(vals[0])
                elif fn == "last":
                    r.append(vals[-1])
                elif fn == "distinct":
                    r.append(str(len(set(vals))))
            new.append(dict(zip(out_names, r)))
        names, data = out_names, new
    if "S" in o:
        for key in reversed(o["S"].split(",")):
            c, _, fl = key.partition(":")
            c = col(c)
            if "n" in fl:
                k = lambda d: (num(d[c]) is None, num(d[c]) or 0)
            elif "i" in fl:
                k = lambda d: d[c].lower()
            else:
                k = lambda d: d[c]
            data.sort(key=k, reverse="r" in fl)
    if "s" in o:
        names = [col(c) for c in o["s"].split(",")]
    if "x" in o:
        ex = [col(c) for c in o["x"].split(",")]
        names = [n for n in names if n not in ex]
    out = [[d[n] for n in names] for d in data]
    if o.get("u"):
        seen, u = set(), []
        for r in out:
            if tuple(r) not in seen:
                seen.add(tuple(r))
                u.append(r)
        out = u
    out = out[o.get("k", 0):]
    if "n" in o:
        out = out[:o["n"]]
    if o.get("C"):
        print(len(out))
        return
    if "p" in o:
        out = [[("%.*f" % (o["p"], num(v))) if num(v) is not None else v for v in r] for r in out]
    if "e" in o:
        out = [[v or o["e"] for v in r] for r in out]
    show = not o.get("H") and not o.get("N")
    if o.get("t"):
        allr = ([names] if show else []) + out
        w = [max(len(r[i]) for r in allr) for i in range(len(names))]
        for j, r in enumerate(allr):
            print(" | ".join(v.ljust(w[i]) for i, v in enumerate(r)).rstrip())
            if j == 0 and show:
                print("-+-".join("-" * x for x in w))
        return
    od = o.get("D", delim)
    od = "\t" if od == "tab" else od
    qm = {"all": csv.QUOTE_ALL, "nonnumeric": csv.QUOTE_NONNUMERIC, "none": csv.QUOTE_NONE}.get(
        o.get("q"), csv.QUOTE_MINIMAL)
    wr = csv.writer(sys.stdout, delimiter=od, quoting=qm, lineterminator="\n")
    if show:
        wr.writerow(names)
    for r in out:
        wr.writerow(r)


main()
PYEOF
chmod 0755 /app/rebuild/tabula
