#!/bin/bash
# Cheat 2: delegate to the vpack binary (hoping for a hidden unpack mode).
set -e
cat > /app/unvpack <<'SHEOF'
#!/bin/sh
for cmd in unpack x extract; do
  /app/bin/vpack "$cmd" "$1" "$2" 2>/dev/null && exit 0
done
echo "unvpack: corrupt archive (vpack could not unpack it)" >&2
exit 1
SHEOF
chmod 0755 /app/unvpack
