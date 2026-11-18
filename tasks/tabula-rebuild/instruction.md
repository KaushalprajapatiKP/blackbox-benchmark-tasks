# Rebuild `tabula` from its observable behaviour

`/app/bin/tabula` is a command line CSV/TSV table processor. Its source code is
not available. A user guide is at `/app/docs/tabula.1.md`, and `tabula --help`
prints an option summary. The guide describes the options but does not spell
out every rule; where the guide and the program disagree or the guide is
silent, the program's actual behaviour is the specification.

Write a clean-room reimplementation at `/app/rebuild/tabula` that behaves
exactly like the original for any command line arguments, input files and
standard input.

## Requirements

- `/app/rebuild/tabula` is a Python 3 program that uses only the Python
  standard library (Python 3.12 is installed). You may split it into extra
  `.py` modules placed in `/app/rebuild/`.
- For every invocation the rebuild must produce the same standard output
  bytes, the same standard error bytes and the same exit status as
  `/app/bin/tabula`. This includes help and version text, all diagnostics and
  their exit codes, and all formatting details (alignment, quoting, number
  formatting, line endings).
- You may learn about the original only by reading the guide and by running
  `/app/bin/tabula` with whatever arguments and inputs you like. Do not read,
  copy, decompile, disassemble or otherwise inspect the bytes of
  `/app/bin/tabula`.
- The rebuild must not start other programs or load native code. It must not
  import `subprocess` or `ctypes`, and must not call `os.system`, `os.exec*`,
  `os.spawn*`, `os.posix_spawn` or `os.fork`. No compiled binaries or shared
  libraries may be placed in `/app/rebuild/`.

## How it is checked

The rebuild is run on a large hidden set of invocations that were recorded
from the original program. They cover every option in the guide, input
parsing, header handling, column selection, filter expressions, sorting,
grouping and aggregation, number formatting, output quoting, the aligned
table output, the order in which processing steps apply, and error handling.

- Each invocation runs as `python3 -I -S <runner> /app/rebuild/tabula ARGS...`
  where the runner adds `/app/rebuild` to the module search path and blocks
  process creation and native code. The working directory is a fresh
  temporary directory containing the input files the invocation names;
  standard input is provided (it may be empty) and the environment contains
  only `PATH` and `LC_ALL=C.UTF-8`. Each invocation has a 10 second limit.
- `/app/bin/tabula` and any copy of it are deleted before grading starts.
- Standard output, standard error and the exit status are compared byte for
  byte. Every invocation must match; there is no partial credit.
