# Agent trajectory: archive-format-recovery

One cold attempt by the agent under test. Only the task prompt, the agent's visible
messages, its tool calls and truncated tool outputs are kept.
Tool calls: 76


## Task prompt given to the agent

```
You are an AI agent solving a terminal task inside a Docker container. The container is already running and named `sn-archive-format-recovery`. Work ONLY inside it, by running commands through Bash as: docker exec sn-archive-format-recovery bash -lc '<command>' (for multi-line file edits, use heredocs inside the bash -lc string, or `docker exec -i ... bash -c 'cat > /path' <<'EOF' ... EOF`).

Rules: do not read, list or search any file on the host machine (no Read/Grep/Glob on host paths, no ls of host dirs); everything you need is in the container. No web access. Do not copy files out of the container.

Start with: docker exec sn-archive-format-recovery cat /instruction.md . That file is your task. Solve it as well as you can, test your work inside the container, and stop when you believe it is complete. Then reply with a short summary of what you changed and how confident you are.
```

### Step 1: Bash

```bash
docker exec sn-archive-format-recovery cat /instruction.md
```

<details><summary>output</summary>

```
`/app/bin/vpack` is an archiver. `vpack pack DIR OUT` packs the directory tree `DIR` into the archive file `OUT`. This build cannot unpack, and the archive format is not documented anywhere. `/app/samples` holds a few archives next to the directories they were packed from. `/app/README.txt` describes the tool.

Write `/app/unvpack`, an executable program, that unpacks these archives:

'''
/app/unvpack ARCHIVE OUTDIR
'''

`OUTDIR` does not exist before the call; your program creates it. On success it exits with status 0 and `OUTDIR` is an exact copy of the directory that was packed:

- the same entries (regular files, directories including empty ones, and symbolic links) with byte-identical names, and nothing else;
- byte-identical regular file contents;
- byte-identical symbolic link targets (links are recreated as links, not followed);
- the same permission bits (`st_mode & 0o7777`, including setuid, setgid and sticky) on every regular file and directory, and on `OUTDIR` itself;
- the same modification time, to the nanosecond, on every entry (files, directories and symbolic links) and on `OUTDIR` itself.

Ownership, access times and extended attributes are not checked.

Your program will be graded on many archives that this same `vpack` produced from other directory trees; anything `vpack` can pack may appear. It runs as root. `vpack` itself will not be installed at that point, so your program must not depend on it or on the contents of `/app/samples`. Each run must finish within 60 seconds; archives are at most a few megabytes.

If `ARCHIVE` is damaged (for example truncated, with bytes appended, or with bytes altered), `unvpack` must exit with status 1 and print a message containing the word `corrupt` on standard error. Only damage that can be detected from the archive itself (its structure and the checksums `vpack` stores in it) is tested. What is left in `OUTDIR` after a damaged archive does not matter.

Python 3.12 is installed. Passing requires every graded archive to be handled correctly.
```
</details>

### Step 2: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat /app/README.txt; ls -laR /app/samples | head -80; ls /app /app/bin; file /app/bin/vpack'
```

<details><summary>output</summary>

```
Exit code 127
vpack packs a directory tree into one archive file. Run `vpack pack DIR OUT.vp` to write the
archive. It keeps regular files, directories (empty ones too) and symbolic links, together with
their permission bits and modification times, and it compresses file contents when that makes
the archive smaller. This build cannot unpack. The archives in /app/samples were made with
`vpack pack /app/samples/NAME /app/samples/NAME.vp`.
/app/samples:
total 48
drwxr-xr-x 7 root root 4096 Nov 17 14:13 .
drwxr-xr-x 1 root root 4096 Nov 17 14:13 ..
drwxr-xr-x 3 root root 4096 Jun 26 01:37 config
-rw-r--r-- 1 root root  278 Nov 17 14:13 config.vp
drwxr-xr-x 3 root root 4096 Jul 13 06:09 data
-rw-r--r-- 1 root root 2639 Nov 17 14:13 data.vp
drwxr-xr-x 4 root root 4096 Jun 19 03:34 mixed
-rw-r--r-- 1 root root  998 Nov 17 14:13 mixed.vp
drwxr-xr-x 4 root root 4096 Apr 21  2026 notes
-rw-r--r-- 1 root root 2005 Nov 17 14:13 notes.vp
drwxr-xr-x 3 root root 4096 Jan  8  2026 webapp
-rw-r--r-- 1 root root  950 Nov 17 14:13 webapp.vp

/app/samples/config:
total 12
drwxr-xr-x 3 root root 4096 Jun 26 01:37 .
drwxr-xr-x 7 root root 4096 Nov 17 14:13 ..
lrwxrwxrwx 1 root root    7 Jun 23 11:44 current -> etc/app
drwxr-xr-x 3 root root 4096 Jul 29 10:13 etc
lrwxrwxrwx 1 root root   23 Jun 29 20:50 localtime -> /usr/share/zoneinfo/UTC

/app/samples/config/etc:
total 12
drwxr-xr-x 3 root root 4096 Jul 29 10:13 .
drwxr-xr-x 3 root root 4096 Jun 26 01:37 ..
drwxr-xr-x 3 root root 4096 Mar 28  2026 app

/app/samples/config/etc/app:
total 16
drwxr-xr-x 3 root root 4096 Mar 28  2026 .
drwxr-xr-x 3 root root 4096 Jul 29 10:13 ..
drwxr-xr-x 2 root root 4096 May 20 19:21 conf.d
-rw-r--r-- 1 root root   80 Aug  6 00:41 main.ini

/app/samples/config/etc/app/conf.d:
total 12
drwxr-xr-x 2 root root 4096 May 20 19:21 .
drwxr-xr-x 3 root root 4096 Mar 28  2026 ..
-rw-r--r-- 1 root root   23 Jul 13 03:33 extra.ini

/app/samples/data:
total 20
drwxr-xr-x 3 root root 4096 Jul 13 06:09 .
drwxr-xr-x 7 root root 4096 Nov 17 14:13 ..
-rw-r--r-- 1 root root  700 May 11  2026 noise.bin
-rw------- 1 root root  189 May 16  2026 readme
drwxr-xr-x 2 root root 4096 Jul 17 11:08 tables

/app/samples/data/tables:
total 12
drwxr-xr-x 2 root root 4096 Jul 17 11:08 .
drwxr-xr-x 3 root root 4096 Jul 13 06:09 ..
-rw-r--r-- 1 root root 2354 May  7  2026 scores.csv

/app/samples/mixed:
total 20
drwxr-xr-x 4 root root 4096 Jun 19 03:34 .
drwxr-xr-x 7 root root 4096 Nov 17 14:13 ..
-rw-r--r-- 1 root root   26 Jun  2 04:15 Makefile
drwxr-xr-x 2 root root 4096 Mar 12  2026 build
-rw-r--r-- 1 root root    0 Aug 14 10:58 empty.txt
drwxr-xr-x 2 root root 4096 Aug  2 11:10 src

/app/samples/mixed/build:
total 12
drwxr-xr-x 2 root root 4096 Mar 12  2026 .
drwxr-xr-x 4 root root 4096 Jun 19 03:34 ..
-rw-r--r-- 1 root root  446 Apr 30  2026 out.log

/app/samples/mixed/src:
total 12
drwxr-xr-x 2 root root 4096 Aug  2 11:10 .
drwxr-xr-x 4 root root 4096 Jun 19 03:34 ..
-rw-r--r-- 1 root root 3152 Apr  4  2026 calc
... [174 more chars truncated]
```
</details>

### Step 3: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /app; cat bin/LICENSE.vpack | head -30; head -c 300 bin/vpack | xxd | head; wc -c bin/vpack; xxd samples/config.vp; which gdb objdump strings python3'
```

<details><summary>output</summary>

```
Exit code 1
MIT License

Copyright (c) 2026 the vpack author

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
bash: line 1: xxd: command not found
67808 bin/vpack
bash: line 1: xxd: command not found
/usr/local/bin/python3
```
</details>

### Step 4: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /app; head -c 200 bin/vpack | od -c | head -8; od -A d -t x1z samples/config.vp; ls -la --time-style=full-iso samples/config samples/config/etc/app'
```

<details><summary>output</summary>

```
0000000 177   E   L   F 002 001 001  \0  \0  \0  \0  \0  \0  \0  \0  \0
0000020 002  \0 267  \0 001  \0  \0  \0 300 021   @  \0  \0  \0  \0  \0
0000040   @  \0  \0  \0  \0  \0  \0  \0     002 001  \0  \0  \0  \0  \0
0000060  \0  \0  \0  \0   @  \0   8  \0  \t  \0   @  \0 033  \0 032  \0
0000100 006  \0  \0  \0 004  \0  \0  \0   @  \0  \0  \0  \0  \0  \0  \0
0000120   @  \0   @  \0  \0  \0  \0  \0   @  \0   @  \0  \0  \0  \0  \0
0000140 370 001  \0  \0  \0  \0  \0  \0 370 001  \0  \0  \0  \0  \0  \0
0000160  \b  \0  \0  \0  \0  \0  \0  \0 003  \0  \0  \0 004  \0  \0  \0
0000000 89 56 50 4b 03 00 8c 9b b7 f2 66 00 86 b4 ea 1c  >.VPK......f.....<
0000016 5b 65 78 74 72 61 5d 0a 65 6e 61 62 6c 65 64 20  >[extra].enabled <
0000032 3d 20 74 72 75 65 0a 5b 73 65 72 76 65 72 5d 0a  >= true.[server].<
0000048 70 6f 72 74 20 3d 20 38 30 38 30 0a 68 6f 73 74  >port = 8080.host<
0000064 20 3d 20 30 2e 30 2e 30 2e 30 0a 0a 5b 6c 6f 67  > = 0.0.0.0..[log<
0000080 5d 0a 6c 65 76 65 6c 20 3d 20 69 6e 66 6f 0a 70  >].level = info.p<
0000096 61 74 68 20 3d 20 2f 76 61 72 2f 6c 6f 67 2f 61  >ath = /var/log/a<
0000112 70 70 2e 6c 6f 67 0a 07 03 00 07 63 75 72 72 65  >pp.log.....curre<
0000128 6e 74 82 cc a8 33 07 65 74 63 2f 61 70 70 02 00  >nt...3.etc/app..<
0000144 03 65 74 63 00 84 89 e9 71 02 02 03 61 70 70 00  >.etc....q...app.<
0000160 00 02 03 06 63 6f 6e 66 2e 64 00 81 9a d8 38 01  >....conf.d....8.<
0000176 04 09 65 78 74 72 61 2e 69 6e 69 00 83 b3 fe 15  >..extra.ini.....<
0000192 17 b7 e2 e6 30 01 03 08 6d 61 69 6e 2e 69 6e 69  >....0...main.ini<
0000208 00 84 b1 f5 48 50 a4 0e bd dc 03 00 09 6c 6f 63  >....HP.......loc<
0000224 61 6c 74 69 6d 65 82 ed fa 09 17 2f 75 73 72 2f  >altime...../usr/<
0000240 73 68 61 72 65 2f 7a 6f 6e 65 69 6e 66 6f 2f 55  >share/zoneinfo/U<
0000256 54 43 77 00 00 00 00 00 00 00 8b 00 00 00 17 99  >TCw.............<
0000272 84 e0 1a 4b 50 56                                >...KPV<
0000278
samples/config:
total 12
drwxr-xr-x 3 root root 4096 2026-06-26 01:37:05.000000000 +0000 .
drwxr-xr-x 7 root root 4096 2026-11-17 14:13:16.000000000 +0000 ..
lrwxrwxrwx 1 root root    7 2026-06-23 11:44:38.000000000 +0000 current -> etc/app
drwxr-xr-x 3 root root 4096 2026-07-29 10:13:56.000000000 +0000 etc
lrwxrwxrwx 1 root root   23 2026-06-29 20:50:04.000000000 +0000 localtime -> /usr/share/zoneinfo/UTC

samples/config/etc/app:
total 16
drwxr-xr-x 3 root root 4096 2026-03-28 00:12:35.000000000 +0000 .
drwxr-xr-x 3 root root 4096 2026-07-29 10:13:56.000000000 +0000 ..
drwxr-xr-x 2 root root 4096 2026-05-20 19:21:15.000000000 +0000 conf.d
-rw-r--r-- 1 root root   80 2026-08-06 00:41:31.000000000 +0000 main.ini
```
</details>

### Step 5: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'mkdir -p /w && cd /w && rm -rf t && mkdir t && cd t && echo -n a > f && touch -d @1000000000.123456789 f && touch -d @1000000000 . && cd .. && /app/bin/vpack pack t t.vp && od -A d -t x1z t.vp; python3 -c "import capstone" ; pip list 2>/dev/null | head -20; which gcc strace ltrace'
```

<details><summary>output</summary>

```
Exit code 1
0000000 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 01 05  >.VPK.........a..<
0000016 00 01 66 00 00 b9 ee 99 15 01 43 be b7 e8 0e 00  >..f.......C.....<
0000032 00 00 00 00 00 00 10 00 00 00 b5 a0 3f f1 1a 4b  >............?..K<
0000048 50 56                                            >PV<
0000050
Traceback (most recent call last):
  File "<string>", line 1, in <module>
ModuleNotFoundError: No module named 'capstone'
Package   Version
--------- -------
iniconfig 2.3.1
packaging 26.3
pip       25.0.1
pluggy    1.7.0
pytest    8.3.4
```
</details>

### Step 6: Bash

```bash
cat > /tmp/h.py <<'EOF'
import os,subprocess,sys,shutil
def pack(setup, name='x'):
    d='/w/'+name
    shutil.rmtree(d,ignore_errors=True); os.makedirs(d)
    setup(d)
    subprocess.run(['/app/bin/vpack','pack',d,d+'.vp'],check=True)
    return open(d+'.vp','rb').read()
def hx(b): return ' '.join('%02x'%c for c in b)
EOF
docker cp /tmp/h.py sn-archive-format-recovery:/w/h.py 2>&1 | head -2
docker exec sn-archive-format-recovery bash -lc 'cat > /w/e1.py <<EOF
import sys; sys.path.insert(0,"/w")
from h import *
for ns in [(0,0),(1,0),(2,0),(1000,0),(1000000000,0),(1000000000,1),(1000000000,500000000),(1000000000,999999999),(1000000001,0)]:
    def s(d):
        os.utime(d,ns=(0,ns[0]*10**9+ns[1]))
    print(ns,hx(pack(s)))
EOF
python3 /w/e1.py'
```

<details><summary>output</summary>

```
(0, 0) 89 56 50 4b 03 00 00 00 00 00 09 00 00 00 00 00 00 00 01 00 00 00 aa d2 6c 6d 1a 4b 50 56
(1, 0) 89 56 50 4b 03 00 02 00 00 00 09 00 00 00 00 00 00 00 01 00 00 00 21 1a 65 c7 1a 4b 50 56
(2, 0) 89 56 50 4b 03 00 04 00 00 00 09 00 00 00 00 00 00 00 01 00 00 00 fd 45 0e e2 1a 4b 50 56
(1000, 0) 89 56 50 4b 03 00 8e 50 00 00 00 0a 00 00 00 00 00 00 00 01 00 00 00 f8 c8 b7 21 1a 4b 50 56
(1000000000, 0) 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 00 0d 00 00 00 00 00 00 00 01 00 00 00 ed bd ee 71 1a 4b 50 56
(1000000000, 1) 89 56 50 4b 03 00 86 b8 d5 a7 00 00 01 01 00 0e 00 00 00 00 00 00 00 01 00 00 00 83 c4 11 14 1a 4b 50 56
(1000000000, 500000000) 89 56 50 4b 03 00 86 b8 d5 a7 00 00 01 80 ed b4 c9 00 00 12 00 00 00 00 00 00 00 01 00 00 00 29 bf cb 08 1a 4b 50 56
(1000000000, 999999999) 89 56 50 4b 03 00 86 b8 d5 a7 00 00 01 82 db ea 92 7f 00 12 00 00 00 00 00 00 00 01 00 00 00 fd 8f fe 88 1a 4b 50 56
(1000000001, 0) 89 56 50 4b 03 00 86 b8 d5 a7 02 00 00 00 0d 00 00 00 00 00 00 00 01 00 00 00 66 75 e7 db 1a 4b 50 56
```
</details>

### Step 7: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat > /w/e2.py <<EOF
import sys; sys.path.insert(0,"/w")
from h import *
for mode in [0o755,0o700,0o1777,0o4755,0o000]:
  for t in [(1782436625,0),(1782436625,123456789),(-5,0)]:
    def s(d):
        os.utime(d,ns=(0,t[0]*10**9+t[1])); os.chmod(d,mode)
    print(oct(mode),t,hx(pack(s)))
EOF
python3 /w/e2.py'
```

<details><summary>output</summary>

```
0o755 (1782436625, 0) 89 56 50 4b 03 00 8c a2 ed cb 22 00 00 00 0d 00 00 00 00 00 00 00 01 00 00 00 9c 1a 50 9d 1a 4b 50 56
0o755 (1782436625, 123456789) 89 56 50 4b 03 00 8c a2 ed cb 22 00 01 b9 ee 99 15 00 11 00 00 00 00 00 00 00 01 00 00 00 ab ff 3a d6 1a 4b 50 56
0o755 (-5, 0) 89 56 50 4b 03 00 09 00 00 00 09 00 00 00 00 00 00 00 01 00 00 00 20 9d 64 10 1a 4b 50 56
0o700 (1782436625, 0) 89 56 50 4b 03 00 8c a2 ed cb 22 2d 00 00 0d 00 00 00 00 00 00 00 01 00 00 00 2f cf c5 ad 1a 4b 50 56
0o700 (1782436625, 123456789) 89 56 50 4b 03 00 8c a2 ed cb 22 2d 01 b9 ee 99 15 00 11 00 00 00 00 00 00 00 01 00 00 00 f7 a7 50 78 1a 4b 50 56
0o700 (-5, 0) 89 56 50 4b 03 00 09 2d 00 00 09 00 00 00 00 00 00 00 01 00 00 00 93 48 f1 20 1a 4b 50 56
0o1777 (1782436625, 0) 89 56 50 4b 03 00 8c a2 ed cb 22 83 12 00 00 0e 00 00 00 00 00 00 00 01 00 00 00 33 0e 68 cb 1a 4b 50 56
0o1777 (1782436625, 123456789) 89 56 50 4b 03 00 8c a2 ed cb 22 83 12 01 b9 ee 99 15 00 12 00 00 00 00 00 00 00 01 00 00 00 6e b5 8a 4d 1a 4b 50 56
0o1777 (-5, 0) 89 56 50 4b 03 00 09 83 12 00 00 0a 00 00 00 00 00 00 00 01 00 00 00 13 c5 32 09 1a 4b 50 56
0o4755 (1782436625, 0) 89 56 50 4b 03 00 8c a2 ed cb 22 8f 00 00 00 0e 00 00 00 00 00 00 00 01 00 00 00 95 c6 1c 9e 1a 4b 50 56
0o4755 (1782436625, 123456789) 89 56 50 4b 03 00 8c a2 ed cb 22 8f 00 01 b9 ee 99 15 00 12 00 00 00 00 00 00 00 01 00 00 00 c6 2d 64 ea 1a 4b 50 56
0o4755 (-5, 0) 89 56 50 4b 03 00 09 8f 00 00 00 0a 00 00 00 00 00 00 00 01 00 00 00 b5 0d 46 5c 1a 4b 50 56
0o0 (1782436625, 0) 89 56 50 4b 03 00 8c a2 ed cb 22 82 6d 00 00 0e 00 00 00 00 00 00 00 01 00 00 00 3b 46 79 2c 1a 4b 50 56
0o0 (1782436625, 123456789) 89 56 50 4b 03 00 8c a2 ed cb 22 82 6d 01 b9 ee 99 15 00 12 00 00 00 00 00 00 00 01 00 00 00 32 15 00 9b 1a 4b 50 56
0o0 (-5, 0) 89 56 50 4b 03 00 09 82 6d 00 00 0a 00 00 00 00 00 00 00 01 00 00 00 1b 8d 23 ee 1a 4b 50 56
```
</details>

### Step 8: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat >> /w/h.py <<EOF
def gv(b,i):
    c=b[i]; i+=1; v=c&127
    while c&128:
        c=b[i]; i+=1; v=((v+1)<<7)|(c&127)
    return v,i
def zz(v): return (v>>1)^-(v&1)
EOF
cd /w; python3 -c "
from h import *
b=open(\"/app/samples/config.vp\",\"rb\").read()
v,i=gv(b,6); print(zz(v),i)
import datetime; print(datetime.datetime.utcfromtimestamp(zz(v)))
print(gv(b,i))
"; ls -la --time-style=full-iso /app/samples | head -4'
```

<details><summary>output</summary>

```
<string>:5: DeprecationWarning: datetime.datetime.utcfromtimestamp() is deprecated and scheduled for removal in a future version. Use timezone-aware objects to represent datetimes in UTC: datetime.datetime.fromtimestamp(timestamp, datetime.UTC).
1774656755 11
2026-03-28 00:12:35
(0, 12)
total 48
drwxr-xr-x 7 root root 4096 2026-11-17 14:13:16.000000000 +0000 .
drwxr-xr-x 1 root root 4096 2026-11-17 14:13:16.000000000 +0000 ..
drwxr-xr-x 3 root root 4096 2026-06-26 01:37:05.000000000 +0000 config
```
</details>

### Step 9: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat >> /w/h.py <<EOF
import struct
def parts(b):
    io,il=struct.unpack("<QI",b[-20:-8])
    return b[:io], b[io:io+il], b[-20:]
EOF
cat > /w/e3.py <<EOF
import sys; sys.path.insert(0,"/w")
from h import *
B=1000000000
def run(label,setup,base_t=B):
    def s(d):
        setup(d); os.utime(d,ns=(0,base_t*10**9))
    b=pack(s); h,ix,f=parts(b)
    print(label,"\n  H+D:",hx(h),"\n  IX:",hx(ix))
def f1(d, name="f", data=b"a", mode=0o644, t=(B,0)):
    p=d+"/"+name; open(p,"wb").write(data); os.chmod(p,mode); os.utime(p,ns=(0,t[0]*10**9+t[1]))
run("file a 644",lambda d:f1(d))
run("file a 600",lambda d:f1(d,mode=0o600))
run("file a 4755",lambda d:f1(d,mode=0o4755))
run("file a t+5",lambda d:f1(d,t=(B+5,0)))
run("file a t-5",lambda d:f1(d,t=(B-5,0)))
run("file a t+5 ns7",lambda d:f1(d,t=(B+5,7)))
run("file name g",lambda d:f1(d,name="g"))
run("file empty",lambda d:f1(d,data=b""))
run("two files",lambda d:(f1(d,name="a"),f1(d,name="b",data=b"zz")))
run("dir empty",lambda d:os.mkdir(d+"/sub"))
run("dir w file",lambda d:(os.mkdir(d+"/sub"),f1(d,name="sub/f")))
run("symlink",lambda d:os.symlink("tgt",d+"/l"))
EOF
python3 /w/e3.py'
```

<details><summary>output</summary>

```
file a 644 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 01 01 00 01 66 00 00 01 43 be b7 e8
file a 600 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 01 01 00 01 66 24 00 01 43 be b7 e8
file a 4755 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 01 01 00 01 66 8f 49 00 01 43 be b7 e8
file a t+5 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 01 01 00 01 66 00 05 01 43 be b7 e8
file a t-5 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a6 76 00 0a 61 
  IX: 01 01 00 01 66 00 00 01 43 be b7 e8
file a t+5 ns7 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 01 05 00 01 66 00 05 07 01 43 be b7 e8
file name g 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 01 01 00 01 67 00 00 01 43 be b7 e8
file empty 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 
  IX: 01 01 00 01 66 00 00 00
two files 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 7a 7a 
  IX: 02 01 00 01 61 00 00 01 43 be b7 e8 01 00 01 62 00 00 02 a1 1b d9 24
dir empty 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 
  IX: 01 06 00 03 73 75 62 00 81 fa 85 b3 50 80 dc f6 da 4b
dir w file 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 61 
  IX: 02 06 00 03 73 75 62 00 81 fa 85 b3 50 80 f6 b6 85 25 01 01 01 66 00 00 01 43 be b7 e8
symlink 
  H+D: 89 56 50 4b 03 00 86 b8 d5 a7 00 00 00 
  IX: 01 07 00 01 6c 81 fa 85 b3 50 81 8a 9d 86 0b 03 74 67 74
```
</details>

### Step 10: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > p.py <<EOF
import sys; sys.path.insert(0,"/w")
from h import *
def parse(b):
    D,ix,f=parts(b)
    i=6
    base,i=gv(D,i); base=zz(base)
    rm,i=gv(D,i); rd,i=gv(D,i)
    print("base",base,"rootmode",oct(rm^0o755),"rd",rd)
    if rd&1: rn,i=gv(D,i); print(" rootns",rn)
    n,j=gv(ix,0); print("count",n)
    ents=[]
    for k in range(n):
        t=ix[j]; par=ix[j+1]; j+=2
        nl,j=gv(ix,j); nm=ix[j:j+nl]; j+=nl
        e=dict(t=t,par=par,name=nm)
        if t&3!=3:
            m,j=gv(ix,j); e["mode"]=m
        d,j=gv(ix,j); e["d"]=d
        if t&4: ns,j=gv(ix,j); e["ns"]=ns
        if t&3==3:
            l,j=gv(ix,j); e["tgt"]=ix[j:j+l]; j+=l
        elif t&3==1:
            sz,j=gv(ix,j); e["size"]=sz
            rest=[]
            if sz: e["crc"]=ix[j:j+4]; j+=4
        print(k+1,e)
    print("leftover",hx(ix[j:]))
    return D,ents
if __name__=="__main__":
    for n in sys.argv[1:]:
        print(n); parse(open(n,"rb").read())
EOF
python3 p.py /app/samples/data.vp /app/samples/mixed.vp'
```

<details><summary>output</summary>

```
Exit code 1
Traceback (most recent call last):
  File "/w/p.py", line 31, in <module>
    print(n); parse(open(n,"rb").read())
/app/samples/data.vp
base 1778180858 rootmode 0o755 rd 11484210
count 4
1 {'t': 1, 'par': 0, 'name': b'noise.bin', 'mode': 0, 'd': 294327, 'size': 700, 'crc': b'^\xb0\xd8#'}
2 {'t': 9, 'par': 0, 'name': b'readme', 'mode': 36, 'd': 768965, 'size': 189, 'crc': b'\x806\r\t'}
3 {'t': 233, 'par': 159, 'name': b'\x00\x06', 'mode': 116, 'd': 97, 'size': 98, 'crc': b'les\x00'}
              ^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/w/p.py", line 17, in parse
    m,j=gv(ix,j); e["mode"]=m
        ^^^^^^^^
  File "/w/h.py", line 10, in gv
    c=b[i]; i+=1; v=c&127
      ~^^^
IndexError: index out of range
```
</details>

### Step 11: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
from h import *
b=open("/app/samples/data.vp","rb").read()
D,ix,f=parts(b)
print(hx(ix)); print(len(D))
print(D[:60])
EOF
cat /app/samples/data/readme | head -c 300'
```

<details><summary>output</summary>

```
04 01 00 09 6e 6f 69 73 65 2e 62 69 6e 00 90 fa 37 84 3c 5e b0 d8 23 09 00 06 72 65 61 64 6d 65 24 ad f6 45 80 3d 80 36 0d 09 e9 9f 02 00 06 74 61 62 6c 65 73 00 81 f3 d3 2a 09 03 0a 73 63 6f 72 65 73 2e 63 73 76 00 00 91 32 8b 68 c6 c9 e2 d3
2538
b'\x89VPK\x03\x00\x8c\x9e\xe6\x8at\x00\x84\xbb\xf72[\x98I$\xads\xc4\xb1\xe2\xb2\xfa\x07\xad;\xb0Vr\xe7K\x1e\xaa\xa5\x88\xc4\x84B\xc1\xcfa\xec\xee\x1de\x0c\xfd\x83\xa1SW\x82V\x7f`x'
all would such been they go even but about can never never.
day same very just made at just up life have like
to both the may here into by off little right as more state
over their have my
```
</details>

### Step 12: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
from h import *
import zlib
b=open("/app/samples/data.vp","rb").read()
D,ix,f=parts(b)
# header len: 6+5+1+4=16? find
off=6+5+1+4
off=15 if False else off
print(off, hx(D[off-2:off+6]))
d=D[off+700:]  # after noise.bin? order unknown
for st in range(10,20):
    for w in (-15,15,31):
        try:
            o=zlib.decompressobj(w).decompress(D[st:st+182]); print(st,w,len(o),o[:40])
        except Exception as e: pass
EOF'
```

<details><summary>output</summary>

```
16 f7 32 5b 98 49 24 ad 73
```
</details>

### Step 13: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
from h import *
import zlib
b=open("/app/samples/data.vp","rb").read()
D,ix,f=parts(b)
s=716
print(hx(D[s:s+20]))
for w in (-15,15,31):
    try:
        o=zlib.decompressobj(w).decompress(D[s:s+182]); print(w,len(o),o[:60])
    except Exception as e: print(w,e)
print(open("/app/samples/data/readme","rb").read()[:60])
EOF'
```

<details><summary>output</summary>

```
81 68 00 61 6c 6c 20 77 6f 75 6c 00 64 20 73 75 63 68 20 62
-15 Error -3 while decompressing data: invalid stored block lengths
15 Error -3 while decompressing data: incorrect header check
31 Error -3 while decompressing data: incorrect header check
b'all would such been they go even but about can never never.\n'
```
</details>

### Step 14: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e4.py <<EOF
import sys; sys.path.insert(0,"/w")
from h import *
def one(data):
    def s(d):
        open(d+"/f","wb").write(data)
    b=pack(s); D,ix,f=parts(b)
    print(len(data),"\n D:",hx(D[6:]),"\n IX:",hx(ix))
one(b"a"*100)
one(b"a"*300)
one(b"ab"*50)
one(b"abcdefgh"*20)
one(b"hello world hello world hello world")
one(b"abcdefghij"+b"abcdefghij"+b"XYZ")
EOF
python3 e4.py'
```

<details><summary>output</summary>

```
100 
 D: 8c ae e2 91 28 00 01 80 83 84 dc 70 0a 02 61 e0 00 00 
 IX: 01 0d 00 01 66 00 00 80 83 ea c2 6a 64 06 64 7a 70 af
300 
 D: 8c ae e2 91 28 00 01 80 8e fa cd 30 0e 02 61 ff 00 00 80 29 
 IX: 01 0d 00 01 66 00 00 80 8e fa cd 30 81 2c 08 09 19 97 89
100 
 D: 8c ae e2 91 28 00 01 80 96 8e 90 70 0c 04 61 62 df 01 00 
 IX: 01 0d 00 01 66 00 00 80 96 ef fd 79 64 07 2c 2f f4 5d
160 
 D: 8c ae e2 91 28 00 01 80 9b ea c6 70 1c 00 61 62 63 64 65 66 67 68 01 ff 07 00 16 
 IX: 01 0d 00 01 66 00 00 80 9c d0 a7 4f 80 20 0f 44 ce 95 26
35 
 D: 8c ae e2 91 28 00 01 80 a2 a7 e7 0b 22 00 68 65 6c 6c 6f 20 77 6f 10 72 6c 64 20 94 0b 00 
 IX: 01 0d 00 01 66 00 00 80 a2 f0 a5 0a 23 12 bb fe 42 0f
23 
 D: 8c ae e2 91 28 00 01 80 aa 91 cd 70 22 00 61 62 63 64 65 66 67 68 04 69 6a 70 09 58 59 5a 
 IX: 01 0d 00 01 66 00 00 80 aa fe ae 10 17 12 82 1b b0 da
```
</details>

### Step 15: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e5.py <<EOF
import sys,random; sys.path.insert(0,"/w")
from h import *
random.seed(1)
def comp(data):
    def s(d):
        open(d+"/f","wb").write(data)
    b=pack(s); D,ix,f=parts(b)
    # parse
    i=6; _,i=gv(D,i); _,i=gv(D,i); rd,i=gv(D,i)
    if rd&1: _,i=gv(D,i)
    n,j=gv(ix,0); t=ix[j]; 
    j+=2; nl,j=gv(ix,j); j+=nl; m,j=gv(ix,j); d,j=gv(ix,j)
    if t&4: ns,j=gv(ix,j)
    sz,j=gv(ix,j)
    if t&8:
        cs,j=gv(ix,j); return D[i:i+cs]
    return None
R=bytes(random.randrange(33,127) for _ in range(40))
for L in [3,4,5,6,8,10,16,17,18,19,20,30,40]:
    data=R+R[:L]
    c=comp(data)
    print("len",L,hx(c) if c else None)
EOF
python3 e5.py'
```

<details><summary>output</summary>

```
len 3 None
len 4 None
len 5 None
len 6 None
len 8 None
len 10 60 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 70 27
len 16 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 8d 27 00
len 17 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 8e 27 00
len 18 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 8f 27 00
len 19 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 90 27 00
len 20 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 91 27 00
len 30 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 9b 27 00
len 40 62 00 32 69 29 41 30 60 5a 5d 00 74 51 3b 2d 5f 24 52 58 00 6e 21 7a 5a 43 7d 3e 6c 00 2e 49 24 23 24 74 66 22 00 51 78 3c 57 7d 24 64 3d 01 a5 27 00
```
</details>

### Step 16: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e6.py <<EOF
import sys,random; sys.path.insert(0,"/w")
from e5 import comp
from h import hx
import random
random.seed(5)
R=bytes(random.randrange(33,127) for _ in range(600))
for O,L in [(10,10),(10,11),(10,12),(10,14),(10,15),(10,16),(10,50),(10,129),(10,130),(10,131),(10,200),(10,300),(100,20),(255,20),(256,20),(257,20),(300,20),(500,20),(590,20),(5,5),(5,6),(5,7),(5,8),(5,9)]:
    data=R+R[-O:][:L] if L<=O else None
    if data is None:
        # overlapping: repeat
        seg=(R[-O:]*(L//O+2))[:L]; data=R+seg
    c=comp(data)
    print(O,L,len(c) if c else None,hx(c[:1]),hx(c[-9:]) if c else None)
EOF
sed -i "s/^for L in/if __name__==\"__main__\":\n  for L in/;" e5.py; python3 - <<EOF
import re
s=open("/w/e5.py").read()
i=s.index("if __name__")
head=s[:i]; tail=s[i:].split("\n")
out=[tail[0],tail[1]]+["  "+l for l in tail[2:]]
open("/w/e5.py","w").write(head+"\n".join(out))
EOF
python3 e6.py'
```

<details><summary>output</summary>

```
Exit code 1
Traceback (most recent call last):
  File "/w/e6.py", line 13, in <module>
    print(O,L,len(c) if c else None,hx(c[:1]),hx(c[-9:]) if c else None)
                                       ~^^^^
TypeError: 'NoneType' object is not subscriptable
```
</details>

### Step 17: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; sed -i "s/hx(c\[:1\])/hx(c[:1]) if c else None/" e6.py; python3 e6.py'
```

<details><summary>output</summary>

```
10 10 None None None
10 11 None None None
10 12 None None None
10 14 None None None
10 15 None None None
10 16 None None None
10 50 None None None
10 129 678 89 3f 10 52 5c 5e 4a fe 09 00
10 130 679 89 10 52 5c 5e 4a ff 09 00 00
10 131 679 89 10 52 5c 5e 4a ff 09 00 01
10 200 679 89 10 52 5c 5e 4a ff 09 00 46
10 300 680 89 52 5c 5e 4a ff 09 00 80 2a
100 20 None None None
255 20 None None None
256 20 None None None
257 20 None None None
300 20 None None None
500 20 None None None
590 20 None None None
5 5 None None None
5 6 None None None
5 7 None None None
5 8 None None None
5 9 None None None
```
</details>

### Step 18: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
from h import *
import zlib,struct
for n in ["config","data","mixed","notes","webapp"]:
    b=open("/app/samples/%s.vp"%n,"rb").read()
    io,il,crc=struct.unpack("<QII",b[-20:-4])
    print(n,io,il,len(b),hex(crc),hex(zlib.crc32(b[io:io+il])),hex(zlib.crc32(b[:io+il])),hex(zlib.crc32(b[6:io+il])),hex(zlib.crc32(b[:io])), hex(zlib.crc32(b[io:-8])))
EOF'
```

<details><summary>output</summary>

```
config 119 139 278 0xe0849917 0xe1692414 0x63cd967e 0x1c987575 0x1ac7d37c 0x28bb0fea
data 2538 81 2639 0x7f289e10 0xfa5330c0 0xc38ecedb 0xd911734b 0xd654bcd1 0xf8be9ce7
mixed 872 106 998 0x6cd068f 0x65df3f14 0x588760b8 0x7b5270df 0xb0b06297 0x9490d489
notes 1882 103 2005 0xc4bd4057 0x1887c7dd 0x7b0e65f2 0x3d1068a4 0xa2ca3aec 0x7b1351db
webapp 820 110 950 0xd1c9a156 0x3a4456c8 0x6c7651de 0xbaa4f2f5 0xd92f3954 0xc40c793a
```
</details>

### Step 19: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
from h import *
import zlib,struct
b=open("/app/samples/config.vp","rb").read()
tgt=struct.unpack("<I",b[-8:-4])[0]
print(hex(tgt))
def crc32c(d,c=0):
    c^=0xffffffff
    for x in d:
        c^=x
        for _ in range(8): c=(c>>1)^(0x82f63b78 if c&1 else 0)
    return c^0xffffffff
n=len(b)
for s in range(0,n):
    for e in range(s,n+1):
        seg=b[s:e]
        if zlib.crc32(seg)==tgt: print("crc32",s,e)
        if zlib.adler32(seg)==tgt: print("adler",s,e)
        if e-s<200 and crc32c(seg)==tgt: print("c",s,e)
EOF'
```

<details><summary>output</summary>

```
0xe0849917
```
</details>

### Step 20: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
from h import *
import zlib,struct,itertools
b=open("/app/samples/config.vp","rb").read()
tgt=struct.unpack("<I",b[-8:-4])[0]
io,il=struct.unpack("<QI",b[-20:-8])
D=b[:io]; ix=b[io:io+il]; f12=b[-20:-8]
hdr=b[:6]
cands={"D":D,"ix":ix,"f12":f12,"hdr":hdr,"body":b[6:io],"magic":b[:4],"Dnohdr":b[16:io], "hdr16":b[:16],"io8":b[-20:-12],"il4":b[-12:-8]}
names=list(cands)
for r in (1,2,3):
  for combo in itertools.permutations(names,r):
    d=b"".join(cands[x] for x in combo)
    if zlib.crc32(d)==tgt: print("direct",combo)
    for seed in (0,0xffffffff):
        pass
# chained
for a in names:
  for c in names:
    v=zlib.crc32(cands[c],zlib.crc32(cands[a]))
    if v==tgt: print("chain",a,c)
for seed in range(0,1):
    pass
# seeds from small ints
for sd in list(range(0,70000))+[0xffffffff,0xdeadbeef,0x56504b89,0x4b5056,0x89504b56]:
    for nm in ("ix","D"):
        if zlib.crc32(cands[nm],sd)==tgt: print("seed",sd,nm)
print("done")
EOF'
```

<details><summary>output</summary>

```
direct ('hdr16', 'ix')
chain hdr16 ix
done
```
</details>

### Step 21: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat > /app/unvpack <<"EOF"
#!/usr/local/bin/python3
import os, sys, struct, zlib

MAGIC = b"\x89VPK"
FMAGIC = b"\x1aKPV"


class Corrupt(Exception):
    pass


def gv(b, i, end=None):
    if end is None:
        end = len(b)
    if i >= end:
        raise Corrupt("truncated varint")
    c = b[i]
    i += 1
    v = c & 127
    n = 1
    while c & 128:
        if i >= end:
            raise Corrupt("truncated varint")
        c = b[i]
        i += 1
        v = ((v + 1) << 7) | (c & 127)
        n += 1
        if n > 12:
            raise Corrupt("varint too long")
    return v, i


def lz_decode(p, expect):
    out = bytearray()
    i = 0
    n = len(p)
    try:
        while i < n:
            ctl = p[i]
            i += 1
            for bit in range(8):
                if i >= n:
                    break
                if not (ctl >> bit) & 1:
                    out.append(p[i])
                    i += 1
                else:
                    b = p[i]
                    i += 1
                    if b < 0x80:
                        L = (b >> 4) + 3
                        off = (((b & 15) << 8) | p[i]) + 1
                        i += 1
                    else:
                        L = (b & 0x7F) + 3
                        off = (p[i] | (p[i + 1] << 8)) + 1
                        i += 2
                        if b == 0xFF:
                            e, i = gv(p, i)
                            L += e
                    if off > len(out):
                        raise Corrupt("bad match offset")
                    if len(out) + L > expect:
                        raise Corrupt("overlong output")
                    if off >= L:
                        s = len(out) - off
                        out += out[s:s + L]
                    else:
                        for _ in range(L):
                            out.append(out[-off])
    except IndexError:
        raise Corrupt("truncated stream")
    return bytes(out)


def parse(b):
    if len(b) < 6 + 20 or b[:4] != MAGIC or b[-4:] != FMAGIC:
        raise Corrupt("bad magic")
    if b[4:6] != b"\x03\x00":
        raise Corrupt("bad version")
    io, il, crc = struct.unpack("<QII", b[-20:-4])
    if io + il + 20 != len(b) or io < 6:
        raise Corrupt("bad footer")
    i = 6
    base, i = gv(b, i, io)
    base = (base >> 1) ^ -(base & 1)
    rm, i = gv(b, i, io)
    rd, i = gv(b, i, io)
    rns = 0
    if rd & 1:
        rns, i = gv(b, i, io)
        if rns >= 10**9:
            raise Corrupt("bad ns")
    hdr_end = i
    ix = b[io:io + il]
    if zlib.crc32(ix, zlib.crc32(b[:hdr_end])) != crc:
        raise Corrupt("index checksum mismatch")
    root = dict(mode=rm ^ 0o755, ns=(base + (rd >> 1)) * 10**9 + rns)
    n, j = gv(ix, 0)
    ents = []
    pos = hdr_end
    for k in range(n):
        if j + 2 > len(ix):
            raise Corrupt("truncated index")
        t = ix[j]
        par = ix[j + 1]
        j += 2
        # parent may be multi-byte varint
        par, j = gv(ix, j - 1)
        nl, j = gv(ix, j)
        if j + nl > len(ix):
            raise Corrupt("truncated index")
        name = ix[j:j + nl]
        j += nl
        kind = t & 3
        e = dict(kind=kind, par=par, name=name)
        if kind == 0 or t & ~0x0F:
            raise Corrupt("bad entry type")
        if kind != 3:
            m, j = gv(ix, j)
            e["mode"] = m ^ (0o644 if kind == 1 else 0o755)
            if e["mode"] > 0o7777:
                raise Corrupt("bad mode")
        d, j = gv(ix, j)
        ns = 0
        if t & 4:
            ns, j = gv(ix, j)
            if ns >= 10**9:
                raise Corrupt("bad ns")
        e["ns"] = (base + d) * 10**9 + ns
        if kind == 3:
            l, j = gv(ix, j)
            if j + l > len(ix):
                raise Corrupt("truncated index")
            e["tgt"] = ix[j:j + l]
            j += l
        elif kind == 1:
            sz, j = gv(ix, j)
            cs = sz
            if t & 8:
                cs, j = gv(ix, j)
            e["size"] = sz
            e["comp"] = bool(t & 8)
            c = 0
            if sz:
                if j + 4 > len(ix):
                    raise Corrupt("truncated index")
                c = struct.unpack("<I", ix[j:j + 4])[0]
                j += 4
            e["crc"] = c
            e["off"] = pos
            e["cs"] = cs
            pos += cs
        elif t & 8:
            raise Corrupt("bad entry type")
        ents.append(e)
    if j != len(ix):
        raise Corrupt("trailing index bytes")
    if pos != io:
        raise Corrupt("data size mismatch")
    for e in ents:
        if e["par"] > len(ents):
            raise Corrupt("bad parent")
    return root, ents


def content(b, e):
    if e["size"] == 0:
        return b""
    raw = b[e["off"]:e["off"] + e["cs"]]
    if e["comp"]:
        pl, i = gv(raw, 0)
        if pl & 1:
            raise Corrupt("unknown codec")
        pl >>= 1
        if i + pl != len(raw):
            raise Corrupt("bad compressed length")
        data = lz_decode(raw[i:], e["size"])
    else:
        data = raw
    if len(data) != e["size"] or zlib.crc32(data) != e["crc"]:
        raise Corrupt("file checksum mismatch")
    return data


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: unvpack ARCHIVE OUTDIR\n")
        return 2
    try:
        b = open(sys.argv[1], "rb").read()
    except OSError as ex:
        sys.stderr.write("cannot read archive: %s\n" % ex)
        return 1
    out = os.fsencode(sys.argv[2])
    try:
        root, ents = parse(b)
        # resolve paths
        paths = [None] * (len(ents) + 1)
        paths[0] = out
        state = [0] * (len(ents) + 1)

        def resolve(k):
            if paths[k] is not None:
                return paths[k]
            if state[k] == 1:
                raise Corrupt("parent cycle")
            state[k] = 1
            e = ents[k - 1]
            pp = e["par"]
            if pp and ents[pp - 1]["kind"] != 2:
                raise Corrupt("parent not a directory")
            nm = e["name"]
            if not nm or b"/" in nm or b"\0" in nm or nm in (b".", b".."):
                raise Corrupt("bad name")
            paths[k] = os.path.join(resolve(pp), nm)
            return paths[k]

        for k in range(1, len(ents) + 1):
            resolve(k)
        seen = set()
        for k in range(1, len(ents) + 1):
            if paths[k] in seen:
                raise Corrupt("duplicate name")
            seen.add(paths[k])
        datas = {}
        for k, e in enumerate(ents, 1):
            if e["kind"] == 1:
                datas[k] = content(b, e)
    except Corrupt as ex:
        sys.stderr.write("unvpack: archive is corrupt: %s\n" % ex)
        return 1
    except Exception as ex:
        sys.stderr.write("unvpack: archive is corrupt (%s)\n" % ex)
        return 1
    os.mkdir(out)
    dirs = [(0, out, root)]
    for k, e in enumerate(ents, 1):
        p = paths[k]
        if e["kind"] == 2:
            os.mkdir(p)
            dirs.append((k, p, e))
        elif e["kind"] == 1:
            with open(p, "wb") as f:
                f.write(datas.pop(k))
            os.chmod(p, e["mode"])
            os.utime(p, ns=(e["ns"], e["ns"]))
        else:
            os.symlink(e["tgt"], p)
            os.utime(p, ns=(e["ns"], e["ns"]), follow_symlinks=False)
    for k, p, e in sorted(dirs, key=lambda x: -x[1].count(b"/")):
        os.chmod(p, e["mode"])
        os.utime(p, ns=(e["ns"], e["ns"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
EOF
chmod +x /app/unvpack; which python3'
```

<details><summary>output</summary>

```
/usr/local/bin/python3
```
</details>

### Step 22: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat > /w/cmp.py <<"EOF"
import os,sys,stat
def walk(root):
    res={}
    def rec(p,rel):
        st=os.lstat(p)
        m=stat.S_IMODE(st.st_mode)
        if stat.S_ISLNK(st.st_mode): v=("l",os.readlink(p),st.st_mtime_ns)
        elif stat.S_ISDIR(st.st_mode):
            v=("d",m,st.st_mtime_ns)
            for n in os.listdir(p): rec(os.path.join(p,n),rel+b"/"+n if isinstance(n,bytes) else rel+"/"+n)
        else: v=("f",m,st.st_mtime_ns,open(p,"rb").read())
        res[rel]=v
    rec(root,b"" if isinstance(root,bytes) else "")
    return res
def same(a,b):
    A=walk(os.fsencode(a)); B=walk(os.fsencode(b))
    bad=[k for k in set(A)|set(B) if A.get(k)!=B.get(k)]
    return bad,A,B
if __name__=="__main__":
    bad,A,B=same(sys.argv[1],sys.argv[2])
    print("BAD" if bad else "OK",[ (k,A.get(k,"-")[:3] if k in A else None, B.get(k,"-")[:3] if k in B else None) for k in bad[:5]])
EOF
cd /w; for n in config data mixed notes webapp; do rm -rf o_$n; /app/unvpack /app/samples/$n.vp o_$n; echo rc=$?; python3 cmp.py /app/samples/$n o_$n; done'
```

<details><summary>output</summary>

```
rc=0
OK []
rc=0
OK []
rc=0
OK []
rc=0
OK []
rc=0
OK []
```
</details>

### Step 23: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cat > /w/fuzz.py <<"EOF"
import os,sys,random,subprocess,shutil,string
sys.path.insert(0,"/w")
from cmp import same
def rndname(r):
    k=r.random()
    if k<0.5: return "".join(r.choice(string.ascii_letters+string.digits+"._- ") for _ in range(r.randint(1,12))).encode().strip(b" ") or b"x"
    if k<0.7: return bytes(r.randrange(1,256) for _ in range(r.randint(1,10))).replace(b"/",b"_")
    if k<0.8: return "é世界😀-%d"%r.randint(0,99) .encode()
    if k<0.9: return b"n"*r.randint(100,250)
    return b"-"+bytes(r.randint(1,50)%256 or 65 for _ in range(3)).replace(b"/",b"_")
def content(r):
    k=r.random()
    if k<0.1: return b""
    if k<0.25: return os.urandom(r.randint(1,3000))
    if k<0.4: return bytes(r.randint(1,50000))
    if k<0.6:
        w=[b"alpha",b"beta ",b"gamma\n",b"delta"]*3
        return b"".join(r.choice(w) for _ in range(r.randint(1,5000)))
    if k<0.7: return (os.urandom(r.randint(1,300))*r.randint(2,400))
    if k<0.8:
        blk=os.urandom(r.randint(100,2000)); return blk+os.urandom(r.randint(1,70000))+blk+os.urandom(30)+blk
    if k<0.9: return bytes(r.choice([0,1,2,255]) for _ in range(r.randint(1,20000)))
    return open("/app/bin/vpack","rb").read()[:r.randint(1,400000)]
def tstamp(r):
    k=r.random()
    s=r.choice([r.randint(0,2**31),r.randint(-10**9,0),r.randint(2**31,2**33),1700000000])
    return s*10**9+(r.randint(0,999999999) if r.random()<0.7 else 0)
def build(root,r):
    os.makedirs(root)
    dirs=[root]; items=[]
    for _ in range(r.randint(1,60)):
        par=r.choice(dirs); nm=rndname(r)
        p=os.path.join(os.fsencode(par),nm)
        if os.path.lexists(p): continue
        k=r.random()
        if k<0.2: os.mkdir(p); dirs.append(p); items.append((p,0))
        elif k<0.3:
            tg=r.choice([b"x",b"../"+rndname(r),b"/abs/"+rndname(r),b"a"*r.randint(100,300),bytes(r.randrange(1,256) for _ in range(5)).replace(b"/",b"-")])
            os.symlink(tg,p); items.append((p,1))
        elif k<0.35 and items:
            fs=[q for q,t in items if t==2]
            if fs: os.link(r.choice(fs),p); items.append((p,2)); continue
            open(p,"wb").write(b"z"); items.append((p,2))
        else:
            open(p,"wb").write(content(r)); items.append((p,2))
    for p,t in items:
        if t!=1 and r.random()<0.8: 
            os.chmod(p,r.choice([0o644,0o755,0o600,0o4755,0o2755,0o1777,0o000,0o444,r.randint(0,0o7777)]))
    allp=[(p,t) for p,t in items]
    for p,t in allp:
        try:
            ts=tstamp(r); os.utime(p,ns=(ts,ts),follow_symlinks=False)
        except Exception as e: pass
    for d in dirs[1:]+[dirs[0]]:
        pass
    # chmod dirs and set times last (deepest first)
    for d in sorted(dirs,key=lambda x:-len(x)):
        ts=tstamp(r); os.utime(d,ns=(ts,ts))
        if r.random()<0.7: os.chmod(d,r.choice([0o755,0o700,0o1777,0o2775,0o555,r.randint(0o400,0o7777)|0o700]))
def main(seed0,n):
    bad=0
    for s in range(seed0,seed0+n):
        r=random.Random(s)
        shutil.rmtree("/w/ft",ignore_errors=True); shutil.rmtree("/w/fo",ignore_errors=True)
        build(b"/w/ft",r)
        rc=subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"],capture_output=True)
        if rc.returncode: print(s,"pack fail",rc.stderr[:100]); continue
        u=subprocess.run(["/app/unvpack","/w/ft.vp","/w/fo"],capture_output=True)
        if u.returncode: print(s,"UNPACK FAIL",u.stderr[-300:]); bad+=1; continue
        b,A,B=same("/w/ft","/w/fo")
        if b: print(s,"DIFF",[(k,str(A.get(k))[:80],str(B.get(k))[:80]) for k in b[:3]]); bad+=1
    print("bad",bad,"of",n)
main(int(sys.argv[1]),int(sys.argv[2]))
EOF
cd /w; python3 fuzz.py 0 40'
```

<details><summary>output</summary>

```
Exit code 1
Traceback (most recent call last):
  File "/w/fuzz.py", line 73, in <module>
    main(int(sys.argv[1]),int(sys.argv[2]))
  File "/w/fuzz.py", line 65, in main
    build(b"/w/ft",r)
  File "/w/fuzz.py", line 32, in build
    par=r.choice(dirs); nm=rndname(r)
                           ^^^^^^^^^^
  File "/w/fuzz.py", line 8, in rndname
    if k<0.8: return "é世界😀-%d"%r.randint(0,99) .encode()
                                  ^^^^^^^^^^^^^^^^^^^^^^^
AttributeError: 'int' object has no attribute 'encode'
```
</details>

### Step 24: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; sed -i "s/return \"é世界😀-%d\"%r.randint(0,99) .encode()/return (\"é世界😀-%d\"%r.randint(0,99)).encode()/" fuzz.py; python3 fuzz.py 0 40 2>&1 | tail -20'
```

<details><summary>output</summary>

```
20 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
21 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
22 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
23 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
24 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
25 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
26 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
27 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
28 UNPACK FAIL b'unvpack: archive is corrupt: unknown codec\n'
29 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
30 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
32 UNPACK FAIL b'unvpack: archive is corrupt: bad compressed length\n'
33 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
34 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
35 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
36 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
37 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
38 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
39 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
bad 39 of 40
```
</details>

### Step 25: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys; sys.path.insert(0,"/w")
import random,shutil,subprocess
exec(open("fuzz.py").read().split("def main")[0])
for s in range(0,12):
    r=random.Random(s)
    shutil.rmtree("/w/ft",ignore_errors=True)
    build(b"/w/ft",r)
    subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"])
    b=open("/w/ft.vp","rb").read()
    print(s,b[:12].hex(),len(b))
EOF'
```

<details><summary>output</summary>

```
0 8956504b030286a5bb9b3987 243773
1 8956504b030083f4e3e2252d 81979
2 8956504b030285debd953b87 282722
3 8956504b0302868ac7f53787 75836
4 8956504b030284eeba8e1500 91052
5 8956504b030285d8fca35783 104740
6 8956504b030285afa6b3452d 81396
7 8956504b0302838ed3f64593 24481
8 8956504b030285a7a080138f 24389
9 8956504b030285ef84a60f00 102711
10 8956504b030285d4bfbf6583 70001
11 8956504b030280e0e98f762d 122864
```
</details>

### Step 26: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; sed -i "s/if b\[4:6\] != b\"\\\\x03\\\\x00\":/if b[4] != 3:/" /app/unvpack; grep -n "b\[4\]" /app/unvpack; python3 fuzz.py 0 40 2>&1 | tail -20'
```

<details><summary>output</summary>

```
79:    if b[4] != 3:
20 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
21 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
22 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
23 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
24 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
25 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
26 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
27 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
28 UNPACK FAIL b'unvpack: archive is corrupt: unknown codec\n'
29 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
30 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
32 UNPACK FAIL b'unvpack: archive is corrupt: bad compressed length\n'
33 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
34 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
35 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
36 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
37 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
38 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
39 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type\n'
bad 39 of 40
```
</details>

### Step 27: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; sed -i "s/raise Corrupt(\"bad entry type\")/raise Corrupt(\"bad entry type %d\" % t)/" /app/unvpack; python3 fuzz.py 0 40 2>&1 | sort | uniq -c -f1 | head'
```

<details><summary>output</summary>

```
      1 0 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 149\n'
      1 1 UNPACK FAIL b'unvpack: archive is corrupt: bad compressed length\n'
      1 10 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
      2 11 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 139\n'
      1 13 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 133\n'
      1 14 UNPACK FAIL b'unvpack: archive is corrupt: bad compressed length\n'
      1 15 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 132\n'
      1 16 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 136\n'
      1 17 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 139\n'
      1 18 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 132\n'
```
</details>

### Step 28: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e7.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
def t(label,setup):
    b=pack(setup); print(label,hx(b[4:6]),len(b))
def mk(d,name,data):
    open(d+"/"+name,"wb").write(data)
t("plain",lambda d:mk(d,"a",b"x"))
t("hardlink",lambda d:(mk(d,"a",b"x"),os.link(d+"/a",d+"/b")))
t("dupcontent",lambda d:(mk(d,"a",b"xyz"*1000),mk(d,"b",b"xyz"*1000)))
t("dup small",lambda d:(mk(d,"a",b"xyz"),mk(d,"b",b"xyz")))
t("big 100k rand",lambda d:mk(d,"a",os.urandom(100000)))
t("big 100k zeros",lambda d:mk(d,"a",bytes(100000)))
t("longname",lambda d:mk(d,"a"*200,b"x"))
t("utf8",lambda d:mk(d,"é",b"x"))
t("highbyte",lambda d:mk(d.encode() and d,"\udcff",b"x"))
t("sym",lambda d:os.symlink("a",d+"/l"))
t("fifo",lambda d:os.mkfifo(d+"/p"))
t("emptydir",lambda d:os.mkdir(d+"/e"))
t("setuid",lambda d:(mk(d,"a",b"x"),os.chmod(d+"/a",0o4755)))
t("zero",lambda d:mk(d,"a",b""))
t("two",lambda d:(mk(d,"a",b"x"),mk(d,"b",b"y")))
EOF
python3 e7.py'
```

<details><summary>output</summary>

```
vpack: warning: skipping /w/x/p: unsupported file type
plain 03 00 56
hardlink 03 00 69
dupcontent 03 00 81
dup small 03 00 71
big 100k rand 03 00 100057
big 100k zeros 03 00 108
longname 03 00 256
utf8 03 00 57
highbyte 03 00 56
sym 03 00 51
fifo 03 00 39
emptydir 03 00 50
setuid 03 00 57
zero 03 00 51
two 03 00 73
```
</details>

### Step 29: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys; sys.path.insert(0,"/w")
import random,shutil,subprocess,os
exec(open("fuzz.py").read().split("def main")[0])
for s in range(0,12):
    r=random.Random(s)
    shutil.rmtree("/w/ft",ignore_errors=True)
    build(b"/w/ft",r)
    subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"])
    b=open("/w/ft.vp","rb").read()
    n=0;tot=0;mx=0;nl=0;high=0
    for dp,dn,fn in os.walk(b"/w/ft"):
        for x in dn+fn:
            n+=1; nl=max(nl,len(x)); high+= any(c>127 for c in x)
        for x in fn:
            p=os.path.join(dp,x)
            if os.path.isfile(p) and not os.path.islink(p): sz=os.path.getsize(p); tot+=sz; mx=max(mx,sz)
    print(s,b[5],n,tot,mx,nl,high)
EOF'
```

<details><summary>output</summary>

```
0 2 55 1103200 67808 242 15
1 0 8 103430 58720 242 2
2 2 56 1041988 67808 150 19
3 2 16 282558 67808 233 1
4 2 16 255078 69894 201 3
5 2 40 462780 75468 216 11
6 2 51 508335 70052 240 13
7 2 21 273830 67808 219 8
8 2 15 196251 67808 171 2
9 2 30 542444 67808 226 9
10 2 37 522688 98670 238 11
11 2 29 308070 67808 215 8
```
</details>

### Step 30: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e8.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
def t(label,setup):
    b=pack(setup); D,ix,f=parts(b); print(label,hx(b[4:6]),len(b),hx(ix[:40]))
def mk(d,name,data):
    open(d+"/"+name,"wb").write(data)
t("rand 70000",lambda d:mk(d,"a",os.urandom(70000)))
t("rand 66000",lambda d:mk(d,"a",os.urandom(66000)))
t("rand 65536",lambda d:mk(d,"a",os.urandom(65536)))
t("rand 65535",lambda d:mk(d,"a",os.urandom(65535)))
t("zeros 70000",lambda d:mk(d,"a",bytes(70000)))
t("text 70000",lambda d:mk(d,"a",(b"hello world %d\n"*1)*1 and b"".join(b"line %d\n"%i for i in range(9000))))
t("text 20000",lambda d:mk(d,"a",b"".join(b"line %d\n"%i for i in range(2500))))
t("2 files 40k",lambda d:(mk(d,"a",os.urandom(40000)),mk(d,"b",os.urandom(40000))))
EOF
python3 e8.py'
```

<details><summary>output</summary>

```
rand 70000 03 00 70057 01 05 00 01 61 00 00 81 e0 b4 92 1d 83 a1 70 7c 80 ec 98
rand 66000 03 00 66057 01 05 00 01 61 00 00 81 eb cf a4 66 83 82 50 d4 5d 44 a7
rand 65536 03 00 65593 01 05 00 01 61 00 00 81 f3 d2 ce 6c 82 ff 00 9a bb 04 3d
rand 65535 03 00 65592 01 05 00 01 61 00 00 82 83 88 a9 24 82 fe 7f 81 f1 68 24
zeros 70000 03 00 94 01 0d 00 01 61 00 00 82 8d ca ab 53 83 a1 70 24 dc c8 a9 a6
text 70000 03 00 29407 01 0d 00 01 61 00 00 82 9f b8 f4 52 84 b5 3a 80 e4 23 5b 4c 7f d1
text 20000 03 00 8207 01 0d 00 01 61 00 00 82 b4 bc 97 5b 80 b9 52 be 54 f7 61 19 d7
2 files 40k 03 00 80072 02 05 00 01 61 00 00 9c ef e1 09 81 b7 40 de 01 d6 20 05 00 01 62 00 00 9d 9f a4 0a 81 b7 40 fc 93 52 55
```
</details>

### Step 31: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e9.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
V=open("/app/bin/vpack","rb").read()
def t(label,data):
    def s(d): open(d+"/a","wb").write(data)
    b=pack(s); D,ix,f=parts(b); print(label,hx(b[4:6]),len(b),hx(ix[:30]))
for n in [1000,5000,20000,40000,67808]:
    t(n,V[:n])
EOF
python3 e9.py'
```

<details><summary>output</summary>

```
1000 03 00 499 01 0d 00 01 61 00 00 82 84 f1 20 86 68 82 3b 52 a7 7a a7
5000 03 00 2372 01 0d 00 01 61 00 00 88 b0 90 38 a6 08 91 0c 1d a7 50 4c
20000 03 00 8426 01 0d 00 01 61 00 00 96 b7 b0 67 80 9b 20 c0 31 66 0d 6d 59
40000 03 00 8433 01 0d 00 01 61 00 00 a4 81 d4 67 81 b7 40 c0 38 97 33 3d 4e
67808 03 00 9362 01 0d 00 01 61 00 00 a8 98 9a 2d 83 90 60 c7 59 0b 64 e5 ca
```
</details>

### Step 32: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys,os,subprocess,shutil
sys.path.insert(0,"/w")
import random
exec(open("fuzz.py").read().split("def main")[0])
r=random.Random(8)
shutil.rmtree("/w/ft",ignore_errors=True); build(b"/w/ft",r)
def flag():
    subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"],capture_output=True)
    return open("/w/ft.vp","rb").read()[5]
print(flag())
# try removing each entry
changed=True
while changed:
    changed=False
    for dp,dn,fn in sorted(os.walk(b"/w/ft",topdown=False)):
        for x in fn+dn:
            p=os.path.join(dp,x)
            bak=None
            if os.path.islink(p) or os.path.isfile(p):
                os.rename(p,"/w/bak_f")
                if flag()==2: changed=True; os.remove("/w/bak_f")
                else: os.rename("/w/bak_f",p)
            elif os.path.isdir(p) and not os.listdir(p):
                os.rename(p,"/w/bak_d")
                if flag()==2: changed=True; os.rmdir("/w/bak_d")
                else: os.rename("/w/bak_d",p)
print(subprocess.run("ls -laR --time-style=full-iso /w/ft",shell=True,capture_output=True,text=True).stdout)
EOF'
```

<details><summary>output</summary>

```
Exit code 1
Traceback (most recent call last):
  File "<stdin>", line 27, in <module>
  File "/usr/local/lib/python3.12/subprocess.py", line 550, in run
2
    stdout, stderr = process.communicate(input, timeout=timeout)
                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/local/lib/python3.12/subprocess.py", line 1209, in communicate
    stdout, stderr = self._communicate(input, endtime, timeout)
                     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/local/lib/python3.12/subprocess.py", line 2153, in _communicate
    stdout = self._translate_newlines(stdout,
             ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/local/lib/python3.12/subprocess.py", line 1086, in _translate_newlines
    data = data.decode(encoding, errors)
           ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
UnicodeDecodeError: 'utf-8' codec can't decode byte 0xdc in position 291: invalid continuation byte
```
</details>

### Step 33: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import os,stat
for dp,dn,fn in os.walk(b"/w/ft"):
    for x in dn+fn:
        p=os.path.join(dp,x); st=os.lstat(p)
        print(p[:60],oct(st.st_mode),st.st_size,st.st_nlink, st.st_mtime_ns)
EOF'
```

<details><summary>output</summary>

```
b'/w/ft/5' 0o40755 4096 2 1794925992337388003
b'/w/ft/4\xdcX+' 0o40700 4096 3 1794925992239388003
b'/w/ft/-\t\r.' 0o41777 4096 3 1700000000871866444
b'/w/ft/AZd6.6X_y' 0o100644 20853 1 2814328896670845658
b'/w/ft/amaPgk3ZKlC' 0o106622 0 1 1700000000878415225
b'/w/ft/5/sbwr' 0o104755 5542 1 45019316000000000
b'/w/ft/5/X3LrqBp48_ql' 0o101777 14019 3 -293025718452784599
b'/w/ft/5/\xab\xb4\x0f\xa7\xc0\xdd' 0o102755 67808 1 -641412852000000000
b'/w/ft/5/nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn' 0o101777 14019 3 -293025718452784599
b'/w/ft/4\xdcX+/n6Vlkj6jJW9' 0o47731 4096 2 263756872000000000
b'/w/ft/4\xdcX+/h' 0o100644 11294 1 234810071521502237
b'/w/ft/4\xdcX+/BAXbh0V' 0o101777 14019 3 -293025718452784599
b'/w/ft/-\t\r./ysyM3m5' 0o44755 4096 2 1794925992202388003
b'/w/ft/-\t\r./ysyM3m5/tUef' 0o100644 45764 1 -847519817763136229
b'/w/ft/-\t\r./ysyM3m5/QQOAfUM' 0o105673 2933 1 2627234081000000000
```
</details>

### Step 34: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e10.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
def t(label,setup):
    b=pack(setup); D,ix,f=parts(b); print(label,hx(b[4:6]),len(b),hx(b[:20]),hx(ix[:30]))
def f(sec,ns=0):
    def s(d):
        open(d+"/a","wb").write(b"x"); os.utime(d+"/a",ns=(0,sec*10**9+ns))
    return s
t("neg",f(-5)); t("2^31+",f(2**31+5)); t("2^32+",f(2**32+5)); t("2^40",f(2**40)); t("ns",f(5,999999999))
def hl(d):
    open(d+"/a","wb").write(os.urandom(5000)); os.link(d+"/a",d+"/b"); os.link(d+"/a",d+"/c")
t("hl3",hl)
t("hl3b",lambda d:(open(d+"/a","wb").write(b"x"),os.link(d+"/a",d+"/b"),os.link(d+"/a",d+"/c")))
EOF
python3 e10.py'
```

<details><summary>output</summary>

```
neg 03 00 51 89 56 50 4b 03 00 09 00 8c ae e2 95 71 81 bb a8 fd 68 78 01 01 01 00 01 61 00 00 01 83 16 dc 8c
2^31+ 03 00 55 89 56 50 4b 03 00 8c ae e2 95 66 00 01 81 c5 e7 e0 68 78 01 01 01 00 01 61 00 80 a7 8d b3 52 01 83 16 dc 8c
2^32+ 03 00 55 89 56 50 4b 03 00 8c ae e2 95 66 00 01 81 cd b8 a8 68 78 01 01 01 00 01 61 00 88 a7 8d b3 52 01 83 16 dc 8c
2^40 03 00 55 89 56 50 4b 03 00 8c ae e2 95 66 00 01 81 d6 bf fe 28 78 01 01 01 00 01 61 00 b0 a7 8d b3 4c 01 83 16 dc 8c
ns 03 00 56 89 56 50 4b 03 00 0a 00 8c ae e2 95 5d 81 de 80 ad 09 78 01 01 05 00 01 61 00 00 82 db ea 92 7f 01 83 16 dc 8c
hl3 03 00 5084 89 56 50 4b 03 00 8c ae e2 95 66 00 01 81 e2 87 ba 08 66 f2 03 05 00 01 61 00 00 81 e2 87 ba 08 a6 08 c9 23 ed dc 15 00 01 62 00 00 81 e2 87 ba 08 a6
hl3b 03 00 82 89 56 50 4b 03 00 8c ae e2 95 66 00 01 81 e6 9e 92 68 78 03 03 05 00 01 61 00 00 81 e6 9e 92 68 01 83 16 dc 8c 15 00 01 62 00 00 81 e6 9e 92 68 01 00
```
</details>

### Step 35: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e11.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
def t(label,setup):
    b=pack(setup); D,ix,f=parts(b); print(label,hx(b[4:6]),"\n H:",hx(b[6:16]),"\n IX:",hx(ix), "\n D:",hx(D[16:]))
t("hl3b",lambda d:(open(d+"/a","wb").write(b"x"),os.link(d+"/a",d+"/b"),os.link(d+"/a",d+"/c")))
t("hl2 + other",lambda d:(open(d+"/a","wb").write(b"xy"),os.link(d+"/a",d+"/c"),open(d+"/b","wb").write(b"q"),os.mkdir(d+"/dd"),os.link(d+"/a",d+"/dd/z")))
EOF
python3 e11.py'
```

<details><summary>output</summary>

```
hl3b 03 00 
 H: 8c ae e2 95 7a 00 01 80 92 85 
 IX: 03 05 00 01 61 00 00 80 92 85 c0 2d 01 83 16 dc 8c 15 00 01 62 00 00 80 92 85 c0 2d 01 00 15 00 01 63 00 00 80 92 85 c0 2d 01 00 
 D: c0 2d 78
hl2 + other 03 00 
 H: 8c ae e2 95 7a 00 01 80 a3 d7 
 IX: 05 05 00 01 61 00 00 80 a1 a6 d0 2d 02 99 28 e6 8f 05 00 01 62 00 00 80 a3 d7 e6 6d 01 27 ae 00 f5 15 00 01 63 00 00 80 a1 a6 d0 2d 02 00 06 00 02 64 64 00 00 80 a3 d7 e6 6d 15 04 01 7a 00 00 80 a1 a6 d0 2d 02 00 
 D: e6 6d 78 79 71
```
</details>

### Step 36: Bash

```bash
cd /w; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
s=open("/app/unvpack").read()
s=s.replace("""        if kind == 0 or t & ~0x0F:""","""        if kind == 0 or t & ~0x1F or (t & 16 and kind != 1):""")
s=s.replace("""            sz, j = gv(ix, j)
            cs = sz
            if t & 8:
                cs, j = gv(ix, j)
""","""            sz, j = gv(ix, j)
            cs = sz
            if t & 16:
                if t & 8:
                    raise Corrupt("bad entry type")
                tg, j = gv(ix, j)
                if tg >= k or ents[tg]["kind"] != 1 or ents[tg].get("link") is not None or ents[tg]["size"] != sz:
                    raise Corrupt("bad hardlink")
                e["link"] = tg
                e["size"] = sz
                e["comp"] = False
                e["crc"] = 0
                e["off"] = 0
                e["cs"] = 0
                ents.append(e)
                continue
            if t & 8:
                cs, j = gv(ix, j)
""")
s=s.replace("""            if e["kind"] == 1:
                datas[k] = content(b, e)""","""            if e["kind"] == 1 and e.get("link") is None:
                datas[k] = content(b, e)""")
s=s.replace("""            with open(p, "wb") as f:
                f.write(datas.pop(k))""","""            with open(p, "wb") as f:
                f.write(datas[e["link"] + 1] if e.get("link") is not None else datas[k])""")
s=s.replace("""        e = dict(kind=kind, par=par, name=name)""","""        e = dict(kind=kind, par=par, name=name, link=None)""")
open("/app/unvpack","w").write(s)
EOF
python3 fuzz.py 0 40 2>&1 | tail -15'
```

<details><summary>output</summary>

```
(eval):cd:1: no such file or directory: /w
25 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 137\n'
26 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 146\n'
27 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 143\n'
28 UNPACK FAIL b'unvpack: archive is corrupt: unknown codec\n'
29 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
30 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 140\n'
32 UNPACK FAIL b'unvpack: archive is corrupt: bad compressed length\n'
33 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
34 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
35 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
36 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 136\n'
37 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 144\n'
38 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 142\n'
39 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 132\n'
bad 39 of 40
```
</details>

### Step 37: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > dbg.py <<EOF
import sys,os,random,shutil,subprocess
sys.path.insert(0,"/w"); sys.path.insert(0,"/app")
exec(open("/w/fuzz.py").read().split("def main")[0])
import importlib.machinery,importlib.util
ld=importlib.machinery.SourceFileLoader("unv","/app/unvpack"); spec=importlib.util.spec_from_loader("unv",ld); u=importlib.util.module_from_spec(spec); ld.exec_module(u)
seed=int(sys.argv[1])
r=random.Random(seed); shutil.rmtree("/w/ft",ignore_errors=True); build(b"/w/ft",r)
subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"])
b=open("/w/ft.vp","rb").read()
root,ents=u.parse(b)
for k,e in enumerate(ents,1):
    if e["kind"]==1:
        try: u.content(b,e)
        except Exception as ex:
            raw=b[e["off"]:e["off"]+e["cs"]]
            print(k,ex,e["name"][:20],e["size"],e["cs"],e["comp"],raw[:16].hex(), e["link"])
EOF
python3 dbg.py 1'
```

<details><summary>output</summary>

```
1 bad compressed length b'-2-yMK' 16558 6368 True e13600ff00ff01ffff0102800200ff00 None
```
</details>

### Step 38: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat >> dbg.py <<EOF
e=[e for e in ents if e["kind"]==1 and e["size"]==16558][0]
raw=b[e["off"]:e["off"]+e["cs"]]
print(raw[6290:6368].hex())
print(len(raw))
EOF
python3 dbg.py 1'
```

<details><summary>output</summary>

```
1 bad compressed length b'-2-yMK' 16558 6368 True e13600ff00ff01ffff0102800200ff00 None
42272b260b41b8222800258002ff382f351220415139302235ff202748d6ff23f32190227921ff226639e9362841e3ff22ef334542da225d269840f9130b3adb3f23fc24b6488d363c107256b7ff
6368
```
</details>

### Step 39: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
s=open("/app/unvpack").read()
a=s.index("def lz_decode(p, expect):")
b=s.index("def parse(b):")
new_lz=s[a:b].replace("def lz_decode(p, expect):\n    out = bytearray()\n","def lz_decode(p, expect, out):\n").replace("    return bytes(out)\n","    return out\n")
s=s[:a]+new_lz+s[b:]
old=s[s.index("    if e[\"comp\"]:\n        pl, i = gv(raw, 0)"):s.index("    if len(data) != e[\"size\"]")]
new="""    if e["comp"]:
        out = bytearray()
        i = 0
        while i < len(raw):
            v, i = gv(raw, i)
            ln = v >> 1
            if i + ln > len(raw):
                raise Corrupt("bad block length")
            if v & 1:
                out += raw[i:i + ln]
            else:
                lz_decode(raw[i:i + ln], e["size"], out)
            i += ln
        data = bytes(out)
    else:
        data = raw
"""
s=s.replace(old,new)
open("/app/unvpack","w").write(s)
EOF
grep -n "expect" /app/unvpack; python3 fuzz.py 0 40 2>&1 | tail -15'
```

<details><summary>output</summary>

```
33:def lz_decode(p, expect, out):
62:                    if len(out) + L > expect:
23 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 150\n'
24 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 146\n'
25 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 137\n'
26 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 146\n'
27 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 143\n'
29 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
30 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 140\n'
33 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
34 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
35 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 141\n'
36 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 136\n'
37 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 144\n'
38 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 142\n'
39 UNPACK FAIL b'unvpack: archive is corrupt: bad entry type 132\n'
bad 35 of 40
```
</details>

### Step 40: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys,os,random,shutil,subprocess
sys.path.insert(0,"/w")
from h import *
exec(open("/w/fuzz.py").read().split("def main")[0])
r=random.Random(8); shutil.rmtree("/w/ft",ignore_errors=True); build(b"/w/ft",r)
subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"])
b=open("/w/ft.vp","rb").read()
D,ix,f=parts(b)
print(hx(b[:30])); print(hx(ix[:200])); print(len(ix), hx(f))
EOF'
```

<details><summary>output</summary>

```
89 56 50 4b 03 02 85 a7 a0 80 13 8f 12 80 ff e2 e4 0d 80 d0 a8 c8 72 87 22 20 62 65 74 61
83 07 84 5a 00 0f 06 00 04 2d 09 0d 2e 00 83 12 88 bd df a1 4a 82 00 9e dd bf 4c 06 01 07 79 00 73 79 4d 33 6d 35 8f 00 00 87 e9 85 cd 1a 80 a9 85 00 bf 29 09 02 07 51 51 4f 00 41 66 55 4d 93 1f 8b f7 00 f1 91 6b 95 75 83 13 f9 00 1d f1 57 0d 02 04 74 55 00 65 66 00 00 ef f8 81 1b 00 81 e4 44 16 24 df 42 5f 01 00 56 34 dc 58 2b 2d ca b0 00 87 79 81 82 ea c6 6a 0d 00 05 07 42 41 58 62 68 30 00 56 83 5b 81 87 b2 d0 13 00 81 83 f6 af 29 ec 43 a9 00 23 44 d3 67 3e 0d 05 01 00 68 00 83 83 8b 95 21 80 00 f7 d4 fb 1d d7 1e a1 32 00 d5 65 67 1b 02 05 0b 6e 00 36 56 6c 6b 6a 36 6a 4a 00 57 39 9b 34 83
369 c0 5d 00 00 00 00 00 00 71 01 00 00 89 83 54 d1 1a 4b 50 56
```
</details>

### Step 41: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e12.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
for n in [1,2,5,10,14,15,16,17,20,30,64,100,127,128,129,200]:
    def s(d):
        for i in range(n): open(d+"/f%03d"%i,"wb").write(b"x%d"%i)
    b=pack(s); D,ix,f=parts(b); print(n,hx(b[4:6]),len(b),hx(ix[:8]))
EOF
python3 e12.py'
```

<details><summary>output</summary>

```
1 03 00 60 01 05 00 04 66 30 30 30
2 03 00 81 02 05 00 04 66 30 30 30
5 03 00 144 05 05 00 04 66 30 30 30
10 03 00 249 0a 05 00 04 66 30 30 30
14 03 00 329 0e 05 00 04 66 30 30 30
15 03 00 355 0f 05 00 04 66 30 30 30
16 03 00 377 10 05 00 04 66 30 30 30
17 03 00 393 11 05 00 04 66 30 30 30
20 03 00 459 14 05 00 04 66 30 30 30
30 03 02 596 83 33 86 34 00 1e 05 00
64 03 02 1207 88 3a 8e 2e 00 40 05 00
100 03 02 1833 8d 62 96 3a 00 64 05 00
127 03 02 2365 91 57 9d 0a 00 7f 05 00
128 03 02 2340 91 5d 9c 50 00 80 00 05
129 03 02 2388 91 7c 9d 28 00 80 01 05
200 03 02 3685 9c 06 ad 14 00 80 48 05
```
</details>

### Step 42: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e13.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
for n in [21,22,23,24,25,26,28,30]:
    def s(d):
        for i in range(n): open(d+"/f%03d"%i,"wb").write(b"x%d"%i)
    b=pack(s); D,ix,f=parts(b); print(n,hx(b[4:6]),len(ix),hx(ix[:14]))
n=30
def s(d):
    for i in range(n): open(d+"/f%03d"%i,"wb").write(b"x%d"%i)
b=pack(s); D,ix,f=parts(b); print(hx(ix))
EOF
python3 e13.py'
```

<details><summary>output</summary>

```
21 03 00 383 15 05 00 04 66 30 30 30 00 00 b9 ee e9 00
22 03 00 419 16 05 00 04 66 30 30 30 00 00 80 a5 ce bd
23 03 00 437 17 05 00 04 66 30 30 30 00 00 81 97 cc 95
24 03 00 448 18 05 00 04 66 30 30 30 00 00 80 ec a0 98
25 03 00 459 19 05 00 04 66 30 30 30 00 00 82 a6 a8 ad
26 03 00 495 1a 05 00 04 66 30 30 30 00 00 80 e9 9e bd
28 03 02 443 83 06 85 6e 00 1c 05 00 04 66 30 30 30 00
30 03 02 470 83 33 86 24 00 1e 05 00 04 66 30 30 30 00
83 34 86 30 00 1e 05 00 04 66 30 30 30 00 00 00 81 e9 9d e9 6b 02 50 ad d1 e6 80 30 12 31 10 12 a4 80 b1 57 02 3b e1 e1 f7 30 12 02 32 00 12 ea c3 d9 4e 02 81 28 b0 e8 6e 30 12 33 00 12 f4 ec 80 d3 7b 02 17 80 ef 19 30 12 02 34 00 12 f5 91 fb 57 02 b4 28 15 8b 87 30 12 35 00 12 f7 b3 80 9f 57 02 22 25 8c f0 30 12 02 36 00 12 fa b0 e6 6f 02 98 28 74 85 69 30 12 37 00 12 fe b1 40 cc 1f 02 0e 44 82 40 97 38 01 00 12 ff 80 84 4d 02 9f 59 14 3d 8e 30 12 39 10 12 a9 cf 19 a0 02 09 69 3a f9 20 12 31 00 bd 00 82 8a cc e1 1a 03 e4 d7 0c 27 45 30 12 00 bd 82 a5 d8 d2 c0 43 03 72 e7 20 32 30 12 00 bd 00 82 ac bd c4 79 03 c8 b6 0c 29 ab 30 12 00 bd 82 b0 de 84 c0 21 03 5e 86 2e dc 30 12 00 bd 00 82 b1 d1 b9 59 03 fd 13 0c 4a 42 30 12 00 bd 82 b7 ef cd c0 38 03 6b 23 4d 35 30 12 00 bd 00 82 b9 cc bd 73 03 d1 72 0c 44 ac 30 12 00 bd 82 d6 ef f7 40 33 03 47 42 43 db 30 12 38 00 00 01 95 9e a0 7a 03 d6 08 5f fc 4b 30 11 39 00 01 ad 00 e3 8e 20 03 40 6f fb 3c 01 20 11 32 30 00 01 ae ee fe 20 46 03 27 84 0a 31 52 32 31 00 00 01 af 9c ae 15 03 b1 24 b4 0d 31 51 32 32 00 11 da c0 20 76 03 0b e5 04 31 9c 32 33 00 00 01 b1 a4 d0 7f 03 9d 04 d5 03 31 9b 32 34 00 01 dd 80 d0 ba 43 03 3e 40 67 31 4e 00 32 35 00 01 80 bb 8b ac 20 43 03 a8 70 60 31 4e 32 36 01 00 12 bc cb 88 1e 03 12 21 12 69 31 9a 32 37 00 12 be c6 a5 a0 3b 03 84 11 6e 31 9a 32 00 b6 00 80 c2 f7 ed 2c 03 15 0c 0c d1 60 30 12 00 b7 80 c9 da fb 00 74 03 83 3c d6 17
```
</details>

### Step 43: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
s=open("/app/unvpack").read()
s=s.replace("""    if b[4] != 3:""","""    if b[4] != 3 or b[5] & ~2:""")
old="""    ix = b[io:io + il]
    if zlib.crc32(ix, zlib.crc32(b[:hdr_end])) != crc:
        raise Corrupt("index checksum mismatch")
"""
new="""    ix = b[io:io + il]
    hc = zlib.crc32(b[:hdr_end])
    if zlib.crc32(ix, hc) != crc:
        raise Corrupt("index checksum mismatch")
    if b[5] & 2:
        ix = decode_blocks(ix, 1)
"""
assert old in s
s=s.replace(old,new)
# generic block decoder
s=s.replace("def parse(b):","""def decode_blocks(raw, with_len=0):
    i = 0
    out = bytearray()
    want = None
    if with_len:
        want, i = gv(raw, 0)
    while i < len(raw):
        v, i = gv(raw, i)
        ln = v >> 1
        if i + ln > len(raw):
            raise Corrupt("bad block length")
        if v & 1:
            out += raw[i:i + ln]
        else:
            lz_decode(raw[i:i + ln], want if want is not None else 1 << 40, out)
        i += ln
    if want is not None and len(out) != want:
        raise Corrupt("index length mismatch")
    return bytes(out)


def parse(b):""",1)
open("/app/unvpack","w").write(s)
EOF
python3 fuzz.py 0 40 2>&1 | tail -15'
```

<details><summary>output</summary>

```
20 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
23 UNPACK FAIL b'unvpack: archive is corrupt: bad version\n'
bad 2 of 40
```
</details>

### Step 44: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; sed -i "s/or b\[5\] & ~2:/or b[5] \& ~7:/" /app/unvpack; python3 fuzz.py 20 1 2>&1|tail -3; python3 - <<EOF
import sys,os,random,shutil,subprocess
sys.path.insert(0,"/w")
from h import *
exec(open("/w/fuzz.py").read().split("def main")[0])
for s in (20,23):
    r=random.Random(s); shutil.rmtree("/w/ft",ignore_errors=True); build(b"/w/ft",r)
    subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"])
    b=open("/w/ft.vp","rb").read(); print(s,b[4:6].hex())
EOF'
```

<details><summary>output</summary>

```
20 UNPACK FAIL b'unvpack: archive is corrupt: varint too long\n'
bad 1 of 1
20 0303
23 0303
```
</details>

### Step 45: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e14.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
def t(label,setup):
    b=pack(setup); print(label,hx(b[4:6]),len(b))
def mk(d,name,data=b"x"): open(d+"/"+name if isinstance(name,str) else os.fsencode(d)+b"/"+name,"wb").write(data)
t("nonutf8",lambda d:mk(d,b"\xff\xfe"))
t("neg",lambda d:(mk(d,"a"),os.utime(d+"/a",ns=(0,-5*10**9))))
t("far",lambda d:(mk(d,"a"),os.utime(d+"/a",ns=(0,5*10**18))))
t("nsroot",lambda d:(os.utime(d,ns=(0,12345678912345))))
t("symdangling",lambda d:os.symlink("zz"*200,d+"/l"))
t("longname",lambda d:mk(d,"q"*250))
t("longname2",lambda d:mk(d,"q"*255))
t("nested",lambda d:(os.makedirs(d+"/a/b/c/d/e"),mk(d,"a/b/c/d/e/f")))
t("mode0",lambda d:(mk(d,"a"),os.chmod(d+"/a",0)))
t("sticky dir",lambda d:(os.mkdir(d+"/a"),os.chmod(d+"/a",0o1777)))
t("manyhl",lambda d:(mk(d,"a"),[os.link(d+"/a",d+"/h%d"%i) for i in range(5)]))
t("dupbig",lambda d:(mk(d,"a",b"hello world "*500),mk(d,"b",b"hello world "*500)))
t("empty dup",lambda d:(mk(d,"a",b""),mk(d,"b",b"")))
t("sym2dir",lambda d:(os.mkdir(d+"/dd"),os.symlink("dd",d+"/l")))
t("dotfiles",lambda d:(mk(d,".hidden"),mk(d,"..x")))
t("space",lambda d:(mk(d," a "),mk(d,"a\nb")))
t("mixed case",lambda d:(mk(d,"B"),mk(d,"a"),mk(d,"_")))
EOF
python3 e14.py'
```

<details><summary>output</summary>

```
nonutf8 03 00 57
neg 03 00 51
far 03 00 55
nsroot 03 00 37
symdangling 03 00 451
longname 03 00 306
longname2 03 00 311
nested 03 00 111
mode0 03 00 57
sticky dir 03 00 51
manyhl 03 00 126
dupbig 03 00 91
empty dup 03 00 63
sym2dir 03 00 64
dotfiles 03 00 77
space 03 00 73
mixed case 03 00 82
```
</details>

### Step 46: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys,os,subprocess,shutil,random
sys.path.insert(0,"/w")
exec(open("fuzz.py").read().split("def main")[0])
r=random.Random(20)
shutil.rmtree("/w/ft",ignore_errors=True); build(b"/w/ft",r)
def flag():
    subprocess.run(["/app/bin/vpack","pack","/w/ft","/w/ft.vp"],capture_output=True)
    return open("/w/ft.vp","rb").read()[5]
print(flag())
changed=True
while changed:
    changed=False
    for dp,dn,fn in sorted(os.walk(b"/w/ft",topdown=False)):
        for x in fn+dn:
            p=os.path.join(dp,x)
            if os.path.islink(p) or os.path.isfile(p):
                os.rename(p,"/w/bak_f")
                if flag()&1: changed=True; os.remove("/w/bak_f")
                else: os.rename("/w/bak_f",p)
            elif os.path.isdir(p) and not os.listdir(p):
                os.rename(p,"/w/bak_d")
                if flag()&1: changed=True; os.rmdir("/w/bak_d")
                else: os.rename("/w/bak_d",p)
for dp,dn,fn in os.walk(b"/w/ft"):
    for x in dn+fn:
        p=os.path.join(dp,x); st=os.lstat(p)
        print(p[:60],oct(st.st_mode),st.st_size,st.st_nlink, st.st_mtime_ns)
print(flag())
EOF'
```

<details><summary>output</summary>

```
3
b'/w/ft/-)\x18\x18' 0o40600 4096 2 1794926230417469002
b'/w/ft/N' 0o40755 4096 3 1794926229632469002
b'/w/ft/-)\x18\x18/\xc3\xa9\xe4\xb8\x96\xe7\x95\x8c\xf0\x9f\x98\x80-29' 0o120777 1 1 -633949137598462552
b'/w/ft/N/-\x13\x10\x0b' 0o44755 4096 2 1794926230441469002
b'/w/ft/N/-\x13\x10\x0b/\xc3\xa9\xe4\xb8\x96\xe7\x95\x8c\xf0\x9f\x98\x80-29' 0o120777 10 1 1700000000464848771
1
```
</details>

### Step 47: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > e15.py <<EOF
import sys,os; sys.path.insert(0,"/w")
from h import *
def t(label,setup):
    b=pack(setup); D,ix,f=parts(b); print(label,hx(b[4:6]),len(b),hx(ix))
def mk(d,name,data=b"x"): open(d+"/"+name,"wb").write(data)
t("dupname",lambda d:(os.mkdir(d+"/a"),os.mkdir(d+"/b"),mk(d,"a/x"),mk(d,"b/x")))
t("symunicode",lambda d:(os.symlink("t",d+"/é世界😀")))
t("sym unicode 2 dirs",lambda d:(os.mkdir(d+"/a"),os.mkdir(d+"/b"),os.symlink("t",d+"/a/é世"),os.symlink("t",d+"/b/é世")))
t("sym neg",lambda d:(os.symlink("t",d+"/l"),os.utime(d+"/l",ns=(0,-600*10**9),follow_symlinks=False)))
t("sym 0777 tgt",lambda d:(os.symlink("t",d+"/l")))
EOF
python3 e15.py'
```

<details><summary>output</summary>

```
dupname 03 01 92 01 01 78 04 06 00 01 61 00 00 80 cd d7 eb 4e 25 01 00 00 00 80 cd d7 eb 4e 01 83 16 dc 8c 06 00 01 62 00 00 80 ce d1 f4 4e 35 03 00 00 00 80 d3 b4 a1 4e 01 01
symunicode 03 00 62 01 07 00 0c c3 a9 e4 b8 96 e7 95 8c f0 9f 98 80 00 81 a6 e0 e1 29 01 74
sym unicode 2 dirs 03 01 90 01 05 c3 a9 e4 b8 96 04 06 00 01 61 00 00 81 d0 e8 c1 0e 27 01 00 00 81 d0 e8 c1 0e 01 74 06 00 01 62 00 00 81 d0 e8 c1 0e 27 03 00 00 81 d0 e8 c1 0e 01 74
sym neg 03 00 47 01 03 00 01 6c 00 01 74
sym 0777 tgt 03 00 51 01 07 00 01 6c 00 81 f4 8d 8e 0e 01 74
```
</details>

### Step 48: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
s=open("/app/unvpack").read()
s=s.replace("raise Corrupt(\"bad version\")","raise Corrupt(\"bad version\")")
s=s.replace("t & ~0x1F or","t & ~0x3F or")
old="""    n, j = gv(ix, 0)
    ents = []"""
new="""    j = 0
    table = []
    if b[5] & 1:
        tc, j = gv(ix, j)
        for _ in range(tc):
            l, j = gv(ix, j)
            if j + l > len(ix):
                raise Corrupt("truncated name table")
            table.append(ix[j:j + l])
            j += l
    n, j = gv(ix, j)
    ents = []"""
assert old in s; s=s.replace(old,new)
old="""        nl, j = gv(ix, j)
        if j + nl > len(ix):
            raise Corrupt("truncated index")
        name = ix[j:j + nl]
        j += nl
"""
new="""        if t & 0x20:
            ni, j = gv(ix, j)
            if ni >= len(table):
                raise Corrupt("bad name reference")
            name = table[ni]
        else:
            nl, j = gv(ix, j)
            if j + nl > len(ix):
                raise Corrupt("truncated index")
            name = ix[j:j + nl]
            j += nl
"""
assert old in s; s=s.replace(old,new)
s=s.replace("""if tg >= k or ents[tg]["kind"] != 1 or ents[tg].get("link") is not None or ents[tg]["size"] != sz:""","""if tg >= k or ents[tg]["kind"] != 1 or ents[tg]["size"] != sz:""")
s=s.replace("""f.write(datas[e["link"] + 1] if e.get("link") is not None else datas[k])""","""f.write(datas[root_of(k)])""")
s=s.replace("""            if e["kind"] == 1 and e.get("link") is None:
                datas[k] = content(b, e)""","""            if e["kind"] == 1 and e.get("link") is None:
                datas[k] = content(b, e)

        def root_of(k):
            while ents[k - 1].get("link") is not None:
                k = ents[k - 1]["link"] + 1
            return k""")
open("/app/unvpack","w").write(s)
EOF
sed -n "/datas = {}/,/os.mkdir(out)/p" /app/unvpack; python3 fuzz.py 0 60 2>&1 | tail -15'
```

<details><summary>output</summary>

```
        datas = {}
        for k, e in enumerate(ents, 1):
            if e["kind"] == 1 and e.get("link") is None:
                datas[k] = content(b, e)

        def root_of(k):
            while ents[k - 1].get("link") is not None:
                k = ents[k - 1]["link"] + 1
            return k
    except Corrupt as ex:
        sys.stderr.write("unvpack: archive is corrupt: %s\n" % ex)
        return 1
    except Exception as ex:
        sys.stderr.write("unvpack: archive is corrupt (%s)\n" % ex)
        return 1
    os.mkdir(out)
bad 0 of 60
```
</details>

### Step 49: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; time python3 fuzz.py 60 200 2>&1 | tail -8'
```

<details><summary>output</summary>

```
bad 0 of 200

real	0m47.914s
user	0m26.555s
sys	0m15.847s
```
</details>

### Step 50: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; cat > flip.py <<EOF
import sys,subprocess,shutil,os
for n in sys.argv[1:]:
    b=bytearray(open(n,"rb").read())
    und=[];bad=0
    def run(x):
        open("/w/fl.vp","wb").write(x); shutil.rmtree("/w/flo",ignore_errors=True)
        p=subprocess.run(["/app/unvpack","/w/fl.vp","/w/flo"],capture_output=True)
        return p
    for i in range(len(b)):
        for delta in (1,0x80):
            x=bytearray(b); x[i]^=delta
            p=run(bytes(x))
            if p.returncode==0: und.append((i,delta))
            elif p.returncode!=1 or b"corrupt" not in p.stderr: bad+=1; print("weird",i,p.returncode,p.stderr[-100:])
    for cut in range(0,len(b)):
        p=run(bytes(b[:cut]))
        if p.returncode!=1 or b"corrupt" not in p.stderr: print("trunc",cut,p.returncode,p.stderr[-100:])
    for ex in (b"\0",b"abc",b"\x1aKPV"):
        p=run(bytes(b)+ex)
        if p.returncode!=1 or b"corrupt" not in p.stderr: print("append",ex,p.returncode)
    print(n,"undetected flips:",und[:20],len(und))
EOF
python3 flip.py /app/samples/config.vp /app/samples/mixed.vp /app/samples/data.vp 2>&1 | tail -20'
```

<details><summary>output</summary>

```
Command did not complete within its 120s timeout and was moved to the background (ID: bueq9rzqd). Output is being written to: <tmp> You will be notified when it completes. It is running now: do not start a second copy while it runs. If it is no longer needed, stop it with TaskStop. No output had been read from it when it was moved. A program that buffers its output, or a pipe into one such as tail, can keep the output file empty until the command finishes. To check interim output, use Read on that file path.
Session cwd remains <host-path> directory changes made by the backgrounded command do not apply to subsequent commands.
```
</details>

### Step 51: Bash

```bash
cd /w 2>/dev/null; docker exec sn-archive-format-recovery bash -lc 'mkdir -p /w2 && cd /w2 && python3 - <<EOF
import os,random,subprocess,sys
sys.path.insert(0,"/w")
from cmp import same
import shutil
shutil.rmtree("/w2/big",ignore_errors=True); os.makedirs("/w2/big/sub")
r=random.Random(3)
words=[bytes(r.randrange(97,123) for _ in range(r.randint(2,9))) for _ in range(3000)]
txt=b" ".join(r.choice(words) for _ in range(500000))[:3000000]
open("/w2/big/text","wb").write(txt)
blk=os.urandom(200000)
open("/w2/big/far","wb").write(blk+os.urandom(300000)+blk+os.urandom(100000)+blk[5000:150000])
open("/w2/big/zeros","wb").write(bytes(2500000))
open("/w2/big/sub/rnd","wb").write(os.urandom(1500000))
open("/w2/big/sub/mix","wb").write(b"".join((os.urandom(50) if i%3==0 else bytes(r.choice(words)) ) for i in range(60000)))
subprocess.run(["/app/bin/vpack","pack","/w2/big","/w2/big.vp"],check=True)
import time; t=time.time()
subprocess.run(["rm","-rf","/w2/bigo"])
p=subprocess.run(["/app/unvpack","/w2/big.vp","/w2/bigo"],capture_output=True); print(p.returncode,p.stderr, time.time()-t, os.path.getsize("/w2/big.vp"))
b,A,B=same("/w2/big","/w2/bigo"); print("bad",b)
EOF'
```

<details><summary>output</summary>

```
0 b'' 1.259300947189331 5056088
bad []
```
</details>

### Step 52: Bash

```bash
cd /w 2>/dev/null; sed -i 's/    os.mkdir(out)\n/XX/' /dev/null; docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
s=open("/app/unvpack").read()
s=s.replace("    os.mkdir(out)\n    dirs = [(0, out, root)]","    parent = os.path.dirname(out)\n    if parent:\n        os.makedirs(parent, exist_ok=True)\n    os.mkdir(out)\n    dirs = [(0, out, root)]")
open("/app/unvpack","w").write(s)
EOF
mkdir -p /w3/e && cd /w3 && touch -d @1500000000.5 e && /app/bin/vpack pack e e.vp && /app/unvpack e.vp /w3/x/y/out; echo rc=$?; python3 /w/cmp.py e x/y/out'
```

<details><summary>output</summary>

```
sed: 1: "/dev/null
": extra characters at the end of n command
rc=0
OK []
```
</details>

### Step 53: Monitor

```bash
while docker exec sn-archive-format-recovery pgrep -f flip.py >/dev/null; do sleep 5; done; echo finished
```

<details><summary>output</summary>

```
<tool_use_error>InputValidationError: Monitor failed due to the following issue:
The required parameter `description` is missing

This tool's schema was not sent to the API — it was not in the discovered-tool set derived from message history. Without the schema in your prompt, typed parameters (arrays, numbers, booleans) get emitted as strings and the client-side parser rejects them. Load the tool first: call ToolSearch with query "select:Monitor", then retry this call. For reference, this tool's input schema is: {"$schema":"https://json-schema.org/draft/2020-12/schema","type":"object","properties":{"description":{"description":"Short human-readable description of what you are monitoring (shown in notifications).","type":"string"},"timeout_ms":{"description":"Kill the monitor after this deadline. Default 300000ms. Deadlines above 1800000ms are capped to 1800000ms. You are notified at expiry and can re-arm.","default":300000,"type":"number","minimum":1000,"maximum":3600000},"command":{"description":"Shell command or script. Each stdout line is an event; exit ends the watch.","type":"string"},"ws":{"description":"WebSocket to open. Each text frame is an event; binary frames are reported as a placeholder line. Socket close ends the watch. Cannot be combined with command.","type":"object","properties":{"url":{"type":"string"},"protocols":{"type":"array","items":{"type":"string","pattern":"^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"}}},"required":["url"],"additionalProperties":false}},"required":["description","timeout_ms"],"additionalProperties":false}</tool_use_error>
```
</details>

### Step 54: Bash

```bash
for i in $(seq 1 22); do docker exec sn-archive-format-recovery pgrep -f flip.py >/dev/null || break; sleep 5; done; docker exec sn-archive-format-recovery pgrep -f flip.py | head -1
```

<details><summary>output</summary>

```
OCI runtime exec failed: exec failed: unable to start container process: exec: "pgrep": executable file not found in $PATH
```
</details>

### Step 55: Bash

```bash
sleep 100; echo done
```

<details><summary>output</summary>

```
<tool_use_error>Blocked: sleep 100 followed by: echo done. To wait for a condition, use Monitor with an until-loop (e.g. `until <check>; do sleep 2; done`). To wait for a command you started, use run_in_background: true. Do not chain shorter sleeps to work around this block.</tool_use_error>
```
</details>

### Step 56: ToolSearch

```bash
{"query": "select:Monitor", "max_results": 1}
```

<details><summary>output</summary>

```

```
</details>

### Step 57: Monitor

```bash
while docker exec sn-archive-format-recovery sh -c 'for p in /proc/[0-9]*; do grep -q flip.py $p/cmdline 2>/dev/null && exit 0; done; exit 1'; do sleep 5; done; echo finished
```

<details><summary>output</summary>

```
Monitor started (task biyelcpuf, expires in 15m unless the source ends first; you get one notice at expiry — re-arm if you still need the watch). You will be notified on each event. Keep working — do not poll or sleep. Events may arrive while you are waiting for the user — an event is not their reply.
```
</details>

### Step 58: Bash

```bash
echo waiting
```

<details><summary>output</summary>

```
waiting
```
</details>

### Step 59: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import subprocess
# empty dir archive and dedupe chain quick check
EOF
ls -la /app/unvpack'
```

<details><summary>output</summary>

```
-rwxr-xr-x 1 root root 9565 Nov 17 14:41 /app/unvpack
```
</details>

### Step 60: Monitor

```bash
sleep 240; echo tick
```

<details><summary>output</summary>

```
Monitor started (task bgolnjk3c, expires in 10m unless the source ends first; you get one notice at expiry — re-arm if you still need the watch). You will be notified on each event. Keep working — do not poll or sleep. Events may arrive while you are waiting for the user — an event is not their reply.
```
</details>

### Step 61: Bash

```bash
cat <tmp>
```

<details><summary>output</summary>

```
(Bash completed with no output)
```
</details>

### Step 62: Monitor

```bash
f=<tmp> until [ -s $f ]; do sleep 5; done; cat $f | head -40
```

<details><summary>output</summary>

```
Monitor started (task b80tq1uvq, expires in 15m unless the source ends first; you get one notice at expiry — re-arm if you still need the watch). You will be notified on each event. Keep working — do not poll or sleep. Events may arrive while you are waiting for the user — an event is not their reply.
```
</details>

### Step 63: Bash

```bash
echo ok
```

<details><summary>output</summary>

```
ok
```
</details>

### Step 64: Monitor

```bash
sleep 300; echo tick2
```

<details><summary>output</summary>

```
Monitor started (task bur8jj5ce, expires in 10m unless the source ends first; you get one notice at expiry — re-arm if you still need the watch). You will be notified on each event. Keep working — do not poll or sleep. Events may arrive while you are waiting for the user — an event is not their reply.
```
</details>

### Step 65: Bash

```bash
echo ok
```

<details><summary>output</summary>

```
ok
```
</details>

### Step 66: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; sed -i "s#/w/fl.vp#/w/fl2.vp#g; s#/w/flo#/w/flo2#g" flip.py; timeout 100 python3 flip.py /app/samples/config.vp 2>&1 | tail'
```

<details><summary>output</summary>

```
/app/samples/config.vp undetected flips: [] 0
```
</details>

### Step 67: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; timeout 110 python3 flip.py /app/samples/mixed.vp 2>&1 | tail'
```

<details><summary>output</summary>

```
/app/samples/mixed.vp undetected flips: [(398, 128)] 1
```
</details>

### Step 68: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys; sys.path.insert(0,"/w"); sys.path.insert(0,"/app")
import importlib.machinery,importlib.util
ld=importlib.machinery.SourceFileLoader("unv","/app/unvpack"); spec=importlib.util.spec_from_loader("unv",ld); u=importlib.util.module_from_spec(spec); ld.exec_module(u)
b=open("/app/samples/mixed.vp","rb").read()
root,ents=u.parse(b)
for e in ents: print(e["name"],e["kind"],e.get("off"),e.get("cs"),e.get("comp"),e.get("size"))
print(b[390:405].hex())
EOF'
```

<details><summary>output</summary>

```
b'Makefile' 1 16 26 False 26
b'build' 2 None None None None
b'out.log' 1 42 365 True 446
b'empty.txt' 1 407 0 False 0
b'src' 2 None None None None
b'calc.py' 1 407 465 True 3152
e46421956f756768120a3140757011
```
</details>

### Step 69: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
s=open("/app/unvpack").read()
old="""                if i >= n:
                    break
"""
new="""                if i >= n:
                    if ctl >> bit:
                        raise Corrupt("bad control byte")
                    break
"""
assert old in s
open("/app/unvpack","w").write(s.replace(old,new))
EOF
python3 fuzz.py 300 120 2>&1|tail -4; for n in config data mixed notes webapp; do rm -rf o_$n; /app/unvpack /app/samples/$n.vp o_$n && python3 cmp.py /app/samples/$n o_$n; done; timeout 60 python3 flip.py /app/samples/mixed.vp | tail -3'
```

<details><summary>output</summary>

```
bad 0 of 120
OK []
OK []
OK []
OK []
OK []
```
</details>

### Step 70: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; timeout 115 python3 flip.py /app/samples/data.vp | tail -3'
```

<details><summary>output</summary>

```
(Bash completed with no output)
```
</details>

### Step 71: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import subprocess
b=bytearray(open("/app/samples/data.vp","rb").read())
for i in (896,2525,2529):
    x=bytearray(b); x[i]^=0x80; open("/w/q.vp","wb").write(x)
    p=subprocess.run(["rm","-rf","/w/qo"]); p=subprocess.run(["/app/unvpack","/w/q.vp","/w/qo"],capture_output=True); print(i,p.returncode,p.stderr)
EOF'
```

<details><summary>output</summary>

```
896 1 b'unvpack: archive is corrupt: bad control byte\n'
2525 1 b'unvpack: archive is corrupt: bad control byte\n'
2529 0 b''
```
</details>

### Step 72: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys; sys.path.insert(0,"/app")
import importlib.machinery,importlib.util
ld=importlib.machinery.SourceFileLoader("unv","/app/unvpack"); spec=importlib.util.spec_from_loader("unv",ld); u=importlib.util.module_from_spec(spec); ld.exec_module(u)
b=open("/app/samples/data.vp","rb").read()
root,ents=u.parse(b)
for e in ents:
    if e["kind"]==1: print(e["name"],e["off"],e["cs"],e["size"],e["comp"])
print(b[2500:2538].hex())
EOF'
```

<details><summary>output</summary>

```
b'noise.bin' 16 700 700 False
b'readme' 716 182 189 True
b'scores.csv' 898 1640 2354 True
6112b201ad134a04a6bf2161062301cc131c32d1026e6f27083f223e000f029e04db16433183
```
</details>

### Step 73: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
b=bytearray(open("/app/samples/data.vp","rb").read())
import struct
print(b[2536:2539].hex(), len(b))
io=struct.unpack("<Q",b[-20:-12])[0]; print(io)
EOF'
```

<details><summary>output</summary>

```
318304 2639
2538
```
</details>

### Step 74: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys; sys.path.insert(0,"/app")
import importlib.machinery,importlib.util
ld=importlib.machinery.SourceFileLoader("unv","/app/unvpack"); spec=importlib.util.spec_from_loader("unv",ld); u=importlib.util.module_from_spec(spec); ld.exec_module(u)
b=bytearray(open("/app/samples/data.vp","rb").read())
root,ents=u.parse(bytes(b))
e=[e for e in ents if e["name"]==b"scores.csv"][0]
a=u.content(bytes(b),e)
b[2529]^=0x80
raw=bytes(b[e["off"]:e["off"]+e["cs"]])
# walk blocks
i=0
while i<len(raw):
    v,i=u.gv(raw,i); print("block",i,v>>1,v&1); i+=v>>1
print(len(raw))
# show tail region of block
print(raw[-40:].hex(), 2529-e["off"])
EOF'
```

<details><summary>output</summary>

```
block 2 1638 0
1640
2c656112b201ad134a04a6bf2161062301cc131c32d1026e6f27083f223e008f029e04db16433183 1631
```
</details>

### Step 75: Bash

```bash
docker exec sn-archive-format-recovery bash -lc 'cd /w; python3 - <<EOF
import sys; sys.path.insert(0,"/app")
import importlib.machinery,importlib.util
ld=importlib.machinery.SourceFileLoader("unv","/app/unvpack"); spec=importlib.util.spec_from_loader("unv",ld); u=importlib.util.module_from_spec(spec); ld.exec_module(u)
b=bytearray(open("/app/samples/data.vp","rb").read())
root,ents=u.parse(bytes(b))
e=[e for e in ents if e["name"]==b"scores.csv"][0]
def toks(raw):
    p=raw[2:]; i=0; n=len(p); out=0; res=[]
    while i<n:
        ctl=p[i]; i+=1
        for bit in range(8):
            if i>=n: break
            if not (ctl>>bit)&1: i+=1; out+=1; res.append(("L",out))
            else:
                bb=p[i]; i+=1
                if bb<0x80: L=(bb>>4)+3; off=(((bb&15)<<8)|p[i])+1; i+=1
                else:
                    L=(bb&0x7f)+3; off=p[i]|(p[i+1]<<8); off+=1; i+=2
                    if bb==0xff: x,i=u.gv(p,i); L+=x
                out+=L; res.append(("M",L,off,out,i))
    return res,out
raw=bytes(b[e["off"]:e["off"]+e["cs"]])
r,o=toks(raw); print(r[-4:],o)
EOF'
```

<details><summary>output</summary>

```
[('M', 3, 671, 2341, 1632), ('M', 3, 1244, 2344, 1634), ('M', 4, 1604, 2348, 1636), ('M', 6, 388, 2354, 1638)] 2354
```
</details>

`/app/unvpack` is written and working. It round-trips every sample and about 380 random trees that I packed with `vpack` and compared against the originals. It exits 1 with "corrupt" on stderr for damaged archives.

**How I got the format**
There was no disassembler or `xxd` in the container, so I worked it out black-box, packing small controlled trees and diffing the bytes.

**What it handles**
- **Header and metadata:** times are stored as a base time plus deltas, with optional nanoseconds. Modes are stored XORed against the default (0644 for files, 0755 for dirs). The root's mode and time sit in the header.
- **Flat index:** it covers files, dirs and symlinks. Entries point to their parent entry, and the sample archives list them as a depth-first walk.
- **Compression:** file data is a block stream. Each block is either an LZ-style codec (control bytes with match tokens in a short and a long form) or a stored raw block. The index itself is compressed when it grows past about 28 entries.
- **Name table:** repeated names are stored once and referenced.
- **Identical-content files:** these are stored as a reference to an earlier file. Each one still gets its own mode and mtime.
- **Large files:** multi-MB files with matches reaching back more than 64KB decode correctly, in about 1.3s for a 5MB archive.
- **Output:** the program creates `OUTDIR` and any missing parent directories. Dir modes and times are set deepest-first so they stick. Symlink times are set without following the link.

**Corruption**
The footer checksum covers the header plus the stored index, so any change there is caught. Per-file crc32 catches data damage. The program also rejects:
- bad magic, version or flag bits
- footer length mismatches
- truncated or inconsistent index and block structure
- bad match offsets
- set bits in a final control byte that has no item
- trailing bytes

I flipped every byte (two bit patterns) and tried every truncation and several appends on `config.vp` and `mixed.vp`. Everything was rejected except one flip in `mixed.vp`, which I fixed by rejecting set bits in a final control byte with no item. I only spot-checked three positions in `data.vp`. The one flip that still passes, at byte 2529 of `data.vp`, lands on a match-offset byte and produces identical output. No checksum can catch that. I did not run the full flip sweep on `notes.vp` or `webapp.vp`.

**Confidence**
High. The remaining risk is a rare `vpack` feature that my random trees never produced, such as an unseen codec bit.

### Step 76: SubagentHandback

```bash
{"message": "/app/unvpack is written and working. It round-trips all 5 samples and about 380 random trees packed with vpack, compared against the originals for names, contents, link targets, modes and nanosecond mtimes. It exits 1 with \"corrupt\" on stderr for damaged archives.\n\nI worked the format out black-box, because the container had no disassembler or xxd. I packed small controlled trees and diffed the bytes.\n\nWhat it handles:\n- **Header and metadata:** times are a base time plus deltas, with optional nanoseconds. Modes are stored XORed against the default (0644 for files, 0755 for dirs). The root's mode and time sit in the header.\n- **Index:** a flat list of entries (files, dirs, symlinks), each pointing to its parent entry.\n- **Compression:** file data is a block stream. Each block is either an LZ-style codec (control bytes with a short and a long match form) or a stored raw block. The index itself is compressed when it grows past about 28 entries.\n- **Name table:** repeated names are stored once and referenced.\n- **Identical-content files:** stored as a reference to an earlier file. Each still gets its own mode and mtime.\n- **Large files:** multi-MB files with matches reaching back over 64KB decode correctly, in about 1.3s for a 5MB archive.\n- **Output:** the program creates OUTDIR and any missing parent directories. Dir modes and times are set deepest-first. Symlink times are set without following the link.\n\nCorruption checks:\n- The footer checksum covers the header plus the stored index.\n- Each file has a crc32.\n- It also rejects bad magic, version or flag bits, footer length mismatches, inconsistent index or block structure, bad match offsets, set bits in a final control byte that has no item, and trailing bytes.\n\nI flipped every byte (two bit patterns) and tried every truncation and several appends on config.vp and mixed.vp. Everything was rejected except one flip in mixed.vp, which I fixed by rejecting set bits in a final control byte with no item. After that I only spot-checked three positions in data.vp, and did not run the full sweep on notes.vp or webapp.vp. The one flip that still passes, at byte 2529 of data.vp, lands on a match-offset byte and produces identical output, so no checksum can catch it.\n\nConfidence is high. The remaining risk is a rare vpack feature my random trees never produced, such as an unseen codec bit."}
```

<details><summary>output</summary>

```
{"success":true,"message":"Report delivered to your caller."}
```
</details>

