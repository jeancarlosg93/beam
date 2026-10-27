---
name: Beam
description: A restrained Omarchy video projection controller built with Qt Quick.
colors:
  main: "#0e0e10"
  panel: "#19191c"
  foreground: "#eeeeef"
  secondary: "#b6b6bf"
  divider: "#303036"
  row-hover: "#25252b"
  disabled-icon: "#62626c"
  error-panel: "#402129"
  error-foreground: "#ffccd5"
  accent-fallback: "#FFD60A"
typography:
  app-title:
    fontSize: "24px"
    fontWeight: 600
  current-title:
    fontSize: "22px"
    fontWeight: 600
  section-title:
    fontSize: "18px"
    fontWeight: 600
  status:
    fontSize: "12px"
  path:
    fontSize: "11px"
  clock:
    fontFamily: "monospace"
rounded:
  sharp-fallback: "0px"
spacing:
  tight: "3px"
  small: "8px"
  regular: "12px"
  outer: "24px"
components:
  queue:
    backgroundColor: "{colors.panel}"
  queue-hover:
    backgroundColor: "{colors.row-hover}"
  icon-button:
    textColor: "{colors.foreground}"
    width: "40px"
    height: "40px"
  icon-button-disabled:
    textColor: "{colors.disabled-icon}"
    width: "40px"
    height: "40px"
  error-banner:
    backgroundColor: "{colors.error-panel}"
    textColor: "{colors.error-foreground}"
    padding: "{spacing.small}"
---

# Design System: Beam

## Overview

**Creative North Star: “The Omarchy projection desk.”** This describes the established Omarchy direction, rather than introducing an independent brand. Beam operates as a quiet native Linux controller: dark Qt Material surfaces, compact controls, a readable queue, and the current desktop accent. Mode: Operate; Omarchy remains the visual authority.

The controller makes output state easy to read while the separate mpv window shows only media or standby. Its signature is a stable projection surface that returns to black or a chosen image on Stop or noncontinuous end of media. Pause retains the media frame.

**Key Characteristics:**

- Native Qt Quick Controls Material in dark mode.
- Live Omarchy accent and Hyprland corner behavior.
- Flat tonal hierarchy with restrained emphasis.
- Operator controls grouped above and below a flexible queue.

The frontmatter records actual static primitives and the documented fallback values. Live Qt expressions are carried in `.impeccable/design.json`; the fallback accent and sharp fallback must never replace runtime bindings.

## Colors

The palette combines near-black surfaces, clear neutral text, and one live desktop accent.

### Primary

- **Live desktop accent:** `theme.accent`, bound to `Material.accent`, reads `accent` from `~/.local/state/omarchy/current/theme/colors.toml`. The `accent-fallback` primitive is used only when that file or its six-digit hex accent is unavailable. File and directory watches re-arm after changes, with an 80ms debounce.
- **Readable accent ink:** `theme.accentForeground` selects black or white using relative luminance. `BeamButton` explicitly colors its `Text` content item with this value when highlighted; native Material's highlighted text does not reliably honor foreground alone.

### Neutral

- **Main:** the window canvas.
- **Panel:** the queue surface and Material primary surface.
- **Foreground:** primary names, labels, and enabled neutral icons.
- **Secondary:** explanatory copy, paths, clocks, and status when projection is off.
- **Divider:** thin structural separators.
- **Row hover:** a quiet queue hover surface.
- **Disabled icon:** unavailable icon actions.

### Semantic feedback

`error-panel` and `error-foreground` form the dismissible error banner. Missing-file names and status use the error foreground. The current queue item uses `Qt.alpha(theme.accent, .14)`; this is a Qt color expression, not a fixed palette swatch.

**The Live Accent Rule.** Bind accent use to `theme.accent`; choose highlighted button text through `theme.accentForeground`. Do not freeze the fallback palette into a theme.

## Typography

The body inherits the Qt Material/platform font. No application font family or universal body pixel size is set in code; the earlier provisional 14px body specification is not an implementation token. Clock labels explicitly use the platform monospace family. Sizes in frontmatter are QML `font.pixelSize` values, expressed in Qt logical pixels rather than browser CSS pixels.

The app title and current-item title use demi-bold emphasis. Section headings use the smaller section-title role. Queue titles use the inherited body size and become demi-bold for the current item. Secondary paths use the path role; output descriptions and playback state use the status role. The empty-state heading uses the section-title size at its inherited weight. Shortcut help uses the current-title size at its inherited weight and a text line-height multiplier of 1.6.

Use right elision for queue titles and current-item names; use middle elision for paths and standby filenames. Preserve translation-aware labels and inherited text direction.

## Layout

The default window is 920×760 logical pixels; its minimum is 720×740. There are no web breakpoints or alternate navigation layouts. A single column fills the window with outer margins and regular vertical spacing. Rows use small or regular gaps; closely related stacked text uses tight spacing.

The header leads into display selection and projection control, output guidance, and inline standby controls. The queue occupies the flexible center, with a minimum height of 100 logical pixels. Queue rows are 62 logical pixels tall, with 12px left and 8px right insets and a one-pixel queue inset. Continue and repeat settings sit beneath the queue. A divider separates the current media, speed, timeline, transport, and volume group anchored below it. The shortcut popup is centered, padded with the outer spacing token, and constrained to the smaller of 500 logical pixels or window width minus 48.

At minimum size the structure remains intact; the queue gives up height and scrolls. The reviewed Danish strings fit at that size. Layout mirroring follows the Qt application's text direction. Display choice is disabled during projection, preventing a live output change through that field.

## Elevation & Depth

Beam's custom surfaces are flat and distinguished by tone, dividers, and the translucent current-item accent. There are no custom shadows or elevation tokens. Qt Material owns native button, popup, focus, hover, pressed, and disabled rendering; preserve those platform treatments instead of inventing a parallel effect system.

## Shapes

Corners follow the effective Hyprland `decoration:rounding` value, read through `hyprctl` and refreshed every second. Without Hyprland the fallback is sharp. `theme.radius` is passed to `Material.roundedScale` on the window, buttons, and combo boxes; custom queue/error rectangles use it as their QML radius. These are different Qt properties, so do not claim that Material's rendered corner radius is numerically identical to the rectangle radius.

Icons are consistent monochrome vector assets, with 20×20 logical-pixel artwork in 40×40 icon controls. No decorative imagery is part of the controller; the optional standby image belongs to the output.

## Components

### Buttons

`BeamButton` is the native Material button with inherited metrics and live corner binding. Normal labels use foreground; highlighted labels use readable accent ink. Disabled label opacity is 0.38. Its explicit text item stays centered, renders plain text, and elides on the right. Start projection is highlighted while output is off; End projection uses the normal treatment.

`IconButton` is a native Material tool button with an accessible name and a tooltip delayed 500ms. Enabled icons use foreground; highlighted transport uses the live accent; disabled icons use the disabled-icon primitive. Standard textual tool buttons retain native Material styling.

### Output and standby controls

The display combo box expands beside the projection action. Inline standby shows Black screen or the selected image basename, with a full-path tooltip. Choose image opens selection; Use black appears only when an image exists. The status copy distinguishes projection off, connecting, standby, playing, and paused.

### Queue

The panel is a clipped, vertically scrollable list. Each row keeps play, title/path, move up/down, and remove together. The active item has a subtle accent tint and stronger title; hover uses the neutral hover primitive. Empty queue copy names both drag/drop and Add media entry points. Queue tools disable when their action cannot apply.

### Native fields and transport

Display and speed use Material combo boxes. Timeline and volume use Material sliders; timeline disables until active media has a duration. Continue automatically and Repeat playlist use a native switch and checkbox. Keep standard Qt focus, hover, keyboard, and disabled affordances. The help popup lists the application's existing keyboard shortcuts and closes through Close, Escape, or an outside press.

### Standby output

The separate mpv output remains open on Stop and noncontinuous EOF, returning to black or the selected image. Continuous playback advances through the queue; repeat is a distinct setting. Do not add a competing controller preview or expose controller controls on the projection surface.

## Do's and Don'ts

### Do:

- **Do** inherit Omarchy's live accent and Hyprland corner behavior.
- **Do** preserve explicit readable ink for highlighted button labels.
- **Do** keep output and standby above the flexible queue and transport below it.
- **Do** retain accessible control names, native keyboard affordances, text elision, and translation-aware layout.
- **Do** verify populated, empty, compact, translated, and fallback-accent states when changing layout or theme behavior.

### Don't:

- **Don't** treat fallback accent or sharp corners as fixed runtime values.
- **Don't** introduce browser tokens, web breakpoints, CSS component replicas, or invented font metrics into this native Qt system.
- **Don't** add decorative depth or a competing video preview to the restrained controller.
- **Don't** collapse stopped, paused, and standby into one visual state.
