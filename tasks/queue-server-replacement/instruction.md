The machine has a small job queue and key/value server, `/app/bin/qsrv`. It speaks a
line-based TCP protocol. A user-level description is in `/app/docs/PROTOCOL.md`, with a
few short transcripts in `/app/docs/examples/`. The binary is the only complete source of
truth for how the server behaves; the documentation is incomplete. Start it with
`/app/bin/qsrv --port PORT` and talk to it over 127.0.0.1 to see what it does.

Write a drop-in replacement at `/app/replacement/qsrv` that behaves identically to the
original on the wire.

Requirements:

- The replacement is a single Python 3 script using only the standard library. It is
  started as `python3 /app/replacement/qsrv --port PORT`, must listen on 127.0.0.1 at that
  port within a few seconds, and must keep serving until it is killed.
- For any sequence of client traffic, every byte the replacement sends on every connection,
  and the points at which it closes connections, must match what `/app/bin/qsrv` does for
  the same traffic. This covers commands, replies, error codes and messages, argument
  quoting and parsing, limits, logical time, job and key state, transactions, namespaces,
  multiple simultaneous connections, pipelined input and the server information output.
- Treat the binary as a black box: run it and observe it. Do not disassemble, decompile,
  trace the system calls of, or copy the binary, and do not make the replacement depend on
  it in any way.

How it is graded:

- The original binary and any copy of it are deleted before grading. The replacement runs
  under a Python audit hook that rejects starting processes, `ctypes`, and native
  extension modules from outside the Python installation.
- Grading replays several hundred client sessions that were recorded from the original
  server. Each session starts a fresh replacement process, opens one or more connections
  one after another, and then sends data step by step: single commands, several commands
  in one write, lines split across writes, LF or CR LF line endings, and malformed input.
  After each step the grader reads the bytes the original produced on each connection and
  compares them exactly. When the grader closes a connection it shuts down its sending
  side and expects the server to close the connection. After a step that produces no
  output the grader waits about 100 ms before the next step.
- A session passes only if every byte and every connection close matches. The task passes
  only if every session passes.
