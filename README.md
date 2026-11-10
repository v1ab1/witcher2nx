# Witcher2NX

Experimental Nintendo Switch homebrew port of **The Witcher 2: Assassins of Kings** for Atmosphère. ReXGlue statically recompiles the Xbox 360 PowerPC executable. The current Switch build translates the game's Direct3D calls through Renut and Plume to deko3d.

Build from your own Xbox 360 game dump. This repository contains the port source and build tools.

## Status and performance

The 0.1.32 build reached the tutorial in a modified Ryujinx build. Movement and camera controls were verified. The hardware figures below come from the preceding 0.1.31 build.

| Hardware benchmark | Configuration or result |
| --- | --- |
| Console | Nintendo Switch OLED with an 8 GB RAM mod |
| Mode | Handheld, 1280 × 720 |
| Scene | Tutorial dock |
| CPU clock | 1963 MHz (1963.5 MHz in the profile) |
| GPU clock | 1152 MHz |
| RAM clock | 2333 MHz |
| Game frame rate | **43.2–45.1 FPS** across eight consecutive 10-second gameplay samples |

These results are specific to the scene and the overclocked test unit. Performance at stock clocks, on a standard 4 GB Switch and across later chapters is unverified.

## Known issues

- Audio can stutter or crackle in the first startup video. The cause is still under investigation.
- Infrequent crashes have been observed during gameplay.
- Disc 2 is unsupported. The current runtime reports success from [`XamSwapDisc`](sdk/src/kernel/xam/xam_info.cpp#L322) without switching the mounted data. Progress can stop when the game needs the second disc.
- The deko3d renderer returns a fallback sample count for occlusion queries after their fence completes. Some visibility effects may differ from the original.
- A full playthrough and sustained 60 FPS have yet to be verified.

## Build

Use an extracted copy of disc 1 from the PL/RU Xbox 360 release. Place `default.xex` and the game data in the same directory. The supported executable has Title ID `4E4D0855`, Media ID `414AAB82` and version `0.0.0.4`. The generator verifies its SHA-256.

| File | SHA-256 |
| --- | --- |
| `default.xex` | `bea7c7d831ce285785fd47e2f7f0835db29316b216459d8d249ed0255f3c8ad0` |

- Python 3.11+, Git, CMake 3.25+ and Make or Ninja.
- The host CLI from [ReXGlue 0.10.0](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0). On macOS, install the Xcode Command Line Tools for the codegen memory shim.
- devkitA64, libnx, deko3d, Switch tools and the Switch Expat, zlib and zstd portlibs. [devkitPro setup](https://devkitpro.org/wiki/Getting_Started)
- A pinned SA2-NX checkout and host shader tools. The current Plume configuration also takes a Switch NVK SDK path. [deko3d build guide](deko3d/README.md), [NVK SDK setup](mesa/README.md)

Follow the [deko3d build guide](deko3d/README.md) to generate the guest code and shaders from the same dump. The build produces `out/deko3d/witcher2_deko.nro` and the shader generation step produces the two `.bin` files below.

## Install and run

Copy the generated files and the extracted disc 1 data to the Atmosphère SD card.

```text
sd:/switch/witcher2/
├── witcher2.nro          ← out/deko3d/witcher2_deko.nro
├── deko3d_glsl.bin       ← app/generated/deko3d/deko3d_glsl.bin
├── deko3d_shaders.bin    ← app/generated/deko3d/deko3d_shaders.bin
└── game_root/
    ├── default.xex
    ├── Data/
    └── ... (other extracted game files)
```

Hold **R** while opening an installed Switch title to launch hbmenu in application mode, then select **The Witcher 2 deko3d**. The optional configuration file is `sd:/switch/witcher2/witcher2.toml`. Logs are under `sd:/switch/witcher2/logs/`, including `graphics.log` and, when available, `rex/rex_profile.log` and `rex/rex_crash.log`.

## Source layout

| Directory | Contents |
| --- | --- |
| [deko3d](deko3d/) | Current Switch app, Renut renderer and shader tools |
| [app](app/) | Guest bootstrap, function metadata and earlier Vulkan build target |
| [sdk](sdk/) | ReXGlue SDK fork with Horizon support |
| [mesa](mesa/) | Switch NVK dependency setup |
| [patches](patches/) | Codegen fixes and SDK provenance |
| [tools](tools/) | Code generation, dependency setup and the Switch toolchain file |

See the [third-party notices](THIRD_PARTY_NOTICES.md) for upstream credits and licenses.
