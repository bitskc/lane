#!/usr/bin/env bash
# Registers the Lane Raycast extension into Raycast.
# Raycast must be installed and running.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$DIR"

command -v npm >/dev/null || { echo "install: npm not found on PATH" >&2; exit 1; }

echo "Installing Raycast extension dependencies..."
npm install --silent

echo "Building and registering extension with Raycast..."
npx ray develop --non-interactive &
PID=$!
sleep 4
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

echo "✅ Lane extension is now installed in Raycast!"
echo "Open Raycast and search for 'Lane' to see the commands."
