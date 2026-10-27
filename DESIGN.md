---
name: Beam
description: A compact Omarchy shell-style video projection controller.
colors:
  background-fallback: "#0e0e10"
  foreground-fallback: "#eeeeef"
  secondary-fallback: "#b6b6bf"
  selection-fallback: "#35353c"
  red-fallback: "#ef667a"
  accent-fallback: "#FFD60A"
typography:
  body:
    fontFamily: "monospace"
  body-size-fallback:
    fontSize: "12px"
rounded:
  square: "0px"
  slider-track: "2px"
  slider-handle: "6px"
spacing:
  tight: "3px"
  small: "8px"
  outer: "16px"
components:
  button:
    rounded: "{rounded.square}"
    height: "28px"
    padding: "6px 10px"
  icon-button:
    rounded: "{rounded.square}"
    width: "28px"
    height: "28px"
    padding: "5px"
  combo-box:
    rounded: "{rounded.square}"
    height: "28px"
    padding: "0px 28px 0px 10px"
  checkbox:
    height: "28px"
  queue:
    rounded: "{rounded.square}"
  queue-row:
    height: "44px"
  slider-handle:
    rounded: "{rounded.slider-handle}"
    width: "12px"
    height: "12px"
---

# Design System: Beam

## Overview

**Creative North Star: “The Omarchy shell projection panel.”** Beam follows Omarchy shell panels in Operate mode: compact monospace typography, square flat controls, thin borders, and the live desktop palette. The user's shell-panel direction supersedes the earlier large dark Material controller. Light and dark themes are equally native to the system.

**Key Characteristics:**

- Full live palette and shell base-font size.
- Dense single-window controls with a flexible playlist.
- Square buttons and fields, small circular slider handles.
- Separate mpv media/standby output; no competing controller preview.

Portable frontmatter records actual fixed geometry and explicitly named fallback values. Dynamic QML bindings live in `.impeccable/design.json`; fallback colors are not a screenshot palette or fixed runtime theme.

## Colors

### Primary

The accent comes from `theme.accent`, read from `~/.local/state/omarchy/current/theme/colors.toml`. It marks projection status, highlighted transport, and focused controls. Highlighted text buttons retain the theme foreground over a translucent accent surface; they are no longer solid accent buttons with contrast-selected ink.

### Neutral

`theme.colors.background` supplies both window and playlist backgrounds. `foreground` supplies primary text, vectors, and alpha-derived borders; `light_foreground` supplies secondary copy and unselected paths in both theme modes. The selected row path uses `foreground` for sufficient contrast over `selection`. The current playlist row uses `selection`, rather than a fixed dark surface or accent wash. Hover uses foreground at 0.08 alpha. Playlist borders use foreground at 0.16 alpha; structural dividers use 0.12.

### Semantic feedback

Missing-file text uses `theme.colors.red`. Error banners combine this role with a 0.1-alpha red background. All six-digit hex color entries in colors.toml are loaded; missing roles retain Theme defaults. The frontmatter records the fallback values used by these components, not the active light palette. Theme also initializes `dark_background` and `lighter_background`, which current custom QML surfaces do not use.

**The Whole Theme Rule.** Keep playlist, controls, text, selection, and feedback bound to their live roles. Do not retain a hardcoded dark playlist inside a light shell theme. Material mode follows the file's `mode = "light"`; otherwise it is dark.

## Typography

All controller text uses the system `monospace` alias, matching the shell's font choice without naming a specific installed family. Body size binds to `theme.fontSize`, read live from shell.toml `base-size`, clamped to 8–24 logical pixels with the documented fallback. Header size is base+4; current-item title and help heading are base+2; playlist and empty-state headings are base+1; queue paths are base−1. At the fallback base these are 16, 14, 13, and 11 logical pixels respectively. Header, playlist, and current-item titles are demi-bold; active queue names also become demi-bold.

Elide titles on the right and paths/standby filenames in the middle. Preserve translation-aware labels and Qt text-direction mirroring. QML pixel sizes are logical desktop units; `px` in frontmatter is portable notation, not CSS source.

## Layout

Default window: 800×580 logical pixels. Minimum: 620×540. One column uses the outer spacing token for margins and small spacing between major groups. Output selection and projection action lead, followed by guidance and inline standby controls. Playlist occupies the flexible center, with a 100-pixel minimum height, one-pixel inner inset, clipped scrolling, and compact rows. Each row uses 12-pixel left and 8-pixel right insets, with small gaps.

Continue automatically and Repeat playlist sit below the queue. Current media, state, speed, timeline, transport, and volume remain grouped below a divider. At reduced sizes the queue gives up height and scrolls; there are no browser breakpoints. The help popup is centered, uses outer padding, and caps width at the smaller of 500 or window width minus 48. The speed selector is 78 pixels wide; volume's preferred slider width is 100.

## Elevation & Depth

Custom surfaces are flat. Alpha-derived fills and one-pixel outlines convey hover, focus, selection, and grouping without custom shadows. Qt Quick Controls Material remains the underlying control framework, but button, combo box, checkbox, slider, icon button, and help-popup visuals are explicitly shell-styled. Do not reintroduce the previous spacious Material appearance.

## Shapes

Buttons, dropdowns, checkboxes, playlist, and popup backgrounds are square. Check indicators are 14×14 with an 8×8 square checked mark. Slider tracks are four pixels tall with the track rounding token; handles use the circular handle token. Icon artwork is 16×16 inside the icon-button geometry.

Hyprland rounding is still read into `theme.radius` and supplied to Material's roundedScale, and the error banner uses it as a QML radius. This does not override the custom components' explicit square geometry. No-Hyprland fallback is zero; Material roundedScale and Rectangle.radius are different Qt properties.

## Components

### Buttons and icon controls

`BeamButton` has compact fixed-height geometry, foreground text, and a one-pixel outline. Default background is foreground at 0.04 alpha; flat buttons are transparent without a border at rest. Hover/focus uses 0.08 alpha and pressed uses 0.22; highlighted rest uses accent at 0.12. Highlighted/focused borders use accent at 0.4 alpha; disabled borders use 0.16. Disabled text opacity is 0.4. Labels remain centered plain text with right elision.

`IconButton` uses foreground or highlighted accent, with disabled icon opacity 0.35. Its surface is transparent at rest, foreground at 0.08 for hover/focus and 0.18 when pressed; keyboard focus adds an accent border. Accessible names and 500ms tooltips identify vector actions.

### Dropdowns and checkboxes

`BeamComboBox` uses foreground text, a drawn chevron, a square outlined field, and a square popup on the live background. The popup scrolls above a 240-pixel height cap. Focus changes its border to accent. `BeamCheckBox` replaces both previous switch and checkbox visuals with a compact square indicator and foreground square checked mark; focus outlines the indicator in accent.

### Sliders

`BeamSlider` uses a foreground-alpha track and progress fill with a small circular foreground handle. Disabled handles use the selection role. Keyboard focus adds a two-pixel accent handle border. The control's implicit height is 24, with six-pixel horizontal padding.

### Playlist and output

The playlist follows the complete live theme, including selection and secondary text. Each row retains play, title/path, move, and remove actions. The empty state explains drop and Add media entry points. Output selection disables while projection is active. Standby names either Black screen or the chosen image; Use black appears only when an image exists.

### Standby signature

The separate mpv output stays open on Stop or noncontinuous EOF, returning to black or the chosen image. Pause retains the media frame. State copy distinguishes projection off, connecting, standby, playing, and paused. Preserve accessible control names, existing keyboard shortcuts, native interaction semantics, and the shortcut popup.

## Do's and Don'ts

### Do:

- **Do** inherit the full live palette and shell font size, including the playlist.
- **Do** retain compact square controls and small circular slider handles.
- **Do** keep display/standby above the flexible queue and transport below it.
- **Do** check light, dark, compact, empty, and translated states when changing visuals.

### Don't:

- **Don't** freeze fallback dark colors into QML or document them as the active light theme.
- **Don't** restore oversized rounded Material buttons or large typography.
- **Don't** invent browser breakpoints, CSS replicas, or static font sizes for dynamic roles.
- **Don't** merge pause, stop, and standby into one state or add a competing controller preview.
