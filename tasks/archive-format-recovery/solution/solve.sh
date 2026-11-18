#!/bin/bash
# Reference solution: install the hand-written decoder as /app/unvpack.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
install -m 0755 "$HERE/unvpack.py" /app/unvpack
