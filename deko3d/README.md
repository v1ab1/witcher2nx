# deko3d build

The Switch executable translates the game's Xbox 360 Direct3D calls through
Renut and Plume to deko3d. The [main README](../README.md) records the current
test results and known issues.

The [main build instructions](../README.md) list the supported game dump,
devkitPro tools and SDK dependencies. Install the deko3d devkitPro package as
well. The pinned Plume version requires the NVK SDK path at configure time.
The deko3d target links libdeko3d.

## Generate local files

Use the same dump to generate both the guest code and shader data. Keep the
generated files under `app/generated` or `out`.

```sh
python3 tools/fetch_thirdparty.py
python3 tools/generate.py --rexglue /path/to/rexglue --game-root /path/to/disc1

git clone https://github.com/ChanseyIsTheBest/sa2-nx.git /path/to/sa2-nx
git -C /path/to/sa2-nx checkout baf523b8e93a1b02d8b73d13aebf86780f8cc0c6
```

Prepare host builds of XenosRecomp (`XENOS_RECOMP_RENUT=ON`,
`XENOS_RECOMP_DXIL=OFF`) and uam (`uam_tool`) from the pinned SA2-NX
checkout, with DXC for the host. Install SPIRV-Cross, the zstd command-line
tool and the Python `xxhash` module. See the
[SA2-NX shader tool instructions](https://github.com/ChanseyIsTheBest/sa2-nx/blob/baf523b8e93a1b02d8b73d13aebf86780f8cc0c6/docs/deko3d-renderer-guide.md).

Pass the decrypted and decompressed image of `default.xex` and the
`Data/*shader.cache` files from disc 1. The SA2-NX ReXGlue codegen fork can
dump the loaded image when `REXGLUE_DUMP_IMAGE` is set to an absolute path.

```sh
python3 deko3d/generate_shaders.py \
  --sa2-source /path/to/sa2-nx \
  --xenos-recomp /path/to/XenosRecomp \
  --spirv-cross /path/to/spirv-cross --uam /path/to/uam \
  --out app/generated/deko3d \
  /path/to/default_image.bin /path/to/disc1/Data/*shader.cache
```

Renut dumps any missing shader container to `shader_capture/` next to the
NRO. Pass the captured `.bin` files as additional inputs and rebuild to
include those shaders.

## Build and install

```sh
export DEVKITPRO=/opt/devkitpro
cmake -S deko3d -B out/deko3d \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/switch/cmake/switch-devkitA64.cmake" \
  -DREXGLUE_SWITCH_NVK_SDK=/path/to/nvk-sdk \
  -DW2_SA2_SOURCE_DIR=/path/to/sa2-nx \
  -DREXGLUE_BUILD_TESTS=OFF
cmake --build out/deko3d --target witcher2_deko --parallel
```

If deko3d is installed separately from libnx, set `W2_DEKO3D_DEVKITPRO` to
the devkitPro layout used by Plume and `W2_DEKO3D_INCLUDE_DIR` to the folder
containing its deko3d headers.

Follow the [SD card layout and launch instructions](../README.md#install-and-run)
in the main README.
