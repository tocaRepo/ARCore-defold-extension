# Physical device validation

Prepared 2026-10-24. Physical evidence belongs here separately from host fixtures,
resource compilation and APK inspection. See docs/verification.md for host results.

## Device matrix

| Model | OS / API | ARCore preparation | Extension runtime result |
| --- | --- | --- | --- |
| Motorola moto g(30), attached over ADB | Android 12 / API 31 | Google Play Services for AR 1.57.262640093 installed | Live camera/tracking, two surface anchors, independent removal, tracking recovery and reset observed; owner reports markers stay aligned |

These results cover one device and do not establish broad device support or
independent certification.

The owner granted Camera and the example reached actual tracking. A local
screen inspection showed the live rear-camera image and HUD. Logs show two
anchor handles and detected planes; the owner reports both markers stay aligned
while moving. Tracking loss reported insufficient_light and later recovered.
Home/hot-reopen preserved the process/handles; Remove changed 2/2 to 1/2; Reset
cleared the session/handles, resumed, and reached tracking on the same worker.
The owner confirmed that the earlier dark image came from placing the phone
face down. Repeating Home/hot-reopen with the rear camera uncovered showed the
live image and both markers again. No script/JNI abort or native crash was
observed in the approximately eight-minute test window. This is one device, not broad
support or an independent certification claim.

Android build: `S0RCS32.41-10-19-23`; device codename caprip. Tested APK SHA-256:
`c5dcaff53c59330bf76745853ff8e7ef183a80092e79a54cf7ac14dba50176df`.

## Manual acceptance procedure

Record model/variant, Android build, Google Play Services for AR version, APK
hash, extension revision, session options, lighting and test duration.

| Test | Expected behavior | Recorded result |
| --- | --- | --- |
| Fresh launch / grant permission | Camera and readable tracking status; no second camera | Passed: native SDK/session/frame path, live image and tracking |
| Deny camera permission | Inline permission_denied; no crash or repeated poll prompt | Pending |
| Grant/revoke via Settings | Pause on denial; recover or explicit restart/retry without stale handles | Pending |
| Live alignment while turning | Image, world marker and projection agree; portrait and landscape | Portrait: owner reports alignment while moving; live image/two markers inspected after resume. Landscape not tested |
| Floor/table and wall hits | Taps hit observed polygons, local +Y follows surface | Surface placements succeeded; separate orientation coverage not independently logged |
| Two markers | Both stable while walking; removing one keeps the other | Owner reports both aligned; logs 2/2 then independent Remove to 1/2 |
| Tracking loss/recovery | Useful reason, input gated, no stale-image interaction | insufficient_light reported, then tracking recovered with handles retained; stale-image gate covered by host tests |
| Resize/rotation during held touch | Gesture canceled; matching geometry on next complete image | Pending |
| Background/lock/resume | Native pause/resume; new complete image required before input | Two Home/hot-reopen cycles preserved process/handles; uncovered camera image and both markers visible afterward. Lock not tested |
| Activity recreation | Permission/result handling reconciles with current activity | Pending |
| Repeated reset/start/stop | Old IDs rejected, camera and new markers recover | Physical Reset cleared/resumed on the same worker; two new markers were then placed. Repeated/stale-ID cases also pass host tests |
| Airplane-mode cold launch | Services prepared beforehand; no install/cloud request | Pending |
| Sustained run / thermal | Record FPS, conversion/upload cost, image stalls and stability | Approximately eight-minute functional smoke check without crash; quantitative FPS/thermal measurements pending |
| Alternative config, if exposed | Selected before resume, old handles invalidated; default preserved otherwise | Pending |
| Missing/outdated services | Inline arcore_required; no automatic installer | Pending |

Do not erase another application's data or change phone networking/permissions
without the owner's authorization. Use the example's own package for tests.
Keep raw logs, screenshots/videos and APKs local in ignored directories; record
concise text evidence here. A second certified device is useful before broad
support claims.
