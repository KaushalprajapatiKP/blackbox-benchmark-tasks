#!/bin/bash
# Authoring only: record the hidden session set from the binary inside the task image.
# Run from the task directory:  bash authoring/build_sessions.sh
set -euo pipefail
SEED=20261118
PER=32
docker build -q -t qsrv-task-env environment/ >/dev/null
docker run --rm -v "$PWD":/w -w /w qsrv-task-env bash -c "
set -e
python3 authoring/gen_sessions.py $SEED $PER /tmp/gen.json
for k in 1 2 3; do
  python3 authoring/record.py record '[\"/app/bin/qsrv\"]' /tmp/gen.json /tmp/rec\$k.json 4
done
python3 authoring/stable.py /tmp/rec1.json /tmp/rec2.json /tmp/rec3.json tests/sessions.json
python3 -c \"import hashlib,os;p='/app/bin/qsrv';print(hashlib.sha256(open(p,'rb').read()).hexdigest(), os.path.getsize(p))\" > tests/qsrv.sha256
cat tests/qsrv.sha256
"
