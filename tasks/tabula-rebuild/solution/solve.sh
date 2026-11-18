#!/bin/bash
# Reference solution: install the clean-room Python rebuild of tabula.
set -euo pipefail
mkdir -p /app/rebuild
cat > /app/rebuild/tabula <<'TABULA_EOF'
#!/usr/bin/env python3
# tabula clean-room rebuild (reference solution). MIT License.
import os
import sys

VERSION = b"2.3.1"

HELP = b"""Usage: tabula [OPTION]... [FILE]
Process a CSV or TSV table read from FILE (or standard input when FILE is
absent or '-') and write the result to standard output.

Input:
  -d, --delimiter=CHAR      input field delimiter (default ',' or TAB for *.tsv)
  -H, --no-header           input has no header row; columns are named c1, c2, ...
      --trim                strip leading and trailing blanks from every field
      --strict              reject rows whose field count differs from the header

Selection and filtering:
  -s, --select=LIST         output only the listed columns, in the given order
  -x, --exclude=LIST        output every column except the listed ones
  -w, --where=EXPR          keep only rows for which EXPR is true
  -u, --unique              drop repeated output rows
  -k, --skip=N              skip the first N output rows
  -n, --limit=N             output at most N rows

Ordering and grouping:
  -S, --sort=KEYS           sort rows by KEYS (COL[:FLAGS], FLAGS from n N r i)
  -g, --group=LIST          group rows by the listed columns
  -a, --agg=SPECS           aggregate columns ([NAME=]FUNC(COL), comma separated)

Output:
  -D, --out-delimiter=CHAR  output field delimiter (default: the input delimiter)
  -N, --no-header-out       do not print the header row
  -q, --quote=MODE          quoting: minimal, all, nonnumeric or none
  -e, --empty=STR           print STR in place of empty fields
  -p, --precision=N         print numbers with N decimal places (0-9)
  -r, --round=MODE          rounding for --precision: half-even, half-up, down, up
  -T, --thousands           group integer digits with commas
  -t, --pretty              print an aligned table instead of CSV
  -m, --max-width=N         with --pretty, shorten cells wider than N columns
  -c, --columns             list the column names and exit
  -C, --count               print the number of output rows and exit

  -h, --help                display this help and exit
  -V, --version             output version information and exit

Exit status: 0 success, 1 usage error, 2 bad expression or spec,
3 bad column reference, 4 output error, 5 input error, 6 file error.
"""

TRY = b"Try 'tabula --help' for more information.\n"


class Exit(Exception):
    def __init__(self, code, out=b"", err=b""):
        self.code, self.out, self.err = code, out, err


def die(code, msg):
    raise Exit(code, err=b"tabula: " + msg + b"\n")


def die_usage(msg):
    raise Exit(1, err=b"tabula: " + msg + b"\n" + TRY)


def q(b):
    return b"'" + b + b"'"


# ---------------------------------------------------------------- decimals

DIG = b"0123456789"


def dec_parse(s):
    """Return (m, scale) or None."""
    i = 0
    neg = False
    if s[:1] in (b"+", b"-"):
        neg = s[:1] == b"-"
        i = 1
    m = 0
    digits = 0
    scale = 0
    dot = False
    for c in s[i:]:
        if 48 <= c <= 57:
            digits += 1
            if digits > 15:
                return None
            m = m * 10 + c - 48
            if dot:
                scale += 1
        elif c == 46 and not dot:
            dot = True
        else:
            return None
    if not digits:
        return None
    return (-m if neg else m, scale)


def is_num(s):
    return dec_parse(s) is not None


def dec_cmp(a, b):
    s = max(a[1], b[1])
    x = a[0] * 10 ** (s - a[1])
    y = b[0] * 10 ** (s - b[1])
    return (x > y) - (x < y)


def render(m, scale, thousands=False):
    neg = m < 0
    digs = str(abs(m))
    if len(digs) < scale + 1:
        digs = "0" * (scale + 1 - len(digs)) + digs
    ip = digs[: len(digs) - scale]
    fp = digs[len(digs) - scale:] if scale else ""
    if thousands:
        parts = []
        while len(ip) > 3:
            parts.insert(0, ip[-3:])
            ip = ip[:-3]
        parts.insert(0, ip)
        ip = ",".join(parts)
    out = ("-" if neg else "") + ip + ("." + fp if scale else "")
    return out.encode()


HALF_EVEN, HALF_UP, DOWN, UP = range(4)


def round_div(num, den, mode):
    neg = num < 0
    a = -num if neg else num
    qt, r = divmod(a, den)
    if mode == DOWN:
        pass
    elif mode == UP:
        if r:
            qt += 1
    elif mode == HALF_UP:
        if 2 * r >= den:
            qt += 1
    else:
        if 2 * r > den or (2 * r == den and qt & 1):
            qt += 1
    return -qt if neg else qt


def dec_round_to(d, n, mode):
    m, s = d
    if s <= n:
        return m * 10 ** (n - s)
    return round_div(m, 10 ** (s - n), mode)


# ---------------------------------------------------------------- options

LONGOPTS = [
    (b"delimiter", 1, "d"), (b"out-delimiter", 1, "D"), (b"no-header", 0, "H"),
    (b"no-header-out", 0, "N"), (b"select", 1, "s"), (b"exclude", 1, "x"),
    (b"where", 1, "w"), (b"sort", 1, "S"), (b"group", 1, "g"),
    (b"agg", 1, "a"), (b"unique", 0, "u"), (b"limit", 1, "n"),
    (b"skip", 1, "k"), (b"precision", 1, "p"), (b"round", 1, "r"),
    (b"thousands", 0, "T"), (b"quote", 1, "q"), (b"empty", 1, "e"),
    (b"pretty", 0, "t"), (b"max-width", 1, "m"), (b"columns", 0, "c"),
    (b"count", 0, "C"), (b"strict", 0, "STRICT"), (b"trim", 0, "TRIM"),
    (b"help", 0, "h"), (b"version", 0, "V"),
]
SHORT_ALL = b"dDHNsxwSgaunkprTqetmcChV"
SHORT_ARG = b"dDsxwSgankprqem"
MINIMAL, ALL, NONNUMERIC, NONE = range(4)


class Opts:
    def __init__(self):
        self.in_delim = b","
        self.in_delim_set = False
        self.out_delim = None
        self.no_header = False
        self.no_header_out = False
        self.lists = {}
        self.where = []
        self.unique = False
        self.limit = -1
        self.skip = 0
        self.precision = -1
        self.round_mode = HALF_EVEN
        self.thousands = False
        self.quote_mode = MINIMAL
        self.quote_set = False
        self.empty = None
        self.pretty = False
        self.max_width = 0
        self.columns = False
        self.count = False
        self.strict = False
        self.trim = False
        self.file = None


def parse_num(s, name):
    if not (1 <= len(s) <= 9) or any(c < 48 or c > 57 for c in s):
        die(1, b"invalid number for " + name + b": " + q(s))
    return int(s)


def parse_delim(s, name):
    if s in (b"tab", b"\\t"):
        return b"\t"
    if len(s) == 1 and s not in (b'"', b"\n", b"\r"):
        return s
    die(1, b"invalid delimiter for " + name + b": " + q(s))


def apply_opt(O, code, arg):
    if code == "d":
        O.in_delim = parse_delim(arg, b"--delimiter")
        O.in_delim_set = True
    elif code == "D":
        O.out_delim = parse_delim(arg, b"--out-delimiter")
    elif code == "H":
        O.no_header = True
    elif code == "N":
        O.no_header_out = True
    elif code in "sxSga" and len(code) == 1:
        if code in O.lists:
            O.lists[code] += b"," + arg
        else:
            O.lists[code] = arg
    elif code == "w":
        O.where.append(arg)
    elif code == "u":
        O.unique = True
    elif code == "n":
        O.limit = parse_num(arg, b"--limit")
    elif code == "k":
        O.skip = parse_num(arg, b"--skip")
    elif code == "p":
        v = parse_num(arg, b"--precision")
        if v > 9:
            die(1, b"precision out of range (0-9): " + q(arg))
        O.precision = v
    elif code == "r":
        modes = {b"half-even": HALF_EVEN, b"half-up": HALF_UP, b"down": DOWN, b"up": UP}
        if arg not in modes:
            die(1, b"invalid rounding mode: " + q(arg) + b" (expected half-even, half-up, down, up)")
        O.round_mode = modes[arg]
    elif code == "T":
        O.thousands = True
    elif code == "q":
        modes = {b"minimal": MINIMAL, b"all": ALL, b"nonnumeric": NONNUMERIC, b"none": NONE}
        if arg not in modes:
            die(1, b"invalid quote mode: " + q(arg) + b" (expected minimal, all, nonnumeric, none)")
        O.quote_mode = modes[arg]
        O.quote_set = True
    elif code == "e":
        O.empty = arg
    elif code == "t":
        O.pretty = True
    elif code == "m":
        v = parse_num(arg, b"--max-width")
        if v < 2:
            die(1, b"--max-width must be at least 2")
        O.max_width = v
    elif code == "c":
        O.columns = True
    elif code == "C":
        O.count = True
    elif code == "STRICT":
        O.strict = True
    elif code == "TRIM":
        O.trim = True
    elif code == "h":
        raise Exit(0, out=HELP)
    elif code == "V":
        raise Exit(0, out=b"tabula " + VERSION + b"\n")


def parse_args(argv):
    O = Opts()
    nfiles = 0
    only_pos = False
    i = 0
    while i < len(argv):
        a = argv[i]
        if only_pos or a[:1] != b"-" or len(a) == 1:
            if nfiles >= 1:
                die_usage(b"extra operand " + q(a))
            O.file = a
            nfiles += 1
            i += 1
            continue
        if a == b"--":
            only_pos = True
            i += 1
            continue
        if a[1:2] == b"-":
            body = a[2:]
            eq = body.find(b"=")
            name = body[:eq] if eq >= 0 else body
            found = None
            for opt in LONGOPTS:
                if opt[0] == name:
                    found = opt
                    break
            if found is None:
                matches = [o for o in LONGOPTS if o[0].startswith(name)]
                if not matches:
                    die_usage(b"unrecognized option '--" + name + b"'")
                if len(matches) > 1:
                    msg = b"tabula: option '--" + name + b"' is ambiguous; possibilities:"
                    for o in matches:
                        msg += b" '--" + o[0] + b"'"
                    raise Exit(1, err=msg + b"\n" + TRY)
                found = matches[0]
            arg = None
            if not found[1]:
                if eq >= 0:
                    die_usage(b"option '--" + found[0] + b"' doesn't allow an argument")
            elif eq >= 0:
                arg = body[eq + 1:]
            elif i + 1 < len(argv):
                i += 1
                arg = argv[i]
            else:
                die_usage(b"option '--" + found[0] + b"' requires an argument")
            apply_opt(O, found[2], arg)
            i += 1
            continue
        j = 1
        while j < len(a):
            c = a[j:j + 1]
            if c not in [SHORT_ALL[k:k + 1] for k in range(len(SHORT_ALL))]:
                die_usage(b"invalid option -- '" + c + b"'")
            if c in [SHORT_ARG[k:k + 1] for k in range(len(SHORT_ARG))]:
                if j + 1 < len(a):
                    arg = a[j + 1:]
                elif i + 1 < len(argv):
                    i += 1
                    arg = argv[i]
                else:
                    die_usage(b"option requires an argument -- '" + c + b"'")
                apply_opt(O, c.decode(), arg)
                break
            apply_opt(O, c.decode(), None)
            j += 1
        i += 1
    return O


# ---------------------------------------------------------------- input

def parse_csv(buf, delim):
    d = delim[0]
    nul = buf.find(b"\0")
    if nul >= 0:
        die(5, b"line %d: NUL byte in input" % (buf.count(b"\n", 0, nul) + 1))
    n = len(buf)
    pos = 3 if buf[:3] == b"\xef\xbb\xbf" else 0
    line = 1
    recs = []
    while pos < n:
        if buf[pos] == 10:
            pos += 1
            line += 1
            continue
        if buf[pos] == 13 and pos + 1 < n and buf[pos + 1] == 10:
            pos += 2
            line += 1
            continue
        fields = []
        rline = line
        while True:
            if pos < n and buf[pos] == 34:
                fline = line
                pos += 1
                f = bytearray()
                while True:
                    if pos >= n:
                        die(5, b"line %d: unterminated quoted field" % fline)
                    c = buf[pos]
                    if c == 34:
                        if pos + 1 < n and buf[pos + 1] == 34:
                            f.append(34)
                            pos += 2
                            continue
                        pos += 1
                        break
                    if c == 10:
                        line += 1
                    f.append(c)
                    pos += 1
                fields.append(bytes(f))
                if pos >= n:
                    break
                if buf[pos] == d:
                    pos += 1
                    continue
                if buf[pos] == 10:
                    pos += 1
                    line += 1
                    break
                if buf[pos] == 13 and pos + 1 < n and buf[pos + 1] == 10:
                    pos += 2
                    line += 1
                    break
                die(5, b"line %d: unexpected character after closing quote" % line)
            start = pos
            while pos < n:
                c = buf[pos]
                if c == d or c == 10 or (c == 13 and pos + 1 < n and buf[pos + 1] == 10):
                    break
                pos += 1
            fields.append(buf[start:pos])
            if pos >= n:
                break
            if buf[pos] == d:
                pos += 1
                continue
            pos += 1 if buf[pos] == 10 else 2
            line += 1
            break
        recs.append((fields, rline))
    return recs


ERRTEXT = {2: b"No such file or directory", 13: b"Permission denied", 20: b"Not a directory",
           21: b"Is a directory"}


def errtext(e):
    return ERRTEXT.get(e, b"Input/output error")


# ---------------------------------------------------------------- columns

def all_digits(s):
    return len(s) > 0 and all(48 <= c <= 57 for c in s)


def find_col(names, name):
    for i, n in enumerate(names):
        if n == name:
            return i
    return -1


def index_col(s, ncols):
    if len(s) > 9:
        die(3, b"column index out of range: " + s)
    v = int(s)
    if v < 1 or v > ncols:
        die(3, b"column index out of range: " + s)
    return v - 1


def check_list_syntax(lst):
    for it in lst.split(b","):
        if not it:
            die(2, b"empty item in column list " + q(lst))


def resolve_list(lst, names):
    ncols = len(names)
    out = []
    for it in lst.split(b","):
        dash = it.find(b"-")
        if all_digits(it):
            lo = hi = index_col(it, ncols)
        elif dash >= 0 and all_digits(it[:dash]) and (all_digits(it[dash + 1:]) or dash + 1 == len(it)):
            lo = index_col(it[:dash], ncols)
            hi = index_col(it[dash + 1:], ncols) if dash + 1 < len(it) else ncols - 1
        elif it[:1] == b"-" and all_digits(it[1:]):
            if ncols < 1:
                die(3, b"column index out of range: 1")
            lo = 0
            hi = index_col(it[1:], ncols)
        else:
            lo = hi = find_col(names, it)
            if lo < 0:
                die(3, b"unknown column " + q(it))
        step = 1 if lo <= hi else -1
        out.extend(range(lo, hi + step, step))
    return out


def resolve_name_or_index(names, s):
    if all_digits(s):
        return index_col(s, len(names))
    c = find_col(names, s)
    if c < 0:
        die(3, b"unknown column " + q(s))
    return c


# ---------------------------------------------------------------- expressions

(T_IDENT, T_BQ, T_DOLLAR, T_NUM, T_STR, T_OP, T_AND, T_OR, T_NOT, T_LP, T_RP, T_END) = range(12)
OPERANDS = (T_IDENT, T_BQ, T_DOLLAR, T_NUM, T_STR)


def expr_err(off, msg):
    raise Exit(2, err=b"tabula: expression error at offset %d: " % off + msg + b"\n")


def isdig(c):
    return 48 <= c <= 57


def is_ident_start(c):
    return 65 <= c <= 90 or 97 <= c <= 122 or c == 95


def is_ident_char(c):
    return is_ident_start(c) or isdig(c)


def tokenize(s):
    n = len(s)
    i = 0
    toks = []
    while True:
        while i < n and s[i] in b" \t\n\r":
            i += 1
        off = i
        text = None
        if i >= n:
            toks.append((T_END, off, i, None))
            return toks
        c = s[i]
        nx = s[i + 1] if i + 1 < n else -1
        if c == 40:
            typ = T_LP
            i += 1
        elif c == 41:
            typ = T_RP
            i += 1
        elif c == 96:
            k = s.find(b"`", i + 1)
            if k < 0:
                expr_err(i, b"unterminated quoted column name")
            typ = T_BQ
            text = s[i + 1:k]
            i = k + 1
        elif c in (39, 34):
            b = bytearray()
            j = i + 1
            while True:
                if j >= n:
                    expr_err(i, b"unterminated string")
                if s[j] == c:
                    break
                if s[j] == 92:
                    if j + 1 >= n:
                        expr_err(i, b"unterminated string")
                    j += 1
                    e = s[j]
                    b.append(10 if e == 110 else 9 if e == 116 else e)
                    j += 1
                    continue
                b.append(s[j])
                j += 1
            typ = T_STR
            text = bytes(b)
            i = j + 1
        elif c == 36:
            if nx == 61:
                typ, text = T_OP, b"$="
                i += 2
            elif nx >= 0 and isdig(nx):
                j = i + 1
                while j < n and isdig(s[j]):
                    j += 1
                typ, text = T_DOLLAR, s[i + 1:j]
                i = j
            else:
                expr_err(i, b"invalid column reference")
        elif isdig(c) or (c == 46 and nx >= 0 and isdig(nx)) or (
                c in (43, 45) and nx >= 0 and (isdig(nx) or (nx == 46 and i + 2 < n and isdig(s[i + 2])))):
            j = i + 1 if c in (43, 45) else i
            while j < n and (isdig(s[j]) or s[j] == 46):
                j += 1
            typ, text = T_NUM, s[i:j]
            i = j
        elif is_ident_start(c):
            j = i
            while j < n and is_ident_char(s[j]):
                j += 1
            w = s[i:j].lower()
            if w == b"and":
                typ = T_AND
            elif w == b"or":
                typ = T_OR
            elif w == b"not":
                typ = T_NOT
            else:
                typ, text = T_IDENT, s[i:j]
            i = j
        elif c == 61:
            typ = T_OP
            if nx == 61:
                text = b"=="
                i += 2
            else:
                text = b"="
                i += 1
        elif c == 33:
            if nx == 61:
                typ, text = T_OP, b"!="
                i += 2
            else:
                typ = T_NOT
                i += 1
        elif c == 60:
            typ = T_OP
            if nx == 61:
                text = b"<="
                i += 2
            elif nx == 62:
                text = b"<>"
                i += 2
            else:
                text = b"<"
                i += 1
        elif c == 62:
            typ = T_OP
            if nx == 61:
                text = b">="
                i += 2
            else:
                text = b">"
                i += 1
        elif c == 126:
            typ, text = T_OP, b"~"
            i += 1
        elif c == 94 and nx == 61:
            typ, text = T_OP, b"^="
            i += 2
        elif c == 38 and nx == 38:
            typ = T_AND
            i += 2
        elif c == 124 and nx == 124:
            typ = T_OR
            i += 2
        else:
            if 0x21 <= c <= 0x7E:
                expr_err(i, b"invalid character '" + bytes([c]) + b"'")
            expr_err(i, b"invalid character 0x%02x" % c)
        toks.append((typ, off, i, text))


class Parser:
    def __init__(self, src):
        self.src = src
        self.toks = tokenize(src)
        self.pos = 0
        self.operands = []

    def cur(self):
        return self.toks[self.pos]

    def unexpected(self):
        t = self.cur()
        if t[0] == T_END:
            expr_err(t[1], b"unexpected end of expression")
        expr_err(t[1], b"unexpected token '" + self.src[t[1]:t[2]] + b"'")

    def parse(self):
        n = self.parse_or()
        if self.cur()[0] != T_END:
            self.unexpected()
        return n

    def parse_or(self):
        left = self.parse_and()
        while self.cur()[0] == T_OR:
            self.pos += 1
            left = ["or", left, self.parse_and()]
        return left

    def parse_and(self):
        left = self.parse_not()
        while self.cur()[0] == T_AND:
            self.pos += 1
            left = ["and", left, self.parse_not()]
        return left

    def parse_not(self):
        if self.cur()[0] == T_NOT:
            self.pos += 1
            return ["not", self.parse_not()]
        return self.parse_cmp()

    def parse_cmp(self):
        t = self.cur()
        if t[0] == T_LP:
            self.pos += 1
            n = self.parse_or()
            if self.cur()[0] != T_RP:
                expr_err(self.cur()[1], b"expected ')'")
            self.pos += 1
            return n
        if t[0] in OPERANDS:
            node = {"kind": "truth", "lk": t[0], "lt": t[3], "lc": -1, "rc": -1}
            self.operands.append(node)
            self.pos += 1
            if self.cur()[0] == T_OP:
                node["op"] = self.cur()[3]
                self.pos += 1
                if self.cur()[0] not in OPERANDS:
                    self.unexpected()
                node["kind"] = "cmp"
                node["rk"] = self.cur()[0]
                node["rt"] = self.cur()[3]
                self.pos += 1
            return node
        self.unexpected()


def resolve_operand(names, kind, text):
    if kind == T_DOLLAR:
        return index_col(text, len(names))
    if kind == T_BQ:
        c = find_col(names, text)
        if c < 0:
            die(3, b"unknown column " + q(text))
        return c
    if kind == T_IDENT:
        return find_col(names, text)
    return -1


def truthy(v):
    if not v:
        return False
    d = dec_parse(v)
    if d is not None and d[0] == 0:
        return False
    return True


def bytecmp(a, b):
    return (a > b) - (a < b)


def evaluate(n, row):
    if isinstance(n, list):
        if n[0] == "or":
            return evaluate(n[1], row) or evaluate(n[2], row)
        if n[0] == "and":
            return evaluate(n[1], row) and evaluate(n[2], row)
        return not evaluate(n[1], row)
    lv = row[n["lc"]] if n["lc"] >= 0 else n["lt"]
    if n["kind"] == "truth":
        return truthy(lv)
    rv = row[n["rc"]] if n["rc"] >= 0 else n["rt"]
    op = n["op"]
    if op == b"~":
        return rv in lv
    if op == b"^=":
        return lv.startswith(rv)
    if op == b"$=":
        return lv.endswith(rv)
    x, y = dec_parse(lv), dec_parse(rv)
    c = dec_cmp(x, y) if (x is not None and y is not None) else bytecmp(lv, rv)
    if op in (b"=", b"=="):
        return c == 0
    if op in (b"!=", b"<>"):
        return c != 0
    if op == b"<":
        return c < 0
    if op == b"<=":
        return c <= 0
    if op == b">":
        return c > 0
    return c >= 0


# ---------------------------------------------------------------- sorting

def parse_sortkeys(spec):
    keys = []
    for it in spec.split(b","):
        colon = it.rfind(b":")
        k = {"numeric": False, "natural": False, "reverse": False, "icase": False}
        if colon >= 0:
            for f in it[colon + 1:]:
                if f == 110:
                    k["numeric"] = True
                elif f == 78:
                    k["natural"] = True
                elif f == 114:
                    k["reverse"] = True
                elif f == 105:
                    k["icase"] = True
                else:
                    die(2, b"invalid sort flag '" + bytes([f]) + b"' in key " + q(it))
            if k["numeric"] and k["natural"]:
                die(2, b"conflicting sort flags in key " + q(it))
            k["col"] = it[:colon]
        else:
            k["col"] = it
        if not k["col"]:
            die(2, b"empty sort key")
        keys.append(k)
    return keys


def fold(c):
    return c + 32 if 65 <= c <= 90 else c


def cmp_fold(a, b, icase):
    if icase:
        a = bytes(fold(c) for c in a)
        b = bytes(fold(c) for c in b)
    return bytecmp(a, b)


def natcmp(s, t, icase):
    i = j = 0
    n1, n2 = len(s), len(t)
    while True:
        if i == n1 and j == n2:
            return 0
        if i == n1:
            return -1
        if j == n2:
            return 1
        if isdig(s[i]) and isdig(t[j]):
            while i < n1 and s[i] == 48:
                i += 1
            while j < n2 and t[j] == 48:
                j += 1
            i0, j0 = i, j
            while i < n1 and isdig(s[i]):
                i += 1
            while j < n2 and isdig(t[j]):
                j += 1
            a, b = s[i0:i], t[j0:j]
            if len(a) != len(b):
                return -1 if len(a) < len(b) else 1
            if a != b:
                return -1 if a < b else 1
            continue
        if isdig(s[i]):
            return -1
        if isdig(t[j]):
            return 1
        c1 = fold(s[i]) if icase else s[i]
        c2 = fold(t[j]) if icase else t[j]
        if c1 != c2:
            return -1 if c1 < c2 else 1
        i += 1
        j += 1


def key_cmp(k, a, b):
    if k["numeric"]:
        x, y = dec_parse(a), dec_parse(b)
        if x is not None and y is not None:
            c = dec_cmp(x, y)
        elif x is not None:
            return -1
        elif y is not None:
            return 1
        else:
            return 0
    elif k["natural"]:
        c = natcmp(a, b, k["icase"])
    else:
        c = cmp_fold(a, b, k["icase"])
    return -c if k["reverse"] else c


# ---------------------------------------------------------------- aggregates

AGG_NAMES = [b"count", b"sum", b"avg", b"min", b"max", b"first", b"last", b"distinct"]


def split_aggs(spec):
    items = []
    depth = 0
    start = 0
    for p in range(len(spec) + 1):
        c = spec[p] if p < len(spec) else 0
        if c == 40:
            depth += 1
        elif c == 41:
            depth -= 1
        if p == len(spec) or (c == 44 and depth == 0):
            items.append(spec[start:p])
            start = p + 1
    return items


def parse_aggs(spec):
    aggs = []
    for it in split_aggs(spec):
        lp = it.find(b"(")
        if lp < 0 or len(it) < 3 or it[-1:] != b")":
            die(2, b"invalid aggregate spec " + q(it))
        arg = it[lp + 1:-1]
        if not arg or b"(" in arg or b")" in arg:
            die(2, b"invalid aggregate spec " + q(it))
        eq = it.find(b"=", 0, lp)
        alias = None
        if eq >= 0:
            if eq == 0:
                die(2, b"invalid aggregate spec " + q(it))
            alias = it[:eq]
            fname = it[eq + 1:lp]
        else:
            fname = it[:lp]
        if not fname:
            die(2, b"invalid aggregate spec " + q(it))
        if fname not in AGG_NAMES:
            die(2, b"unknown aggregate function " + q(fname))
        if arg == b"*" and fname != b"count":
            die(2, b"'*' is only valid with count")
        aggs.append({"func": fname, "alias": alias, "arg": arg, "col": -1})
    return aggs


def agg_value(a, rows):
    f = a["func"]
    col = a["col"]
    if f == b"count":
        return b"%d" % sum(1 for r in rows if col < 0 or r[col])
    if f == b"first":
        return rows[0][col] if rows else b""
    if f == b"last":
        return rows[-1][col] if rows else b""
    if f == b"distinct":
        return b"%d" % len({r[col] for r in rows if r[col]})
    if f in (b"sum", b"avg"):
        total = 0
        scale = 0
        cnt = 0
        for r in rows:
            d = dec_parse(r[col])
            if d is None:
                continue
            if d[1] > scale:
                total *= 10 ** (d[1] - scale)
                scale = d[1]
            total += d[0] * 10 ** (scale - d[1])
            cnt += 1
        if not cnt:
            return b""
        if f == b"sum":
            return render(total, scale)
        if scale <= 6:
            num, den = total * 10 ** (6 - scale), cnt
        else:
            num, den = total, cnt * 10 ** (scale - 6)
        s = render(round_div(num, den, HALF_EVEN), 6)
        s = s.rstrip(b"0")
        if s.endswith(b"."):
            s = s[:-1]
        return s
    vals = [r[col] for r in rows if r[col]]
    if not vals:
        return b""
    allnum = all(is_num(v) for v in vals)
    best = vals[0]
    for v in vals[1:]:
        c = dec_cmp(dec_parse(v), dec_parse(best)) if allnum else bytecmp(v, best)
        if (f == b"min" and c < 0) or (f == b"max" and c > 0):
            best = v
    return best


# ---------------------------------------------------------------- display

ZERO_W = [(0x0300, 0x036F), (0x0483, 0x0489), (0x0591, 0x05BD), (0x0610, 0x061A), (0x064B, 0x065F),
          (0x1AB0, 0x1AFF), (0x1DC0, 0x1DFF), (0x200B, 0x200F), (0x20D0, 0x20FF), (0xFE00, 0xFE0F),
          (0xFE20, 0xFE2F)]
WIDE_W = [(0x1100, 0x115F), (0x2E80, 0x303E), (0x3041, 0x33FF), (0x3400, 0x4DBF), (0x4E00, 0x9FFF),
          (0xA000, 0xA4CF), (0xAC00, 0xD7A3), (0xF900, 0xFAFF), (0xFE30, 0xFE4F), (0xFF00, 0xFF60),
          (0xFFE0, 0xFFE6), (0x1F300, 0x1F64F), (0x1F900, 0x1F9FF), (0x20000, 0x2FFFD), (0x30000, 0x3FFFD)]


def cp_width(cp):
    for lo, hi in ZERO_W:
        if lo <= cp <= hi:
            return 0
    for lo, hi in WIDE_W:
        if lo <= cp <= hi:
            return 2
    return 1


def units(v):
    out = []
    i = 0
    n = len(v)
    while i < n:
        c = v[i]
        if c < 0x20 or c == 0x7F:
            out.append((bytes([94, c ^ 0x40]), 2))
            i += 1
            continue
        if c < 0x80:
            out.append((bytes([c]), 1))
            i += 1
            continue
        if 0xC2 <= c <= 0xDF:
            ln, cp = 2, c & 0x1F
        elif 0xE0 <= c <= 0xEF:
            ln, cp = 3, c & 0x0F
        elif 0xF0 <= c <= 0xF4:
            ln, cp = 4, c & 0x07
        else:
            ln = 0
        ok = ln > 0
        if ok:
            for k in range(1, ln):
                if i + k >= n or not (0x80 <= v[i + k] <= 0xBF):
                    ok = False
                    break
                cp = (cp << 6) | (v[i + k] & 0x3F)
        if ok:
            s1 = v[i + 1]
            if (c == 0xE0 and s1 < 0xA0) or (c == 0xED and s1 > 0x9F) or \
                    (c == 0xF0 and s1 < 0x90) or (c == 0xF4 and s1 > 0x8F):
                ok = False
        if not ok:
            out.append((bytes([c]), 1))
            i += 1
            continue
        out.append((v[i:i + ln], cp_width(cp)))
        i += ln
    return out


def display_cell(v, maxw):
    us = units(v)
    total = sum(w for _, w in us)
    if maxw <= 0 or total <= maxw:
        return b"".join(b for b, _ in us), total
    out = []
    tw = 0
    for b, w in us:
        if tw + w > maxw - 1:
            break
        out.append(b)
        tw += w
    return b"".join(out) + "…".encode(), tw + 1


# ---------------------------------------------------------------- output

def needs_quote(v, delim, ncols):
    if not v:
        return ncols == 1
    if delim in v or b'"' in v or b"\n" in v or b"\r" in v:
        return True
    if v[:1] in (b" ", b"\t") or v[-1:] in (b" ", b"\t"):
        return True
    return False


def write_csv_row(out, O, row, delim, line):
    ncols = len(row)
    parts = []
    for j, v in enumerate(row):
        if O.quote_mode == ALL:
            qq = True
        elif O.quote_mode == NONNUMERIC:
            qq = (not is_num(v)) or needs_quote(v, delim, ncols)
        elif O.quote_mode == NONE:
            if needs_quote(v, delim, ncols):
                die(4, b"cannot write field without quoting (line %d, column %d)" % (line, j + 1))
            qq = False
        else:
            qq = needs_quote(v, delim, ncols)
        parts.append(b'"' + v.replace(b'"', b'""') + b'"' if qq else v)
    out.append(delim.join(parts) + b"\n")


def write_pretty(out, O, names, rows, show_header):
    ncols = len(names)
    if ncols == 0:
        return
    right = []
    for j in range(ncols):
        vals = [r[j] for r in rows if r[j]]
        right.append(bool(vals) and all(is_num(v) for v in vals))
    widths = [1] * ncols
    disp = []
    src = ([names] if show_header else []) + rows
    for r in src:
        d = [display_cell(v, O.max_width) for v in r]
        for j, (_, w) in enumerate(d):
            widths[j] = max(widths[j], w)
        disp.append(d)
    for idx, d in enumerate(disp):
        cells = []
        for j, (b, w) in enumerate(d):
            pad = b" " * (widths[j] - w)
            cells.append(pad + b if right[j] else b + pad)
        out.append(b" | ".join(cells).rstrip(b" ") + b"\n")
        if idx == 0 and show_header:
            out.append(b"-+-".join(b"-" * w for w in widths) + b"\n")


def format_number(O, v):
    d = dec_parse(v)
    if d is None:
        return None
    if O.precision >= 0:
        return render(dec_round_to(d, O.precision, O.round_mode), O.precision, O.thousands)
    return render(d[0], d[1], O.thousands)


# ---------------------------------------------------------------- main

def run(argv, stdin_reader):
    O = parse_args(argv)
    L = O.lists
    if "s" in L and "x" in L:
        die(1, b"--select and --exclude are mutually exclusive")
    if ("s" in L or "x" in L) and ("g" in L or "a" in L):
        die(1, b"--select/--exclude cannot be combined with --group/--agg")
    if O.count and O.pretty:
        die(1, b"--count cannot be combined with --pretty")
    if O.quote_set and O.pretty:
        die(1, b"--quote cannot be combined with --pretty")
    if O.max_width and not O.pretty:
        die(1, b"--max-width requires --pretty")

    parsers = []
    roots = []
    for w in O.where:
        p = Parser(w)
        roots.append(p.parse())
        parsers.append(p)
    for k in "sxg":
        if k in L:
            check_list_syntax(L[k])
    aggs = parse_aggs(L["a"]) if "a" in L else None
    skeys = parse_sortkeys(L["S"]) if "S" in L else None

    fname = O.file
    if fname is not None and fname != b"-":
        try:
            st = os.stat(fname)
        except OSError as e:
            die(6, b"cannot open " + q(fname) + b": " + errtext(e.errno))
        import stat as _stat
        if _stat.S_ISDIR(st.st_mode):
            die(6, b"cannot read " + q(fname) + b": " + errtext(21))
        try:
            with open(fname, "rb") as fh:
                buf = fh.read()
        except OSError as e:
            die(6, b"cannot open " + q(fname) + b": " + errtext(e.errno))
        if not O.in_delim_set and fname.endswith(b".tsv"):
            O.in_delim = b"\t"
    else:
        buf = stdin_reader()
    out_delim = O.out_delim if O.out_delim is not None else O.in_delim
    recs = parse_csv(buf, O.in_delim)
    if O.trim:
        recs = [([f.strip(b" \t") for f in fs], ln) for fs, ln in recs]
    if not recs and not O.no_header:
        die(5, b"empty input")
    ncols = len(recs[0][0]) if recs else 0
    names = []
    for j in range(ncols):
        if not O.no_header and recs[0][0][j]:
            names.append(recs[0][0][j])
        else:
            names.append(b"c%d" % (j + 1))
    data = recs if O.no_header else recs[1:]
    rows = []
    for fs, ln in data:
        if len(fs) != ncols:
            if O.strict:
                die(5, b"line %d: expected %d fields, got %d" % (ln, ncols, len(fs)))
            fs = (fs + [b""] * ncols)[:ncols]
        rows.append(fs)

    if O.columns:
        raise Exit(0, out=b"".join(b"%d\t" % (j + 1) + n + b"\n" for j, n in enumerate(names)))

    for p in parsers:
        for node in p.operands:
            node["lc"] = resolve_operand(names, node["lk"], node["lt"])
            if node["kind"] == "cmp":
                node["rc"] = resolve_operand(names, node["rk"], node["rt"])
    grouped = "g" in L or "a" in L
    sel = None
    if "s" in L:
        sel = resolve_list(L["s"], names)
    if "x" in L:
        ex = set(resolve_list(L["x"], names))
        sel = [j for j in range(ncols) if j not in ex]
        if not sel:
            die(3, b"no columns left to output")
    gcols = resolve_list(L["g"], names) if "g" in L else []
    if grouped:
        if aggs is None:
            aggs = [{"func": b"count", "alias": None, "arg": b"*", "col": -1}]
        for a in aggs:
            a["col"] = resolve_name_or_index(names, a["arg"]) if a["arg"] != b"*" else -1
            if a["alias"] is not None:
                a["header"] = a["alias"]
            elif a["col"] < 0:
                a["header"] = b"count"
            else:
                a["header"] = a["func"] + b"_" + names[a["col"]]

    rows = [r for r in rows if all(evaluate(rt, r) for rt in roots)]

    if grouped:
        groups = {}
        order = []
        for r in rows:
            key = tuple(r[c] for c in gcols)
            if key not in groups:
                groups[key] = []
                order.append(key)
            groups[key].append(r)
        if "g" not in L and not order:
            order = [()]
            groups[()] = []
        newrows = []
        for key in order:
            members = groups[key]
            newrows.append([members[0][c] for c in gcols] + [agg_value(a, members) for a in aggs])
        names = [names[c] for c in gcols] + [a["header"] for a in aggs]
        rows = newrows

    if skeys is not None:
        import functools
        for k in skeys:
            k["idx"] = resolve_name_or_index(names, k["col"])

        def cmp(a, b):
            for k in skeys:
                c = key_cmp(k, a[k["idx"]], b[k["idx"]])
                if c:
                    return c
            return 0
        rows = sorted(rows, key=functools.cmp_to_key(cmp))

    if sel is not None:
        names = [names[j] for j in sel]
        rows = [[r[j] for j in sel] for r in rows]

    if O.unique:
        seen = set()
        uniq = []
        for r in rows:
            t = tuple(r)
            if t not in seen:
                seen.add(t)
                uniq.append(r)
        rows = uniq

    rows = rows[O.skip:]
    if O.limit >= 0:
        rows = rows[:O.limit]

    if O.count:
        raise Exit(0, out=b"%d\n" % len(rows))

    fmt_rows = []
    for r in rows:
        nr = []
        for v in r:
            if O.precision >= 0 or O.thousands:
                f = format_number(O, v)
                if f is not None:
                    v = f
            if O.empty is not None and not v:
                v = O.empty
            nr.append(v)
        fmt_rows.append(nr)

    out = []
    show_header = not O.no_header and not O.no_header_out
    if O.pretty:
        write_pretty(out, O, names, fmt_rows, show_header)
    elif names:
        line = 1
        if show_header:
            write_csv_row(out, O, names, out_delim, line)
            line += 1
        for r in fmt_rows:
            write_csv_row(out, O, r, out_delim, line)
            line += 1
    raise Exit(0, out=b"".join(out))


def main():
    argv = [os.fsencode(a) for a in sys.argv[1:]]
    try:
        run(argv, lambda: sys.stdin.buffer.read())
    except Exit as e:
        sys.stdout.buffer.write(e.out)
        sys.stdout.buffer.flush()
        sys.stderr.buffer.write(e.err)
        sys.stderr.buffer.flush()
        sys.exit(e.code)


if __name__ == "__main__":
    main()
TABULA_EOF
chmod 0755 /app/rebuild/tabula
