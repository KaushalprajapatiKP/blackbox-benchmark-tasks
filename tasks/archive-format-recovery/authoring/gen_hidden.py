# Authoring-time generator for the hidden test archives.  Not shipped to the agent.
# Copyright (c) 2026 the task author. MIT License.
#
# Run inside the built environment image (it needs /app/bin/vpack and root):
#   docker run --rm -v $TASK:/task vpack-env python3 /task/authoring/gen_hidden.py /task/tests/data
# Every archive is round-tripped through the reference decoder before it is kept,
# and every corruption case must be rejected by both the strict and lenient decoder.
import hashlib
import importlib.util
import json
import os
import random
import shutil
import stat
import subprocess
import sys
import zlib

SEED = 20261117
WORK = "/tmp/vgen"
BASE_T = 1767225600

spec = importlib.util.spec_from_file_location("ref", "/task/solution/unvpack.py")
ref = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ref)

WORDS = ("alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi omicron pi rho "
         "sigma tau upsilon phi chi psi omega river stone cloud ember quartz meadow lantern harbor "
         "copper violet signal anchor thunder willow cedar falcon prism orbit vector kernel buffer "
         "socket packet thread mutex cursor cache index ledger").split()


# ---------------------------------------------------------------- content
def prose(rng, n):
    return (" ".join(rng.choice(WORDS) for _ in range(n)) + "\n").encode()


def rand_bytes(rng, n):
    return rng.randbytes(n)


def runs(rng, n):
    out = bytearray()
    while len(out) < n:
        kind = rng.randrange(4)
        if kind == 0:
            out += bytes([rng.randrange(256)]) * rng.randint(3, 3000)
        elif kind == 1:
            pat = rng.randbytes(rng.randint(2, 20))
            out += (pat * 4000)[: rng.randint(20, 6000)]
        elif kind == 2:
            out += prose(rng, rng.randint(5, 80))
        else:
            out += rng.randbytes(rng.randint(1, 40))
    return bytes(out[:n])


def long_distance(rng):
    chunk = prose(rng, rng.randint(150, 600)) + rng.randbytes(rng.randint(50, 400))
    out = bytearray(chunk)
    for _ in range(rng.randint(2, 4)):
        out += rng.randbytes(rng.randint(4200, 15000))
        out += chunk[: rng.randint(len(chunk) // 2, len(chunk))]
    return bytes(out)


def multiblock(rng):
    out = bytearray()
    target = rng.randint(40_000, 260_000)
    while len(out) < target:
        k = rng.randrange(3)
        if k == 0:
            out += prose(rng, rng.randint(500, 4000))
        elif k == 1:
            out += rng.randbytes(rng.randint(5000, 40000))
        else:
            out += runs(rng, rng.randint(2000, 30000))
    return bytes(out)


def any_content(rng):
    k = rng.randrange(10)
    if k < 2:
        return b""
    if k < 5:
        return prose(rng, rng.randint(1, 600))
    if k < 6:
        return rand_bytes(rng, rng.randint(1, 3000))
    if k < 8:
        return runs(rng, rng.randint(10, 9000))
    if k < 9:
        return long_distance(rng)
    return multiblock(rng) if rng.random() < 0.3 else prose(rng, rng.randint(100, 3000))


# ---------------------------------------------------------------- attributes
def t_recent(rng, _kind):
    return (BASE_T + rng.randint(0, 30_000_000)) * 10**9


def t_ns(rng, _kind):
    return (BASE_T + rng.randint(-50_000_000, 50_000_000)) * 10**9 + rng.randrange(10**9)


def t_wide(rng, _kind):
    sec = rng.choice([
        rng.randint(-2**31, -1),
        rng.randint(0, 10**9),
        rng.randint(2**31, 15_000_000_000),
        -1, 0, 1, 2**31 - 1, 2**31,
    ])
    ns = rng.choice([0, rng.randrange(10**9), 999_999_999, 1])
    return sec * 10**9 + ns


def t_mixed(rng, kind):
    return rng.choice([t_recent, t_ns, t_wide])(rng, kind)


def m_default(_rng, kind):
    return 0o755 if kind == "d" else 0o644


def m_common(rng, kind):
    if kind == "d":
        return rng.choice([0o755, 0o700, 0o750, 0o775, 0o1777, 0o2755])
    return rng.choice([0o644, 0o600, 0o755, 0o640, 0o444, 0o700, 0o664])


def m_any(rng, _kind):
    return rng.randrange(0o10000)


# ---------------------------------------------------------------- tree building
class Tree:
    def __init__(self, root, rng, timefn=t_recent, modefn=m_default):
        self.root = root
        self.rng = rng
        self.timefn = timefn
        self.modefn = modefn
        self.dirs = [b""]
        os.makedirs(root)

    def p(self, rel):
        return os.path.join(os.fsencode(self.root), rel) if rel else os.fsencode(self.root)

    def mkdir(self, rel):
        os.mkdir(self.p(rel))
        self.dirs.append(rel)
        return rel

    def file(self, rel, data):
        with open(self.p(rel), "wb") as f:
            f.write(data)

    def link(self, rel, target):
        os.symlink(target, self.p(rel))

    def finalize(self):
        rng = self.rng
        root = os.fsencode(self.root)
        for dirpath, dirnames, filenames in os.walk(root, topdown=False):
            for n in filenames + dirnames:
                p = os.path.join(dirpath, n)
                st = os.lstat(p)
                if stat.S_ISLNK(st.st_mode):
                    kind = "l"
                elif stat.S_ISDIR(st.st_mode):
                    kind = "d"
                else:
                    kind = "f"
                if kind != "l":
                    os.chmod(p, self.modefn(rng, kind))
                t = self.timefn(rng, kind)
                os.utime(p, ns=(t, t), follow_symlinks=False)
        os.chmod(root, self.modefn(rng, "d"))
        t = self.timefn(rng, "d")
        os.utime(root, ns=(t, t))


def name(rng, pool=None):
    if pool and rng.random() < 0.7:
        return rng.choice(pool)
    return ("%s_%d" % (rng.choice(WORDS), rng.randrange(1000))).encode()


def random_tree(t, n_entries, depth_max=5, pool=None, content=any_content, link_p=0.08,
                dir_p=0.18, empty_dir_p=0.3):
    rng = t.rng
    for _ in range(n_entries):
        parent = rng.choice(t.dirs)
        if parent.count(b"/") + 1 >= depth_max:
            parent = b""
        nm = name(rng, pool)
        rel = os.path.join(parent, nm) if parent else nm
        if os.path.lexists(t.p(rel)):
            continue
        r = rng.random()
        if r < dir_p:
            t.mkdir(rel)
        elif r < dir_p + link_p:
            t.link(rel, rng.choice([b"../" + name(rng), name(rng), b"/etc/" + name(rng),
                                    b"./a/b/../c", b"x" * rng.randint(1, 300)]))
        else:
            t.file(rel, content(rng))


# ---------------------------------------------------------------- categories
def c_basic(t):
    random_tree(t, t.rng.randint(5, 15), depth_max=3, content=lambda r: prose(r, r.randint(5, 300)),
                link_p=0.05)


def c_empty(t):
    rng = t.rng
    v = t.variant
    if v == 0:
        return
    if v == 1:
        cur = b""
        for i in range(rng.randint(3, 8)):
            cur = t.mkdir(os.path.join(cur, b"e%d" % i) if cur else b"e%d" % i)
        return
    for i in range(rng.randint(5, 20)):
        parent = rng.choice(t.dirs)
        rel = os.path.join(parent, b"z%d" % i) if parent else b"z%d" % i
        if rng.random() < 0.4:
            t.mkdir(rel)
        else:
            t.file(rel, b"" if rng.random() < 0.8 else b"\0")


def c_runs(t):
    rng = t.rng
    t.file(b"aaaa", b"a" * rng.randint(1000, 16384))
    t.file(b"period", runs(rng, rng.randint(5000, 20000)))
    t.file(b"exact130", b"Q" * (130 + rng.randint(0, 3)) + b"tail" + b"Z" * 3 + b"Z" * 127)
    t.file(b"blockfill", (b"xyz" * 20000)[: 16384 * rng.randint(1, 3) + rng.randint(-2, 2)])
    for i in range(rng.randint(1, 4)):
        t.file(b"r%d" % i, runs(rng, rng.randint(50, 40000)))


def c_longdist(t):
    rng = t.rng
    for i in range(rng.randint(1, 3)):
        t.file(b"ld%d.bin" % i, long_distance(rng))
    base = rng.randbytes(3000)
    t.file(b"near_window", base + rng.randbytes(65536 - 3000 - rng.randint(0, 50)) + base)


def c_multiblock(t):
    rng = t.rng
    for i in range(rng.randint(1, 3)):
        t.file(b"big%d" % i, multiblock(rng))
    t.file(b"small.txt", prose(rng, 50))


def c_incompressible(t):
    rng = t.rng
    for i in range(rng.randint(2, 6)):
        t.file(b"rnd%d" % i, rand_bytes(rng, rng.choice([1, 2, 3, 15, 4096, 16384, 16385, 40000])))


def c_namededup(t):
    rng = t.rng
    pool = [b"index.txt", b"README", b"data.bin", b"config", b"src", b"x", b"main.c", b"a"]
    for i in range(rng.randint(3, 12)):
        d = t.mkdir(b"dir%02d" % i)
        for nm in rng.sample(pool, rng.randint(2, len(pool))):
            rel = os.path.join(d, nm)
            if nm in (b"src", b"config") and rng.random() < 0.5:
                t.mkdir(rel)
                t.file(os.path.join(rel, b"README"), prose(rng, rng.randint(0, 30)))
            elif rng.random() < 0.15:
                t.link(rel, b"../dir00/" + nm)
            else:
                t.file(rel, prose(rng, rng.randint(0, 80)))


def c_bigtable(t):
    rng = t.rng
    pool = [("%s.%s" % (rng.choice(WORDS), rng.choice(["c", "h", "txt", "py"]))).encode()
            for _ in range(30)]
    random_tree(t, rng.randint(250, 700), depth_max=6, pool=pool if t.variant % 2 else None,
                content=lambda r: prose(r, r.randint(0, 20)) if r.random() < 0.8 else b"",
                link_p=0.05, dir_p=0.2)


def c_deep(t):
    rng = t.rng
    cur = b""
    total = 0
    depth = rng.randint(30, 60)
    for i in range(depth):
        ln = rng.randint(1, 120)
        if total + ln + 1 > 3300:
            break
        nm = (b"d%d_" % i + b"n" * ln)[:255]
        total += len(nm) + 1
        cur = t.mkdir(os.path.join(cur, nm) if cur else nm)
        if rng.random() < 0.3:
            t.file(os.path.join(cur, b"f"), prose(rng, rng.randint(0, 40)))
    t.file(os.path.join(cur, b"L" * 200), b"deep leaf\n")
    t.file(b"N" * 255, b"long name\n")


def c_modes(t):
    random_tree(t, t.rng.randint(15, 40), depth_max=4, link_p=0.05)


def c_times(t):
    random_tree(t, t.rng.randint(10, 30), depth_max=4, link_p=0.15)


def c_symlinks(t):
    rng = t.rng
    d = t.mkdir(b"target_dir")
    t.file(os.path.join(d, b"inside"), prose(rng, 20))
    targets = [b"target_dir", b"target_dir/inside", b"/absolute/path/elsewhere", b"dangling",
               b"../../outside", b"..", b".", b"t" * 1000, b"caf\xe9/\xff", b"a b c",
               b"/" * 3 + b"slashes//", b"with\nnewline"]
    for i, tg in enumerate(targets):
        if rng.random() < 0.8:
            t.link(b"l%02d" % i, tg)
    sub = t.mkdir(b"sub")
    t.link(os.path.join(sub, b"up"), b"../target_dir")


def c_shared(t):
    rng = t.rng
    blobs = [prose(rng, rng.randint(10, 400)), rand_bytes(rng, rng.randint(10, 3000)),
             runs(rng, rng.randint(100, 30000)), b"same\n"]
    for i in range(rng.randint(2, 5)):
        t.mkdir(b"g%d" % i)
    for i in range(rng.randint(8, 20)):
        parent = rng.choice(t.dirs)
        rel = os.path.join(parent, b"copy%d" % i) if parent else b"copy%d" % i
        t.file(rel, rng.choice(blobs) if rng.random() < 0.8 else any_content(rng))


def c_names(t):
    rng = t.rng
    names = [b"caf\xc3\xa9", b"na\xc3\xafve file", b"\xe2\x9c\x93 check", b"bad\xff\xfeutf8",
             b"\xe9t\xe9", b"line\nbreak", b"tab\there", b" leading space", b"trailing space ",
             b"-dash", b"#hash", b"*star?", b"\x01ctl", b"UPPER", b"upper", b"z" * 255]
    for nm in names:
        if rng.random() < 0.85:
            if rng.random() < 0.25:
                t.mkdir(nm)
                t.file(os.path.join(nm, nm), prose(rng, 5))
            else:
                t.file(nm, prose(rng, rng.randint(0, 40)))


def c_kitchen(t):
    rng = t.rng
    pool = [name(rng) for _ in range(12)]
    random_tree(t, rng.randint(30, 120), depth_max=7, pool=pool, link_p=0.1)
    if rng.random() < 0.6:
        t.file(b"dup_a", b"duplicate payload\n" * 50)
        t.file(b"dup_b", b"duplicate payload\n" * 50)
    if rng.random() < 0.5:
        t.mkdir(b"empty_here")


CATEGORIES = [
    ("basic", c_basic, 6, t_recent, m_default),
    ("empty", c_empty, 4, t_mixed, m_common),
    ("runs", c_runs, 5, t_recent, m_default),
    ("longdist", c_longdist, 4, t_ns, m_default),
    ("multiblock", c_multiblock, 5, t_recent, m_common),
    ("incompressible", c_incompressible, 3, t_recent, m_default),
    ("namededup", c_namededup, 5, t_ns, m_common),
    ("bigtable", c_bigtable, 4, t_mixed, m_common),
    ("deep", c_deep, 3, t_ns, m_common),
    ("modes", c_modes, 4, t_recent, m_any),
    ("times", c_times, 6, t_wide, m_default),
    ("symlinks", c_symlinks, 4, t_mixed, m_common),
    ("shared", c_shared, 4, t_ns, m_any),
    ("names", c_names, 3, t_ns, m_common),
    ("kitchen", c_kitchen, 8, t_mixed, m_any),
]


# ---------------------------------------------------------------- manifest
def manifest(root):
    root = os.fsencode(root)
    st = os.lstat(root)
    out = {"root": {"mode": stat.S_IMODE(st.st_mode), "mtime_ns": st.st_mtime_ns}, "entries": {}}

    def rec(dirp, rel):
        for de in sorted(os.scandir(dirp), key=lambda d: d.name):
            r = de.name if not rel else rel + b"/" + de.name
            s = os.lstat(de.path)
            e = {"mtime_ns": s.st_mtime_ns}
            if stat.S_ISLNK(s.st_mode):
                e["type"] = "l"
                e["target"] = os.readlink(de.path).hex()
            elif stat.S_ISDIR(s.st_mode):
                e["type"] = "d"
                e["mode"] = stat.S_IMODE(s.st_mode)
                rec(de.path, r)
            else:
                e["type"] = "f"
                e["mode"] = stat.S_IMODE(s.st_mode)
                with open(de.path, "rb") as f:
                    data = f.read()
                e["size"] = len(data)
                e["sha256"] = hashlib.sha256(data).hexdigest()
            out["entries"][r.hex()] = e

    rec(root, b"")
    return out


def decode_ok(path, outdir, lenient=False):
    ref.STRICT = not lenient
    with open(path, "rb") as f:
        data = f.read()
    try:
        r, entries = ref.parse(data)
    except ref.Corrupt:
        return False
    if outdir:
        ref.extract(r, entries, os.fsencode(outdir))
    return True


def features(path):
    with open(path, "rb") as f:
        data = f.read()
    ref.STRICT = True
    r, entries = ref.parse(data)
    feats = set()
    flags = data[5]
    if flags & 1:
        feats.add("nametable")
    if flags & 2:
        feats.add("tablz")
    for e in entries:
        if e["type"] == 1:
            if e["size"] == 0:
                feats.add("emptyfile")
            elif e["shared"] is not None:
                feats.add("shared")
            elif e.get("lz"):
                feats.add("lz")
                if e["size"] > 16384:
                    feats.add("multiblock")
            else:
                feats.add("stored")
        if e["mtime"] % 10**9:
            feats.add("ns")
        if e["mtime"] < 0:
            feats.add("negtime")
        if e["type"] == 3:
            feats.add("symlink")
    return feats


# ---------------------------------------------------------------- corruption
def corrupt_variants(rng, data, info):
    hdr_end, table_off, blobs = info
    n = len(data)
    out = []

    def flip(pos, mask=None):
        b = bytearray(data)
        b[pos] ^= mask or (1 << rng.randrange(8))
        return bytes(b)

    out.append(("trunc_zero", b""))
    out.append(("trunc_3", data[:3]))
    out.append(("trunc_last", data[:-1]))
    out.append(("trunc_footer", data[:-20]))
    out.append(("trunc_mid", data[: rng.randrange(6, n - 1)]))
    out.append(("trunc_data", data[: rng.randrange(hdr_end, table_off + 1)]))
    out.append(("append_1", data + b"\0"))
    out.append(("append_junk", data + rng.randbytes(64)))
    out.append(("flip_magic", flip(rng.randrange(4))))
    out.append(("flip_header", flip(rng.randrange(4, hdr_end))))
    out.append(("flip_table", flip(rng.randrange(table_off, n - 20))))
    out.append(("flip_footer_off", flip(n - 20 + rng.randrange(4))))
    out.append(("flip_footer_len", flip(n - 12 + rng.randrange(2))))
    out.append(("flip_footer_crc", flip(n - 8 + rng.randrange(4))))
    out.append(("flip_trailer", flip(n - 4 + rng.randrange(4))))
    for kind in ("stored", "lz"):
        cands = [b for b in blobs if b[2] == kind]
        if cands:
            s, e, _ = rng.choice(cands)
            out.append(("flip_%s" % kind, flip(rng.randrange(s, e))))
    if blobs:
        s, e, _ = rng.choice(blobs)
        b = bytearray(data)
        b[s:e] = rng.randbytes(e - s)
        out.append(("scramble_blob", bytes(b)))
    return out


def layout(data):
    ref.STRICT = True
    r, entries = ref.parse(data)
    table_off = int.from_bytes(data[-20:-12], "little")
    hr = ref.Reader(data, 6, table_off)
    hr.varint()
    hr.varint()
    rt = hr.varint()
    if rt & 1:
        hr.varint()
    hdr_end = hr.pos
    blobs = []
    pos = hdr_end
    for e in entries:
        if e["type"] == 1 and e["size"] and e["shared"] is None:
            blobs.append((pos, pos + e["clen"], "lz" if e["lz"] else "stored"))
            pos += e["clen"]
    return hdr_end, table_off, blobs


def main(dest):
    rng = random.Random(SEED)
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    if os.path.isdir(dest):
        shutil.rmtree(dest)
    os.makedirs(dest)
    manifests = {}
    corrupt_meta = {}
    all_feats = set()
    archives = []
    for cname, fn, count, timefn, modefn in CATEGORIES:
        for v in range(count):
            arch_name = "%s_%02d" % (cname, v)
            root = os.path.join(WORK, arch_name)
            t = Tree(root, random.Random(rng.getrandbits(64)), timefn, modefn)
            t.variant = v
            fn(t)
            t.finalize()
            man = manifest(root)
            ap = os.path.join(dest, arch_name + ".vp")
            subprocess.run(["/app/bin/vpack", "pack", root, ap], check=True)
            if manifest(root) != man:
                raise SystemExit("tree changed while packing " + arch_name)
            for lenient in (False, True):
                outd = os.path.join(WORK, "out_" + arch_name + str(lenient))
                if not decode_ok(ap, outd, lenient):
                    raise SystemExit("reference rejected " + arch_name)
                if manifest(outd) != man:
                    raise SystemExit("round trip mismatch " + arch_name)
                shutil.rmtree(outd)
            f = features(ap)
            all_feats |= f
            manifests[arch_name] = man
            archives.append(arch_name)
            print(arch_name, os.path.getsize(ap), len(man["entries"]), sorted(f))
            shutil.rmtree(root)

    crng = random.Random(SEED + 1)
    picks = crng.sample(archives, 12)
    ncase = 0
    for arch_name in picks:
        with open(os.path.join(dest, arch_name + ".vp"), "rb") as f:
            data = f.read()
        variants = corrupt_variants(crng, data, layout(data))
        crng.shuffle(variants)
        kept = 0
        for label, blob in variants:
            if kept >= 4:
                break
            if blob == data:
                continue
            p = os.path.join(dest, "corrupt_%02d.vp" % ncase)
            with open(p, "wb") as f:
                f.write(blob)
            if decode_ok(p, None) or decode_ok(p, None, lenient=True):
                os.unlink(p)
                print("skip undetectable", arch_name, label)
                continue
            corrupt_meta["corrupt_%02d" % ncase] = {"from": arch_name, "kind": label}
            ncase += 1
            kept += 1
    with open(os.path.join(dest, "manifests.json"), "w") as f:
        json.dump({"valid": manifests, "corrupt": corrupt_meta}, f, sort_keys=True)
    print("valid", len(manifests), "corrupt", ncase, "features", sorted(all_feats))
    total = sum(os.path.getsize(os.path.join(dest, x)) for x in os.listdir(dest))
    print("total bytes", total)


if __name__ == "__main__":
    main(sys.argv[1])
