# queue-server-replacement

## Statement summary

The environment ships a stripped, self-written C server `/app/bin/qsrv`: a job queue
and versioned key/value broker with a line-based TCP protocol (36 commands). The docs
(`/app/docs/PROTOCOL.md`) are user-guide level: the command list with a one-line purpose
and basic syntax, plus three easy transcripts in `/app/docs/examples/`. The agent must write
`/app/replacement/qsrv`, a single Python 3 standard-library script, that behaves
byte-for-byte identically on the wire. Failure category: stateful black-box behaviour
inference.

## Environment

- Multi-stage `environment/Dockerfile`: `gcc:12.5.0-bookworm` compiles `src/qsrv.c`
  statically and strips it; the final image is `python:3.12.15-slim-bookworm` with
  `pytest==8.3.4`, the binary at `/app/bin/qsrv`, and the docs. The C source is not in the
  final image (checked with `find`; only `/app/bin/qsrv` matches).
- `environment/src/qsrv.c` (1678 lines, MIT) is the server source. `solution/qsrv.py`
  (993 lines, MIT) is the reference replacement; `solution/solve.sh` writes it to
  `/app/replacement/qsrv`.
- `authoring/` (not part of Harbor) holds the seeded session generator, the recorder, the
  stability filter, the example generator, the script generator for solve.sh and the
  cheats, and a coverage report. `bash authoring/build_sessions.sh` rebuilds
  `tests/sessions.json` and `tests/qsrv.sha256` from the binary inside the task image.

## Grading

`tests/test.sh`:
1. `scrub.py` deletes `/app/bin/qsrv` and every file on the filesystem whose SHA-256
   matches the binary (hash recorded at authoring time, plus the hash of whatever is at
   `/app/bin/qsrv` at grading time).
2. `test_replay.py` replays 446 recorded sessions (9716 steps, 93 server-side closes).
   Each session starts a fresh `python3 -I run_audited.py /app/replacement/qsrv --port P`,
   waits until the port is LISTEN in `/proc/net/tcp` (so no probe connection disturbs
   connection ids), opens the session's connections in order, and sends each step. For
   every connection that produced output in the recording, the grader reads exactly that
   many bytes (2 s timeout, early stop on first wrong byte) and compares them; recorded
   server-side closes must also happen. Steps with no recorded output are followed by a
   100 ms settle. After the last step no further bytes may arrive.
3. `run_audited.py` installs `sys.addaudithook` that raises on `subprocess.Popen`,
   `os.system`, `os.exec*`, `os.posix_spawn`, `os.spawn*`, `os.fork*`, `pty.spawn`, every
   `ctypes.*` event, and imports of `.so` modules from outside the Python installation.
4. Reward is 1 only if all sessions pass. A per-category table is printed and written to
   `/logs/verifier/sessions_report.txt`.

Session categories (32 each, lexer 30 after the stability filter): lexer, commands,
numeric, kv, ttl, queue-basic, queue-order, visibility, multi-conn, transactions,
namespaces, pipelining, info, limits. All 30 error codes appear in the recordings
(`python3 authoring/coverage.py tests/sessions.json`).

Determinism: the server has no wall-clock behaviour (all time is the `TICK` logical
clock). Sessions were recorded three times from the binary; only sessions whose
recordings were byte-identical across all three were kept (2 of 448 were dropped because
a reply under heavy recording load landed after the 40 ms idle window; both were stable
when re-recorded alone).

## Difficulty

Every behaviour below is independent, simple once known, absent from the docs, and
observable only by running the binary. One wrong byte in any one of them fails the task.

1. Line framing: LF or CR LF; a 4096-byte limit that counts a trailing CR; over-long input
   gives `E_LINE` and closes the connection, also when the line arrives in pieces.
2. Control bytes (other than tab) give `E_CTRL`; blank lines produce no reply.
3. Double-quote escapes `\n \t \r \0 \\ \" \xHH`; anything else `E_ESCAPE`; single quotes
   are literal; a closing quote must be followed by whitespace; quotes inside bare words
   are literal; 64-token limit (`E_ARGS`).
4. Unknown command echo: upper-cased, truncated to 24 bytes, non-printables as `?`.
5. Per-command arity table and the lower-case name in `E_ARITY` messages.
6. Integer parsing: optional `-`, leading zeros allowed, more than 18 digits is a range
   error not a format error, per-argument bounds and names in the message.
7. Option parsing: case-insensitive keywords, unknown / duplicate / missing value errors,
   and the order of checks (name, payload length, options, existence, limits, capacity).
8. Hidden defaults: PRI 5 (lower is served first), TTR 30, MAXLEN 100, MAXNACK 3; ready
   ties broken by a global ready sequence, not by id.
9. `QDECLARE` returns CREATED, EXISTS or `E_CONFLICT`, comparing only the options given.
10. `PUSH` auto-creates queues; 8 queues and 64 keys per namespace; capacity counts
    ready, delayed and reserved jobs but not dead ones.
11. Visibility timeouts count as failures and can dead-letter a job.
12. `TICK` order: key expiry, then due delays and timeouts sorted by (due tick, id), then
    blocked waiters are served, then remaining waiters time out.
13. `NACK` replies READY / DELAYED / DEAD; `RELEASE` can change priority and never counts
    a failure; `TOUCH` returns the new deadline.
14. Job checks in order: other namespace or missing (`E_NOJOB`), dead, not reserved, owned
    by another connection; options are checked only after ownership.
15. `KICK` is limited by free capacity and resets failure counts; `DEADLIST` is in
    dead-letter order.
16. `BRESERVE` blocks the connection (later pipelined lines wait), waiters are served in
    wait order, timeout reply is `*-1`, inside MULTI it does not block, and `QDROP` /
    `FLUSH` wake waiters with an error.
17. A closing connection releases its reserved jobs (in id order) to waiters; connection
    ids are never reused.
18. Key versions restart at 1 after delete; `SET` clears a TTL while `CAS` and `INCR` keep
    it; `CAS` with version 0 creates; `EXPIRE 0` removes the TTL; `TTL` returns -1 / -2.
19. `INCR` accepts only canonical integers and overflows past 10^15.
20. `KEYS` glob (`*`, `?`), byte-order sort, pattern length limit.
21. MULTI: lexer, unknown, arity and queue-full (16) errors at queue time abort the
    transaction; runtime errors become array elements; `USE` is refused without aborting;
    nested `MULTI` does not abort; `QUIT` inside MULTI closes at once.
22. Namespaces: job ids are global but jobs are invisible across namespaces; namespace
    names allow no dot and at most 16 bytes.
23. `INFO` sections and counters (commands counts lexer errors and queued lines, not EXEC
    sub-commands; namespaces counts only non-empty ones).
24. `QSTAT` and `JOBINFO` layouts with state-dependent fields; `$-1` versus `*-1` nils.

The instruction states the outcome contract and where truth lives (the binary) and lists
the graded areas, but no symptom or rule. Visible examples cover only the easy regime.

## Shortcuts and how the grader stops them

| Cheat (in `cheats/`) | Idea | Why it fails |
|---|---|---|
| `proxy_binary.sh` | copy the binary next to the script and also embed it base64; the script `os.execv`s it | scrub deletes the hash-identical copy; the embedded copy is written to /tmp at start but `os.exec` is blocked by the audit hook, so no server listens |
| `replay_examples.sh` | hardcode the visible example transcripts | hidden sessions are different traffic |
| `docs_only_baseline.sh` | a generous docs-only baseline: the reference implementation with only the undocumented specifics replaced by plausible guesses (generic `-ERR` errors, guessed defaults and limits, timeouts not counted as failures, guessed INFO layout) | most sessions contain at least one error or default; see pass count below |

## What it does not measure

Real concurrency or performance, socket edge cases outside the grader's step model
(simultaneous writes on several connections, slow readers), the connection limit (32),
and server start-up flags other than `--port`.

## Validation

All runs are real Docker runs with Harbor 0.25 (`-o` pointed at a scratch jobs dir).

| Run | Command | Result |
|---|---|---|
| oracle #1 | `harbor run -p queue-server-replacement -a oracle` | reward 1, 446/446 sessions, verifier pytest 48 s |
| oracle #2 | same | reward 1 |
| nop | `harbor run -p queue-server-replacement -a nop` | reward 0 (no replacement file) |
| cheat proxy_binary | task copy with `cheats/proxy_binary.sh` as solve.sh, `-a oracle` | reward 0, 0/446; scrub removed 1 copy, audit hook raised `blocked by grader: os.exec` (also confirmed by running the cheat directly in the image) |
| cheat replay_examples | same with `cheats/replay_examples.sh` | reward 0, 0/446 |
| docs-only baseline | same with `cheats/docs_only_baseline.sh` | reward 0, 18/446 sessions pass (kv 1, ttl 5, multi-conn 1, transactions 2, pipelining 1, info 1, limits 7, all other categories 0) |

Before Harbor, the Python reference was checked differentially against the binary on
1680 further generated sessions (seeds 11, 12, 13): 1680/1680 identical.
