#!/bin/bash
# Upload WASM build files to the server
# Usage: ./deploy.sh [server_ip]
# Run this from your local machine (Windows Git Bash)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
REMOTE_USER="root"
REMOTE_HOST="${1:-YOUR_SERVER_IP}"
REMOTE="$REMOTE_USER@$REMOTE_HOST"
REMOTE_TMP="/tmp/kys_deploy"

FILES=(kyschess.html kyschess.js kyschess.wasm kyschess.data)

echo "=== Checking build files ==="
for f in "${FILES[@]}"; do
    if [ ! -f "$BUILD_DIR/$f" ]; then
        echo "ERROR: $BUILD_DIR/$f not found. Run rebuild.sh first."
        exit 1
    fi
    echo "  $(ls -lh "$BUILD_DIR/$f" | awk '{print $5, $NF}')"
done

echo ""
echo "=== Uploading to $REMOTE ==="
ssh "$REMOTE" "mkdir -p $REMOTE_TMP"
scp "${FILES[@]/#/$BUILD_DIR/}" "$REMOTE:$REMOTE_TMP/"

echo ""
echo "Done. Now SSH into the server and run deploy_server.sh"
