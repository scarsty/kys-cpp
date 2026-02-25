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

for f in "${FILES[@]}"; do
    echo "  Copying $f ..."
    docker cp "$TMP_DIR/$f" "$CID:$DEST/$f"
done

echo ""
echo "=== Pre-compressing .wasm and .data ==="
docker exec "$CID" gzip -kf "$DEST/kyschess.wasm"
docker exec "$CID" gzip -kf "$DEST/kyschess.data"

echo ""
echo "=== Verifying ==="
docker exec "$CID" ls -lh "$DEST/"

rm -rf "$TMP_DIR"

echo ""
echo "Done. App at https://tiexuedanxin.net/kys/kyschess.html"
