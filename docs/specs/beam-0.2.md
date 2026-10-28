# Beam 0.2 — screening workflow and release readiness

Status: approved for implementation by the user on 2026-10-28.
Repository: https://github.com/jeancarlosg93/beam
Implementation owner: GPT-6.1 Sol, medium reasoning.

## Objective

Make Beam reliable and comfortable for operating a live screening: prepare the next video privately, know what the audience sees, choose the audio destination, recover instantly to black, and reuse a show playlist. Implement all eight work areas below as working features, not mock controls or documentation-only promises.

## Preserve the established product

- C++17 / Qt Quick with qmake6, native portal file dialogs, dedicated mpv output and private JSON IPC. Keep Omarchy's compact square controls, system monospace font, live light/dark palette and themed playlist. Read PRODUCT.md, DESIGN.md and the applicable Omarchy app / Impeccable skills.
- The output connector is explicitly selected. Start projection opens standby; opening the controller never automatically starts media or projection.
- Pause holds the frame. Stop returns to the selected standby image or black. End projection closes the output and releases inhibition. Default EOF behavior returns to standby and waits for the operator; automatic continuation remains optional.
- Keep MPRIS and normal media keys. Preserve the tested pause intent across asynchronous file loads. No duplicate MPRIS source from an auxiliary preview player.
- Never modify source media. Preserve/migrate existing saved sessions. Use atomic writes and report failures in the UI.
- Keep ordinary operation compact (currently 800×580 default, 620×540 minimum). Use an expandable settings area for occasional controls. Optional preview may expand the window or reduce the queue area, but the collapsed normal layout must remain usable at the existing minimum.
- User-specific Hyprland rules already installed on this machine target only `^beam-output$`, with `no_dim = true` and `idle_inhibit = "always"`. Do not overwrite unrelated desktop configuration.

## 1. Private cueing

Separate the selected/cued item from the actively projected item, using stable item IDs that survive reorder and distinguish duplicate paths.

- A single row click selects a cue without changing the audience output or current playback.
- Show cue title, duration, missing-file state and an editable start position. Inspecting/scrubbing the cue remains private.
- A clearly labeled Play cue action starts that cue at its configured starting point. Existing row play actions remain explicit live actions.
- Transport controls and MPRIS describe/control the active media while it exists; selecting another cue must not redirect pause, seek or metadata away from the active media.
- Do not silently stop a live video merely to inspect another file. Maintain useful selection after EOF.

Acceptance: project A, select and scrub B, verify A continues unchanged; Play cue switches to B at its start point. Reordering/removing a different entry preserves A's identity. Duplicate filenames remain independently addressable.

## 2. Optional confidence preview

- A hideable preview on the controller shows the actual output state: Playing, Paused, Standby, Blackout or Projection off, with elapsed/remaining time where applicable.
- Provide a private cue preview when inspecting an unplayed item, clearly distinguished from the live output preview.
- Live frames may be sampled at a modest rate; do not claim frame accuracy or create an independently running preview that can drift from the actual output. No sound from previews.
- Hidden preview must stop its capture/decoding work. Show honest unavailable/loading states if capture fails; never leave an old frame labeled live.
- Standby images and blackout must be represented correctly.

Acceptance: live preview corresponds to the active output, cue inspection does not alter output, paused frame remains stable, and preview-disabled mode performs no periodic frame capture. Verify CPU/process cleanup and no audio duplication.

## 3. Audio output

- Enumerate available mpv audio devices with readable names and an explicit System default option.
- Allow selection in settings, persist stable device identity, and restore it when available.
- Apply changes safely and report errors. When a saved device disappears, visibly identify that condition and offer System default rather than silently choosing an arbitrary device.
- Keep master volume/mute behavior and MPRIS volume coherent.

Acceptance: test enumeration, applying/persisting a device, unavailable-device handling, mute and volume. Use injectable device data for unavailable hardware paths, and distinguish those tests from real hardware checks.

## 4. Instant blackout

- Add a clear Blackout toggle and a documented keyboard shortcut (B if unassigned).
- Blackout temporarily replaces only the audience picture with black while retaining playback position and the preexisting playing/paused state. It must not implement blackout by Stop/reloading the media.
- Let the operator choose whether audio continues during blackout. Preserve the prior mute/volume state on restore; avoid a stale restoration overwriting a deliberate volume change.
- Define Stop, EOF, Next and End projection behavior while blacked out. Prefer keeping blackout visibly engaged across track transitions until explicitly cleared, and clear it on End projection.
- The status and confidence preview must make blackout unmistakable.

Acceptance: toggle blackout while playing and paused, check position/state continuity on restore, verify both audio policies and the transition cases above. No flashing video on the audience output during blackout transitions.

## 5. Reusable playlists

- Save/load named Beam playlists with versioned structured data, including stable items, cue start points and per-item presentation settings. Keep autosaved session recovery.
- Import/export local M3U/M3U8 with UTF-8 support and relative paths resolved against the playlist directory. Preserve order and duplicates. Clearly document settings that M3U cannot represent.
- Support drag-to-reorder as well as accessible existing move controls.
- Probe media duration asynchronously, show per-item and total known duration, and distinguish unknown durations rather than presenting an inaccurate total.
- Flag missing/unreadable media before playback, with a useful relink/remove recovery. Opening another playlist must not silently discard unsaved named-playlist changes.
- Do not treat imported metadata as shell commands or enable remote URLs as an accidental side effect.

Acceptance: round-trip a named playlist with duplicate/non-ASCII/spaced paths and item settings; import/export relative M3U; test missing files, failed writes and old-session migration; reorder via actual QML interaction; verify duration jobs never block the UI.

## 6. Display setup

- Show readable connector/model information while retaining stable connector identity for output routing.
- Identify displays using temporary, clearly numbered overlays that dismiss automatically or with Escape and clean up fully.
- Add an explicitly invoked test pattern for framing/aspect/color checks. Return to standby after testing; do not unexpectedly replace live playback when merely opening settings.
- Offer an explicit installer/setup option for the Beam-only no-dimming/keep-awake rules. Make it idempotent, back up files, preserve unrelated settings and validate Hyprland reload/configerrors. An opt-in flag is acceptable for a CLI installer.
- Hotplug, output loss and wake/reconnect must have clear recovery; never silently move live projection onto a different screen.

Acceptance: test simulated monitor loss/reappearance and identity preservation, overlay cleanup, test-pattern exit and safe repeat setup. Verify physical multi-monitor placement when hardware is available and report exactly what was tested.

## 7. Presentation settings

- Offer embedded subtitle and audio-track selection, including subtitles off. Display language/title information where present and handle files without tracks.
- Provide fit/aspect choices with safe default Preserve original aspect; do not stretch video by default. Provide a reset to default.
- Support per-item volume overrides (inherit master by default), with clearly defined interaction with master mute/volume and MPRIS. Document the chosen simple semantics.
- Persist item settings in Beam playlists and restore them on playback; changing a private cue's settings must not change the currently live item.

Acceptance: use generated multi-track fixtures; verify chosen tracks/aspect/volume apply to the intended item and do not leak to the next item. Confirm missing-track fallback and reset behavior.

## 8. Installation, packaging and releases

- Keep a simple source/user install path. Update runtime/build/test dependency declarations accurately for the chosen implementation.
- Provide versioned Arch packages and a reproducible release build/check process. Add CI for the meaningful build/test checks, using an isolated session bus where appropriate.
- Prepare release notes, package checksums, installation/update instructions and accurate light/dark screenshots for the README. Use synthetic demo media only.
- Version consistently across app, package and release artifacts. Target 0.2.0 for this feature set.
- The implementing subagent prepares the release and reports its assets and validation. The parent reviews, then handles publishing commits/tag/GitHub release using the user's existing authorization. Do not upload unfinished builds or invent release verification.

## Engineering and validation

Use small coherent backend components/models as needed rather than adding all work to one ever-growing QObject. Keep process execution argument-based, bound concurrent media probing, protect IPC lifecycle and correlate asynchronous responses so stale cue/device/file-load events cannot modify the new active item.

Extend the existing tests with behavioral coverage for cue/live separation, stable identity, playlist persistence/migration, blackout restoration, device/track settings, preview cleanup, monitor recovery and MPRIS. Exercise real mpv where practical and actual QML input for interaction changes. Repeat timing-sensitive checks only to validate a concrete race or unresolved concern. Maintain existing transport/session/theme tests.

Inspect rendered light/dark, collapsed/expanded preview/settings, minimum-size, empty/error and populated states. All visible user-facing additions are translatable; update the shipped English/Danish coverage and help shortcuts. Respect skill-required review/documentation steps.

## Completion contract

Deliver implementation, passing appropriate tests, screenshots, updated README/PRODUCT/DESIGN and packaging/release preparation. Add `docs/specs/beam-0.2-status.md` mapping every numbered work area to changed files, test evidence and any genuine remaining limitations. No checked-off feature may be just a stub. If an external dependency or hardware limitation blocks a portion, implement all independent work and report the precise blocker; do not substitute simulated success for real verification.
