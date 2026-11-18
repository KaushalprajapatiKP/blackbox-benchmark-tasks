#!/bin/bash
# Grades /app/unvpack against the hidden archives in /tests/data.
mkdir -p /logs/verifier
# The archiver is not available at grading time.
rm -f /app/bin/vpack
hash -r
cd /tests
if python3 -m pytest -q -p no:cacheprovider --tb=line -rf /tests/test_unvpack.py > /logs/verifier/pytest.log 2>&1; then
  echo 1 > /logs/verifier/reward.txt
else
  echo 0 > /logs/verifier/reward.txt
fi
tail -n 80 /logs/verifier/pytest.log
exit 0
