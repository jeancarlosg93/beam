# Beam 0.2.0

Beam now separates private preparation from the audience output. Select a row and set its start point without interrupting the live video; **Play cue** performs the switch.

- Optional sampled confidence preview and silent private cue frames, with capture work stopped when hidden.
- Named versioned Beam playlists, local UTF-8 M3U/M3U8 exchange, drag reorder, asynchronous durations and missing-file relinking.
- Audio device selection, embedded audio/subtitle track choices, fit/aspect settings and per-item volume.
- Blackout (`B`) keeps playback running or paused in place, with an audio continuation policy.
- Numbered display identification, test pattern, explicit disconnect recovery and an opt-in backed-up Hyprland setup command.
- Compact square Omarchy styling, live light/dark colors, English/Danish coverage, MPRIS and existing pause/standby defaults retained.

## Updating

Build and install the new source with `bin/install --user`, or install the generated Arch package with `sudo pacman -U beam-0.2.0-1-<arch>.pkg.tar.zst`. Existing sessions migrate automatically to version 2; source media remain untouched. To preserve an independent copy of the old session, copy `~/.local/share/beam/session.json` before first launch.

`bin/release` runs build/tests and prepares a reproducible source archive, a local source PKGBUILD, an Arch package and SHA256SUMS under `dist/`. It never tags, pushes or publishes. Run it as an ordinary user with Qt development packages, mpv, ffmpeg, dbus, glib2, Python, makepkg and fakeroot installed. Verify the downloaded artifacts with `sha256sum -c SHA256SUMS` before installation.

The physical display/audio checks outstanding for this build are recorded in `docs/specs/beam-0.2-status.md`; automated headless checks do not establish physical projector placement or speaker routing.
