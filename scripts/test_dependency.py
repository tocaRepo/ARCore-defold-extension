#!/usr/bin/env python3
"""Resolve/build an independent consumer from a SHA-256-pinned source archive.

Default: localhost dependency resolution only. --build also invokes the native
build service, subject to the same authorization as scripts/build.py.
"""
import argparse
import configparser
import hashlib
import http.server
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import threading
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from build import BOB_SHA256


def archive_bytes():
    memory = io.BytesIO()
    with zipfile.ZipFile(memory, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        # Defold's native-extension traversal uses explicit ZIP directory
        # entries. Match a real git/GitHub archive, rather than a files-only ZIP.
        directories = {"extension-arcore/"}
        for folder in ("arcore", "arcore_render", "example"):
            directories.add("extension-arcore/" + folder + "/")
            for directory in (ROOT / folder).rglob("*"):
                if directory.is_dir():
                    directories.add("extension-arcore/" + directory.relative_to(ROOT).as_posix() + "/")
        for directory in sorted(directories):
            info = zipfile.ZipInfo(directory, (2026, 1, 1, 0, 0, 0))
            info.external_attr = (0o40755 << 16) | 0x10
            archive.writestr(info, b"")
        # Include the example deliberately: Bob's library include_dirs must keep
        # it out of the consumer's mounted dependency, rather than ZIP filtering.
        for folder in ("arcore", "arcore_render", "example"):
            for file in sorted((ROOT / folder).rglob("*")):
                if file.is_file():
                    info = zipfile.ZipInfo("extension-arcore/" + file.relative_to(ROOT).as_posix(), (2026, 1, 1, 0, 0, 0))
                    archive.writestr(info, file.read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
        info = zipfile.ZipInfo("extension-arcore/game.project", (2026, 1, 1, 0, 0, 0))
        archive.writestr(info, (ROOT / "game.project").read_bytes(), compress_type=zipfile.ZIP_DEFLATED)
    return memory.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bob", type=Path, default=os.environ.get("DEFOLD_BOB"))
    parser.add_argument("--java", default=os.environ.get("DEFOLD_JAVA", "java"))
    parser.add_argument("--build", action="store_true")
    parser.add_argument("--platform", choices=("android", "windows", "linux"), default="android")
    parser.add_argument("--build-server", default=os.environ.get("DEFOLD_BUILD_SERVER", "https://build.defold.com"))
    args = parser.parse_args()
    if not args.bob or hashlib.sha256(args.bob.read_bytes()).hexdigest() != BOB_SHA256:
        raise RuntimeError("Supply the exact pinned Bob 1.13.1 jar with --bob or DEFOLD_BOB.")
    payload = archive_bytes()
    digest = hashlib.sha256(payload).hexdigest()
    work = ROOT / "work/dependency" / digest[:16]
    work.mkdir(parents=True, exist_ok=True)
    (work / (digest + ".zip")).write_bytes(payload)
    consumer = work / "consumer"
    shutil.copytree(ROOT / "tests/consumer", consumer, dirs_exist_ok=True)
    request_path = "/" + digest + ".zip"
    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path != request_path:
                self.send_error(404); return
            self.send_response(200); self.send_header("Content-Length", str(len(payload))); self.send_header("ETag", digest); self.end_headers(); self.wfile.write(payload)
        def log_message(self, *args):
            pass
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    project = (consumer / "game.project").read_text().replace("title = ARCore Consumer", "title = ARCore Consumer\ndependencies#0 = http://127.0.0.1:" + str(server.server_port) + request_path)
    (consumer / "game.project").write_text(project)
    command = [args.java, "--enable-native-access=ALL-UNNAMED", "-jar", str(args.bob.resolve())]
    print("Pinned dependency archive SHA256:", digest, flush=True)
    try:
        subprocess.run(command + ["resolve"], cwd=consumer, check=True)
        # Resolve downloads the immutable archive. Its root bootstrap is never
        # copied into the consuming app, which owns all three startup resources.
        config = configparser.ConfigParser()
        config.read(consumer / "game.project")
        assert config["bootstrap"]["main_collection"] == "/consumer/main.collectionc"
        assert config["bootstrap"]["render"] == "/consumer/render.renderc"
        assert config["input"]["game_binding"] == "/consumer/input.input_bindingc"
        assert not (consumer / "example").exists()
        if args.build:
            subprocess.run([sys.executable, ROOT / "scripts/build.py", args.platform, "--bob", args.bob.resolve(), "--java", args.java,
                            "--root", consumer, "--build-server", args.build_server], check=True)
            compiled = (consumer / "build" / args.platform / "game.projectc").read_text()
            assert "/consumer/main.collectionc" in compiled and "/consumer/render.renderc" in compiled
            assert "/example/" not in compiled
            assert not (consumer / "build" / args.platform / "example").exists()
            if args.platform == "android":
                from verify_android_apk import verify
                apk = consumer / "bundles/android/ARCoreConsumer/ARCoreConsumer.apk"
                verify(apk, "libARCoreConsumer.so", "com.defold.arcore.consumer")
                with zipfile.ZipFile(apk) as package:
                    assert b"ARCore experimental extension initialized" in package.read("lib/arm64-v8a/libARCoreConsumer.so")
        print("PASS independent consumer: own bootstrap/input/render, pinned archive resolved" + (", native build and library boundary verified" if args.build else " (native build not requested)"))
    finally:
        server.shutdown(); server.server_close(); thread.join()


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, AssertionError, subprocess.CalledProcessError) as error:
        sys.exit("FAIL: " + str(error))
