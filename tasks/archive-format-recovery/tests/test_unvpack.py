# Hidden grader for /app/unvpack.  Copyright (c) 2026 the task author. MIT License.
import hashlib
import json
import os
import shutil
import stat
import subprocess
import tempfile

import pytest

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
UNVPACK = "/app/unvpack"
TIMEOUT = 60

with open(os.path.join(DATA, "manifests.json")) as f:
    META = json.load(f)
VALID = sorted(META["valid"])
CORRUPT = sorted(META["corrupt"])


def run(archive, outdir):
    # Copy the archive so the program cannot learn anything from its location.
    work = tempfile.mkdtemp(prefix="g")
    src = os.path.join(work, "in.vp")
    shutil.copyfile(archive, src)
    try:
        return subprocess.run([UNVPACK, src, outdir], capture_output=True, timeout=TIMEOUT,
                              cwd=work, stdin=subprocess.DEVNULL)
    finally:
        os.unlink(src)


def snapshot(root):
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
            elif stat.S_ISREG(s.st_mode):
                e["type"] = "f"
                e["mode"] = stat.S_IMODE(s.st_mode)
                with open(de.path, "rb") as fh:
                    data = fh.read()
                e["size"] = len(data)
                e["sha256"] = hashlib.sha256(data).hexdigest()
            else:
                e["type"] = "other"
            out["entries"][r.hex()] = e

    rec(root, b"")
    return out


def describe(path_hex):
    return repr(bytes.fromhex(path_hex))


def test_unvpack_is_executable():
    assert os.path.isfile(UNVPACK), "/app/unvpack is missing"
    assert os.access(UNVPACK, os.X_OK), "/app/unvpack is not executable"


@pytest.mark.parametrize("name", VALID)
def test_valid_archive(name, tmp_path):
    out = str(tmp_path / "out")
    res = run(os.path.join(DATA, name + ".vp"), out)
    assert res.returncode == 0, "exit %d: %s" % (res.returncode, res.stderr[-400:])
    want = META["valid"][name]
    got = snapshot(out)
    missing = sorted(set(want["entries"]) - set(got["entries"]))
    extra = sorted(set(got["entries"]) - set(want["entries"]))
    assert not missing, "missing entries: %s" % [describe(m) for m in missing[:5]]
    assert not extra, "unexpected entries: %s" % [describe(m) for m in extra[:5]]
    for key, w in want["entries"].items():
        g = got["entries"][key]
        for field in ("type", "target", "size", "sha256", "mode", "mtime_ns"):
            assert g.get(field) == w.get(field), "%s %s: got %r want %r" % (
                describe(key), field, g.get(field), w.get(field))
    assert got["root"] == want["root"], "OUTDIR attributes: got %r want %r" % (got["root"], want["root"])


@pytest.mark.parametrize("name", CORRUPT)
def test_corrupt_archive(name, tmp_path):
    out = str(tmp_path / "out")
    res = run(os.path.join(DATA, name + ".vp"), out)
    assert res.returncode == 1, "exit %d, expected 1 (%s)" % (res.returncode, META["corrupt"][name]["kind"])
    assert b"corrupt" in res.stderr.lower(), "stderr lacks 'corrupt': %r" % res.stderr[-300:]
