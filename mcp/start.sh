#!/usr/bin/env bash
set -euo pipefail
noryn_mcp_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$noryn_mcp_dir"
if [[ ! -d node_modules ]]; then npm ci; fi
node server/setup.mjs
node server/bridge.mjs &
noryn_bridge_pid=$!
trap 'kill "$noryn_bridge_pid" 2>/dev/null || true' EXIT
npm run dev
