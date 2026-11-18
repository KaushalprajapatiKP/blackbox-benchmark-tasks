#!/bin/bash
# Verifier: remove the original binary (and copies), then replay recorded sessions.
mkdir -p /logs/verifier
python3 /tests/scrub.py
cd /tests
python3 -m pytest -q -rA -p no:cacheprovider test_replay.py
rc=$?
if [ $rc -eq 0 ]; then
    echo 1 > /logs/verifier/reward.txt
else
    echo 0 > /logs/verifier/reward.txt
fi
exit 0
