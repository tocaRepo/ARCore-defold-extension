# ARCore-defold-extension - agent instructions

## Environment scope and preservation of project rules

These instructions apply to this existing repository. The workflow is adapted from the owner's IdleTowerDefense, YouAreTheZombies, and codex-cloud-projects/arcane-foundry examples; their gameplay, package identifiers, dependency versions, and branch policies are not rules for this project.

- Use `LOCAL_DEVELOPMENT` for the local Codex app, IDE extension, CLI, WSL, and local or remote developer machines. Use `CODEX_CLOUD` only when the session explicitly identifies itself as a hosted Codex Cloud task. Linux, a container, Bash, or a `/workspace` path alone is not evidence of Cloud. If uncertain, use the local policy and skip Cloud setup.
- Preserve every project-specific rule below, including architecture, state/save ownership, GUI/input rules, platform scope, dependency pins, reference boundaries, and Git restrictions. The environment sections qualify existing validation wording: local no-build/no-engine/static-review-only restrictions remain fully effective locally; confirmed Cloud may provision its external tool/skill cache, build, and check the actual game as described below. That narrow Cloud exception does not authorize unrelated project changes, release publication, or weakening test prohibitions.
- Preserve the original ARCore host-regression, Android, APK-inspection, and dependency-check commands below. They remain applicable in confirmed Cloud when relevant; do not execute them locally or add unrelated test frameworks. Cloud build permission does not authorize new unit-test suites.
- Where older repository-access rules permit local reads only, the sole Cloud setup exception is fetching `https://github.com/tocaRepo/codex-skills` into the dedicated external cache below. It does not authorize cloning or changing game/reference repositories, or ignoring their instructions.
- Read applicable ancestor and nested instructions, `game.project`, and `git status --short` before implementation changes. Preserve unrelated owner changes and work in the intended checkout and branch. Respect the project's existing commit/branch policy; commit, push, tag, sign, publish, or deploy only when the user requests it.
- Documentation-only changes require source/diff review in both environments, not a Defold build or game launch.

## Project configuration and build prerequisites

The existing Defold project is rooted in this repository, where `game.project` is present. Continue it in place; do not scaffold another game or copy the examples' directory layout.

The existing project instructions pin **Defold/Bob 1.13.1**. Keep that pin and the corresponding engine/native constraints. Install a compatible matching Bob/JDK combination rather than upgrading the engine to match a reference setup.

- Treat the existing `game.project`, checked-in build configuration, and project-specific rules as authoritative for bootstrap resources, display/input, package identity, platforms, native extensions, and dependency pins. Application version fields are not Defold engine versions. Do not copy the examples' versions or add dependencies, shipping platforms, frameworks, or scaffolding merely to enable verification.
- Keep runtime resource paths project-relative and case-correct for Linux. Windows reference paths in existing rules are read-only documentation, not required Cloud dependencies. Follow this project's existing ownership patterns and libraries; the environment workflow does not replace its architecture.

## LOCAL_DEVELOPMENT

- Never run Defold or the game locally, directly or through a skill/helper: no editor launch/automation, `dmengine`, preview, emulator/device launch, Bob/`bob.jar`, dependency resolution, build, bundle, APK/AAB packaging, or deployment. The owner performs local build/runtime/device verification.
- Local validation is static reading and review only: directory/file inspection, text searches, resource-reference review, image inspection, and read-only Git status/diff review. Do not execute gameplay Lua, existing build/regression commands, tests, syntax/parser/linter tools, resource/shader validators, or substitute validation harnesses locally.
- Do not execute the Cloud commands below, install their Linux packages/toolchain, or replace local skills with Cloud copies on the owner's machine. Use the active shell's syntax, including PowerShell on Windows.
- Report exactly what was inspected and the checks left to the owner. Static review does not establish build, runtime, device, performance, or in-engine visual correctness.

## Accessing skills

Use the current session's skill catalog and read the relevant skill's exact `SKILL.md` plus only the supporting resources needed. Resolve relative resources against that skill folder and preserve complete `references/`, `scripts/`, and `assets/` directories. A skill never overrides the active environment or project restrictions.

On this owner's local Windows setup, personal Defold skills are under `C:/Users/tobia/.codex/skills/`; bundled/plugin skills can live elsewhere, so the session catalog is authoritative. For example:

```powershell
Get-Content -LiteralPath 'C:\Users\tobia\.codex\skills\defold-gui-layout\SKILL.md'
```

| Task | Skill to consult when available |
| --- | --- |
| GUI positioning, sizing, alignment, spacing, or clipping | `defold-gui-layout` |
| Druid widgets, GUI animation, or Monarch navigation | `defold-gui-skills` |
| JSON-backed GUI hexagon maps | `defold-hexagon-maps-skill`, only for a relevant map task |
| Original generated raster assets or image edits | `imagegen`, when relevant |

Do not scaffold over this existing project with `defold-new-mobile-game`. Respect explicit-invocation requirements of skills such as `defold-playtest` or `defold-gui-validation` if installed; ordinary permitted Cloud build/runtime checks do not require invoking them. If a needed skill is unavailable, report it and continue independent permitted work.

## CODEX_CLOUD

**Cloud only: skip this entire section unless this is an explicitly identified hosted Codex Cloud task.** For implementation/resource changes, build with the project's matching Defold toolchain and perform relevant checks of the actual game when the hosted environment supports them. These ordinary Cloud setup/build/runtime checks are authorized without another permission request, within the existing project and environment boundaries.

### Prepare the hosted environment

1. Use the existing checkout and intended branch. Locate its root with `git rev-parse --show-toplevel`, read the project instructions, and confirm the intended `game.project`. Do not assume an example's `/workspace/<name>` path or create a replacement project when configuration is missing.
2. Inspect provisioned tools before installation. Use the Cloud environment's installation/setup mechanism to prepare Bash, Git, curl, CA certificates, tar, checksum tools, Python 3 when needed, and the matching Bob plus a compatible JDK. Read the selected Bob release's Java requirements; never assume a copied reference JDK suits every project. Obtain tools from official releases and verify downloads against trusted checksums or approved artifact metadata.
3. Prepare platform-specific build requirements only when needed. For compatible Linux runtime checks, prepare Xvfb, xauth, xdotool, required X11/Mesa/audio libraries, fonts, and capture tooling through the permitted package sources. The availability of a Linux host does not prove this project's native integrations can run there.
4. Keep tools, skills, caches, logs, captures, bundles, and disposable saves outside the source checkout, for example under `/workspace/.defold/`. Do not commit machine-specific paths or skill symlinks. Use distinct project/task runtime directories and do not change other projects.
5. Configure persistent values through environment settings or source a dedicated external environment file in each task shell. Exports in a setup shell do not automatically persist into task shells. Configure installation/startup in the current Cloud environment; use setup/maintenance scripts where the legacy environment requires them. An instruction file documents setup and does not install or publish an environment by itself.
6. Respect the environment's network/proxy policy and verified TLS. Allow only needed official GitHub download/dependency hosts, approved package sources, and the configured native-extension build service such as `build.defold.com`. Adapt Java trust/proxy settings to the supplied environment; never hardcode a reference proxy, disable certificate checks, or use forwarding to bypass denied destinations. Keep credentials and signing material out of repository files and logs.

Configure `DEFOLD_VERSION` from the authoritative project/environment pin, `DEFOLD_JAVA` as the compatible Java executable, and `DEFOLD_BOB_SHA256` as the approved Bob artifact hash. This Cloud-only example caches Bob without changing the game or choosing an engine version:

```bash
set -euo pipefail
: "${DEFOLD_VERSION:?Set the confirmed project Defold version}"
: "${DEFOLD_JAVA:?Set the compatible Java executable}"
: "${DEFOLD_BOB_SHA256:?Set the trusted expected Bob SHA-256}"
DEFOLD_TOOLS_ROOT="${DEFOLD_TOOLS_ROOT:-/workspace/.defold}"
export DEFOLD_BOB="$DEFOLD_TOOLS_ROOT/$DEFOLD_VERSION/bob.jar"
mkdir -p "$(dirname "$DEFOLD_BOB")"
if [ ! -f "$DEFOLD_BOB" ]; then
  curl -fL --retry 3 \
    "https://github.com/defold/defold/releases/download/$DEFOLD_VERSION/bob.jar" \
    -o "$DEFOLD_BOB"
fi
printf '%s  %s\n' "$DEFOLD_BOB_SHA256" "$DEFOLD_BOB" | sha256sum -c -
"$DEFOLD_JAVA" -version
"$DEFOLD_JAVA" -jar "$DEFOLD_BOB" --version
```

Confirm the reported Bob version matches the project before building. A checksum mismatch, unknown version, or incompatible JDK is a blocker to resolve, not a reason to remove verification or upgrade the engine. Source the approved external environment file in later shells; add any release-required JVM options before `-jar` rather than copying options unsupported by an older pinned JDK.

### Install and use shared skills in Cloud

The examples pin the owner's shared skill repository to `0833f8a2613bc7fcf6dd5a2899cf7d2b89ed3ff5`. This is a deliberate reference pin, not a claim about the newest revision. Adopt a different revision only after review. Run the following only against the dedicated Cloud cache:

```bash
set -euo pipefail
DEFOLD_SKILLS_ROOT=/workspace/.defold/codex-skills
DEFOLD_SKILLS_URL=https://github.com/tocaRepo/codex-skills.git
DEFOLD_SKILLS_REVISION=0833f8a2613bc7fcf6dd5a2899cf7d2b89ed3ff5
mkdir -p /workspace/.defold
if [ ! -e "$DEFOLD_SKILLS_ROOT" ]; then
  git clone "$DEFOLD_SKILLS_URL" "$DEFOLD_SKILLS_ROOT"
fi
test -d "$DEFOLD_SKILLS_ROOT/.git"
test "$(git -C "$DEFOLD_SKILLS_ROOT" remote get-url origin)" = "$DEFOLD_SKILLS_URL"
if [ -n "$(git -C "$DEFOLD_SKILLS_ROOT" status --porcelain)" ]; then
  printf '%s\n' 'Skill cache has local changes; inspect before updating.' >&2
  exit 1
fi
git -C "$DEFOLD_SKILLS_ROOT" fetch origin "$DEFOLD_SKILLS_REVISION"
git -C "$DEFOLD_SKILLS_ROOT" checkout --detach "$DEFOLD_SKILLS_REVISION"
git -C "$DEFOLD_SKILLS_ROOT" rev-parse HEAD
mkdir -p "$HOME/.agents/skills"
for skill_path in "$DEFOLD_SKILLS_ROOT"/*; do
  [ -f "$skill_path/SKILL.md" ] || continue
  skill_link="$HOME/.agents/skills/$(basename "$skill_path")"
  if [ ! -e "$skill_link" ] && [ ! -L "$skill_link" ]; then
    ln -s "$skill_path" "$skill_link"
  fi
done
```

Preserve pre-existing skills and links. Keep complete folders outside the game checkout. If session discovery misses a skill, read `/workspace/.defold/codex-skills/<skill-name>/SKILL.md` directly or start a fresh session. Do not substitute Windows skill paths in Cloud.

### Build and runtime verification

This repository is an Android ARM64 ARCore extension, not a portable desktop game. Preserve Defold/Bob 1.13.1, engine `574678c7d44be490d874fbed2d0ae6211feec4d9`, ARCore 1.48.0, API 27+, the rear-camera requirement, and the original worker/JNI/lifetime rules below. Inspect the checked-in scripts and their documented prerequisites before invoking them; do not replace their native-extension/dependency checks with a generic Linux game bundle.

In confirmed Cloud, provision the matching JDK, Lua 5.1.5 source tree and required host/native tooling, then run the relevant existing commands from this repository root. For example, with the confirmed prerequisite paths:

```bash
set -euo pipefail
: "${DEFOLD_JAVA_HOME:?Set the matching JDK root}"
: "${DEFOLD_LUA_SOURCE:?Set the Lua 5.1.5 source root}"
cd "$(git rev-parse --show-toplevel)"
python3 scripts/test.py --java-home "$DEFOLD_JAVA_HOME" --lua-source "$DEFOLD_LUA_SOURCE"
python3 scripts/build.py android
python3 scripts/test_dependency.py --build
```

For an actual produced APK, also run `python3 scripts/verify_android_apk.py <apk>` with its real path. Use the existing build scripts' documented JDK/Bob options when they do not consume the environment variables above. Native builds send build inputs to the selected approved Extender service; preserve its existing authorization requirements. A Linux simulated consumer cannot establish real ARCore tracking, camera, or physical-device success. Report host regression, Android compilation/dependency checks, APK inspection, and physical-device results separately.

- Check command exit statuses and logs. Resolve missing/changed dependencies and use valid incremental outputs; do not routinely run `clean` or `distclean`. Fix failures caused by the change and repeat the affected checks.
- For gameplay/UI changes, exercise the affected flow in the actual built game when a suitable runtime is available. Use Xvfb for a compatible Linux bundle, discover its actual executable/window, and capture the real framebuffer when visual evidence is needed. Never claim a simulation or static reading proves native-device behavior.
- Use fresh task-specific save/profile directories outside the checkout; inspect custom save paths as well as XDG locations before launch. Do not access the owner's real saves, accounts, purchases, or production ad interactions. Stop only task-owned processes and remove temporary automation dependencies/configuration overrides before delivery.
- Preserve the project's shipping platforms and native integrations. A Linux debug bundle is a verification target only where compatible; it does not authorize a port, replacing native behavior with fake success, or claiming Android/iOS/Windows/device verification. Use an appropriate supported target/environment where native APIs require it, or report the exact runtime blocker.
- For packaging/native-integration changes, perform the relevant supported platform build when possible, using disposable validation credentials if required. Never use the owner's release keys or publish/deploy a release as part of verification.
- Keep runtime/debug screenshots, videos, APKs/AABs, logs, downloaded tools, and generated build/cache files untracked and out of GitHub releases/attachments. Intentional final storefront assets follow the project's existing policy and require the relevant request. Keep required authored game artwork and provenance versioned.
- Report exact commands, observed build results, runtime flows actually exercised, and remaining blockers/owner checks. Distinguish source review, compilation, runtime, device, performance, and visual evidence. If tooling/network/native services/runtime support is unavailable, continue permitted independent work and state what remains unverified.

### Reference documentation

- [Official OpenAI Cloud environment documentation](https://learn.chatgpt.com/docs/environments/cloud-environments)
- [Official OpenAI skill documentation](https://learn.chatgpt.com/docs/build-skills)
- [Official Defold Bob manual](https://defold.com/manuals/bob/)

## Existing project rules (preserved)

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
