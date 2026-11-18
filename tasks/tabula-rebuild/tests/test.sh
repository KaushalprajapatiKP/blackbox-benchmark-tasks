#!/bin/bash
# Grades /app/rebuild/tabula against cases recorded from the original binary.
mkdir -p /logs/verifier
cd /tests || exit 1
python3 -m pytest -q -rA -p no:cacheprovider /tests/test_outputs.py 2>&1 | tee /logs/verifier/pytest.log
status=${PIPESTATUS[0]}
if [ "$status" -eq 0 ]; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi
exit 0
