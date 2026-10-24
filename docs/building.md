# Reproducible builds and tests

## Tool pins

| Input | Pin |
| --- | --- |
| Bob/Defold | 1.13.1 |
| Engine | 574678c7d44be490d874fbed2d0ae6211feec4d9 |
| Official bob.jar SHA-256 | 8f2b1381fd4d0fb92816403cd0056cb7db5ad4083615be8dcc2d868fb4939938 |
| ARCore Maven | com.google.ar:core:1.48.0 |
| Android | arm64-android only, minimum API 27 |
| Host regression JDK used | Temurin 25.0.4.1+1, Windows x64 |
| Optional host Lua | 5.1.5 source archive SHA-256 2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333 |

Download Bob from the official Defold 1.13.1 release. Use a compatible Java 25
runtime; older system Java installations may fail with class-version errors.
Build/test scripts discover no project-specific machine paths. Tool downloads,
profiles and logs belong outside committed sources or under ignored `work/`.

## Windows PowerShell

```powershell
$env:DEFOLD_BOB = 'C:\tools\defold\1.13.1\bob.jar'
$env:DEFOLD_JAVA = 'C:\tools\jdk-25\bin\java.exe'
python scripts/build.py android
python scripts/verify_android_apk.py 'bundles/android/ARCore Example/ARCoreExample.apk'
python scripts/test_dependency.py --build
python scripts/build.py windows
```

The example's bundle directory is derived by Bob from its title; use the
actual produced path if the platform formats that directory differently.
Do not commit APKs or debug signing keys.

## Linux shell

```bash
export DEFOLD_BOB=/path/to/defold-1.13.1/bob.jar
export DEFOLD_JAVA=/path/to/jdk-25/bin/java
python3 scripts/build.py android
python3 scripts/test_dependency.py --build
python3 scripts/build.py linux
```

Build with `--build-server <url>` or DEFOLD_BUILD_SERVER to use a self-hosted
Extender. The default is `https://build.defold.com`. The standard native build
uploads extension C++/Java, declarations, manifest/Gradle inputs and related
assets to that service; APK packaging is local. Obtain any required local
approval for that upload. This build-time dependency does not create a runtime
network dependency in the ARCore extension.

Keep injected proxy/authentication and verified CA trust in managed environments.
If Bob's Extender client cannot honor a host proxy, configure the
host's approved forwarding route or self-hosted service; do not disable TLS
or silently send sources to another destination.

## Host regression suite

Requires Python 3.10+, a full JDK (headers, javac, JVM library), and C++ tools.
Windows uses Visual Studio C++ Build Tools and discovers an installed x64
toolset; Linux uses CXX or `c++` and CC or `cc`.

```text
python scripts/test.py --java-home <full-jdk>
python scripts/test.py --java-home <full-jdk> --lua-source <extracted-lua-5.1.5>
```

The first command runs native image/registry helpers and the real-JVM
session/worker fixture. The second also compiles the actual native Lua binding
against Lua 5.1 and instrumented engine buffer/vector stand-ins, plus an actual
non-Android module import. Get Lua 5.1.5 from lua.org and verify the checksum
above before extracting it. No network or Android service is used by these tests.

The tests compile production worker/session/frame/plane/hit/anchor code without
changing those implementations. SDK operations are stand-ins that call real
Java, check continuous OS/JNI/Java thread identity and track every acquired
reference. Defold ThreadAttacher is adapted from the pinned engine with its
original license and only the desktop JNI parameter cast adjusted. The JVM
runs with `-Xcheck:jni`; JNI warnings fail the runner.

Fixtures cover loader atomicity, permission wait/denial/revocation, 1,010 frame
updates amid engine detach, pause/resume, stop/restart/final join, strided YUV,
same-frame publication, buffer allocation retry/lifetime, plane merges/stops,
parent references, hit polygon filtering/origin/rotation/resize, two anchors,
independent release, stale handles, capacity exhaustion and camera enumeration.
They do not execute Google's tracking implementation or an Android camera.

## Independent consumer archive

```text
python scripts/test_dependency.py
python scripts/test_dependency.py --build
```

The script creates a source archive from this exact content, prints its SHA-256,
and resolves that immutable archive through a temporary localhost HTTP server.
It copies `tests/consumer` into an isolated work directory with its own
game.project/bootstrap/input/render. The archive deliberately includes the
example; Defold must respect `library.include_dirs` to mount only arcore and
arcore_render. Explicit ZIP directory entries match real git/GitHub archives;
Defold's native extension traversal needs them. No relative source references
or symlinks are used.

The first command resolves only and never uploads native sources. `--build`
also compiles/packages the consumer with the pinned toolchain and verifies its
compiled bootstrap, absence of sample output, and actual packaged native module,
SDK libraries, Java bridges and manifest. A resources-only build is not accepted
as proof of native dependency consumption. It has the same Extender
upload requirement as an ordinary native build. In a published repository,
also test the immutable GitHub commit archive you intend consumers to use.

## APK inspection

`scripts/verify_android_apk.py` uses pure Python ELF64 and Android binary XML
parsers. It checks exact ARM64 ABI, real libdl.so dependency/four undefined
loader imports, both SDK libraries, all current mandatory SDK exports, Java
bridge descriptors, package ID, camera permission, min SDK and AR Required
features/metadata. It does not claim APK signature or hardware verification.

For the independent consumer:

```text
python scripts/verify_android_apk.py <consumer.apk> --engine libARCoreConsumer.so --package com.defold.arcore.consumer
```

Use Android build-tools apksigner separately if signature verification is
required. Never use a release key for this debug validation workflow.
