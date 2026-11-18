# Black-box benchmark tasks

Three Terminal-Bench style tasks in Harbor format, built around one failure category of
coding agents: **inferring exact behaviour from a program you can run but not read**.

In each task the source of truth is a compiled, stripped program written for the task. The
documentation covers only the purpose and the basic surface. Every edge rule has to be found by
running the program, forming a hypothesis, and checking it. Grading is all-or-nothing over a
large hidden suite recorded from the original program, and the original is deleted before
grading.

Author: Kaushal Prajapati (kaushalprajapati5296@gmail.com). Tools used: see [TOOLS.md](TOOLS.md).

## Tasks

| Task | What the agent must build | Hidden suite | Docs-only baseline |
|---|---|---|---|
| [`queue-server-replacement`](tasks/queue-server-replacement) | A drop-in Python replacement for `qsrv`, a job-queue and versioned key/value server with a line protocol (36 commands, 30 error codes, logical clock, transactions, blocking reserves) | 446 recorded multi-connection sessions, 14 categories, about 9,700 steps, compared byte for byte | 18 / 446 sessions |
| [`archive-format-recovery`](tasks/archive-format-recovery) | An unpacker for `vpack`, an archiver with an undocumented container format and its own LZ variant; only a pack-only binary and 5 easy sample archives are given | 68 valid archives in 15 categories plus 48 damaged archives (117 tests) | stored-only decoder: 8 / 68 archives |
| [`tabula-rebuild`](tasks/tabula-rebuild) | A clean-room rebuild of `tabula`, a CSV/TSV processor with 26 options, a filter language, exact decimal aggregates, Unicode-width table output and about 50 error messages | 749 recorded invocations in 13 categories, stdout, stderr and exit code byte for byte | man-page-only rebuild: 295 / 749 |

## Validation (real Docker runs with Harbor 0.25)

Every task: reference solution scores 1 on two runs, a no-op agent scores 0, and every
shortcut in the task's `cheats/` folder scores 0. Each task README has the full table.

## Agent trajectories

`trajectories/<task>/` holds one cold attempt per task by a strong coding agent working
inside the task container, graded by the task's own verifier:

| Task | Tool calls | Reward | Result |
|---|---|---|---|
| queue-server-replacement | 96 | 1 | 446 / 446 sessions |
| archive-format-recovery | 76 | 1 | 117 / 117 tests |
| tabula-rebuild | 67 | 0 | 745 / 749 cases; missed an escaped tab delimiter, a comma inside an aggregate argument, and quoting when the delimiter is a digit or `.` |

`trajectory.md` is the run (prompt, commands, truncated outputs). `grading.txt` is the
verifier output. One run per task is a length and difficulty signal, not a pass rate.

## Running a task

```bash
pip install harbor            # or uv tool install harbor
harbor run -p tasks/tabula-rebuild -a oracle   # reference solution, expect reward 1
harbor run -p tasks/tabula-rebuild -a nop      # does nothing, expect reward 0
```

To check a shortcut, copy the task, replace `solution/solve.sh` with a script from `cheats/`,
and run the oracle agent on the copy; it should score 0.

## Task layout

```
tasks/<task>/
  instruction.md     the statement the agent sees
  task.toml          metadata, difficulty, solution and verification explanations, timeouts
  environment/       Dockerfile (multi-stage: the program is compiled and stripped, its source
                     never reaches the agent image) and the files placed in /app
  solution/          reference solution (solve.sh)
  tests/             verifier: recorded cases, replay harness, anti-cheat scrub
  cheats/            shortcut solutions used to prove the grader rejects them
  authoring/         generators and recorders that rebuild the hidden suite
```

## Design notes

- **Difficulty from discovery, not from a long spec.** Easier versions of these tasks, where
  the truth was in readable code and docs, were solved in a handful of tool calls no matter how
  many defects they contained. Hiding the truth in an executable is what made agents run long
  probe, compare and revise loops.
- **Fairness.** Every graded behaviour is observable by running the provided program. Visible
  examples cover only the easy regime, and the instruction states the output contract,
  tolerances and grading mechanics.
- **Anti-cheat.** The original binary and any copy of it (found by hash) are deleted before
  grading; replacements run under an audit hook that blocks process creation and native code.
  Proxying, hardcoding and docs-only shortcuts all score 0.
- **What these tasks do not measure:** code quality, performance beyond the stated limits,
  and collaboration over multiple turns.

## Known limitations

- Validation ran on arm64 (Docker on Apple silicon).
- Some verifiers share the agent's container so the binary can be removed in place; an agent
  with root could in principle tamper with the interpreter that runs the tests.
- The hidden suites are in this public repository, so these copies should not be used to score
  models that may have been trained on them.
