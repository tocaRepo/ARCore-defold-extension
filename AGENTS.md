# Project instructions

This is a standalone, experimental Defold ARCore extension made for fun.
There is no commitment to ongoing maintenance, updates, or support.

- Target ARM64 Android, API 27+, rear camera, with Google Play Services for AR installed.
- Keep Defold/Bob 1.13.1, engine 574678c7d44be490d874fbed2d0ae6211feec4d9, and ARCore 1.48.0 pinned.
- Public namespace: `arcore`; extension: `ARCore`; Java package: `com.defold.arcore`.
- Library include directories are exactly `arcore arcore_render`.
- Keep the sample under `example/`; consuming applications own their bootstrap and renderer.
- ARCore owns the camera. Every ARCore/JNI call and reference cleanup runs on the dedicated worker.
- Preserve one continuous JVM attachment through final cleanup and worker join, with synchronous job serialization.
- Keep Lua, dmBuffer, resource, texture, and render operations on Defold's thread.
- Publish pixels, UVs, matrices, plane geometry, and anchor poses as one complete snapshot.
- Preserve the 250 ms image-stall guard and reject interaction with unmatched frames.
- Keep dynamic `dl` linkage and application-class-loader loading. Retain the loaded SDK until process exit.
- Keep plane IDs stable, anchor handles session-scoped, and the live-anchor limit at 16.
- Attached-anchor failure must never fall back silently to a free anchor.
- Keep offline operation free of automatic service installation or cloud calls.
- Preserve SDK declarations, third-party licenses, and notices.

Run `python scripts/test.py --java-home <jdk> --lua-source <lua-5.1.5>` for host regressions.
Use `python scripts/build.py android`, `python scripts/verify_android_apk.py <apk>`,
and `python scripts/test_dependency.py --build` for Android and dependency checks.
Native builds upload build inputs to the selected Extender server; follow the
environment's authorization requirements.

Record automated and physical-device results separately. Keep binaries, signing
material, caches, logs, and captures untracked. Do not publish, push, or tag unless requested.
