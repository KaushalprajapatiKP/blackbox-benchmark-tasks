# Feedback to a junior researcher

> Task under review: "Write a function that sorts a list. The grader checks that the output
> contains the word 'sorted'."

Hi, thanks for sending this in. Here are my notes, most important first.

**1. The grader doesn't test sorting.** It checks for a string, so `print("sorted")` scores
full marks and a correct sort that prints nothing scores zero. A grader has to check the
behaviour the task asks for. Here that means running the submitted function on inputs and
comparing its result with `sorted(input)`.

**2. The statement is underspecified.** Before writing tests, decide and state:
- the function name and signature, and whether it returns a new list or sorts in place;
- the element types (ints only, mixed, strings, objects with a key?);
- order (ascending, descending, stable?), duplicates, empty lists, `None`;
- whether built-ins like `sorted()` are allowed. If they are, the task is a one-liner.

Every behaviour you grade must be stated, or a correct solution can fail on a choice the
statement never asked it to make.

**3. Check the grader before checking the agent.** Every task needs three runs before review:
- a reference solution that scores 1;
- a no-op that scores 0;
- at least one shortcut that scores 0 (here, printing "sorted").

This grader fails the third check immediately.

**4. Even fixed, it measures almost nothing.** A correct sort with a sound grader is solved by
every current model in one step, so it tells us nothing about the models we care about. Start
from a capability you want to measure, ideally one you have seen agents fail at, and design the
task so that capability is the hard part. A sorting task can get there: for example, a sort on
data too large for memory under a stated limit, or a stable multi-key sort whose collation rules
have to be discovered from examples. Then measure: if a strong agent solves it in a few steps,
the difficulty isn't where you think it is.

**Suggested next step:** rewrite the statement with an explicit contract. Replace the grader
with a property test against `sorted()` on seeded random and edge-case inputs, plus a stability
check. Send me the reference, no-op and shortcut results with it. Happy to pair on the harder
version once that is in place.
