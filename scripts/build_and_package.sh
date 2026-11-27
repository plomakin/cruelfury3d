#!/usr/bin/env bash
set -euo pipefail

PROJ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJ"

"${PROJ}/scripts/ensure_deps.sh"

LINKER_FLAGS=""
if command -v mold >/dev/null 2>&1; then
    LINKER_FLAGS="-fuse-ld=mold"
fi

echo "=== [1/2] Compiling Cruel Fury 3D (cruelfury.plm) ==="
cmake -S "$PROJ" -B "${PROJ}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    ${LINKER_FLAGS:+-DCMAKE_EXE_LINKER_FLAGS="$LINKER_FLAGS"}
cmake --build "${PROJ}/build" -j "${CMAKE_BUILD_PARALLEL_LEVEL:-3}"

echo "=== [2/2] Staging arm64 binary & LICENSE into click/ ==="
ARCH_TRIPLET="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || echo aarch64-linux-gnu)"
install -d -m 0755 "${PROJ}/click/lib/${ARCH_TRIPLET}/bin"
install -m 0755 "${PROJ}/build/cruelfury" "${PROJ}/click/lib/${ARCH_TRIPLET}/bin/cruelfury"
cp -f "${PROJ}/LICENSE" "${PROJ}/click/LICENSE"
ls -lh "${PROJ}/click/lib/${ARCH_TRIPLET}/bin/cruelfury"
echo "Cruel Fury 3D build complete!"
