# Beam

A Qt Quick playlist controller for projecting mpv video fullscreen on a display you choose. The operator window stays separate from the output. It follows Omarchy's full light/dark palette and 12px monospace shell typography live, with compact square controls and a theme-colored playlist.

| Light theme | Dark theme |
| --- | --- |
| ![Beam with a demonstration playlist in a light Omarchy theme](docs/screenshots/light.png) | ![Beam with a demonstration playlist in a dark Omarchy theme](docs/screenshots/dark.png) |


1. Add videos with **Add media**, drag and drop, or `beam file.mp4`.
2. Select the output display and choose **Start projection**. Standby is black by default; use **Choose image** for a custom still image.
3. Play a video using its row's play button. Videos stop at the end and return to standby. Use **Next** to cue the following video. Enable **Continue automatically** for continuous playback.

**Pause** holds the video frame. **Stop** returns to standby. **End projection** closes the output window and enables display selection again. The playlist, standby image, output choice, volume, repeat and continuation settings are restored next time. Original media files are never modified. Audio uses the system's default output; change the audio destination in Omarchy's sound settings if necessary.

The selected item stays selected after it finishes; Play starts it again. Previous/Next play their target immediately. Repeat wraps Next, and wraps automatic playback when continuation is enabled.

## Install and build

```sh
git clone https://github.com/jeancarlosg93/beam.git
cd beam
bin/build
bin/test
bin/install --user
```

User installation puts the binary in `~/.local/bin/beam` and the launcher entry under `~/.local/share/applications/`. Open **Beam** from `Super + Space`.

For a system package, `bin/install` runs `makepkg -fsi` and needs administrator authentication. The resulting Arch package is written to `pkgbuild/`.

Build dependencies: Qt 6 base, declarative, Wayland, SVG, a C++17 compiler and make. Runtime: mpv and xdg-desktop-portal with an active file chooser backend. Tests also need ffmpeg and gdbus (glib2). No libmpv development package is needed.

The app starts a dedicated mpv process with its own private IPC socket and `--no-config`, so personal mpv bindings/scripts cannot affect the clean projection output. Output is selected by connector name, using mpv's `screen-name` and `fs-screen-name` options. Disconnecting the selected output ends projection. Switch display targets by ending and restarting projection.

## Omarchy projection rules

To keep the projection bright when the controller has focus and prevent idle screen-off while the output is open, add this to `~/.config/hypr/looknfeel.lua`:

```lua
o.window("^beam-output$", { no_dim = true, idle_inhibit = "always" })
```

Reload with `hyprctl reload`, then check `hyprctl configerrors`. The exception targets the separate projection window, including paused video and standby. Ending projection releases its idle inhibitor. Installing Beam does not edit Hyprland configuration automatically.

## System media controls

Beam implements [MPRIS](https://specifications.freedesktop.org/mpris/latest/), the Linux media-player D-Bus API. Omarchy's existing play/pause and next/previous keys control it through the shell media service. It appears as **Beam** in system media controls with track title and playback state. When several players are open, the shell's selected player determines which one receives media keys.

Play/pause, next/previous, stop, seek, playback rate, repeat mode and volume are exposed. Stop returns the projection to standby. System next/previous preserve paused or stopped state; the app's row play buttons explicitly start playback. The standard service is `org.mpris.MediaPlayer2.beam`; additional instances use a PID suffix. No desktop keybindings are changed.

## Shortcuts

| Key | Action |
| --- | --- |
| Space | Play / pause |
| S or Escape | Stop to standby |
| Left / Right | Seek backward / forward 5 seconds |
| Ctrl + Left / Right | Play previous / next video |
| M | Mute / unmute |
| Ctrl + O | Add media |
| ? | Show shortcuts |
| Q | Quit and close projection |

## State, language and verification

Session data is saved atomically to the Qt application data directory (normally `~/.local/share/beam/session.json`). Missing files remain in the queue and show an error when played. Omarchy's language file is watched live; English and Danish interface catalogs are included, with English fallback for untranslated strings and other languages. No Omarchy configuration is changed.

`bin/test` exercises session recovery, playlist ordering, actual mpv playback/pause/seek/speed/volume/mute, standby after EOF, continuous repeat, shutdown/restart, live palette/font reload, translation parsing, external D-Bus media-control calls and real Qt Quick keyboard interaction. `BEAM_LIVE_TEST=1 build/tests/beam_tests qmlInteraction` runs the interaction check on the active Wayland session. The external display workflow still needs physical verification with a second monitor; development was done with only eDP-1 connected.

For isolated QA, `BEAM_STATE_FILE`, `BEAM_LANGUAGE_FILE` and `BEAM_THEME_DIRECTORY` override source paths. `BEAM_CAPTURE=/absolute/file.png` captures the controller after initial rendering and exits; `BEAM_CAPTURE_COMPACT=1` requests the minimum size. Demo screenshots use synthetic files under `/tmp/beam-demo`; these are not installed or added to your real playlist.

Licensed under MIT. Theme and language infrastructure is adapted from the Omarchy app templates (Omacut / Monologue, David Heinemeier Hansson).
