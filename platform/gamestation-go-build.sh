#!/usr/bin/env bash
# Build the software-rendered Linux/armhf EDuke32 executable for Gamestation Go.

set -euo pipefail

: "${SYSROOT:?Set SYSROOT to the Gamestation Go SDK sysroot.}"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CROSS="${CROSS:-arm-linux-gnueabihf-}"
SDLCONFIG="${SDLCONFIG:-${CROSS}sdl2-config}"
OUTPUT_DIR="${OUTPUT_DIR:-$root/package/gamestation-go}"
BUILD_DIR="${BUILD_DIR:-$root/obj/gamestation-go}"
TARGET_CFLAGS="${TARGET_CFLAGS:-}"
TARGET_LDFLAGS="${TARGET_LDFLAGS:-}"
RUNTIME_LIBDIR="${RUNTIME_LIBDIR:-}"

command -v "${CROSS}gcc" >/dev/null
command -v "${CROSS}g++" >/dev/null
command -v "$SDLCONFIG" >/dev/null

make -C "$root" \
    PLATFORM=LINUX \
    ARCH=armv7l \
    CROSS="$CROSS" \
    SDLCONFIG="$SDLCONFIG" \
    HAVE_GTK2=0 \
    STARTUP_WINDOW=0 \
    USE_OPENGL=0 \
    USE_LIBVPX=0 \
    USE_MIMALLOC=0 \
    obj="$BUILD_DIR" \
    "CFLAGS=--sysroot=$SYSROOT -march=armv7-a -mfloat-abi=hard -mfpu=neon -DEDUKE32_GAMESTATION_GO $TARGET_CFLAGS" \
    "CXXFLAGS=--sysroot=$SYSROOT -march=armv7-a -mfloat-abi=hard -mfpu=neon -DEDUKE32_GAMESTATION_GO $TARGET_CFLAGS" \
    "LDFLAGS=--sysroot=$SYSROOT $TARGET_LDFLAGS" \
    eduke32

mkdir -p "$OUTPUT_DIR"
cp "$root/eduke32" "$OUTPUT_DIR/eduke32"
cp "$root/platform/gamestation-go-gamecontrollerdb.txt" "$OUTPUT_DIR/gamecontrollerdb.txt"

if [[ -n "$RUNTIME_LIBDIR" ]]; then
    mkdir -p "$OUTPUT_DIR/lib"
    for library in libSDL2-2.0.so.0 libFLAC.so.8 libogg.so.0 libasound.so.2 libatomic.so.1 libstdc++.so.6 libgcc_s.so.1; do
        cp -L "$RUNTIME_LIBDIR/$library" "$OUTPUT_DIR/lib/$library"
    done
fi

printf 'Built %s\n' "$OUTPUT_DIR/eduke32"
