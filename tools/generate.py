"""Recompile the supported Xbox 360 executable and apply the port's fixes."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
APP = ROOT / "app"
XEX_SHA256 = "bea7c7d831ce285785fd47e2f7f0835db29316b216459d8d249ed0255f3c8ad0"


def generate(rexglue, xex, functions, stage, env):
    stage.mkdir()
    shutil.copyfile(functions, stage / "functions.toml")
    manifest = stage / "witcher2_manifest.toml"
    manifest.write_text(
        '[project]\nname = "witcher2"\nsdk_version = "0.10.0"\n'
        f"game_root = {json.dumps(str(xex.parent), ensure_ascii=False)}\n\n"
        '[entrypoint]\nout_directory_path = "out"\nincludes = ["functions.toml"]\n'
        f"file_path = {json.dumps(str(xex), ensure_ascii=False)}\n",
        encoding="utf-8",
    )
    subprocess.run([rexglue, "codegen", manifest.name], cwd=stage, env=env, check=True)
    return stage / "out"


def recover_vtables(main, recovery, functions, output):
    """Add missing vtable methods whose generated calls can be resolved."""
    registered = set(re.findall(
        r"\{ 0x([0-9A-F]+), \w+ \}", (main / "witcher2_init.cpp").read_text()
    ))
    wanted = set(re.findall(
        r'^"0x([0-9A-F]+)" = \{\} # Data references:', functions.read_text(), re.M
    )) - registered
    method_pattern = re.compile(r"^DEFINE_REX_FUNC\(sub_([0-9A-F]{8})\) \{\n(.*?)^\}", re.M | re.S)
    invalid = ("REX_UNIMPLEMENTED", "Unresolved branch", "FATAL: unresolved", "ERROR:")
    methods = {}
    for path in recovery.glob("witcher2_recomp.*.cpp"):
        for address, body in method_pattern.findall(path.read_text()):
            if address in wanted and not any(marker in body for marker in invalid):
                if address in methods:
                    raise ValueError(f"Duplicate recovered method {address}")
                methods[address] = body

    calls = re.compile(r"\bsub_([0-9A-F]{8})\(ctx, base\)")
    while True:
        available = registered | methods.keys()
        unavailable = {address for address, body in methods.items()
                       if not set(calls.findall(body)) <= available}
        if not unavailable:
            break
        for address in unavailable:
            del methods[address]

    # These dispatch targets were observed during boot and gameplay.
    required = {"82472828", "82D7D648", "82D7D668", "82F71660", "82F71680"}
    missing = required - (registered | methods.keys())
    if missing:
        raise ValueError(f"Missing required vtable methods: {', '.join(sorted(missing))}")

    declarations, definitions, entries = [], [], []
    for address, body in sorted(methods.items()):
        signature = f"void GuestFunction{address}(PPCContext& ctx, uint8_t* base)"
        declarations.append(signature + ";")
        body = calls.sub(
            lambda match: f"GuestFunction{match[1]}(ctx, base)" if match[1] in methods else match[0],
            body,
        )
        definitions.append(f"{signature} {{\n{body}}}\n")
        entries.append(f"    {{0x{address}, GuestFunction{address}}},")

    parts = [
        "// Generated from the verified vtable entries in recovery/functions.toml.",
        "\n".join(declarations),
        "\n".join(definitions),
        "const PPCFuncMapping kRecoveredVtableMethods[] = {\n" + "\n".join(entries) + "\n};",
    ]
    output.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
    print(f"Recovered {len(methods)}/{len(wanted)} vtable methods.", flush=True)


def install_generated(source, destination, backup):
    if destination.exists():
        destination.rename(backup)
    try:
        source.rename(destination)
    except OSError:
        if backup.exists():
            backup.rename(destination)
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rexglue", required=True, type=Path, help="ReXGlue 0.10.0 executable")
    parser.add_argument("--game-root", required=True, type=Path, help="Extracted Xbox 360 disc 1")
    args = parser.parse_args()

    xex = args.game_root.resolve() / "default.xex"
    if not xex.is_file():
        parser.error(f"Missing {xex}")
    with xex.open("rb") as game:
        if hashlib.file_digest(game, "sha256").hexdigest() != XEX_SHA256:
            parser.error(f"Unsupported default.xex. Expected SHA-256 {XEX_SHA256}")

    rexglue = args.rexglue.resolve()
    version = subprocess.check_output([rexglue, "--version"], text=True).strip()
    if version != "0.10.0":
        parser.error(f"ReXGlue 0.10.0 is required, found {version}")

    with tempfile.TemporaryDirectory(dir=APP, prefix=".codegen-") as temp:
        stage = Path(temp)
        env = os.environ.copy()
        if sys.platform == "darwin":
            shim = stage / "rexglue_backing.dylib"
            subprocess.run(
                ["cc", "-dynamiclib", ROOT / "tools/rexglue_backing.c", "-o", shim], check=True
            )
            libraries = env.get("DYLD_INSERT_LIBRARIES")
            env["DYLD_INSERT_LIBRARIES"] = f"{shim}:{libraries}" if libraries else str(shim)
            env["WITCHER2_REX_BACKING_DIR"] = str(stage)

        generated = generate(rexglue, xex, APP / "functions.toml", stage / "main", env)
        recovery_functions = APP / "recovery/functions.toml"
        recovery = generate(rexglue, xex, recovery_functions, stage / "recovery", env)

        subprocess.run(
            ["git", "apply", "--unidiff-zero", f"--directory={generated.relative_to(ROOT)}",
             ROOT / "patches/generated-witcher2.patch"],
            cwd=ROOT, check=True,
        )
        output = stage / "generated"
        output.mkdir()
        recover_vtables(generated, recovery, recovery_functions, output / "recovered_vtables.inc")
        generated.rename(output / "default")
        install_generated(output, APP / "generated", stage / "previous")

    print(f"Recompiled sources are in {APP / 'generated'}")


if __name__ == "__main__":
    main()
