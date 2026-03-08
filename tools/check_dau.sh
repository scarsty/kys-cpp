#!/bin/bash

# Check daily active users from nginx logs in Docker container
CONTAINER=$(docker ps --filter "ancestor=dawuxia" --format "{{.Names}}" | head -n 1)

if [ -z "$CONTAINER" ]; then
    echo "Error: No running container from dawuxia image found"
    exit 1
fi

echo "Daily Active Users (by unique IP):"
echo "==================================="

# Read current log
docker exec "$CONTAINER" bash -c '
LOG_DIR="/var/log/nginx"
LOG_FILE="kys-visits.log"

if [ -f "$LOG_DIR/$LOG_FILE" ]; then
    awk "{print substr(\$4,2,11), \$1}" "$LOG_DIR/$LOG_FILE" | sort -u | cut -d" " -f1 | uniq -c | awk "{print \$2\": \"\$1\" users\"}"
fi

for log in "$LOG_DIR/$LOG_FILE".*.gz; do
    if [ -f "$log" ]; then
        zcat "$log" | awk "{print substr(\$4,2,11), \$1}" | sort -u | cut -d" " -f1 | uniq -c | awk "{print \$2\": \"\$1\" users\"}"
    fi
done
'
