"""Grader for the tabula clean-room rebuild. MIT License.

Every case in cases.json was recorded from the original binary at authoring
time. The rebuild must reproduce stdout, stderr and the exit code exactly for
all of them. Before any case runs, the original binary (and any byte copy of
it found on the filesystem) is removed, and the rebuild runs under an audit
hook that blocks process creation and native code.
"""
import base64
import hashlib
import json
import os
import subprocess
import tempfile

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
REBUILD_DIR = "/app/rebuild"
REBUILD = "/app/rebuild/tabula"
ORIGINAL = "/app/bin/tabula"
SANDBOX = os.path.join(HERE, "sandbox_run.py")
# identifying string compiled into the original binary (split so this file
# does not itself match the scan)
MARKER = ("TABULA-" + "IDENT-3f9c2a7e51d04b88").encode()
SKIP_ROOTS = ("/proc", "/sys", "/dev", "/run", "/logs", HERE)
CASE_TIMEOUT = 10

with open(os.path.join(HERE, "cases.json")) as fh:
    CASES = json.load(fh)


def _b(s):
    return base64.b64decode(s)


def _file_has_marker(path, original_hash):
    try:
        if os.path.getsize(path) > 64 * 1024 * 1024:
            return False
        with open(path, "rb") as fh:
            data = fh.read()
    except OSError:
        return False
    if MARKER in data:
        return True
    return original_hash is not None and hashlib.sha256(data).hexdigest() == original_hash


def _walk_files(root):
    for dirpath, dirnames, filenames in os.walk(root, followlinks=False):
        if any(dirpath == s or dirpath.startswith(s + "/") for s in SKIP_ROOTS):
            dirnames[:] = []
            continue
        for name in filenames:
            p = os.path.join(dirpath, name)
            if os.path.islink(p) or not os.path.isfile(p):
                continue
            yield p


@pytest.fixture(scope="session")
def original_hash():
    try:
        with open(ORIGINAL, "rb") as fh:
            return hashlib.sha256(fh.read()).hexdigest()
    except OSError:
        return None


def test_rebuild_present():
    assert os.path.isfile(REBUILD), "missing /app/rebuild/tabula"


def test_rebuild_has_no_native_code_or_binary_copy(original_hash):
    offenders = []
    for p in _walk_files(REBUILD_DIR):
        base = os.path.basename(p)
        with open(p, "rb") as fh:
            head = fh.read(4)
        if head == b"\x7fELF" or base.endswith(".so") or ".so." in base:
            offenders.append(p + " (native code)")
        elif _file_has_marker(p, original_hash):
            offenders.append(p + " (copy of the original binary)")
    assert not offenders, "forbidden files in /app/rebuild: %s" % offenders


@pytest.fixture(scope="session")
def original_removed(original_hash):
    """Delete the original binary and every byte copy of it anywhere."""
    removed = []
    for p in _walk_files("/"):
        if p == ORIGINAL or _file_has_marker(p, original_hash):
            try:
                os.remove(p)
                removed.append(p)
            except OSError:
                pass
    print("\nremoved original binary copies: %s" % removed)
    return removed


def _run_case(cs):
    with tempfile.TemporaryDirectory() as td:
        for name, data in cs["files"].items():
            p = os.path.join(td, name)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "wb") as fh:
                fh.write(_b(data))
        args = [os.fsencode(a) for a in cs["args"]]
        env = {"PATH": "/usr/local/bin:/usr/bin:/bin", "LC_ALL": "C.UTF-8"}
        cmd = [b"python3", b"-I", b"-S", os.fsencode(SANDBOX), REBUILD.encode()] + args
        try:
            p = subprocess.run(cmd, input=_b(cs["stdin"]), cwd=td, env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               timeout=CASE_TIMEOUT)
        except subprocess.TimeoutExpired:
            return None
        return p.stdout, p.stderr, p.returncode


def test_all_cases_match(original_removed):
    assert not os.path.exists(ORIGINAL)
    per_cat = {}
    failures = []
    for cs in CASES:
        got = _run_case(cs)
        exp = (_b(cs["stdout"]), _b(cs["stderr"]), cs["code"])
        ok = got == exp
        tally = per_cat.setdefault(cs["cat"], [0, 0])
        tally[1] += 1
        if ok:
            tally[0] += 1
        else:
            failures.append(cs["id"])
    lines = ["", "%-10s %8s %8s" % ("category", "passed", "total")]
    for cat, (ok, tot) in per_cat.items():
        lines.append("%-10s %8d %8d" % (cat, ok, tot))
    passed = sum(v[0] for v in per_cat.values())
    lines.append("%-10s %8d %8d" % ("TOTAL", passed, len(CASES)))
    print("\n".join(lines))
    with open("/logs/verifier/categories.txt", "w") as fh:
        fh.write("\n".join(lines) + "\n")
    assert not failures, "%d of %d cases differ (first few: %s)" % (
        len(failures), len(CASES), failures[:10])
