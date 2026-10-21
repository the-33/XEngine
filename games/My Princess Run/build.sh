#!/usr/bin/env bash
set -euo pipefail
GAME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$GAME_DIR/../.." && pwd)"
exec python3 "$ROOT/tools/build.py" game --project-dir "$GAME_DIR" "$@"
