"""Removes the original qsrv binary and every byte-identical copy of it."""
import hashlib
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SKIP = ("/proc", "/sys", "/dev", "/run", "/tests", "/logs")


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def main():
    known = set()
    sizes = set()
    with open(os.path.join(HERE, "qsrv.sha256")) as f:
        for line in f:
            if line.strip():
                h, n = line.split()
                known.add(h)
                sizes.add(int(n))
    orig = "/app/bin/qsrv"
    if os.path.isfile(orig):
        known.add(sha(orig))
        sizes.add(os.path.getsize(orig))
        os.remove(orig)
    removed = 0
    for root, dirs, files in os.walk("/", topdown=True):
        if root.startswith(SKIP) and root != "/":
            dirs[:] = []
            continue
        dirs[:] = [d for d in dirs if os.path.join(root, d) not in SKIP]
        for fn in files:
            p = os.path.join(root, fn)
            try:
                if os.path.islink(p) or not os.path.isfile(p) or os.path.getsize(p) not in sizes:
                    continue
                if sha(p) in known:
                    os.remove(p)
                    removed += 1
            except OSError:
                continue
    print("scrub: removed %d copies" % removed)


if __name__ == "__main__":
    main()
