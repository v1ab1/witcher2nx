#!/usr/bin/env python3
"""Generate the deko3d shader metadata and packages from local game data."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile


def extract(inputs, destination, find_headers, shader_hash):
    for source in inputs:
        data = source.read_bytes()
        for offset in find_headers(data):
            flags, virtual_size, physical_size = struct.unpack_from(">III", data, offset)
            container = data[offset:offset + virtual_size + physical_size]
            stage = "vs" if flags & 1 else "ps"
            (destination / f"{stage}_{shader_hash(container)}.bin").write_bytes(container)


def fix_hlsl(source, destination):
    text = source.read_text()
    if source.stem.startswith("ps_"):
        text, changes = re.subn(r"\bcubeMapDirections\[2\](?=;)", "cubeMapDirections[4]", text)
        expected = 1
    else:
        text, changes = re.subn(
            r"^(\s*)(in float4 iPosition([5-7]) : POSITION\3,)$",
            r"\1[[vk::location(\3)]] \2", text, flags=re.MULTILINE,
        )
        expected = 3
    if changes != expected:
        raise ValueError(f"Unexpected HLSL layout in {source.name}")
    (destination / source.name).write_text(text)


def seed_cache(glsl, dksh, destination):
    with destination.open("wb") as output:
        output.write(struct.pack("<4sI16s", b"BDKC", 1, b"uam110-sw8-bnx4"))
        seen = set()
        for path in sorted(glsl.glob("*.glsl")):
            key = 0xCBF29CE484222325
            for byte in path.read_bytes():
                key = ((key ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
            if key in seen:
                continue
            code = (dksh / f"{path.stem}.dksh").read_bytes()
            output.write(struct.pack("<QI", key, len(code)))
            output.write(code)
            seen.add(key)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sa2-source", required=True, type=Path)
    parser.add_argument("--xenos-recomp", required=True, type=Path)
    parser.add_argument("--spirv-cross", default="spirv-cross")
    parser.add_argument("--uam", default="uam")
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("inputs", nargs="+", type=Path,
                        help="Unpacked XEX image, Data/*shader.cache and optional captured shader containers")
    args = parser.parse_args()
    sa2_source = args.sa2_source.resolve()
    sys.path.insert(0, str(sa2_source / "tools"))
    import deko3d_shader_package as package
    import deko3d_shaders as convert
    from sa2_dump_shaders import find_shader_headers, xxhash

    convert.SPIRV_CROSS = args.spirv_cross
    convert.UAM = args.uam
    package.ZSTD = "zstd"
    shader_common = sa2_source / "app/thirdparty/reblue-XenosRecomp/XenosRecomp/shader_common.h"
    xenos = args.xenos_recomp.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=out) as temporary:
        work = Path(temporary)
        containers = work / "containers"
        overrides = work / "overrides"
        selected = work / "selected"
        for directory in (containers, overrides, selected, work / "glsl", work / "vk", work / "dksh"):
            directory.mkdir()
        extract(args.inputs, containers, find_shader_headers, xxhash.xxh3_64_hexdigest)
        if not any(containers.iterdir()):
            parser.error("No shader containers found in the inputs")

        for stem in ("ps_6d0cc9a7fc66686b", "vs_ff563f4eebb07b71"):
            container = containers / f"{stem}.bin"
            if container.is_file():
                shutil.copyfile(container, selected / container.name)
        if any(selected.iterdir()):
            subprocess.run([xenos, selected, work / "selected.cpp", shader_common, work / "hlsl"], check=True)
            for shader in sorted((work / "hlsl").glob("*.hlsl")):
                fix_hlsl(shader, overrides)

        spirv = work / "spirv"
        env = dict(os.environ, BANJO_DEKO3D_SPIRV_DIR=str(spirv), BANJO_HLSL_OVERRIDE=str(overrides))
        subprocess.run([xenos, containers, work / "shader_cache.cpp", shader_common], env=env, check=True)
        failures = sorted(spirv.glob("*.fail"))
        shaders = sorted(spirv.glob("*.spv"))
        if failures or not shaders:
            raise SystemExit(f"XenosRecomp emitted {len(shaders)} shaders and {len(failures)} failures")
        with ThreadPoolExecutor(args.jobs) as workers:
            for stem, reason, detail, _, _ in workers.map(lambda path: convert.one(str(path), str(work)), shaders):
                if reason:
                    raise SystemExit(f"{stem}: {reason}: {detail}")

        sys.argv = [sys.argv[0], "--glsl", str(work / "glsl"), "--out", str(work)]
        package.main()
        seed_cache(work / "glsl", work / "dksh", work / "deko3d_shaders.bin")
        for name in ("shader_cache.cpp", "deko3d_glsl.bin", "deko3d_shaders.bin"):
            (work / name).replace(out / name)


if __name__ == "__main__":
    main()
