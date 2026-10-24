# Third-party notices and provenance

## Google ARCore SDK 1.48.0

`arcore/include/arcore_c_api.h` is the unmodified SDK declaration file from
google-ar/arcore-android-sdk v1.48.0, commit
`b73725b586bb18b622d6e1c6aee9650176d4679d`. SHA-256:
`f81eef1795e97942ce4ae7a39c53d6f68a9807cfd900b29959c97e9b20bcc704`.

Its copyright notice identifies Google LLC and the ARCore Additional Terms of
Service. The full upstream multi-section license is preserved at
`arcore/ARCORE-LICENSE.txt`. Its first section explicitly covers the header
alongside SDK binary artifacts; the wrapper's MIT license does not relicense it.

The build declares `com.google.ar:core:1.48.0` from Google's Maven repository.
No SDK AAR or native binary is committed. The recorded SDK AAR SHA-256 is
`219afa51796eb2ff12ce97706480ce5cf56d708eff68653ec0239536a722c265`.
Actual packaged export verification is performed by scripts/verify_android_apk.py.

## Defold regression fixture

`tests/defold_thread_attacher.cpp` and the ThreadAttacher declaration in
`tests/stubs/dmsdk/dlib/android.h` adapt the implementation from Defold engine
`574678c7d44be490d874fbed2d0ae6211feec4d9`,
`engine/dlib/src/dlib/android.cpp` and `engine/dlib/src/dmsdk/dlib/android.h`.

Copyright 2020–2026 The Defold Foundation; 2014–2020 King;
[Defold License 1.0](https://www.defold.com/license).
The fixture changes only the desktop JNI AttachCurrentThread parameter cast,
provides an Android activity stand-in and omits unrelated engine APIs.
It is test-only and is excluded by the library boundary.

## Host Lua tests

Optional binding tests compile an external Lua 5.1.5 source tree. Lua is
copyright 1994–2012 Lua.org, PUC-Rio and licensed under MIT. The source and
its COPYRIGHT are downloaded separately, not included in this repository.
