#!/usr/bin/env python3
"""Compile and run production core and real-JVM worker/session regressions.

Windows: Visual Studio C++ Build Tools, JDK 17+ (including headers and jvm.lib).
Linux: g++ or clang++, JDK 17+ (including lib/server/libjvm.so).
No Android device, network, or Defold build service is used by these tests.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def msvc_environment(work):
    if shutil.which("cl"):
        return os.environ.copy()
    base = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)"))
    matches = list((base / "Microsoft Visual Studio").glob("*/BuildTools/VC/Auxiliary/Build/vcvarsall.bat"))
    matches += list((base / "Microsoft Visual Studio").glob("*/*/VC/Auxiliary/Build/vcvarsall.bat"))
    if not matches:
        raise RuntimeError("Install Visual Studio C++ Build Tools, or run from an x64 Native Tools prompt.")
    script = work / "compiler-env.cmd"
    script.write_text('@call "' + str(sorted(matches)[-1]) + '" x64 >nul\n@set\n')
    output = subprocess.check_output(["cmd.exe", "/d", "/c", str(script)], text=True, errors="replace")
    env = os.environ.copy()
    for line in output.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key.upper()] = value
    # Some installations retain an older toolset while vcvarsall's default
    # version file names an uninstalled one. Use an actually installed compiler.
    if not shutil.which("cl", path=env.get("PATH")):
        toolsets = sorted((sorted(matches)[-1].parents[2] / "Tools/MSVC").glob("*/bin/Hostx64/x64/cl.exe"))
        if toolsets:
            compiler = toolsets[-1]
            toolset = compiler.parents[3]
            env["PATH"] = str(compiler.parent) + os.pathsep + env.get("PATH", "")
            env["INCLUDE"] = str(toolset / "include") + os.pathsep + env.get("INCLUDE", "")
            env["LIB"] = str(toolset / "lib/x64") + os.pathsep + env.get("LIB", "")
    return env


def run(command, **kwargs):
    print("+", subprocess.list2cmdline([str(x) for x in command]), flush=True)
    subprocess.run([str(x) for x in command], check=True, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java-home", type=Path, default=os.environ.get("JAVA_HOME"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "c++"))
    parser.add_argument("--core-only", action="store_true")
    parser.add_argument("--lua-source", type=Path, help="Optional extracted official Lua 5.1.5 source to test the actual Lua binding and dmBuffer boundary")
    args = parser.parse_args()
    work = ROOT / "work/tests"
    work.mkdir(parents=True, exist_ok=True)
    windows = os.name == "nt"
    env = msvc_environment(work) if windows else os.environ.copy()
    source = ROOT / "arcore/src"
    core = work / ("core_test.exe" if windows else "core_test")
    compiler = shutil.which("cl", path=env.get("PATH")) if windows else args.cxx
    if not compiler:
        raise RuntimeError("The configured C++ compiler was not found.")
    common = [compiler, "/nologo", "/std:c++14", "/EHsc", "/MD", "/W3"] if windows else [compiler, "-std=c++11", "-Wall", "-Wextra", "-pthread"]
    include_flag = "/I" if windows else "-I"
    output_flag = (lambda p: ["/Fe:" + str(p)]) if windows else (lambda p: ["-o", str(p)])
    run(common + [include_flag + str(source), ROOT / "tests/core_test.cpp"] + output_flag(core), cwd=work, env=env)
    run([core], cwd=work, env=env)
    if args.core_only:
        return
    if not args.java_home:
        raise RuntimeError("Set JAVA_HOME or --java-home to a full JDK for the real-JVM regression.")
    jdk = args.java_home.resolve()
    classes = work / "classes"
    classes.mkdir(exist_ok=True)
    javac = jdk / ("bin/javac.exe" if windows else "bin/javac")
    run([javac, "-d", classes] + sorted((ROOT / "tests/java").rglob("*.java")), cwd=work, env=env)
    includes = [ROOT / "tests/stubs", ROOT / "tests", source, ROOT / "arcore/include", jdk / "include", jdk / ("include/win32" if windows else "include/linux")]
    native_sources = [ROOT / "tests/jni_test.cpp", ROOT / "tests/fake_arcore.cpp", ROOT / "tests/defold_thread_attacher.cpp"]
    native_sources += [source / name for name in ["ar_session.cpp", "ar_frame.cpp", "ar_planes.cpp", "ar_anchors.cpp", "ar_hits.cpp"]]
    binary = work / ("jni_test.exe" if windows else "jni_test")
    define = "/DARCORE_HOST_TEST" if windows else "-DARCORE_HOST_TEST"
    libraries = [jdk / "lib/jvm.lib"] if windows else ["-L" + str(jdk / "lib/server"), "-Wl,-rpath," + str(jdk / "lib/server"), "-ljvm", "-lm"]
    run(common + [define] + [include_flag + str(p) for p in includes] + native_sources + libraries + output_flag(binary), cwd=work, env=env)
    if windows:
        env["PATH"] = str(jdk / "bin/server") + os.pathsep + str(jdk / "bin") + os.pathsep + env.get("PATH", "")
    # -Xcheck:jni is mandatory in the executable. Treat JVM diagnostics as test failures.
    print("+ real JVM -Xcheck:jni", flush=True)
    result = subprocess.run([str(binary), str(classes)], cwd=work, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout)
    if result.returncode or any(marker in result.stdout for marker in ("WARNING in native method", "FATAL ERROR in native method", "WARNING: JNI")):
        raise RuntimeError("Real-JVM regression failed or emitted CheckJNI diagnostics.")
    if not args.lua_source:
        print("SKIP real-Lua binding checks: supply --lua-source pointing to Lua 5.1.5.")
        return
    lua_source = args.lua_source.resolve() / "src"
    if not (lua_source / "lua.h").is_file():
        raise RuntimeError("--lua-source must contain src/lua.h (Lua 5.1.5).")
    c_sources = [p for p in sorted(lua_source.glob("*.c")) if p.name not in ("lua.c", "luac.c", "print.c")]
    lua_objects = []
    for file in c_sources:
        obj = work / (file.stem + (".obj" if windows else ".o"))
        if windows:
            command = [compiler, "/nologo", "/TC", "/MD", "/D_CRT_SECURE_NO_WARNINGS", "/c", file, "/Fo:" + str(obj)]
        else:
            command = [os.environ.get("CC", "cc"), "-c", file, "-o", obj]
        run(command, cwd=work, env=env)
        lua_objects.append(obj)
    bridge_includes = includes + [lua_source]
    native_only = native_sources[1:]
    bridge_binary = work / ("bridge_test.exe" if windows else "bridge_test")
    defines = ["ARCORE_HOST_TEST", "ARCORE_BRIDGE_TEST", "DM_PLATFORM_ANDROID", "__aarch64__"]
    flags = [("/D" if windows else "-D") + d for d in defines]
    run(common + flags + [include_flag + str(p) for p in bridge_includes] + [ROOT / "tests/bridge_test.cpp", ROOT / "tests/bridge_sdk.cpp"] + native_only + lua_objects + libraries + output_flag(bridge_binary), cwd=work, env=env)
    result = subprocess.run([str(bridge_binary), str(classes)], cwd=work, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(result.stdout)
    if result.returncode or any(marker in result.stdout for marker in ("WARNING in native method", "FATAL ERROR in native method", "WARNING: JNI")):
        raise RuntimeError("Actual Lua/engine-buffer boundary regression failed.")
    unsupported = work / ("unsupported_test.exe" if windows else "unsupported_test")
    run(common + [("/D" if windows else "-D") + "ARCORE_BRIDGE_TEST"] + [include_flag + str(p) for p in bridge_includes] + [ROOT / "tests/unsupported_test.cpp", ROOT / "tests/bridge_sdk.cpp"] + lua_objects + ([] if windows else ["-lm"]) + output_flag(unsupported), cwd=work, env=env)
    run([unsupported], cwd=work, env=env)
    runner = work / ("lua_runner.exe" if windows else "lua_runner")
    run(common + [include_flag + str(lua_source), ROOT / "tests/lua_runner.cpp"] + lua_objects + ([] if windows else ["-lm"]) + output_flag(runner), cwd=work, env=env)
    run([runner, ROOT / "tests/sample_test.lua"], cwd=ROOT, env=env)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit("FAIL: " + str(error))
