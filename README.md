# Witcher2NX

This branch contains the experimental [deko3d renderer](deko3d/README.md)
used in the 0.1.14 test build. The original Vulkan build is documented below.

Experimental Nintendo Switch homebrew port of **The Witcher 2: Assassins of Kings**.
It statically recompiles the Xbox 360 executable with ReXGlue and renders through
the Xenos Vulkan backend and Mesa NVK on Horizon.

The 0.1.9 test build reached the tutorial in a modified Ryujinx test setup and
produced audio. Geometry and color corruption remained visible, and menu text
was missing in that run. Physical Switch gameplay and performance for 0.1.9
still need validation.

The repository contains the port source, SDK changes and build tooling. Use your
own game dump for the local build and SD card installation.

## Build the NRO

Requirements:

- Python 3.11+, Git, CMake 3.25+ and a build tool such as Make.
- The host CLI from [ReXGlue 0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0).
  On macOS, install the Xcode command-line tools for the codegen memory shim.
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) with devkitA64, libnx,
  Switch tools and the Switch Expat, zlib and zstd portlibs.
- A Switch NVK SDK prefix containing `lib/libvulkan.a`. See [Mesa setup](mesa/README.md).

Extract disc 1 of the Xbox 360 release so that `default.xex` and the game data
are in the same directory. Code generation currently supports the tested PL/RU
executable with Title ID `4E4D0855`, Media ID `414AAB82` and version `0.0.0.4`.

| File | SHA-256 |
| --- | --- |
| `default.xex` | `bea7c7d831ce285785fd47e2f7f0835db29316b216459d8d249ed0255f3c8ad0` |

From the repository root:

```sh
python3 tools/fetch_thirdparty.py
python3 tools/generate.py --rexglue /path/to/rexglue --game-root /path/to/disc1

export DEVKITPRO=/opt/devkitpro
export DEVKITA64="$DEVKITPRO/devkitA64"
NVK_SDK=/path/to/mesa-horizon-sdk/opt/devkitpro/portlibs/switch

cmake -S app -B out/switch \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/tools/switch/cmake/switch-devkitA64.cmake" \
  -DREXGLUE_SWITCH_NVK_SDK="$NVK_SDK" \
  -DWITCHER2_RENDERER=ON \
  -DREXGLUE_BUILD_TESTS=OFF
cmake --build out/switch --parallel
```

The result is `out/switch/witcher2.nro`. Use the `--parallel` job limit if the
compiler runs low on memory. The generation step also reconstructs missing
vtable methods and applies the [game-specific fixes](patches/README.md).

## Install

Copy `witcher2.nro` and the extracted disc 1 files to the Atmosphère SD card:

```text
sd:/switch/witcher2/
├── witcher2.nro
└── game_root/
    ├── default.xex
    ├── Data/
    └── ... (other disc 1 files)
```

Launch hbmenu using title takeover (hold **R** while opening an installed game),
then select **The Witcher 2 - Hardware Test**. The graphics log is written to
`sd:/switch/witcher2/logs/graphics.log`.

## Project layout

| Directory | Contents |
| --- | --- |
| [app](app/) | Game-specific bootstrap, renderer setup and function metadata |
| [sdk](sdk/) | ReXGlue SDK fork with the Horizon runtime and port fixes |
| [mesa](mesa/) | Switch Vulkan dependency setup |
| [patches](patches/) | Generated-code fixes and SDK provenance |
| [tools](tools/) | Code generation, dependency setup and the Switch toolchain file |

See [third-party notices](THIRD_PARTY_NOTICES.md) for upstream credits and licenses.
