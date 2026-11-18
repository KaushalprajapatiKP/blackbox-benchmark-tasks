#!/usr/bin/env python3
"""Seeded generator of tabula invocation cases (authoring tool, MIT).

Writes a JSON list of case specs: {id, cat, args, files, stdin}. All byte
strings are base64 encoded. Expected outputs are recorded separately by
running the original binary (record_cases.py).
"""
import base64
import json
import random
import sys

R = random.Random(20261117)
CASES = []


def b64(b):
    return base64.b64encode(b).decode()


def add(cat, args, files=None, stdin=b""):
    files = files or {}
    CASES.append({
        "cat": cat,
        "args": [a if isinstance(a, str) else a.decode("utf-8", "surrogateescape") for a in args],
        "files": {k: b64(v) for k, v in files.items()},
        "stdin": b64(stdin),
    })


# ------------------------------------------------------------------ datasets

SALES = (
    "id,region,product,qty,price,date,note\n"
    "1,north,widget,12,3.50,2026-01-04,\n"
    "2,south,gadget,+4,10,2026-01-05,rush\n"
    "3,north,gizmo,.75,2.125,2026-01-05,\"fragile, handle\"\n"
    "4,east,widget,7,3.5,2026-01-06,\n"
    "5,west,doohickey,N/A,0.10,2026-01-07,backorder\n"
    "6,south,widget,-2,3.50,2026-01-07,return\n"
    "7,north,gadget,0,10.00,2026-01-08,\n"
    "8,east,gizmo,1e3,2.13,2026-01-08,typo qty\n"
    "9,west,widget,007,3.495,2026-01-09,\n"
    "10,south,gizmo,3,,2026-01-10,no price\n"
    "11,north,widget,12,3.50,2026-01-04,\n"
    "12,East,Gadget,5,9.995,2026-01-11,\n"
).encode()

PEOPLE = (
    "name,city,age,score\n"
    "Ana,Lisboa,34,88.5\n"
    "王芳,北京,29,91\n"
    "José,Madrid,41,79.25\n"
    "Zoë,Berlin,,85\n"
    "テスト,東京,25,-3\n"
    "Bob \U0001F600,Austin,52,100.0\n"
    "한글,서울,38,77\n"
    "è́x,Paris,19,abc\n"
).encode("utf-8")

VERSIONS = (
    "file,size\n"
    "file10.txt,10\n"
    "file2.txt,2\n"
    "File1.txt,1\n"
    "file02.txt,2\n"
    "img12a,120\n"
    "img12,12\n"
    "a,0\n"
    ",\n"
    "file1.txt,1\n"
    "file010.txt,10\n"
    "B,5\n"
    "b,6\n"
).encode()

MESSY = (
    "\"key\",\"value\",\"comment\"\n"
    "a,\" padded \",\"has \"\"quotes\"\"\"\n"
    "b,\"multi\nline\",ok\n"
    "c,plain,\"comma, inside\"\n"
    "d,,\n"
    "e,\ttabbed,trailing \n"
    "f,x\"y,mid quote\n"
).encode()

NUMS = (
    "label,v\n"
    "a,1\n"
    "b,1.0\n"
    "c,-0\n"
    "d,0.5\n"
    "e,2.5\n"
    "f,-2.5\n"
    "g,1234567.891\n"
    "h,123456789012345\n"
    "i,1234567890123456\n"
    "j,5.\n"
    "k,+3\n"
    "l,.25\n"
    "m,0.125\n"
    "n,-0.004\n"
    "o,x\n"
    "p,\n"
    "q,1e2\n"
    "r, 7\n"
).encode()

TSV = (
    "col a\tcol b\tn\n"
    "x\t1\t3\n"
    "y, z\t2\t-1\n"
    "\"q\"\"t\"\t3\t10\n"
).encode()

STD_FILES = {
    "sales.csv": SALES,
    "people.csv": PEOPLE,
    "versions.csv": VERSIONS,
    "messy.csv": MESSY,
    "nums.csv": NUMS,
    "data.tsv": TSV,
}


def F(*names):
    return {n: STD_FILES[n] for n in names}


# ------------------------------------------------------------------ 1. cli

def cat_cli():
    c = "cli"
    add(c, ["--help"])
    add(c, ["-h"])
    add(c, ["--version"])
    add(c, ["-V"])
    add(c, ["--vers"])
    add(c, ["--he"])
    add(c, ["-x", "id", "-V"], F("sales.csv"))
    add(c, ["-z"])
    add(c, ["-tz"], F("sales.csv"))
    add(c, ["--frobnicate"])
    add(c, ["--frobnicate=3"])
    add(c, ["--s", "x"])
    add(c, ["--no-h"])
    add(c, ["--no-header-"])
    add(c, ["--=x"])
    add(c, ["--c"])
    add(c, ["--co"])
    add(c, ["--cou", "sales.csv"], F("sales.csv"))
    add(c, ["--col", "sales.csv"], F("sales.csv"))
    add(c, ["--select"])
    add(c, ["-s"])
    add(c, ["-ts"])
    add(c, ["--unique=yes", "sales.csv"], F("sales.csv"))
    add(c, ["--pretty=1"])
    add(c, ["sales.csv", "people.csv"], F("sales.csv", "people.csv"))
    add(c, ["sales.csv", "-t", "people.csv"], F("sales.csv", "people.csv"))
    add(c, ["-sid,qty", "sales.csv"], F("sales.csv"))
    add(c, ["-tusregion", "sales.csv"], F("sales.csv"))
    add(c, ["--select=region", "--sel", "product", "sales.csv"], F("sales.csv"))
    add(c, ["sales.csv", "--limit", "2"], F("sales.csv"))
    add(c, ["--", "-n"], {"-n": SALES})
    add(c, ["-n", "1", "--", "-n"], {"-n": SALES})
    add(c, ["-"], stdin=SALES)
    add(c, ["-n", "2"], stdin=SALES)
    add(c, ["-n", "-1", "sales.csv"], F("sales.csv"))
    add(c, ["-n", "", "sales.csv"], F("sales.csv"))
    add(c, ["--limit=+2", "sales.csv"], F("sales.csv"))
    add(c, ["--limit=1234567890", "sales.csv"], F("sales.csv"))
    add(c, ["--limit=000000003", "sales.csv"], F("sales.csv"))
    add(c, ["-k", "x", "sales.csv"], F("sales.csv"))
    add(c, ["-p", "10", "sales.csv"], F("sales.csv"))
    add(c, ["-p", "1.5", "sales.csv"], F("sales.csv"))
    add(c, ["-p9", "-s", "price", "sales.csv"], F("sales.csv"))
    add(c, ["-t", "-m", "1", "sales.csv"], F("sales.csv"))
    add(c, ["-t", "-m", "0", "sales.csv"], F("sales.csv"))
    add(c, ["-m", "5", "sales.csv"], F("sales.csv"))
    add(c, ["-d", "ab", "sales.csv"], F("sales.csv"))
    add(c, ["-d", '"', "sales.csv"], F("sales.csv"))
    add(c, ["-d", "", "sales.csv"], F("sales.csv"))
    add(c, ["-D", "comma", "sales.csv"], F("sales.csv"))
    add(c, ["-D", "tab", "-n", "2", "sales.csv"], F("sales.csv"))
    add(c, ["-D", "\\t", "-n", "2", "sales.csv"], F("sales.csv"))
    add(c, ["-q", "quoted", "sales.csv"], F("sales.csv"))
    add(c, ["-q", "ALL", "sales.csv"], F("sales.csv"))
    add(c, ["-r", "half_even", "sales.csv"], F("sales.csv"))
    add(c, ["-r", "ceiling", "-p", "1", "sales.csv"], F("sales.csv"))
    add(c, ["-s", "id", "-x", "qty", "sales.csv"], F("sales.csv"))
    add(c, ["-s", "id", "-g", "region", "sales.csv"], F("sales.csv"))
    add(c, ["-x", "id", "-a", "count(*)", "sales.csv"], F("sales.csv"))
    add(c, ["-C", "-t", "sales.csv"], F("sales.csv"))
    add(c, ["-t", "-q", "all", "sales.csv"], F("sales.csv"))
    add(c, ["-q", "all", "-t", "-C", "sales.csv"], F("sales.csv"))
    add(c, ["-p", "2", "-p", "abc", "sales.csv"], F("sales.csv"))
    add(c, ["-p", "abc", "-z", "sales.csv"], F("sales.csv"))
    add(c, ["-z", "-p", "abc", "sales.csv"], F("sales.csv"))
    add(c, ["-p", "1", "-p", "0", "-s", "price", "sales.csv"], F("sales.csv"))
    add(c, ["missing.csv"])
    add(c, ["-t", "missing.csv"])
    add(c, ["subdir"], {"subdir/x.csv": b"a\n1\n"})
    add(c, ["-w", "(", "missing.csv"])
    add(c, ["-s", "nope", "missing.csv"])
    add(c, ["-C", "--", "--count"], {"--count": SALES})


# ------------------------------------------------------------------ 2. parse

def cat_parse():
    c = "parse"
    add(c, ["messy.csv"], F("messy.csv"))
    add(c, ["-t", "messy.csv"], F("messy.csv"))
    add(c, [], stdin=b"a,b\r\n1,2\r\n3,4\r\n")
    add(c, [], stdin=b"a,b\r\n1,2\r3\r\n")
    add(c, [], stdin=b"\xef\xbb\xbfa,b\n1,2\n")
    add(c, ["-c"], stdin=b"\xef\xbb\xbfa,b\n1,2\n")
    add(c, [], stdin=b"a,b\n\n1,2\n\r\n\n3,4\n\n")
    add(c, [], stdin=b"a,b\n1,2")
    add(c, [], stdin=b"a,b\n1,2,\n")
    add(c, [], stdin=b"a,b,c\n1,2\n4\n5,6,7,8\n")
    add(c, ["--strict"], stdin=b"a,b,c\n1,2,3\n4,5\n")
    add(c, ["--strict"], stdin=b"a,b\n\"x\ny\nz\",1\n\n1,2,3\n")
    add(c, ["--strict"], stdin=b"a,b\n1,2\n3,4,5\n6\n")
    add(c, [], stdin=b"a,b\n\"open,1\n2,3\n")
    add(c, [], stdin=b"a,b\n1,2\n\"x\nmore\n")
    add(c, [], stdin=b"a,b\n\"x\"y,1\n")
    add(c, [], stdin=b"a,b\n\"multi\nline\" ,1\n")
    add(c, [], stdin=b"a,b\n\"x\"\r1\n")
    add(c, [], stdin=b"a,b\n1,2\n3,\x004\n")
    add(c, [], stdin=b"a\x00b\n")
    add(c, [], stdin=b"")
    add(c, [], stdin=b"\n\n\r\n")
    add(c, ["-H"], stdin=b"")
    add(c, ["-H", "-C"], stdin=b"\n")
    add(c, ["-H", "-c"], stdin=b"")
    add(c, ["-C"], stdin=b"a,b\n")
    add(c, ["-t"], stdin=b"a,b\n")
    add(c, [], stdin=b"\"\"\n\"\"\n\n\"\"\n")
    add(c, [], stdin=b"a\n\n \n\"\"\nx\n")
    add(c, [], stdin=b"a,b\n\"\",\"\"\n,\n")
    add(c, [], stdin=b"a,b\nx\"\",\"y\"\"\"\n")
    add(c, [], stdin=b"a;b\n1;2\n")
    add(c, ["-d", ";"], stdin=b"a;b\n1;\"2;3\"\n4,5;6\n")
    add(c, ["-d", "tab"], stdin=b"a\tb\n1\t2\n")
    add(c, ["data.tsv"], F("data.tsv"))
    add(c, ["-d", ",", "data.tsv"], F("data.tsv"))
    add(c, ["-D", ",", "data.tsv"], F("data.tsv"))
    add(c, ["data.TSV"], {"data.TSV": TSV})
    add(c, ["x.tsv.csv"], {"x.tsv.csv": TSV})
    add(c, ["-"], stdin=TSV)
    add(c, ["--trim"], stdin=b" a , b\t\n  1 ,\t2 \n\" 3 \", 4\n")
    add(c, [], stdin=b" a , b\n 1 , 2 \n")
    add(c, ["--trim", "-c"], stdin=b" a ,  ,c\n1,2,3\n")
    add(c, [], stdin=b"a,b\n\"line1\r\nline2\",x\n")
    add(c, ["-d", "|"], stdin=b"a|b\n\"p|q\"|r\n")
    add(c, ["-d", " "], stdin=b"a b c\n1  2\n")
    add(c, [], stdin=b"\xef\xbb\xbf\n")
    add(c, [], stdin=b"x\n\xff\xfe,\n")
    add(c, ["--strict", "-H"], stdin=b"1,2\n3\n")
    add(c, ["-n", "0"], stdin=b"a,\"b\nunterminated")
    add(c, ["-c"], stdin=b"a,b\n\"oops\n")
    add(c, ["--strict", "-c"], stdin=b"a,b\n1\n")


# ------------------------------------------------------------------ 3. header

def cat_header():
    c = "header"
    add(c, ["-H"], stdin=b"1,2,3\n4,5,6\n")
    add(c, ["-H", "-c"], stdin=b"1,2,3\n4,5,6\n")
    add(c, ["-H", "-s", "c2,c1"], stdin=b"1,2,3\n4,5,6\n")
    add(c, ["-H", "-t"], stdin=b"x,10\ny,2\n")
    add(c, ["-H", "-g", "c1"], stdin=b"x,1\ny,2\nx,3\n")
    add(c, ["-H", "-H", "-N"], stdin=b"x,1\n")
    add(c, ["-c"], stdin=b"a,,c,\n1,2,3,4\n")
    add(c, [], stdin=b"a,,c,\n1,2,3,4\n")
    add(c, ["-s", "c2,c4"], stdin=b"a,,c,\n1,2,3,4\n")
    add(c, ["-s", "c3"], stdin=b"a,,c,\n1,2,3,4\n")
    add(c, ["-t"], stdin=b"a,,c,\n1,2,3,4\n")
    add(c, ["-c"], stdin=b"x,x,y\n1,2,3\n")
    add(c, ["-s", "x"], stdin=b"x,x,y\n1,2,3\n")
    add(c, ["-s", "3,x,2"], stdin=b"x,x,y\n1,2,3\n")
    add(c, ["-g", "x", "-a", "sum(x)"], stdin=b"x,x,y\n1,2,3\n1,5,6\n")
    add(c, ["-N"], F("sales.csv") and {}, stdin=SALES)
    add(c, ["-N", "-t", "-s", "1-3"], stdin=SALES)
    add(c, ["-N", "-C"], stdin=SALES)
    add(c, ["-c", "people.csv"], F("people.csv"))
    add(c, ["-c", "-s", "zzz", "people.csv"], F("people.csv"))
    add(c, ["-c", "-w", "$9 = 1", "people.csv"], F("people.csv"))
    add(c, ["-c", "-H", "people.csv"], F("people.csv"))
    add(c, ["-s", "2"], stdin=b"1,2\nx,y\n")
    add(c, ["-s", "1-1"], stdin=b"2,1\nx,y\n")
    add(c, ["-c"], stdin=b"only\n")
    add(c, ["-H", "-c"], stdin=b"a\nb,c,d\n")
    add(c, ["-H"], stdin=b"a\nb,c,d\n")
    add(c, ["--trim", "-c"], stdin=b"  \t ,b\n1,2\n")
    add(c, ["-c"], stdin=b"  ,b\n1,2\n")
    add(c, ["-s", "c1"], stdin=b"c2,\n1,2\n")
    add(c, ["-c"], stdin=b"c2,\n1,2\n")
    add(c, ["-c"], stdin=b"\"a,b\",\"c\nd\"\n1,2\n")
    add(c, ["-t"], stdin=b"\"a,b\",\"c\nd\"\n1,2\n")


# ------------------------------------------------------------------ 4. select

def cat_select():
    c = "select"
    f = F("sales.csv")
    for sel in ["product,id", "3", "2-4", "4-2", "5-", "-2", "id,id,1", "7-1", "note",
                "1-", "6-6", "date,3-1,qty"]:
        add(c, ["-s", sel, "sales.csv"], f)
    for sel in ["0", "8", "1-8", "0-2", "9-", "-0", "-9", "Product", "", "id,", ",id", "id,,qty",
                "a-b", "2-x", "1--2", "12345678901", "007"]:
        add(c, ["-s", sel, "sales.csv"], f)
    for ex in ["note", "1,3", "2-7", "1-7", "price,price", "zz", "0", "7-"]:
        add(c, ["-x", ex, "sales.csv"], f)
    add(c, ["-s", "id", "-s", "qty", "sales.csv"], f)
    add(c, ["-s", "id", "-s", "", "sales.csv"], f)
    add(c, ["-x", "note", "-x", "date,id", "sales.csv"], f)
    add(c, ["-s", "1"], stdin=b"10,2\na,b\n")
    add(c, ["-s", "10"], stdin=b"10,2\na,b\n")
    add(c, ["-s", "2-"], stdin=b"a,b\n1,2\n")
    add(c, ["-s", "a b"], stdin=b"a b,c\n1,2\n")
    add(c, ["-s", " a"], stdin=b"a, a\n1,2\n")
    add(c, ["-s", "-"], stdin=b"-,x\n1,2\n")
    add(c, ["-s", "1-2-3"], stdin=b"1-2-3,x\n1,2\n")
    add(c, ["-x", "2"], stdin=b"a,b,c\n1,2\n")
    add(c, ["-s", "3,1", "-S", "2"], stdin=b"a,b,c\n1,z,x\n2,y,w\n")
    add(c, ["-H", "-s", "1"], stdin=b"")
    add(c, ["-H", "-s", "-1"], stdin=b"")


# ------------------------------------------------------------------ 5. where (evaluation)

def cat_where():
    c = "where"
    f = F("sales.csv")
    exprs = [
        "qty > 5", "qty >= 7", "qty < 1", "qty = 12", "qty == 12.0", "price = 3.5", "price != 3.5",
        "price <> 3.50", "qty < 0", "qty = '7'", "qty = \"007\"", "qty = 7", "qty > 1e2",
        "qty > '1000'", "region = north", "region = North", "region = 'north'", "region < m",
        "region >= south", "note", "not note", "!note", "qty", "!qty", "note = ''", "price",
        "product ~ 'dg'", "product ~ g", "product ^= 'gi'", "product $= 'et'", "note ~ ''",
        "region = north and qty > 5", "region = north AND qty > 5 or region = east",
        "region = north and (qty > 5 or region = east)", "not region = north or qty = 3",
        "not (region = north or qty = 3)", "region=north&&price>3", "region=north||price>9",
        "! ! note", "$2 = south", "$4 > $1", "qty > id", "price >= qty", "`region` = west",
        "date > '2026-01-07'", "date ^= 2026", "nonexistent", "nonexistent = nonexistent",
        "region = nonexistent", "id = 1 or id = 11 or id = 12", "price > 3.495", "price < 3.4950",
        "qty = -2", "qty = +4", "qty > -3 and qty < .8", "1", "0", "'0.0'", "'x'", "''",
        "1 = 1.000", "'abc' < 'abd'", "'B' < 'a'", "id > 9", "id > '9'", "region ~ 'th'",
        "note ~ ','", "note $= 'handle'", "PRODUCT = widget", "product = WIDGET or product = Gadget",
        "id = 3 and not note ~ 'x'", "region in north",
    ]
    for e in exprs:
        add(c, ["-w", e, "sales.csv"], f)
    add(c, ["-w", "region = north", "-w", "qty > 5", "sales.csv"], f)
    add(c, ["-w", "id > 2", "--where", "id < 5", "-w", "note", "sales.csv"], f)
    add(c, ["-w", "age > 30", "people.csv"], F("people.csv"))
    add(c, ["-w", "age", "people.csv"], F("people.csv"))
    add(c, ["-w", "score >= 85", "people.csv"], F("people.csv"))
    add(c, ["-w", "name ~ '́'", "people.csv"], F("people.csv"))
    add(c, ["-w", "name > 'Z'", "people.csv"], F("people.csv"))
    add(c, ["-w", "v > 1", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "v = 0", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "v", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "v < 0", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "v >= 123456789012345", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "v = 5", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "v = ' 7'", "nums.csv"], F("nums.csv"))
    add(c, ["--trim", "-w", "v = 7", "nums.csv"], F("nums.csv"))
    add(c, ["-w", "`col a` ~ y", "data.tsv"], F("data.tsv"))
    add(c, ["-w", "`col b` = 2", "data.tsv"], F("data.tsv"))
    add(c, ["-w", "$3 < 0 or $1 = 'q\"t'", "data.tsv"], F("data.tsv"))
    add(c, ["-w", "a = 'x\\ty'"], stdin=b"a\nx\ty\nx\\ty\n")
    add(c, ["-w", "a = \"it\\'s\""], stdin=b"a\nit's\nits\n")
    add(c, ["-w", "a = 'q\\\"'"], stdin=b"a\nq\"\nq\n")
    add(c, ["-w", "a = 'l1\\nl2'"], stdin=b"a\n\"l1\nl2\"\nl1\n")
    add(c, ["-w", "a = 'z\\q'"], stdin=b"a\nzq\nz\\q\n")
    add(c, ["-w", "and = 1"], stdin=b"and,b\n1,2\n")
    add(c, ["-w", "`and` = 1"], stdin=b"and,b\n1,2\n0,3\n")
    add(c, ["-w", "x_1 = 2"], stdin=b"x_1,b\n1,2\n2,3\n")
    add(c, ["-w", "b = 2", "-H"], stdin=b"1,2\n3,4\n")
    add(c, ["-w", "c2 = 2", "-H"], stdin=b"1,2\n3,4\n")
    add(c, ["-w", "c2 = 2"], stdin=b"a,\n1,2\n3,4\n")
    add(c, ["-w", "Or = 1 Or OR = 2"], stdin=b"Or,OR\n1,9\n5,2\n3,3\n")


# ------------------------------------------------------------------ 6. where (errors)

def cat_where_err():
    c = "where_err"
    f = F("sales.csv")
    exprs = [
        "", "   ", "(", "qty >", "qty > 5 and", "and qty", "qty 5", "qty > > 5", "(qty > 5",
        "qty > 5)", "qty = 'abc", "qty = \"abc", "qty = 'abc\\", "`region = 1", "$x = 1",
        "$ = 1", "qty & 5", "qty | 5", "qty ^ 5", "qty # 5", "qty = 5;", "a.b = 1", "qty = 5 6",
        "5abc", "qty = -", "qty = - 5", "(qty > 5) = 1", "not", "!", "()", "qty = ()",
        "qty < 5 < 6", "qty = café", "é = 1", "qty = 5\t\t@", "region = north or or x",
        "$0 = 1", "$8 = 1", "$99999999999 = 1", "`Region` = 1", "`` = 1", "nonexistent = $9",
        "region = `qty", "id = 1 and $8", "qty ~", "~ 5", "qty == == 5", "qty =! 5",
        "((((qty > 1)))", "x = 'unterminated \\'", "qty\x01 = 1",
    ]
    for e in exprs:
        add(c, ["-w", e, "sales.csv"], f)
    add(c, ["-w", "qty > 1", "-w", "(", "sales.csv"], f)
    add(c, ["-w", "$9 = 1", "-w", "(", "sales.csv"], f)
    add(c, ["-w", "$9 = 1", "-s", "zz", "sales.csv"], f)
    add(c, ["-w", "`zz` = 1", "-S", "zz", "sales.csv"], f)
    add(c, ["-w", "(", "-s", ",", "sales.csv"], f)
    add(c, ["-w", "(", "-z", "sales.csv"], f)
    add(c, ["-w", "(", "missing.csv"])
    add(c, ["-w", "$1 = 1"], stdin=b"")
    add(c, ["-H", "-w", "$1 = 1"], stdin=b"")


# ------------------------------------------------------------------ 7. sort

def cat_sort():
    c = "sort"
    fv = F("versions.csv")
    for k in ["file", "file:r", "file:i", "file:ir", "file:N", "file:Nr", "file:Ni", "file:Nir",
              "size:n", "size:nr", "size", "size:r", "2:n", "size:n,file", "size:nr,file:r",
              "size:n,file:N", "file:", "file:rr"]:
        add(c, ["-S", k, "versions.csv"], fv)
    fs = F("sales.csv")
    for k in ["qty:n", "qty:nr", "qty", "price:n", "price:nr,id:nr", "region:i,product:ir",
              "region,qty:n", "note", "note:r", "date:r,id:n", "region:iN", "id", "id:N",
              "id:n", "3:i,1:nr"]:
        add(c, ["-S", k, "sales.csv"], fs)
    add(c, ["-S", "v:n", "nums.csv"], F("nums.csv"))
    add(c, ["-S", "v:nr", "nums.csv"], F("nums.csv"))
    add(c, ["-S", "v:N", "nums.csv"], F("nums.csv"))
    add(c, ["-S", "v", "nums.csv"], F("nums.csv"))
    add(c, ["-S", "name", "people.csv"], F("people.csv"))
    add(c, ["-S", "score:n", "people.csv"], F("people.csv"))
    add(c, ["-S", "age:nr", "people.csv"], F("people.csv"))
    add(c, ["-S", "region", "-S", "id:nr", "sales.csv"], fs)
    add(c, ["-S", "k:N"], stdin=b"k\nx9\nx09\nx009\nx10\nx9a\nx09b\nx\nx0\nx00\n")
    add(c, ["-S", "k:Nr"], stdin=b"k\nx9\nx09\nx009\nx10\nx9a\nx09b\nx\nx0\nx00\n")
    add(c, ["-S", "k:N"], stdin=b"k\n1.10\n1.9\n1.10.2\n1.2\n10\n9\n-3\n-20\n")
    add(c, ["-S", "k:n"], stdin=b"k\n1.10\n1.9\n1.2\n10\n9\n-3\n-20\n\n.5\n-.5\n")
    add(c, ["-S", "k:Ni"], stdin=b"k\nAb2\nab10\naB1\nAB1\n_x\n[y\n")
    add(c, ["-S", "k:i"], stdin=b"k\nb\nB\na\nA\n_\n[\n`\n{\n")
    add(c, ["-S", "k"], stdin="k\né\nz\nZ\n中\n\U0001F600\n\n".encode())
    add(c, ["-S", "k:N"], stdin="k\naé2\naé10\na1\n".encode())
    for k in ["zz", "0", "9", ":n", "region:x", "region:nN", "region:Nn", "qty:n,", ",qty",
              "region:nNz", "Region"]:
        add(c, ["-S", k, "sales.csv"], fs)
    add(c, ["-S", "k:nr"], stdin=b"k,i\n2,a\nx,b\n2,c\n,d\ny,e\n10,f\n2.0,g\n")
    add(c, ["-S", "k:n"], stdin=b"k,i\n2,a\nx,b\n2,c\n,d\ny,e\n10,f\n2.0,g\n")
    add(c, ["-S", "k:r"], stdin=b"k,i\nb,1\na,2\nb,3\na,4\n")


# ------------------------------------------------------------------ 8. group

def cat_group():
    c = "group"
    fs = F("sales.csv")
    for g, a in [
        ("region", None), ("region", "count(*)"), ("region", "sum(qty)"), ("region", "avg(qty)"),
        ("region", "sum(price),avg(price)"), ("region", "min(qty),max(qty)"),
        ("region", "min(price),max(price)"), ("region", "first(note),last(note)"),
        ("region", "count(note),distinct(product)"), ("product", "total=sum(price),n=count(*)"),
        ("region,product", "count(*)"), ("product,region", None), ("3", "sum(5)"),
        ("region", "min(product),max(date)"), ("note", "count(*)"), ("region", "count(qty)"),
        ("region", "sum(id),avg(id)"), ("region,region", "count(*)"), ("region", "avg(price)"),
        ("product", "min(region),max(region)"), ("region", "distinct(note)"),
    ]:
        add(c, ["-g", g] + (["-a", a] if a else []) + ["sales.csv"], fs)
    for a in ["count(*)", "sum(qty),avg(qty),min(qty),max(qty)", "sum(price),avg(price)",
              "first(id),last(id)", "distinct(region),count(note)", "x=count(*),y=sum(price)"]:
        add(c, ["-a", a, "sales.csv"], fs)
    add(c, ["-a", "count(*),sum(qty),min(qty),first(qty)", "-w", "id > 100", "sales.csv"], fs)
    add(c, ["-g", "region", "-w", "id > 100", "sales.csv"], fs)
    add(c, ["-g", "region", "-w", "id > 100", "-t", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "sum(price)", "-S", "sum_price:nr", "sales.csv"], fs)
    add(c, ["-g", "region", "-S", "count:nr,region", "sales.csv"], fs)
    add(c, ["-g", "region", "-S", "2:n", "sales.csv"], fs)
    add(c, ["-g", "region", "-S", "qty", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "sum(qty)", "-a", "avg(price)", "sales.csv"], fs)
    add(c, ["-g", "region", "-g", "product", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "s=sum(qty)", "-p", "2", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "avg(qty)", "-t", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "sum(qty)", "-u", "-N", "sales.csv"], fs)
    add(c, ["-g", "label", "-a", "sum(v),avg(v),min(v),max(v)", "nums.csv"], F("nums.csv"))
    add(c, ["-a", "sum(v),avg(v),min(v),max(v),count(v),distinct(v)", "nums.csv"], F("nums.csv"))
    add(c, ["-a", "avg(v)"], stdin=b"v\n1\n2\n2\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n1\n0\n0\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n0.0000005\n0\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n0.0000015\n0\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n-0.0000005\n0\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n-0.0000015\n0\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n-1\n-2\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n0.1\n0.2\n")
    add(c, ["-a", "sum(v)"], stdin=b"v\n0.1\n0.2\n")
    add(c, ["-a", "sum(v)"], stdin=b"v\n0.10\n0.2\n-0.30\n")
    add(c, ["-a", "sum(v)"], stdin=b"v\n1.\n2\n")
    add(c, ["-a", "sum(v),avg(v)"], stdin=b"v\n999999999999999\n999999999999999\n0.000000000000001\n")
    add(c, ["-a", "avg(v)"], stdin=b"v\n10\n20\n30\n40\n50\n60\n0.0000001\n")
    add(c, ["-a", "min(v),max(v)"], stdin=b"v\n10\n9\n-1\n\n")
    add(c, ["-a", "min(v),max(v)"], stdin=b"v\n10\n9\n-1\nx\n")
    add(c, ["-a", "min(v),max(v)"], stdin=b"v\n1.0\n1\n1.00\n")
    add(c, ["-a", "min(v),max(v)"], stdin=b"v\n\n\n")
    add(c, ["-a", "first(v),last(v),count(v),count(*)"], stdin=b"v\n\nx\n\n")
    add(c, ["-g", "k", "-a", "distinct(v)"], stdin=b"k,v\na,1\na,1.0\na,\nb,x\nb,x\n")
    add(c, ["-g", "k"], stdin=b"k,v\na,1\nA,2\na ,3\na,4\n")
    add(c, ["--trim", "-g", "k"], stdin=b"k,v\na,1\nA,2\na ,3\na,4\n")
    add(c, ["-g", "k"], stdin=b"k,v\n1,1\n1.0,2\n01,3\n1,4\n")
    add(c, ["-H", "-g", "c1", "-a", "sum(c2)"], stdin=b"x,1\ny,2\nx,3\n")
    add(c, ["-g", "k", "-a", "n=count(*),n=count(*)"], stdin=b"k\na\nb\na\n")
    add(c, ["-g", "k", "-a", "Sum=sum(k)"], stdin=b"k\n1\n2\n1\n")
    for a in ["median(qty)", "sum(qty", "sum qty", "sum()", "=sum(qty)", "s=(qty)", "sum(*)",
              "avg(*)", "count(zz)", "count(0)", "count(99)", "sum((qty))", "SUM(qty)",
              "sum(qty),", ",sum(qty)", "sum(qty)x", "sum(qty) ", " sum(qty)", "sum(q,ty)",
              "count(*),median(x),sum(zz)", "s=t=sum(qty)", "sum(qty))"]:
        add(c, ["-a", a, "sales.csv"], fs)
    for g in ["zz", "0", "", "region,", "1-3"]:
        add(c, ["-g", g, "sales.csv"], fs)
    add(c, ["-g", "zz", "-a", "median(x)", "sales.csv"], fs)
    add(c, ["-g", "", "-a", "median(x)", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "sum(zz)", "-S", "qq", "sales.csv"], fs)


# ------------------------------------------------------------------ 9. numbers

def cat_numfmt():
    c = "numfmt"
    fn = F("nums.csv")
    for p in ["0", "1", "2", "3", "9"]:
        add(c, ["-p", p, "nums.csv"], fn)
    for mode in ["half-even", "half-up", "down", "up"]:
        for p in ["0", "1", "2"]:
            add(c, ["-p", p, "-r", mode, "nums.csv"], fn)
    add(c, ["-T", "nums.csv"], fn)
    add(c, ["-T", "-p", "2", "nums.csv"], fn)
    add(c, ["-T", "-p", "0", "-r", "up", "nums.csv"], fn)
    add(c, ["-r", "up", "nums.csv"], fn)
    add(c, ["-T", "-t", "nums.csv"], fn)
    add(c, ["-p", "2", "-t", "nums.csv"], fn)
    add(c, ["-p", "1", "-e", "-", "nums.csv"], fn)
    add(c, ["-p", "1", "-e", "0", "nums.csv"], fn)
    add(c, ["-p", "1", "-q", "nonnumeric", "nums.csv"], fn)
    add(c, ["-T", "-q", "nonnumeric", "nums.csv"], fn)
    add(c, ["-T", "-q", "none", "nums.csv"], fn)
    add(c, ["-T", "-D", ";", "nums.csv"], fn)
    add(c, ["-p", "2", "-w", "v > 1", "nums.csv"], fn)
    add(c, ["-p", "2"], stdin=b"1,2.5\n3,4\n")
    add(c, ["-p", "2", "-H"], stdin=b"1,2.5\n3,4\n")
    add(c, ["-p", "0"], stdin=b"v\n0.5\n1.5\n2.5\n-0.5\n-1.5\n0.49\n-0.51\n")
    add(c, ["-p", "0", "-r", "half-up"], stdin=b"v\n0.5\n1.5\n2.5\n-0.5\n-1.5\n0.49\n-0.51\n")
    add(c, ["-p", "0", "-r", "down"], stdin=b"v\n0.5\n1.9\n-1.9\n-0.1\n")
    add(c, ["-p", "0", "-r", "up"], stdin=b"v\n0.5\n1.1\n-1.1\n-0.1\n0.0\n")
    add(c, ["-p", "1"], stdin=b"v\n0.25\n0.35\n0.250001\n-0.05\n-0.15\n0.05\n")
    add(c, ["-p", "3", "-T"], stdin=b"v\n1000\n-1000000.0005\n999999.9995\n100\n12345.6\n")
    add(c, ["-T"], stdin=b"v\n1000\n-1000000.0005\n999\n000001000\n+1234\n-0\n.5\n")
    add(c, ["-p", "2"], stdin=b"v\n1e5\n0x10\n1,000\n\xd9\xa3\n12-3\n--1\n+-1\n..5\n1..\n")
    add(c, ["-p", "2"], stdin=b"v\n-\n+\n.\n-.\n+.5\n-5.\n")
    add(c, ["-p", "4"], stdin=b"v\n123456789012345\n0.12345678901234\n1.23456789012345\n")
    add(c, ["-p", "9"], stdin=b"v\n999999.123456789012\n12345678.9\n")
    add(c, ["-p", "2", "-g", "k", "-a", "avg(v),sum(v),count(*)"], stdin=b"k,v\na,1\na,2\nb,0.125\nb,0.135\n")
    add(c, ["-T", "-a", "sum(v)"], stdin=b"v\n999999\n1\n")
    add(c, ["-p", "1", "-S", "v:n"], stdin=b"v\n0.04\n-0.04\n0.05\n-0.06\n")
    add(c, ["-p", "1", "-u"], stdin=b"v\n1.04\n1.0\n1.05\n")
    add(c, ["-p", "2", "-c"], stdin=b"1.5,2\n1,2\n")


# ------------------------------------------------------------------ 10. output quoting

def cat_quote():
    c = "quote"
    fm = F("messy.csv")
    for mode in ["minimal", "all", "nonnumeric", "none"]:
        add(c, ["-q", mode, "messy.csv"], fm)
        add(c, ["-q", mode, "sales.csv"], F("sales.csv"))
        add(c, ["-q", mode, "-s", "key", "messy.csv"], fm)
    add(c, ["-q", "none", "-s", "key", "messy.csv"], fm)
    add(c, ["-q", "none", "-x", "value,comment", "-N", "messy.csv"], fm)
    add(c, ["-q", "none", "-s", "comment", "-w", "key > c", "messy.csv"], fm)
    add(c, ["-s", "value"], stdin=b"value\n\n\"\"\nx\n")
    add(c, ["-s", "value"], stdin=b"value,o\n\"\",1\n")
    add(c, ["-s", "value", "-q", "none"], stdin=b"value,o\nx,1\n\"\",1\n")
    add(c, ["-s", "value", "-e", "NULL"], stdin=b"value,o\n\"\",1\n")
    add(c, ["-e", " "], stdin=b"a,b\n,1\n")
    add(c, ["-e", ","], stdin=b"a,b\n,1\n")
    add(c, ["-e", ""], stdin=b"a\n\"\"\n")
    add(c, ["-D", ";"], stdin=b"a,b\n\"x;y\",\"p,q\"\n")
    add(c, ["-D", "tab"], stdin=b"a,b\n\"x\ty\",\"p,q\"\n")
    add(c, ["-D", "1", "-q", "nonnumeric"], stdin=b"a,b\n1,21\nx,3\n")
    add(c, ["-D", ".", "-q", "nonnumeric"], stdin=b"a,b\n1.5,2\n")
    add(c, ["-D", "-"], stdin=b"a,b\n-1,2\n")
    add(c, ["-D", " "], stdin=b"a,b\nx y,2\n")
    add(c, [], stdin=b"a,b\n\"lead\",\" lead\"\n\"trail \",\"\ttab\"\n\"mid dle\",x\n")
    add(c, [], stdin=b"a,b\n\"cr\rhere\",x\n")
    add(c, ["-q", "nonnumeric"], stdin=b"a,b\n1,+2\n.5,5.\n0012,1e3\n,x\n")
    add(c, ["-q", "all", "-N"], stdin=b"a,b\n\"q\"\"\",\n")
    add(c, ["-q", "none"], stdin=b"a b,c\nx,y\n")
    add(c, ["-q", "none"], stdin=b"a,\"b,c\"\nx,y\n")
    add(c, ["-q", "none", "-N"], stdin=b"a,\"b,c\"\nx,y\n")
    add(c, ["-q", "none", "-C"], stdin=b"a,\"b,c\"\nx,y\n")
    add(c, ["-q", "minimal", "-q", "none"], stdin=b"a\n\"x\"\"\"\n")
    add(c, ["-q", "all", "-q", "minimal"], stdin=b"a\nx\n")
    add(c, [], stdin="a\né,\n".encode())


# ------------------------------------------------------------------ 11. pretty

def cat_pretty():
    c = "pretty"
    for f in ["sales.csv", "people.csv", "versions.csv", "messy.csv", "nums.csv", "data.tsv"]:
        add(c, ["-t", f], F(f))
    add(c, ["-t", "-N", "people.csv"], F("people.csv"))
    for m in ["2", "3", "4", "5", "6", "8"]:
        add(c, ["-t", "-m", m, "people.csv"], F("people.csv"))
    add(c, ["-t", "-m", "4", "messy.csv"], F("messy.csv"))
    add(c, ["-t", "-m", "3", "sales.csv"], F("sales.csv"))
    add(c, ["-t", "--max-width=7", "-s", "note", "sales.csv"], F("sales.csv"))
    add(c, ["-t"], stdin="w\n中文字\n中a\nab\n".encode())
    add(c, ["-t", "-m", "4"], stdin="w\n中文字\n中a\nab\nabcd\nabcde\n".encode())
    add(c, ["-t", "-m", "3"], stdin="w\néééé\nx́̂̃yz\n".encode())
    add(c, ["-t", "-m", "2"], stdin="w\n中x\nab\n\U0001F600\U0001F600\n".encode())
    add(c, ["-t"], stdin="w,n\nＡＢ,1\n가,22\nあい,333\n\U0001F680,4\n☃,5\n".encode())
    add(c, ["-t"], stdin="w,n\n​zw,1\n️vs,2\na⃝,3\n҃x,4\n".encode())
    add(c, ["-t"], stdin=b"w,n\n\xff\xfe,1\n\xc3,2\n\xe4\xb8,3\n\xed\xa0\x80,4\n\xf0\x9f\x98,5\n\xc0\xaf,6\n\xf4\x90\x80\x80,7\n")
    add(c, ["-t", "-m", "3"], stdin=b"w\n\xff\xfe\xfd\xfc\n")
    add(c, ["-t"], stdin=b"w,n\n\"a\tb\",1\n\"x\ny\",2\n\"c\rr\",3\n\"\x1b[1m\",4\n\x7f,5\n")
    add(c, ["-t", "-m", "3"], stdin=b"w\n\"\t\t\"\n\"a\tb\"\n")
    add(c, ["-t"], stdin=b"a,b,c\n,,\n,,\n")
    add(c, ["-t"], stdin=b"a,b\n1,\n,2\n-3,x\n")
    add(c, ["-t"], stdin=b"n\n1\n10\n100\n1e3\n")
    add(c, ["-t"], stdin=b"n,m\n1,.5\n10,5.\n100,+7\n")
    add(c, ["-t", "-e", "-"], stdin=b"n,m\n1,\n,2\n")
    add(c, ["-t", "-e", "NA"], stdin=b"n,m\n1,\n,2\n")
    add(c, ["-t", "-p", "1"], stdin=b"n,m\n1,x\n22.25,y\n")
    add(c, ["-t", "-T"], stdin=b"n\n1234567\n12\n")
    add(c, ["-t"], stdin=b"longheadername,b\n1,2\n")
    add(c, ["-t"], stdin=b"x,longheadername\nabc,2\n")
    add(c, ["-t"], stdin=b"a,b\nx  ,y  \n  z,w\n")
    add(c, ["-t", "-D", ";"], stdin=b"a,b\n1,2\n")
    add(c, ["-t", "-H"], stdin=b"a,bb\nccc,1\n")
    add(c, ["-t", "-n", "0"], stdin=b"a,bb\nccc,1\n")
    add(c, ["-t", "-N", "-n", "0"], stdin=b"a,bb\nccc,1\n")
    add(c, ["-t", "-H"], stdin=b"")
    add(c, ["-t", "-g", "region", "-a", "avg(price),sum(qty)", "sales.csv"], F("sales.csv"))
    add(c, ["-t", "-s", "name,score", "-S", "score:nr", "people.csv"], F("people.csv"))


# ------------------------------------------------------------------ 12. pipeline order

def cat_pipeline():
    c = "pipeline"
    fs = F("sales.csv")
    add(c, ["-s", "region", "-u", "sales.csv"], fs)
    add(c, ["-s", "region", "-u", "-n", "2", "sales.csv"], fs)
    add(c, ["-s", "region", "-u", "-k", "1", "-n", "2", "sales.csv"], fs)
    add(c, ["-s", "region", "-u", "-C", "sales.csv"], fs)
    add(c, ["-u", "sales.csv"], fs)
    add(c, ["-x", "id", "-u", "sales.csv"], fs)
    add(c, ["-x", "id", "-u", "-C", "sales.csv"], fs)
    add(c, ["-S", "price:n", "-s", "product", "-u", "sales.csv"], fs)
    add(c, ["-s", "product", "-S", "price:nr", "-n", "3", "sales.csv"], fs)
    add(c, ["-k", "100", "sales.csv"], fs)
    add(c, ["-k", "11", "sales.csv"], fs)
    add(c, ["-k", "3", "-n", "0", "sales.csv"], fs)
    add(c, ["-k", "3", "-n", "2", "-C", "sales.csv"], fs)
    add(c, ["-n", "2", "-k", "3", "sales.csv"], fs)
    add(c, ["-C", "sales.csv"], fs)
    add(c, ["-C", "-w", "qty > 3", "sales.csv"], fs)
    add(c, ["-C", "-g", "region", "sales.csv"], fs)
    add(c, ["-C", "-a", "sum(qty)", "-w", "id > 99", "sales.csv"], fs)
    add(c, ["-C", "-p", "2", "-e", "x", "sales.csv"], fs)
    add(c, ["-w", "qty > 3", "-S", "qty:nr", "-s", "id,qty", "-n", "3", "sales.csv"], fs)
    add(c, ["-g", "region", "-a", "sum(qty)", "-S", "sum_qty:n", "-k", "1", "-n", "2", "sales.csv"], fs)
    add(c, ["-g", "product", "-u", "sales.csv"], fs)
    add(c, ["-s", "region", "-u", "-e", "X", "-p", "1", "sales.csv"], fs)
    add(c, ["-s", "qty", "-p", "0", "-u", "sales.csv"], fs)
    add(c, ["-S", "qty:n", "-x", "qty", "sales.csv"], fs)
    add(c, ["-w", "id <= 3", "-x", "region", "-S", "region", "sales.csv"], fs)
    add(c, ["-s", "region,region", "-u", "sales.csv"], fs)
    add(c, ["-s", "region", "-u", "--trim"], stdin=b"region\nnorth\nnorth \n north\nnorth\n")
    add(c, ["-s", "region", "-u"], stdin=b"region\nnorth\nnorth \n north\nnorth\n")


# ------------------------------------------------------------------ randomized mixes

POOL = ["1", "2", "10", "-1", "0", "3.5", "3.50", "+2", ".5", "x", "X", "abc", "", "a b",
        "été", "中", "1e3", "007", "a,b", "q\"t", "z9", "z10", "Z2", "-0.0",
        "123456789012345", "1234567890123456", "99.995", " pad"]


def cell():
    return R.choice(POOL)


def csv_field(v):
    if any(ch in v for ch in ',"\n') or v.startswith(" "):
        return '"' + v.replace('"', '""') + '"'
    return v


def rand_table(ncols, nrows):
    names = ["k", "v", "w", "t", "u"][:ncols]
    lines = [",".join(names)]
    for _ in range(nrows):
        lines.append(",".join(csv_field(cell()) for _ in range(ncols)))
    return ("\n".join(lines) + "\n").encode()


def rand_mix(cat, n):
    for _ in range(n):
        ncols = R.randint(2, 4)
        tbl = rand_table(ncols, R.randint(3, 12))
        cols = ["k", "v", "w", "t"][:ncols]
        args = []
        if R.random() < 0.5:
            col = R.choice(cols)
            op = R.choice(["=", "!=", "<", "<=", ">", ">=", "~", "^=", "$="])
            lit = R.choice(["1", "'10'", "x", "'X'", "2.0", "-1", "'z'", "''", "3.5"])
            args += ["-w", "%s %s %s" % (col, op, lit)]
        if R.random() < 0.6:
            col = R.choice(cols)
            fl = R.choice(["", ":n", ":nr", ":N", ":r", ":i", ":Ni", ":Nr"])
            args += ["-S", col + fl]
        if R.random() < 0.35:
            g = R.choice(cols)
            args += ["-g", g, "-a",
                     R.choice(["sum(v),avg(v)", "min(v),max(v)", "count(v),distinct(v)",
                               "avg(v),count(*)", "first(v),last(v),sum(v)"])]
        elif R.random() < 0.5:
            args += ["-s", ",".join(R.sample(cols, R.randint(1, ncols)))]
        if R.random() < 0.3:
            args += ["-p", str(R.randint(0, 3))]
            if R.random() < 0.5:
                args += ["-r", R.choice(["half-even", "half-up", "down", "up"])]
        if R.random() < 0.2:
            args += ["-T"]
        if R.random() < 0.3:
            args += ["-t"]
        elif R.random() < 0.3:
            args += ["-q", R.choice(["all", "nonnumeric", "minimal"])]
        if R.random() < 0.2:
            args += ["-u"]
        if R.random() < 0.15:
            args += ["-e", "NA"]
        add(cat, args, stdin=tbl)


def main():
    cat_cli()
    cat_parse()
    cat_header()
    cat_select()
    cat_where()
    cat_where_err()
    cat_sort()
    cat_group()
    cat_numfmt()
    cat_quote()
    cat_pretty()
    cat_pipeline()
    rand_mix("mixed", 60)
    seen = {}
    for i, cs in enumerate(CASES):
        seen[cs["cat"]] = seen.get(cs["cat"], 0) + 1
        cs["id"] = "%s-%03d" % (cs["cat"], seen[cs["cat"]])
    json.dump(CASES, sys.stdout, indent=0)
    sys.stderr.write("generated %d cases: %s\n" % (len(CASES), seen))


if __name__ == "__main__":
    main()
