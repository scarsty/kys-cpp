#!/bin/bash

# Check hits per IP from nginx logs in Docker container
CONTAINER=$(docker ps --filter "ancestor=dawuxia" --format "{{.Names}}" | head -n 1)

if [ -z "$CONTAINER" ]; then
    echo "Error: No running container from dawuxia image found"
    exit 1
fi

echo "Analyzing nginx logs..."
echo ""

docker exec "$CONTAINER" bash -c '
LOG_DIR="/var/log/nginx"
LOG_FILE="kys-visits.log"

# Count total days with data
DAYS=0
if [ -f "$LOG_DIR/$LOG_FILE" ]; then
    DAYS=$((DAYS + 1))
fi
for log in "$LOG_DIR/$LOG_FILE".*.gz; do
    if [ -f "$log" ]; then
        DAYS=$((DAYS + 1))
    fi
done

echo "Total days analyzed: $DAYS"
echo "=================================="
echo ""
echo "Hits per IP (masked):"
echo "--------------------------------"

# Combine all logs and count hits per IP
{
    if [ -f "$LOG_DIR/$LOG_FILE" ]; then
        awk "{print \$1}" "$LOG_DIR/$LOG_FILE"
    fi
    for log in "$LOG_DIR/$LOG_FILE".*.gz; do
        if [ -f "$log" ]; then
            zcat "$log" | awk "{print \$1}"
        fi
    done
} | sort | uniq -c | sort -rn | awk "{
    # Mask IP: show first two octets, mask last two
    split(\$2, ip, \".\")
    printf \"%s.%s.xxx.xxx: %d hits\n\", ip[1], ip[2], \$1
}"
'
