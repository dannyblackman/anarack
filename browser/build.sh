#!/bin/bash
# Build the browser demo for GitHub Pages deployment.
# Copies rev2-panel.html from the plugin, plus browser assets, into dist/.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
DIST="$SCRIPT_DIR/dist"

rm -rf "$DIST"
mkdir -p "$DIST"

# Copy the panel HTML (shared with plugin)
cp "$PROJECT_ROOT/plugin/ui/rev2-panel.html" "$DIST/rev2-panel.html"

# Copy browser assets
cp "$SCRIPT_DIR/juce-shim.js" "$DIST/"
cp "$SCRIPT_DIR/index.html" "$DIST/"

echo "Built browser demo in $DIST/"
echo "  To test: cd $DIST && python3 -m http.server 3000"
echo "  Then open: http://localhost:3000?host=anarack.local:8765"
