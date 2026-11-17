#!/usr/bin/env bash
set -euo pipefail
missing=()
for pkg in qtmultimedia5-dev libgles2-mesa-dev; do
    if ! dpkg -s "$pkg" >/dev/null 2>&1; then
        missing+=("$pkg")
    fi
done
if [ "${#missing[@]}" -gt 0 ]; then
    echo "Installing missing Qt5/GLES dev headers in devbox: ${missing[*]}"
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends "${missing[@]}"
    sudo apt-get clean
fi
echo "Qt5 Multimedia & GLES2 headers ready."
