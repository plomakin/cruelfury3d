#!/usr/bin/env bash
# Publishes a compiled .click revision to OpenStore (open-store.io)
# Usage:
#   OPENSTORE_API_KEY="<your-key>" ./scripts/publish_openstore.sh [changelog]
set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
APP_ID="$(python3 -c "import json; print(json.load(open('${PROJ}/click/manifest.json'))['name'])")"
VERSION="$(python3 -c "import json; print(json.load(open('${PROJ}/click/manifest.json'))['version'])")"
ARCH="$(python3 -c "import json; print(json.load(open('${PROJ}/click/manifest.json'))['architecture'])")"
CLICK_FILE="${PROJ}/release/${APP_ID}_${VERSION}_${ARCH}.click"
CHANGELOG="${1:-v1.1.0: Full mouse and keyboard support for Ubuntu Touch convergence/desktop — WASD + Arrow keys movement, smooth hover mouselook (no click-drag needed, with pointer warp + Wayland edge-turn fallback), LMB fire, RMB Fan burst, MMB/F Mighty Boot, Space/Shift dash, E interact, and 1-4 / Mouse Wheel weapon switching, plus Touch HUD Auto/On/Off toggle (Tab).}"

if [ -z "${OPENSTORE_API_KEY:-}" ]; then
    echo "ERROR: Set OPENSTORE_API_KEY from https://open-store.io/manage (Account -> API Key)." >&2
    exit 1
fi

if [ ! -f "$CLICK_FILE" ]; then
    echo "ERROR: Click file not found at $CLICK_FILE" >&2
    exit 1
fi

CURL_CMD=(curl -4 -fSL)
if command -v devbox >/dev/null 2>&1; then
    CURL_CMD=(devbox curl -4 -fSL)
fi

echo "Uploading $(basename "$CLICK_FILE") to OpenStore (${APP_ID} v${VERSION})..."
"${CURL_CMD[@]}" -X POST \
    "https://open-store.io/api/v4/manage/${APP_ID}/revision?apikey=${OPENSTORE_API_KEY}" \
    -F "file=@${CLICK_FILE}" \
    -F "channel=noble" \
    -F "changelog=${CHANGELOG}"
echo
echo "Upload complete! Check https://open-store.io/app/${APP_ID}"
