#!/usr/bin/env bash
# Tiberium Project: builds DXVK 2.6 with cmake/dxvk/dxvk-2.6-ray-query.patch (its Vulkan device made with
# acceleration structures and ray queries when the game asks, for the ray-traced passes; otherwise it is
# DXVK 2.6 as released), for Linux (native, SDL3) or 64-bit Windows (cross-compiled with MinGW).
#
# Usage:
#   build-dxvk-ray-query.sh linux <out dir> [sdl3 include dir]
#   build-dxvk-ray-query.sh win64 <out dir>
#
# linux: <out dir> gets libdxvk_d3d9.so and libdxvk_d3d8.so (with their .so.0 names); point
#        LD_LIBRARY_PATH at it in place of the released DXVK native libraries. The SDL3 headers are
#        found with pkg-config, or given as the third argument.
# win64: <out dir> gets d3d9.dll and d3d8.dll, for the "dxvk" folder next to GeneralsXZH.exe.
# Needs git, meson, ninja and glslangValidator (and x86_64-w64-mingw32-g++ for win64).
set -euo pipefail

TARGET="${1:-}"
OUT="${2:-}"
if [[ "$TARGET" != "linux" && "$TARGET" != "win64" ]] || [[ -z "$OUT" ]]; then
	sed -n '2,15p' "$0"
	exit 1
fi
HERE="$(cd "$(dirname "$0")" && pwd)"
PATCH="$HERE/../../../cmake/dxvk/dxvk-2.6-ray-query.patch"
WORK="${DXVK_WORK_DIR:-$(mktemp -d)}"
SRC="$WORK/dxvk-2.6"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"

if [[ ! -d "$SRC/.git" ]]; then
	git clone --quiet --depth 1 --branch v2.6 https://github.com/doitsujin/dxvk.git "$SRC"
	git -C "$SRC" submodule update --quiet --init --depth 1
fi
if git -C "$SRC" apply --check "$PATCH" 2>/dev/null; then
	git -C "$SRC" apply "$PATCH"
elif ! git -C "$SRC" apply --reverse --check "$PATCH" 2>/dev/null; then
	echo "the patch does not apply to $SRC" >&2
	exit 1
fi

if [[ "$TARGET" == "linux" ]]; then
	BUILD="$WORK/build-linux"
	if [[ -n "${3:-}" ]]; then
		# a pkg-config file for SDL3's headers (DXVK needs only them; it loads SDL3 at run time)
		mkdir -p "$WORK/pc"
		printf 'Name: sdl3\nDescription: SDL3 headers\nVersion: 3.2.0\nCflags: -I%s\nLibs:\n' "$3" > "$WORK/pc/SDL3.pc"
		export PKG_CONFIG_PATH="$WORK/pc${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
	fi
	[[ -d "$BUILD" ]] || meson setup "$BUILD" "$SRC" --buildtype=release -Denable_dxgi=false -Denable_d3d10=false -Denable_d3d11=false
	ninja -C "$BUILD" src/d3d9/libdxvk_d3d9.so.0.20600 src/d3d8/libdxvk_d3d8.so.0.20600
	for lib in d3d9 d3d8; do
		cp "$BUILD/src/$lib/libdxvk_$lib.so.0.20600" "$OUT/"
		ln -sf "libdxvk_$lib.so.0.20600" "$OUT/libdxvk_$lib.so.0"
		ln -sf "libdxvk_$lib.so.0" "$OUT/libdxvk_$lib.so"
	done
else
	BUILD="$WORK/build-win64"
	[[ -d "$BUILD" ]] || meson setup "$BUILD" "$SRC" --cross-file "$SRC/build-win64.txt" --buildtype=release \
		-Denable_dxgi=false -Denable_d3d10=false -Denable_d3d11=false
	ninja -C "$BUILD" src/d3d9/d3d9.dll src/d3d8/d3d8.dll
	cp "$BUILD/src/d3d9/d3d9.dll" "$BUILD/src/d3d8/d3d8.dll" "$OUT/"
fi
echo "DXVK 2.6 with ray queries ($TARGET): $OUT"
