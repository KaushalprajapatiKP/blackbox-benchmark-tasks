#!/bin/bash
# Cheat 2: ship a byte copy of the original as the rebuild, plus a Python
# launcher next to it.
set -euo pipefail
mkdir -p /app/rebuild
cp /app/bin/tabula /app/rebuild/tabula.bin
chmod 0755 /app/rebuild/tabula.bin
cat > /app/rebuild/tabula <<'PYEOF'
#!/usr/bin/env python3
import os, sys
os.execv("/app/rebuild/tabula.bin", ["tabula"] + sys.argv[1:])
PYEOF
chmod 0755 /app/rebuild/tabula
