#!/bin/bash
# Package WASM build + game assets into a tarball and upload to remote server.
# Usage: ./deploy.sh [server_ip]
# Run this from your local machine (Windows Git Bash)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$SCRIPT_DIR/dist"
GAME_DIR="$BUILD_DIR/game"
REMOTE_USER="root"
REMOTE_HOST="${1:-YOUR_SERVER_IP}"
REMOTE="$REMOTE_USER@$REMOTE_HOST"

TAR_NAME="kys-deploy.tar.gz"
STAGING="/tmp/kys-deploy"

BUILD_FILES=(kyschess.html kyschess.js kyschess.wasm)

# --- Check build files ---
echo "=== Checking build files ==="
for f in "${BUILD_FILES[@]}"; do
    if [ ! -f "$BUILD_DIR/$f" ]; then
        echo "ERROR: $BUILD_DIR/$f not found. Run rebuild.sh first."
        exit 1
    fi
    echo "  $(ls -lh "$BUILD_DIR/$f" | awk '{print $5, $NF}')"
done

if [ ! -d "$GAME_DIR" ]; then
    echo "ERROR: Game assets not found at $GAME_DIR"
    exit 1
fi

# --- Stage and zip ---
echo ""
echo "=== Packaging ==="
rm -rf "$STAGING"
mkdir -p "$STAGING"

cp "${BUILD_FILES[@]/#/$BUILD_DIR/}" "$STAGING/"
cp -r "$GAME_DIR" "$STAGING/game"

rm -f "/tmp/$TAR_NAME"
tar czf "/tmp/$TAR_NAME" -C /tmp kys-deploy/
echo "  Created /tmp/$TAR_NAME ($(ls -lh "/tmp/$TAR_NAME" | awk '{print $5}'))"

# --- Upload ---
echo ""
echo "=== Uploading to $REMOTE ==="
scp "/tmp/$TAR_NAME" "$REMOTE:/tmp/"

echo ""
echo "Done. On the remote host run:"
echo "  cd /tmp && tar xzf $TAR_NAME"
echo "  CONTAINER=\$(docker ps -q)"
echo "  docker exec \$CONTAINER mkdir -p /var/www/html/kys"
echo "  docker cp /tmp/kys-deploy/. \$CONTAINER:/var/www/html/kys/"
