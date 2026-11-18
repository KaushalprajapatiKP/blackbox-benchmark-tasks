# Failure study

## Source of the attempts

All attempts below are my own runs on tasks I wrote, so they can be shared. Each attempt is
one cold run of a strong coding agent working inside the task's Docker container with only the
task statement, graded afterwards by the task's own verifier. In total I ran 16 attempts on 16
tasks while calibrating difficulty: 10 on earlier, easier task versions and 6 on the final
tasks. The trajectories for the final tasks are in `trajectories/` here and in the companion
repository `legacy-parity-tasks`. The earlier tasks are not published; I describe those
attempts from my run notes.

## The eight attempts used

| # | Task | Outcome | What happened |
|---|---|---|---|
| A1 | tabula-rebuild | reward 0, 745 / 749 | The agent built four differential fuzzers, compared about 12,000 invocations against the binary, reported "matched in every run", and still missed 4 rules: an escaped tab passed as the delimiter, a comma inside an aggregate argument, and quoting when the output delimiter is a digit or `.` |
| A2 | fortran-solver-parity | reward 0, 352 / 353 | 235 tool calls. The agent listed "12 patterns" among the limits it matched but never fed a deck with 13; production prints `E111 ... TOO MANY PATTERNS`, the rebuild printed nothing |
| A3 | Python 3.12 port of an old library (earlier task) | reward 0, 380 / 386 | Declared done after 23 calls with the visible suite green. Missed that 3.11+ tracebacks carry `^^^` marker lines its parser misreads (5 hidden failures, no visible test covers it) |
| A4 | same run as A3 | 1 of the 6 failures | To silence a deprecation warning it added a public `datetime` subclass, although the statement forbids adding public classes. Its own summary called this "the one hack" |
| A5 | exact-count test-suite minimisation (earlier task) | reward 1, flawed | Passed every graded instance, but its own timing table shows the solver exceeding the 10 s limit on dense near-zero-cost components. The graded instances had none, so a slow solution passed |
| A6 | nine earlier tasks (C library release, COBOL migration, Fortran revival, and six smaller ones) | 9 of 9 reward 1, in 3 to 23 calls | The statements listed the symptoms, or the truth sat in readable docs. The agent read the whole tree in two or three calls and fixed everything in bulk, including a 15-defect C project in 15 calls |
| A7 | RRULE recurrence expander (earlier task) | reward 1 | The attempt was fine; the defect was in my answer key. The expected outputs came from a widely used library, and in 3 cases that library disagrees with RFC 5545. Cross-checking against an independent implementation caught them before release |
| A8 | queue-server-replacement (authoring run) | not an agent failure | Recording the hidden sessions with 12 recorders in parallel produced 2 sessions whose bytes differed between recordings. Re-recorded alone they were stable. Left in, they would have failed correct solutions at random |

## Categories

**C1. Verification limited to the agent's own hypotheses** (capability gap): A1, A2, A3.
The agent tests thoroughly, but only along the axes it already thought of. Fuzzers draw from the
agent's model of the input space, so a rule outside that model is never exercised; volume
of testing does not fix this. A1 is the clearest case: 12,000 matching runs, 4 misses.

**C2. Limits and boundaries left unprobed** (capability gap): A2, part of A1.
The agent records a limit (12 patterns, a delimiter option) but checks the behaviour inside it,
not at and past it. Error paths past documented limits are where the misses cluster.

**C3. Breaking a stated rule to get to green** (capability or alignment gap): A4.
When one requirement was awkward, the agent broke a different one that was explicitly stated,
and said so in its summary as a known hack rather than as a blocker.

**C4. Grader or answer-key defects** (task defect): A5, A7.
A5 is a flawed solution that passed because the hidden suite lacked the hard regime. A7 is an
answer key that would have failed correct solutions. Both look like agent results unless the
grader is audited.

**C5. Task and environment design defects** (task or environment defect): A6, A8.
A6: when the statement or the readable docs carry the plan, a task measures reading speed, not
the capability it names; all 9 such tasks were solved in at most 23 calls. A8: nondeterminism
in how the expected outputs were captured would turn into random failures.

## Capability gaps vs defects

- **Likely real capability gaps:** C1, C2, C3. They reproduce across different tasks, languages
  and failure surfaces, and every missed behaviour was observable by running the provided program.
- **Task or environment defects:** C4, C5. These say more about the benchmark than the agent.
  Every one was found by checking the grader, not by reading the agent's output.

## What I cannot conclude

- **No pass rates.** One run per task. A 0 on tabula and a 1 on the queue server do not mean
  the first is harder; a second run could flip either.
- **One agent family.** Every attempt used the same model and harness. I cannot separate model
  limits from harness behaviour, for example how much output a tool call returns.
- **Small, self-selected sample.** Eight attempts on tasks I wrote and then hardened in response
  to earlier runs. The categories are hypotheses to test on more runs, not a taxonomy.
- **C1 and C2 overlap.** A2 is both a limit miss and a verification gap. With more data they
  might merge, or split differently.
- **No human baseline.** I don't know how long a skilled engineer takes on these tasks, so
  turn counts are not mapped to human effort.
