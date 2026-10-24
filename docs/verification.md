# Verification evidence — 2026-10-24

This record separates automated host checks, Android build/package checks,
and physical-device observations for this extension.

## Newly run host checks

Host: Windows x64, MSVC 14.16.27023, Temurin JDK 25.0.4.1+1. Tests are checked
in and compile the production worker/session/services and actual Lua binding.

| Check | Result |
| --- | --- |
| Exact SDK header Git blob SHA-256 | Matches f81eef1795e97942ce4ae7a39c53d6f68a9807cfd900b29959c97e9b20bcc704 |
| Official Bob 1.13.1 artifact SHA-256 | Matches 8f2b1381fd4d0fb92816403cd0056cb7db5ad4083615be8dcc2d868fb4939938 |
| Bob version/engine | 1.13.1 / 574678c7d44be490d874fbed2d0ae6211feec4d9 |
| YUV conversion | Odd image dimensions, separate/nontrivial row and pixel strides, padding, colors, null/invalid sizes/lengths/strides pass |
| Anchor registry | Two concurrent handles, independent release, slot reuse, 16-entry exhaustion and session invalidation pass |
| Real-JVM worker/session | -Xcheck:jni; 1,010 update loop amid engine attach/detach; 100,964 instrumented SDK/JNI calls pass with no CheckJNI warnings |
| Loader | App-resolved APK path, failed mandatory symbol does not publish partial API; restart/load succeeds |
| Frames | Image/camera released per update; RGB, UV/view/projection/camera and anchor snapshots match; reuse, exact 250 ms stall threshold and stale SDK-frame gate pass |
| Planes/hits | Polygon filtering, stable IDs, parent merge, stopped tombstones, all three orientations, acquired-reference ownership, bottom-left coordinates, four display rotations and viewport resize pass |
| Anchors | Attached/free calls kept separate, no fallback on failure, invalid plane, polygon rejection, stale frame/handle and resource exhaustion pass |
| Lifecycle | Permission wait/denial/revocation/recovery, failed update, ten pause/resume cycles, repeated stop/start, final cleanup before detach and joined Java thread pass |
| Capabilities | Two actual fixture config descriptors, default camera unmodified, explicit camera selection before resume, invalid selection error, runtime depth distinguished from implemented support pass |
| Actual Lua binding | Lua 5.1.5; error/argument conventions, two anchors, fields, buffer-allocation retry and matching publication pass |
| Engine boundary | Instrumented dmBuffer/vector/ref APIs assert engine-thread ownership; buffer references cleaned after stop/finalization and Lua collection |
| Non-Android module | Actual production unsupported branch imports; unsupported errors, no camera/pixels/tracking; repeatable stop pass |
| Production Java source | Compiles against the pinned Bob Android API jar using javac --release 8; expected deprecation/obsolete target warnings only |
| Example resource compilation | Pinned Bob compiles actual Lua, GUI, meshes, materials/shaders and render resources in a temporary resource-only copy; no native engine or camera tested |
| Independent dependency | Separate game.project resolves and builds a SHA-256-pinned archive; own bootstrap/render/input preserved; native SDK/DEX/manifest checks pass |
| Sample input/lifecycle Lua | Actual sample script: reset during permission denial or poll failure, independent remove, drag/resize/focus cancellation and tracking gates pass |
| Android example | ARM64 debug APK builds with pinned Bob/SDK; complete APK verification passes |
| Android consumer | ARM64 debug APK builds from the archive; full native/SDK/DEX/manifest and dependency-boundary verification passes |

The real-JVM SDK, Android activity and Defold resource APIs are instrumented
stand-ins. These checks verify this extension's ownership, serialization and
contracts; they do not run Google's tracking code or establish physical support.

## Newly built Android artifacts

Both debug APKs use Bob 1.13.1 / engine
`574678c7d44be490d874fbed2d0ae6211feec4d9` and the pinned Maven dependency.
The example and independent consumer contain ARM64 only, libdl.so/four loader
imports, both ARCore SDK libraries, all 92 mandatory SDK exports, renamed Java
bridges and the merged AR Required manifest with minimum SDK 27. The consumer
engine also contains the actual extension initialization marker; a stock engine
or resources-only build is rejected by the checked-in test.

Example APK SHA-256:
`c5dcaff53c59330bf76745853ff8e7ef183a80092e79a54cf7ac14dba50176df`.

Independent consumer APK SHA-256:
`01da87b94e950e870fb1c2c02ea17846e15d3306b723a39a84e5814f7d1bf502`.

The dependency ZIP uses explicit directories and fixed timestamps, matching
Git-style traversal and making identical source content byte-reproducible.
Its SHA-256 is:
`1f650be0edb6e13ca1a482cb0c737ec58639fbf1c90f0255d11e5c21aa6b5d01`.

The first test ZIP omitted directory entries: Bob compiled Lua resources but
skipped native extension traversal. Full APK inspection caught that omission.
The corrected test verifies native code, SDK libraries, Java and merged metadata
alongside the consumer bootstrap and absence of sample output.

Raw builds, keys and logs remain in ignored local work directories; none are
part of the delivered source tree.

## Physical device checks

Device: Motorola moto g(30) / caprip, Android 12 / API 31,
build `S0RCS32.41-10-19-23`, Google Play Services for AR 1.57.262640093.
APK: the example hash above, installed under com.defold.arcore.example.

The owner unlocked the phone and granted Camera. Actual device logs show the
application-class-loader SDK load, all exports resolving, a single worker
attachment and session resume. The example progressed from permission_required
through first-image wait/finding_tracking to tracking. A local screen inspection
showed the live, correctly oriented rear-camera image and authored HUD.

The owner followed the surface-placement prompt and reported both markers stay
aligned while moving the phone. Logs independently show the count reaching
2/2, with plane snapshots being detected. Up to ten registered plane snapshots
were observed; this includes the registry's retained stopped/subsumed entries,
not a claim that all ten were simultaneously tracking.

The stream reported insufficient_light during tracking loss and subsequently
returned to tracking with two anchor handles still present. Background/Home
and a hot reopen preserved the process and two handles, followed by tracking.
A repeated Home/hot-reopen with the rear camera uncovered showed the live
image and both markers again; the owner confirmed that an earlier dark frame
was caused by placing the phone face down. The sample's Remove control changed
2/2 to 1/2. Reset then cleared the registry
and session, resumed ARCore and returned to tracking, on the same native worker
thread as the original session. Two new markers were then placed successfully.
No script exception, CheckJNI abort or native
crash was found in the captured test window. ARCore itself emitted feature-loss
and no-point-hit diagnostics while surfaces were being scanned.

The approximately eight-minute functional check does not establish separate
wall/floor orientation coverage, landscape alignment, permission denial/revocation,
lock/recreation, quantitative sustained performance or airplane-mode cold launch;
those specific physical checks remain pending. A second device was not tested. See device-testing.md for the matrix.
Raw logs and screen captures remain local and are excluded from the source.

## Platform coverage

The production non-Android branch compiles and imports successfully in the
local Lua test. A full desktop engine runtime was not exercised. The Android
example and independent consumer builds completed through the official Defold
build service.
