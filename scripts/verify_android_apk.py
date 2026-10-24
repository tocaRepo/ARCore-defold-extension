#!/usr/bin/env python3
"""Inspect ARM64 APK, SDK exports, shared libdl imports, DEX and binary manifest.

Pure Python; no NDK, readelf, aapt, or Android installation required.
"""
import argparse
from pathlib import Path
import re
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]


class Elf:
    def __init__(self, data):
        if data[:6] != b"\x7fELF\x02\x01":
            raise ValueError("Expected ELF64 little-endian ARM64 library")
        if struct.unpack_from("<H", data, 18)[0] != 183:
            raise ValueError("ELF machine is not AArch64")
        self.data = data
        section_offset = struct.unpack_from("<Q", data, 40)[0]
        entry_size, count, names_index = struct.unpack_from("<HHH", data, 58)
        raw = [struct.unpack_from("<IIQQQQIIQQ", data, section_offset + i * entry_size) for i in range(count)]
        names = self.slice(raw[names_index])
        self.sections = {self.string(names, section[0]): section for section in raw}
        self.symbols = {}
        symbols = self.sections[".dynsym"]
        strings = self.slice(raw[symbols[6]])
        for offset in range(symbols[4], symbols[4] + symbols[5], symbols[9]):
            name, info, other, index, value, size = struct.unpack_from("<IBBHQQ", data, offset)
            self.symbols[self.string(strings, name).split("@")[0]] = index
        self.needed = []
        dynamic = self.sections[".dynamic"]
        dynstrings = self.slice(raw[dynamic[6]])
        for offset in range(dynamic[4], dynamic[4] + dynamic[5], dynamic[9] or 16):
            tag, value = struct.unpack_from("<qQ", data, offset)
            if tag == 1:
                self.needed.append(self.string(dynstrings, value))

    def slice(self, section):
        return self.data[section[4]:section[4] + section[5]]

    @staticmethod
    def string(data, offset):
        return data[offset:data.index(b"\0", offset)].decode("utf-8")


def binary_xml(data):
    """Return start elements and typed attributes from Android compiled XML."""
    strings = []
    elements = []
    offset = 8
    while offset < len(data):
        kind, header_size, size = struct.unpack_from("<HHI", data, offset)
        if size < 8 or offset + size > len(data):
            raise ValueError("Malformed binary manifest chunk")
        if kind == 1:
            count, _, flags, start, _ = struct.unpack_from("<IIIII", data, offset + 8)
            offsets = struct.unpack_from("<" + "I" * count, data, offset + header_size)
            for relative in offsets:
                pos = offset + start + relative
                if flags & 0x100:
                    def length(pos):
                        v = data[pos]
                        return (((v & 0x7f) << 8) | data[pos + 1], pos + 2) if v & 0x80 else (v, pos + 1)
                    _, pos = length(pos)
                    n, pos = length(pos)
                    strings.append(data[pos:pos + n].decode("utf-8"))
                else:
                    n = struct.unpack_from("<H", data, pos)[0]; pos += 2
                    if n & 0x8000:
                        n = ((n & 0x7fff) << 16) | struct.unpack_from("<H", data, pos)[0]; pos += 2
                    strings.append(data[pos:pos + n * 2].decode("utf-16-le"))
        elif kind == 0x102:
            _, name, attribute_start, attribute_size, count = struct.unpack_from("<IIHHH", data, offset + 16)
            attrs = {}
            for i in range(count):
                pos = offset + 16 + attribute_start + i * attribute_size
                _, key, raw, _, _, value_type, value = struct.unpack_from("<IIIHBBI", data, pos)
                if value_type == 3:
                    result = strings[value]
                elif value_type == 0x12:
                    result = bool(value)
                elif raw != 0xffffffff:
                    result = strings[raw]
                else:
                    result = value
                attrs[strings[key]] = result
            elements.append((strings[name], attrs))
        offset += size
    return elements


def verify(path, engine_name, package):
    required = set(re.findall(r"X\((Ar\w+)\)", (ROOT / "arcore/src/ar_api.h").read_text()))
    with zipfile.ZipFile(path) as apk:
        names = apk.namelist()
        abis = {name.split("/")[1] for name in names if re.match(r"lib/[^/]+/[^/]+\.so$", name)}
        if abis != {"arm64-v8a"}:
            raise ValueError("APK must contain only arm64-v8a, got " + str(sorted(abis)))
        prefix = "lib/arm64-v8a/"
        engine_bytes = apk.read(prefix + engine_name)
        engine = Elf(engine_bytes)
        if "libdl.so" not in engine.needed or b"libdl.a is a stub" in engine_bytes:
            raise ValueError("Engine does not use Android's real shared libdl.so")
        for symbol in ("dlopen", "dlsym", "dlerror", "dlclose"):
            if engine.symbols.get(symbol, -1) != 0:
                raise ValueError("Engine must import, not define, " + symbol)
        sdk = Elf(apk.read(prefix + "libarcore_sdk_c.so"))
        if not apk.read(prefix + "libarcore_sdk_jni.so").startswith(b"\x7fELF"):
            raise ValueError("ARCore JNI library is missing")
        missing = required - {name for name, index in sdk.symbols.items() if index}
        if missing:
            raise ValueError("SDK missing mandatory exports: " + ", ".join(sorted(missing)))
        dex = b"".join(apk.read(n) for n in names if re.fullmatch(r"classes\d*\.dex", n))
        for cls in (b"Lcom/defold/arcore/ArCoreLoader;", b"Lcom/defold/arcore/CameraPermission;"):
            if cls not in dex:
                raise ValueError("Java bridge absent from DEX: " + cls.decode())
        manifest = binary_xml(apk.read("AndroidManifest.xml"))
        def values(tag, key="name"):
            return {attrs.get(key): attrs for name, attrs in manifest if name == tag}
        if package and package not in values("manifest", "package"):
            raise ValueError("Unexpected application package")
        if "android.permission.CAMERA" not in values("uses-permission"):
            raise ValueError("Camera permission missing")
        features = values("uses-feature")
        if not features.get("android.hardware.camera.ar", {}).get("required", True):
            raise ValueError("Default sample must declare AR Required")
        if "android.hardware.camera.ar" not in features:
            raise ValueError("AR feature missing")
        if values("meta-data").get("com.google.ar.core", {}).get("value") != "required":
            raise ValueError("Default sample ARCore metadata must be required")
        sdks = [a.get("minSdkVersion", 0) for n, a in manifest if n == "uses-sdk"]
        if not sdks or int(sdks[0]) < 27:
            raise ValueError("Android minSdkVersion must be 27 or higher")
        print(f"PASS {path.name}: ARM64 only, shared libdl imports, both SDK libraries, {len(required)} SDK exports, Java bridges, merged AR Required manifest")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    parser.add_argument("--engine", default="libARCoreExample.so")
    parser.add_argument("--package", default="com.defold.arcore.example")
    args = parser.parse_args()
    try:
        verify(args.apk, args.engine, args.package)
    except (OSError, ValueError, KeyError, struct.error, zipfile.BadZipFile) as error:
        sys.exit("FAIL: " + str(error))
