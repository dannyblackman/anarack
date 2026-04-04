#!/bin/bash
# Deploy browser demo to GitHub Pages (gh-pages branch).
# Run from the project root: bash browser/deploy.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Build
bash "$SCRIPT_DIR/build.sh"

# Deploy to gh-pages
cd /tmp
rm -rf anarack-ghpages
git clone --branch gh-pages --single-branch --depth 1 \
  https://github.com/dannyblackman/anarack.git anarack-ghpages
cd anarack-ghpages

# Replace files (keep CNAME)
rm -f index.html juce-shim.js rev2-panel.html
cp "$SCRIPT_DIR/dist/"* .

git add -A
if git diff --cached --quiet; then
  echo "No changes to deploy."
else
  git commit -m "Update browser demo"
  git push
  echo "Deployed to GitHub Pages!"
fi

cd /tmp && rm -rf anarack-ghpages
