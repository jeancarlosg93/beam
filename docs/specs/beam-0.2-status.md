# Beam 0.2.0 implementation and validation

Implementation date: 2026-10-28. All eight work areas have working code. This record distinguishes automated/headless verification from physical hardware checks. No user media, live playback, installed binary or desktop configuration was changed during this work. The parent review and follow-up fixes are recorded below.

| Spec area | Implementation | Verification |
| --- | --- | --- |
| 1. Private cueing | `src/queue.*` assigns persistent per-occurrence UUIDs; `src/backend.*` separates cue and live indices and snapshots live presentation settings. Row body selects; Play cue explicitly loads at its start. MPRIS track IDs hash the stable item ID. | `privateCueAndPresentation`: real mpv continues A while cue B is selected/scrubbed; paused A is unchanged; Play cue starts B at 1.25s. Duplicate IDs, move/remove identity and private settings are checked. Existing MPRIS transport tests pass. |
| 2. Confidence preview | `backend.*` samples `screenshot-to-file ... window` from the audience mpv at 700ms; response generation prevents stale-frame labeling. One-shot ffmpeg cue frames have no audio. Hidden preview stops timer/decoder and clears sources. QML renders output/paused/standby/blackout/off states distinctly. | `previewsAndDisplayRecovery` creates a real silent cue frame; checks actual mpv screenshot success or explicit unavailable state, timer/process cleanup and unchanged capture counter while hidden. Rendered expanded preview shows independently labeled private cue and projection-off state. Actual GPU output-frame correspondence remains a physical-output check; null video output honestly reports capture unavailable. |
| 3. Audio output | mpv device discovery is asynchronous, with stable identity, readable label, System default, persistence and visible unavailable-device fallback. Apply failures are surfaced and stale device responses ignored. Master volume/mute remain separate from effective item volume. | Actual mpv property queries verify volume/mute policies. Injected synthetic device lists verify selection and loss/default recovery. Device enumeration runs against installed mpv. Physical speaker routing/device unplug was not tested. |
| 4. Instant blackout | A per-load/dynamic black video filter plus subtitle suppression replaces picture without Stop/reload. Latest master mute/volume are recomputed, avoiding stale restoration. Blackout persists across Stop/EOF/Next/cues, clears on End projection. | `blackoutAndAudioDevices`: real mpv filter, subtitle policy, playing/paused position continuity, audio-continue/mute policies, deliberate volume/mute changes, Next, EOF, Stop and End. File load receives blackout options before the first frame. Pixel-level projector flash checks remain hardware verification. |
| 5. Reusable playlists | Atomic version-2 Beam playlists retain IDs/cue/presentation data; autosession migrates 0.1 files. UTF-8 local M3U/M3U8 uses relative paths and retains duplicates. Save/discard guards protect edits. Queue duration/embedded-track probes run one at a time with timeout; missing files relink/remove. Drag handles and move buttons coexist. Headless `check`/`convert` commands expose JSON. | `reusablePlaylists`: non-ASCII/spaced duplicate fixture, settings/ID roundtrip, M3U relative import/export, rejected remote URLs, missing/relinked file, failed write, legacy migration. `qmlInteraction` performs actual row click and drag reorder through Qt mouse events. Unknown durations remain explicit. CLI valid/invalid examples checked locally. |
| 6. Display setup | Connector routing retains readable model/size labels and missing connector identity; output loss closes projection and requires explicit restart. Numbered image cards use separate connector-routed mpv processes and dismiss after 5s/Escape/click. Explicit test pattern refuses live replacement and returns to standby. `bin/setup-output` / `bin/install --setup-output` provide opt-in atomic backup/idempotence/reload validation/rollback, scoped to `^beam-output$`. | Headless test simulates connector loss/reappearance without silently selecting another; exercises test-pattern exit, distinct screen-name/fs-screen-name arguments per connector, real headless process startup and identification cleanup. Three Python setup tests verify preserved unrelated config, backup, repeat safety and failed-validation rollback using temporary files/mocked hyprctl. Physical placement, wake/reconnect and live compositor reload were not exercised. |
| 7. Presentation settings | ffprobe discovers labeled embedded audio/subtitle tracks. Cue settings offer audio auto/off, subtitle off/auto, original aspect, fill/crop, 4:3/16:9, reset and absolute per-item volume or inheritance. Live settings are snapshotted and independent of cue edits. Missing tracks fall back to automatic audio/subtitles off. | Generated two-audio/one-subtitle MKV verifies actual mpv aid/sid/aspect/volume and cue start. Reset, missing-track fallback and private edits of a live occurrence are checked. Master is preserved and inherited defaults restored on replay. |
| 8. Installation/releases | Version 0.2.0 in app and PKGBUILD. ffmpeg runtime dependency added; test dependencies documented. Arch CI builds/tests with a private D-Bus. `bin/release` prepares deterministic source archive, local-source PKGBUILD, Arch package and SHA256SUMS. README/release notes/screenshots updated. | `bin/build`, full `bin/test`, CLI checks, screenshot QA and mechanical Impeccable detector completed. Release artifacts are prepared under `dist/`; package integrity/source checks and checksum verification are recorded below. Remote CI/publishing remain parent actions. |

## Final checks

- Build: qmake6 + C++17 build succeeds; no compiler warnings in the final build.
- Full tests: 11 Qt tests passed, 0 failed; three Python setup tests passed. Real mpv and external gdbus are used under an isolated dbus-run-session, not mocked playback. A sandbox escalation was required for local IPC sockets; only temporary synthetic fixtures were used.
- A generated six-second multitrack fixture contains two labeled audio streams and one subtitle stream. A separate three-second video tests transport/EOF.
- Actual QML keyboard and pointer input covers Play/Stop, row cue selection and drag reorder. Settings geometry is checked in the QML test.
- Rendered and inspected: `docs/screenshots/light.png`, `dark.png`, `compact.png` (620×540), `preview.png` (800×740), `settings.png` (Danish), `empty.png`, `error.png`. Synthetic media and a clearly named demo connector are used. There are no QML warnings in capture logs.
- Impeccable detector over changed QML returned `[]`. The existing stale `.impeccable/design.json` warning was reported; no unrelated sidecar repair was made.
- Probe teardown explicitly disconnects callbacks before killing the process, preventing a queued probe from restarting during destruction. Preview and device IPC responses are correlated; process/decoder shutdown is bounded.

## Local release preparation

Run `bin/release` for the full check/build, or `bin/release --skip-checks` only after the same tree has passed the checks. Output is ignored by git:

- `dist/beam-0.2.0.tar.gz` — deterministic source archive, fixed ownership/order/mtime.
- `dist/PKGBUILD` — builds from that local source archive and pins its SHA-256.
- `dist/beam-0.2.0-1-x86_64.pkg.tar.zst` — versioned Arch package for this machine.
- `dist/SHA256SUMS` — source/package hashes for parent review/publication.

The implementation agent prepared these artifacts without publishing or installing them. Subsequent commit/push and user installation are handled by the parent after review; physical output checks remain with the user.

## Exact remaining limits

Physical multi-monitor fullscreen placement, GPU confidence-frame correspondence, projector blackout transition appearance, monitor wake/reconnect and actual speaker/device routing require the target display/audio hardware. Headless mpv uses `--vo=null --ao=null`; its state/filters/settings are real, but that does not establish pixels on a projector or sound on a selected speaker. Setup rollback/reload responses use mocked hyprctl against temporary files; the user's already-installed Beam-only rule was only read. Arch x86_64 is built locally; aarch64 is declared but not cross-built. GitHub Actions has not run remotely until the parent pushes.

## Changed files

`src/backend.{h,cpp}`, `src/queue.{h,cpp}`, `src/Main.qml`, `src/main.cpp`, `src/mpris.cpp`, `src/i18n/{en,da}.json`, `beam.pro`, `tests/beam_tests.{cpp,pro}`, `tests/setup_output_test.py`, `bin/{test,install,setup-output,release}`, `pkgbuild/PKGBUILD`, `.github/workflows/check.yml`, `.gitignore`, `README.md`, `PRODUCT.md`, `DESIGN.md`, `docs/release-0.2.0.md`, this status record and the seven screenshots above. The approved `docs/specs/beam-0.2.md` is retained.

## Parent review

Reviewed playback/cue isolation, playlist persistence, process cleanup, MPRIS, installation scripts and rendered controller/settings screenshots. Fixed private preview invalidation after cue removal, replacement, playlist loading and cue reset; adding the first item also starts its preview when enabled. Regression coverage checks that the old frame disappears immediately after relinking/removal and that a new cue frame is generated.

The mpv property-query test helper now matches request IDs instead of treating the first received line (which can be an event) as a command reply. Final local build passed; the full suite passed with 11 Qt checks and 3 setup checks after these fixes. Physical display placement, blackout transitions and speaker routing remain the user's hardware acceptance checks.

User acceptance reports uncovered two further issues before publication. The Quit button now explicitly clears the close guard before closing the window; the QML mouse test verifies closing a dirty playlist without saving. Display identification now uses a separate mpv process for each connector, with both screen-name and fs-screen-name set, because Qt top-level screen hints placed both cards on one monitor. Tests verify two distinct connector arguments and process cleanup; actual compositor placement remains a hardware acceptance check. The full suite passed again after both fixes (11 Qt / 3 setup).

Follow-up visual review replaced Material's bright modal scrims with a shared 60% black backdrop for shortcuts, settings and playlist dialogs. Rendered shortcuts in both light/dark themes and refreshed the settings screenshot; modal surfaces retain the active theme palette.

## Preview refresh modes

Added a persisted Output preview selector: Low uses the original 700ms interval; Smooth targets one frame every 84ms (up to 12 fps). Capture remains single-flight, uses JPEG quality 75 and decodes to a bounded 640×360 display size. Hiding previews stops captures in either mode; the private cue remains a still frame. Real achieved refresh depends on output hardware/capture latency.

Verification: 12 Qt checks and 3 setup checks passed, including mode persistence/default/invalid-value handling, faster headless capture requests, suppression of overlapping requests, cleanup and keyboard selection in the actual QML control. Rendered light/dark and Danish preview layouts and refreshed the preview screenshot. Physical GPU frame rate is not established by the headless tests.
