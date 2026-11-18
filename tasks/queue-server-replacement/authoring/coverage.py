"""Summarise what the recorded sessions exercise (authoring only)."""
import collections
import json
import re
import sys

sessions = json.load(open(sys.argv[1]))
codes = collections.Counter()
cats = collections.Counter()
steps = 0
closes = 0
for s in sessions:
    cats[s["category"]] += 1
    for st in s["steps"]:
        steps += 1
        closes += len(st["eof"])
        for out in st["out"].values():
            for m in re.finditer(r"(?m)^-([A-Z_]+) ", out):
                codes[m.group(1)] += 1
print("sessions", len(sessions), "steps", steps, "server-side closes", closes)
print(dict(cats))
print(len(codes), "distinct error codes")
for c, n in sorted(codes.items()):
    print("  %-14s %d" % (c, n))
