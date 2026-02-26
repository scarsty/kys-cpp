#!/bin/bash
# Deploy WASM files from /tmp into the Docker container
# Run this on the server after deploy.sh uploads the files

set -e

TMP_DIR="/tmp/kys_deploy"
DEST="/var/www/html/kys"
FILES=(kyschess.html kyschess.js kyschess.wasm kyschess.data)

CID=$(docker ps -q)
if [ -z "$CID" ]; then
    echo "ERROR: No running Docker container found."
    exit 1
fi
echo "Container: $CID"

docker exec "$CID" mkdir -p "$DEST"

FAILED=0
for f in "${FILES[@]}"; do
    if [ ! -f "$TMP_DIR/$f" ]; then
        echo "ERROR: $TMP_DIR/$f not found."
        FAILED=1
        continue
    fi
    echo "  Copying $f ..."
    if ! cat "$TMP_DIR/$f" | docker exec -i "$CID" tee "$DEST/$f" > /dev/null; then
        echo "ERROR: Failed to copy $f"
        FAILED=1
    fi
done

if [ "$FAILED" -ne 0 ]; then
    echo ""
    echo "ERROR: Some files failed to copy. Not cleaning up $TMP_DIR."
    exit 1
fi

echo ""
echo "=== Pre-compressing .wasm and .data ==="
docker exec "$CID" gzip -kf "$DEST/kyschess.wasm"
docker exec "$CID" gzip -kf "$DEST/kyschess.data"

echo ""
echo "=== Verifying ==="
docker exec "$CID" ls -lh "$DEST/"

# rm -rf "$TMP_DIR"

echo ""
echo "Done. App at https://tiexuedanxin.net/kys/kyschess.html"
