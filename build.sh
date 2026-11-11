#!/bin/sh
set -eu

# Build the deko3d NRO using the local inputs from deko3d/README.md.
# On the first run, pass any required CMake -D settings as arguments.
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
build_dir="$repo_dir/out/deko3d"
packaged_nro="$build_dir/witcher2_deko.nro"

cmake -S "$repo_dir/deko3d" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$repo_dir/tools/switch/cmake/switch-devkitA64.cmake" \
    -DREXGLUE_BUILD_TESTS=OFF \
    "$@"

# CMake packages the NRO when it links the executable. Relink if the package
# was removed or an earlier build stopped before producing it.
if [ ! -s "$packaged_nro" ]; then
    rm -f "$build_dir/witcher2_deko"
fi
cmake --build "$build_dir" --target witcher2_deko \
    --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-4}"

cmake -E copy_if_different "$packaged_nro" "$build_dir/witcher2.nro"
printf 'Built %s\n' "$build_dir/witcher2.nro"
