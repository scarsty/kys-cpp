#!/bin/bash
# Package the WASM build into a clean deploy-ready directory.
# Usage: ./package.sh
# Output: ./dist/ (ready for `wrangler pages deploy ./dist/`)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
GAME_DIR="$SCRIPT_DIR/../work/game-dev"
DIST_DIR="$SCRIPT_DIR/dist"

# --- Validate build ---
for f in kyschess.html kyschess.js kyschess.wasm; do
    if [ ! -f "$BUILD_DIR/$f" ]; then
        echo "ERROR: $BUILD_DIR/$f not found. Run rebuild.sh first."
        exit 1
    fi
done

if [ ! -d "$GAME_DIR" ]; then
    echo "ERROR: Game assets not found at $GAME_DIR"
    exit 1
fi

# --- Stage into dist/ ---
echo "=== Packaging into dist/ ==="
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

cp "$BUILD_DIR/index.html" "$DIST_DIR/"
cp "$BUILD_DIR/kyschess.html" "$DIST_DIR/"
cp "$BUILD_DIR/kyschess.js" "$DIST_DIR/"
cp "$BUILD_DIR/kyschess.wasm" "$DIST_DIR/"
mkdir -p "$DIST_DIR/kys"
cp -r "$GAME_DIR" "$DIST_DIR/kys/game"

# Cloudflare Pages headers for SharedArrayBuffer
cat > "$DIST_DIR/_headers" << 'EOF'
/*
  Cross-Origin-Opener-Policy: same-origin
  Cross-Origin-Embedder-Policy: require-corp
EOF

# EdgeOne headers
cp "$SCRIPT_DIR/edgeone.json" "$DIST_DIR/"

# --- Create zip for EdgeOne ---
ZIP_PATH="$SCRIPT_DIR/dist.zip"
rm -f "$ZIP_PATH"
python3 -c "
import zipfile, os, sys
with zipfile.ZipFile(sys.argv[1], 'w', zipfile.ZIP_DEFLATED) as zf:
    for root, dirs, files in os.walk(sys.argv[2]):
        for f in files:
            full = os.path.join(root, f)
            zf.write(full, os.path.relpath(full, sys.argv[2]))
" "$ZIP_PATH" "$DIST_DIR"
echo "  Created $ZIP_PATH ($(ls -lh "$ZIP_PATH" | awk '{print $5}'))"

# --- Summary ---
FILE_COUNT=$(find "$DIST_DIR" -type f | wc -l)
TOTAL_SIZE=$(du -sh "$DIST_DIR" | awk '{print $1}')
echo "  $FILE_COUNT files, $TOTAL_SIZE total"
echo ""
echo "Deploy with:"
echo "  Cloudflare: wrangler pages deploy $DIST_DIR --project-name=kyschess"
echo "  EdgeOne:    upload $ZIP_PATH"
