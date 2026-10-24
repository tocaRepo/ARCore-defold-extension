#!/usr/bin/env python3
"""Build with pinned Bob 1.13.1. Native builds send sources to the configured Extender.

Set DEFOLD_BOB and DEFOLD_JAVA (or use --bob/--java). DEFOLD_BUILD_SERVER
overrides the default https://build.defold.com. No tool paths are assumed.
"""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import sys

BOB_SHA256 = "8f2b1381fd4d0fb92816403cd0056cb7db5ad4083615be8dcc2d868fb4939938"
ENGINE_SHA = "574678c7d44be490d874fbed2d0ae6211feec4d9"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("platform", choices=["android", "windows", "linux"])
    parser.add_argument("--bob", type=Path, default=os.environ.get("DEFOLD_BOB"))
    parser.add_argument("--java", default=os.environ.get("DEFOLD_JAVA", "java"))
    parser.add_argument("--build-server", default=os.environ.get("DEFOLD_BUILD_SERVER", "https://build.defold.com"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--resolve-only", action="store_true")
    args = parser.parse_args()
    if not args.bob or not args.bob.is_file():
        raise RuntimeError("Set DEFOLD_BOB or --bob to the official Bob 1.13.1 jar.")
    if hashlib.sha256(args.bob.read_bytes()).hexdigest() != BOB_SHA256:
        raise RuntimeError("Bob SHA-256 does not match the pinned 1.13.1 artifact.")
    command = [args.java, "--enable-native-access=ALL-UNNAMED", "-jar", str(args.bob.resolve())]
    version = subprocess.check_output(command + ["--version"], text=True)
    if "1.13.1" not in version or ENGINE_SHA not in version:
        raise RuntimeError("Bob/engine version mismatch.")
    print(version.strip())
    platform = {"android": "arm64-android", "windows": "x86_64-win32", "linux": "x86_64-linux"}[args.platform]
    options = ["--platform", platform, "--variant", "debug", "--max-cpu-threads", "4", "--build-server", args.build_server,
               "--output", "build/" + args.platform, "--bundle-output", "bundles/" + args.platform, "--archive"]
    if args.platform == "android":
        options += ["--architectures", "arm64-android", "--bundle-format", "apk"]
    tasks = ["resolve"] if args.resolve_only else ["resolve", "build", "bundle"]
    print("Native builds upload extension build inputs to:", args.build_server, flush=True)
    subprocess.run(command + options + tasks, cwd=args.root.resolve(), check=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit("FAIL: " + str(error))
