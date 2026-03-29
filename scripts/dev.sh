#!/bin/bash
# Local dev server — serves the client with live reload
# Usage: ./scripts/dev.sh [port]

PORT=${1:-8080}
DIR="$(cd "$(dirname "$0")/../client" && pwd)"

echo "🎹 Anarack dev server → http://localhost:$PORT"
echo "   Serving: $DIR"
echo "   Ctrl+C to stop"
echo ""

# Use Python's http.server — simple and no dependencies
cd "$DIR" && python3 -m http.server "$PORT"
