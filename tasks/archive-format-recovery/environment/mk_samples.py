# Builds the sample trees under /app/samples and packs each with vpack.
# Copyright (c) 2026 the task author. MIT License.
import os
import random
import subprocess
import sys

WORDS = ("the of and to in is that for it as was with be by on not he this are or his from at "
         "which but have an they you were her she there been one all we their has would when "
         "if so no will what more out up into do any your about time can only other new some "
         "could these two may first then do any like my now over such our man me even most made "
         "after also did many before must through back years where much way well down should "
         "because each just those people how too little state good very make world still own "
         "see men work long get here between both life being under never day same another know "
         "while last might us great old year off come since against go came right used take three").split()

BASE_T = 1767225600  # 2026-01-01T00:00:00Z


def prose(rng, n_words):
    out, line = [], []
    for i in range(n_words):
        line.append(rng.choice(WORDS))
        if len(line) >= rng.randint(8, 14):
            out.append(" ".join(line) + ("." if rng.random() < 0.3 else ""))
            line = []
    if line:
        out.append(" ".join(line))
    return ("\n".join(out) + "\n").encode()


def code(rng, n_funcs):
    parts = []
    for i in range(n_funcs):
        name = "fn_%s_%d" % (rng.choice(WORDS), i)
        parts.append("def %s(x, y):\n    total = 0\n    for i in range(x):\n        total += i * y\n"
                     "    return total + %d\n\n" % (name, rng.randint(0, 999)))
    return "".join(parts).encode()


def csv(rng, rows):
    lines = ["id,name,score,city"]
    for i in range(rows):
        lines.append("%d,%s,%d,%s" % (i, rng.choice(WORDS), rng.randint(0, 100), rng.choice(WORDS)))
    return ("\n".join(lines) + "\n").encode()


def write(path, data, mode=0o644):
    with open(path, "wb") as f:
        f.write(data)
    os.chmod(path, mode)


def stamp(root, rng):
    """Give every entry a whole-second mtime, deepest first."""
    for dirpath, dirnames, filenames in os.walk(root, topdown=False):
        for n in filenames + dirnames:
            p = os.path.join(dirpath, n)
            t = BASE_T + rng.randint(0, 20_000_000)
            os.utime(p, ns=(t * 10**9, t * 10**9), follow_symlinks=False)
    t = BASE_T + rng.randint(0, 20_000_000)
    os.utime(root, ns=(t * 10**9, t * 10**9))


def sample_notes(root, rng):
    os.makedirs(os.path.join(root, "drafts"))
    os.makedirs(os.path.join(root, "archive"))
    write(os.path.join(root, "todo.txt"), prose(rng, 60))
    write(os.path.join(root, "drafts", "letter.txt"), prose(rng, 400))
    write(os.path.join(root, "drafts", "outline.md"), b"# Outline\n\n" + prose(rng, 120))


def sample_webapp(root, rng):
    os.makedirs(os.path.join(root, "static"))
    write(os.path.join(root, "index.html"),
          b"<html><head><title>demo</title></head><body>\n" + prose(rng, 200) + b"</body></html>\n")
    write(os.path.join(root, "static", "style.css"),
          b"body { margin: 0; padding: 0; }\nh1 { font-size: 2em; }\np { line-height: 1.4; }\n")
    write(os.path.join(root, "serve.sh"), b"#!/bin/sh\nexec python3 -m http.server 8000\n", 0o755)
    os.symlink("index.html", os.path.join(root, "home.html"))


def sample_data(root, rng):
    os.makedirs(os.path.join(root, "tables"))
    write(os.path.join(root, "tables", "scores.csv"), csv(rng, 150))
    write(os.path.join(root, "noise.bin"), bytes(rng.getrandbits(8) for _ in range(700)))
    write(os.path.join(root, "readme"), prose(rng, 40), 0o600)


def sample_config(root, rng):
    os.makedirs(os.path.join(root, "etc", "app", "conf.d"))
    write(os.path.join(root, "etc", "app", "main.ini"),
          b"[server]\nport = 8080\nhost = 0.0.0.0\n\n[log]\nlevel = info\npath = /var/log/app.log\n")
    write(os.path.join(root, "etc", "app", "conf.d", "extra.ini"), b"[extra]\nenabled = true\n")
    os.symlink("etc/app", os.path.join(root, "current"))
    os.symlink("/usr/share/zoneinfo/UTC", os.path.join(root, "localtime"))


def sample_mixed(root, rng):
    os.makedirs(os.path.join(root, "src"))
    os.makedirs(os.path.join(root, "build"))
    write(os.path.join(root, "src", "calc.py"), code(rng, 30))
    write(os.path.join(root, "Makefile"), b"all:\n\tpython3 src/calc.py\n")
    write(os.path.join(root, "build", "out.log"), prose(rng, 90))
    write(os.path.join(root, "empty.txt"), b"")


SAMPLES = [("notes", sample_notes), ("webapp", sample_webapp), ("data", sample_data),
           ("config", sample_config), ("mixed", sample_mixed)]


def main(dest):
    os.makedirs(dest, exist_ok=True)
    for i, (name, fn) in enumerate(SAMPLES):
        rng = random.Random(1000 + i)
        root = os.path.join(dest, name)
        os.makedirs(root)
        fn(root, rng)
        os.chmod(root, 0o755)
        stamp(root, rng)
        subprocess.run(["vpack", "pack", root, os.path.join(dest, name + ".vp")], check=True,
                       env=dict(os.environ, PATH="/app/bin:" + os.environ.get("PATH", "")))


if __name__ == "__main__":
    main(sys.argv[1])
