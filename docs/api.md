# Lua API, snapshots and lifetimes

The native module registers as global `arcore`. No `require` is needed for the
native namespace. `arcore_render.camera` is a separate optional Lua helper.

## Errors and platform behavior

Expected operation failures return `nil, error`, with
`error = {code = "...", message = "...", native_status = 0}`. A nonzero
`native_status` preserves the SDK status or availability enum where applicable.
Invalid Lua argument types can raise normal Lua argument errors. Unknown option
keys return `unsupported_option`; unimplemented features are not silently enabled.

`poll` represents expected initialization, permission and tracking interruptions
as a status frame, with `error` when applicable. Invalid viewport or worker/buffer
failure returns `nil, error`. The caller must disable interaction after either.

Non-Android native builds import normally. `start`, `hit_test`, `create_anchor`
and `release_anchor` return `unsupported_platform`. `poll` returns a frame with
`native=false`, no pixels, and tracking/fresh/complete false. `stop` returns true.
No desktop camera or simulator is substituted.

## arcore.start([options]) → true | nil, error

Accepted keys:

| Key | Default | Contract |
| --- | --- | --- |
| `near_clip` | 0.05 | Positive float, metres |
| `far_clip` | 50 | Greater than near, at most 100000 m |
| `camera_config_id` | omitted | An actual rear-camera ID from `get_capabilities().camera_configs` |

Calling start always tears down a prior session, clears its complete snapshot,
increments `session_generation`, invalidates its plane/anchor IDs and applies
the new options before resume. A camera config ID can be retained from a prior
session on the same runtime; the new session must enumerate an exact match or
poll reports `invalid_camera_config`. IDs describe runtime configurations, not
promised physical lenses. Omission preserves ARCore's default camera.

Success means the startup request was accepted. Poll to perform permission,
availability, session/configuration and camera initialization. No session install
or update is requested. Explicit start retries a denied permission request;
ordinary polls do not repeatedly prompt. Permanent denial may require Settings.

Defaults: rear camera, horizontal/vertical plane finding, autofocus,
latest-camera-image updates, hardware-buffer texture exposure, depth disabled,
light estimation disabled. Clipping is the unchanged ARCore/OpenGL projection
convention copied into Defold column-major matrices.

## arcore.poll(width, height) → frame | nil, error

Pass the actual full physical viewport dimensions used to render and receive
hit coordinates. Each dimension must be an integer 1–32768. If you use an inset
viewport, translate input into that viewport and pass its dimensions; the
provided example uses the full window.

Always present:

| Field | Meaning |
| --- | --- |
| `native` | True on Android native implementation |
| `status`, `detail` | Current lifecycle/tracking status and readable explanation |
| `tracking` | Current camera interaction gate, including permission, focus, geometry and 250 ms image-stall checks |
| `fresh` | A complete image snapshot was newly published to the engine pixel buffer in this poll |
| `complete` | A complete image/pose snapshot is available |
| `frame_id` | Session-local SDK update sequence for the retained complete snapshot, or 0 |
| `session_generation` | Changes on every accepted start |
| `tracking_failure_reason` | Current SDK diagnostic, independent of retained image data |
| `planes`, `anchors` | Arrays from that complete snapshot, initially empty |
| `error` | Optional structured initialization/status failure |

Present only when a complete snapshot has been copied successfully:

| Field | Meaning |
| --- | --- |
| `pixels` | Reused Defold buffer, stream `rgb`, three uint8 components per pixel |
| `width`, `height` | CPU image dimensions, not viewport size |
| `timestamp` | ARCore camera frame timestamp in nanoseconds, exposed as Lua number |
| `view`, `projection` | Matching Defold matrix4 values |
| `camera_position`, `camera_rotation` | Display-oriented world pose, vector3 and quaternion xyzw |
| `uv0`, `uv1`, `uv2` | vector4 values; XY image UV for NDC corners (-1,-1), (1,-1), (-1,1) |
| `viewport_width`, `viewport_height`, `display_rotation` | Display geometry used for this snapshot; rotation is Android Surface enum 0–3 |

When fresh is false, retain or use the complete snapshot fields unchanged.
No newer camera matrix, plane geometry or anchor pose is combined with old
pixels. Live `status`, `tracking`, `detail`, `tracking_failure_reason` and
`error` may change independently. An absent image for **250 ms or longer**
sets tracking false. A changed display geometry cannot interact until a new
image matches it. After pause, a genuinely new image is required before input.

Even during brief reuse, a hit/creation request fails with `image_not_current`
if ARCore advanced to an SDK frame that did not supply a complete image. A
tracking=true status alone is not a guarantee that a specific old render frame
is current. Pass frame/session IDs on interaction and handle these errors.

**Buffer lifetime:** pixels are reusable storage. Upload or copy their contents
before the next poll, including polls that report errors. Do not use an old
buffer as extension camera data after stop/start or after resize/reallocation.
Keeping the Lua userdata alive does not freeze its bytes. Matrices/poses and
arrays are separate Lua values; retaining them still requires retaining the
corresponding image yourself. Timestamp is a double and can lose nanosecond
precision; use frame_id/session_generation for exact frame comparisons.

### Plane entries

`{id, parent_id?, tracking_state, orientation, pose, extent_x, extent_z,
polygon, frame_id, session_generation}`.

IDs are opaque session-scoped strings. State is `tracking`, `paused` or `stopped`.
Orientation is `horizontal_up`, `horizontal_down` or `vertical`. `pose` is
`{position=vector3, rotation=quaternion}`. Extents are full width/depth in metres.
`polygon` is a flat array `{x1,z1,x2,z2,...}` in plane-local **XZ metres**, local
Y=0, +Y the normal. Transform with the plane pose to world coordinates.
An up-facing plane could be a table or another surface, not necessarily a floor.

Subsumed planes expose `parent_id`; stopped planes remain visible as tombstones.
The registry deliberately retains one SDK reference for every observed plane
until session teardown, including stopped planes. This prevents address reuse
from resurrecting an ID, at a memory cost proportional to planes seen this
session. There is no raw pointer in a public ID. References held by SDK lists,
parent queries and hits are released independently.

### Anchor entries

`{handle, plane_id?, tracking_state, pose, frame_id, session_generation}`.

Entries describe the anchors that existed at the complete image frame.
Newly created anchors appear in the next fresh snapshot. A released anchor
can still appear in an older retained snapshot; remove its application marker
immediately after release and do not infer handle liveness from old frame data.
Stopped SDK anchors occupy registry capacity until explicitly released or stop.

## arcore.hit_test(x, y, [options]) → hits | nil, error

Coordinates are physical viewport pixels, **origin bottom-left**. Domain:
0 ≤ x ≤ last poll width, 0 ≤ y ≤ last poll height. ARCore receives
`(x, height - y)` exactly once. Image UVs are not applied to input coordinates.
The SDK's display geometry handles display rotation and viewport dimensions.

Options: `frame_id`, `session_generation` (both optional positive identifiers).
Provide the IDs from your rendered frame to detect outdated input. Only a
current complete image frame can be queried. Results are sorted by distance,
nearest first. Empty array means no eligible observed surface hit.

Each hit: `{pose, distance, plane_id, frame_id, session_generation}`. Distance
is metres, pose world space. Only tracking, non-subsumed planes with hit poses
inside their observed polygons are returned. Point, instant-placement and
depth hits are excluded. No gameplay distance, orientation preference, use cap
or random selection is applied. Hits must be used before the next poll advances
the SDK frame; pass their IDs into create_anchor.

## arcore.create_anchor(options) → handle | nil, error

Required: `pose = {position = vector3, rotation = quaternion?}`. Rotation
defaults to identity; finite nonzero quaternions are normalized. Position is
ARCore world-space metres. Optional keys: `plane_id`, `frame_id`,
`session_generation`.

With plane_id, attach with `ArTrackable_acquireNewAnchor` to that exact tracked,
non-subsumed plane. Empty, unknown or stale IDs fail. Position must be inside
the observed polygon. Failure never switches to a free anchor.

Omitting plane_id explicitly requests `ArSession_acquireNewAnchor`, a free
world anchor. There is no camera-relative spawn calculation or placement policy.
Creation requires current image/tracking; frame/session IDs can constrain the pose.

The conservative limit is **16 live handles**. The extension or SDK can report
`resource_exhausted`. Handles are opaque strings encoding session generation
and a non-reused serial, never raw SDK pointers. Do not parse them. Use a later
fresh poll's anchor pose for rendering rather than retaining a hit pose forever;
ARCore can refine world coordinates between frames.

## arcore.release_anchor(handle) → true | nil, error

Detaches/releases only the requested handle on its owning worker. Other anchors
remain alive. An unknown, already released or previous-session handle returns
`invalid_anchor`. Release is permitted during pause or tracking loss.

## arcore.get_capabilities() → capabilities

Does not initialize a session or prompt by itself. Poll after start to discover
runtime capabilities. Fields: `native`, `initialized`, `status`,
`session_generation`, `max_anchors`, optional `error`, `implementation`,
`runtime`, `camera_configs`.

Implementation booleans are extension features, independent of device support:
camera_rgb/camera_transforms/planes/hit_test/anchors/tracking_diagnostics/
camera_config_selection true; depth/occlusion/augmented_images/light_estimation/
front_camera/cloud_anchors/geospatial/ios/multiplayer false.

Runtime fields:

- `sdk_version="1.48.0"` (packaged dependency), `sdk_loaded`.
- `availability`: unknown, checking, installed, unsupported, not_installed, outdated.
- `depth`: unknown/supported/unsupported, queried for the selected camera with
  `ArSession_isDepthModeSupported`; extension depth remains unimplemented.
- `rear_camera`: unknown/supported; `front_camera="unknown"` because it is not queried.
- `selected_camera_config_id`: empty before enumeration, otherwise the active ID.

`camera_configs` contains actual **rear-camera** configurations exposed by
`ArSession_getSupportedCameraConfigsWithFilter`, allowing 30/60 FPS targets.
Each entry includes `id`, Android `camera_id`, `facing="back"`, image_width/
image_height, texture_width/texture_height, min_fps/max_fps, SDK bitmasks
depth_sensor_usage/stereo_camera_usage, and `selected`. The ID includes every
listed configuration discriminator. No sensor, ultrawide label, zoom factor,
front-camera support or alternative configuration is fabricated.

## arcore.stop() → true | nil, error

Releases anchors, retained planes/lists, frame, pose and session on the worker;
drops the engine's pixel reference on the engine thread. Repeated stop succeeds.
Later start works using the same continuously attached worker. Native extension
finalization also releases JNI global references before stopping and joining
the worker. A successfully loaded SDK remains loaded until process exit.

## Status and operation codes

Lifecycle/status: stopped, starting, paused, waiting_for_activity, checking_arcore,
permission_required, permission_denied, availability_unknown, arcore_required,
unsupported_device, sdk_load_failed, android_error, session_failed,
arcore_service_failed, configuration_failed, invalid_camera_config,
camera_config_failed, camera_unavailable, finding_tracking, tracking,
frame_unavailable, camera_image_unavailable, display_geometry_changed.

Operation errors additionally include unsupported_platform, unsupported_option,
invalid_options, invalid_viewport, invalid_coordinates, invalid_pose,
worker_unavailable, buffer_unavailable, not_ready, not_tracking,
image_not_current, stale_frame, stale_session, invalid_plane,
plane_not_tracking, outside_polygon, invalid_anchor, anchor_failed,
resource_exhausted.

Tracking failure reasons: none, bad_state, insufficient_light, excessive_motion,
insufficient_features, camera_unavailable, unknown. They are useful diagnostics,
not guarantees that a specific user action will restore tracking.
