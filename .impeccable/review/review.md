# Disposition

**Fix**, then recapture the fallback-accent state. The controller is otherwise ready on the reviewed Linux desktop sizes. No redesign is warranted.

# Evidence scope

Read-only review of `src/Main.qml`, `src/BeamButton.qml`, `src/IconButton.qml`, `src/backend.cpp`, `PRODUCT.md`, and `DESIGN.md`, with supporting inspection of the theme contrast implementation. Visually inspected all five supplied actual-Wayland screenshots: `desktop.png`, `compact.png`, `empty.png`, `danish.png`, and `readme.png`. Desktop captures represent the 920×760 logical window; compact and Danish captures represent 720×740. Captures use 2× display scaling and synthetic sample filenames.

The direction is the established Omarchy visual authority in Operate mode: restrained dark surfaces, live accent, native Qt Material controls, sharp app surfaces, output and standby controls above the queue, transport below. This is a native Qt application; no web detector was applied. The parent reports six passing automated checks, including mpv transport and Qt keyboard interaction, plus successful real Wayland playback. Those results were supplied evidence, not rerun by this reviewer. Only the laptop screen was connected, so physical second-monitor placement remains unverified.

# Material findings

1. **Fallback primary-button contrast fails in `readme.png`.** “Start projection” is visibly white on the bright yellow fallback accent. White on #FFD60A is approximately 1.4:1, below the 4.5:1 body-text floor. `BeamButton.qml` now requests `theme.accentForeground`, and the theme computes black for this accent; this leaves either a stale screenshot or a native Material label-color override to resolve. The rendered fallback must show a dark label before this evidence can pass.

No control overlaps, displaced transport, clipped action labels, or unreadable primary text were found in the other four captures. The playlist's partially visible next row is normal clipped scrolling content; the row actions remain within the queue. The Danish strings fit at minimum size. The empty state explains both file-drop and picker entry points, and unavailable transport controls are visibly disabled.

# Craft assessment

Hierarchy and spacing fit a desktop controller. Display choice and the primary projection action are immediately findable; standby is a compact, named setting. The central queue has readable item names with secondary paths, coherent vector controls, and a restrained active-row accent. Transport, current-item title, state, position, speed, and volume remain grouped and anchored below the playlist at both reviewed sizes. The blue live-accent capture is legible and the dark palette suits the projection use scene.

Icon controls expose accessible names through `IconButton`; display, speed, timeline, and volume controls also have names. Keyboard shortcuts cover the core playback actions and have an on-screen reference. Backend state and copy distinguish projection off, connecting, standby, playing, and paused; Stop and noncontinuous EOF return to standby while the projection process remains open. Pause holds the media frame. Native Material affordances are appropriate here; added decoration or bespoke interaction would not improve this brief.

# Required fixes

1. Verify the fallback build and ensure the highlighted projection button renders a dark foreground on yellow. If the current code already does so, replace the stale `readme.png`; otherwise correct the native button label styling and recapture. Reinspect the actual rendered fallback state after the change.

No other required visual fixes arise from the supplied evidence. Retain the stated external-monitor verification limitation in delivery notes.
