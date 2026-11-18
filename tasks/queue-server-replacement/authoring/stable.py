"""Keep only sessions whose recordings are byte-identical across runs."""
import json
import sys

runs = [json.load(open(p)) for p in sys.argv[1:-1]]
keep = []
dropped = []
for group in zip(*runs):
    if all(g == group[0] for g in group[1:]):
        keep.append(group[0])
    else:
        dropped.append(group[0]["id"])
json.dump(keep, open(sys.argv[-1], "w"), separators=(",", ":"))
print("kept %d, dropped %d %s" % (len(keep), len(dropped), dropped))
