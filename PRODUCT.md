# Beam
<!-- impeccable:product-schema 1 -->
## Platform
Linux desktop (Qt Quick; the skill's web/iOS/Android enum does not cover Linux).
## Stack
C++17 / Qt Quick, qmake6, external mpv controlled over a private JSON IPC socket. User requested Qt and Omarchy aesthetics.
## Users
An Omarchy user operating a playlist while projecting video fullscreen to a chosen monitor.
## Product Purpose
Keep the controls on the operator's desktop and send only the video or standby image to the selected output.
## Capabilities and Constraints
Monitor selection, playlist, regular transport controls. Pause after each video is the confirmed default. Between videos, show black or a user-selected image. Omarchy full light/dark palette and compact shell styling follow live, including the playlist. System media keys integrate through MPRIS. Originals remain untouched.
## Open Decisions
App name Beam chosen during implementation. Additional audience and remote-control requirements are undecided.

## Screening workflow (0.2)

A selected cue and the active audience item have independent stable identities. Cue inspection, start position and presentation edits are private until Play cue. Optional confidence frames come from the live mpv output; private cue frames are silent one-shot decodes. Blackout preserves transport and may retain audio. Named Beam playlists retain presentation settings; UTF-8 local M3U exchanges paths/order only. Audio-device selection, display identification/test pattern and explicit output-loss recovery support setup without changing the default standby/EOF behavior.
