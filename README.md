# hyprscrolling

A scrolling (column-based) layout plugin for [Hyprland](https://hyprland.org/).

Inspired by [hyprscroller](https://github.com/dawsers/hyprscroller) (MIT) — rebuilt from scratch because upstream wasn't being updated, with extra features added on top. The Hyprland API and initial implementation were developed with AI assistance.

> **Work in progress.** Core functionality is stable and daily-driven.

---

## Features

- Column-based scrolling layout (like PaperWM / Scroller)
- Column pinning to left/right screen edge
- Column collapse/expand toggle
- Zen mode — show only the focused column
- Focus history (back/forward navigation)
- Per-window-class automatic column width rules
- Column-level workspace movement
- Preset column width cycling
- All columns fit-to-screen operations

---

## Installation

### hyprpm (recommended)

```bash
hyprpm add https://github.com/kuroiko0429/hyprscroller-ng
hyprpm enable hyprscrolling
```

Then add to your `hyprland.conf`:

```
general {
    layout = hyprscrolling
}
```

### Build from source

**Requirements:** `hyprland`, `libdrm`, `libinput`, `libudev`, `pangocairo`, `pixman-1`, `wayland-server`, `xkbcommon`

```bash
git clone https://github.com/kuroiko0429/hyprscroller-ng
cd hyprscroller-ng
make
```

This produces `hyprscrolling.so` in the project directory.

Add to your `hyprland.conf`:

```
plugin = /path/to/hyprscrolling.so
general {
    layout = hyprscrolling
}
```

---

## Configuration

All options go inside `plugin { hyprscrolling { ... } }` in your Hyprland config.

| Option | Type | Default | Description |
|---|---|---|---|
| `column_width` | float (0–1) | `0.5` | Default column width as a fraction of monitor width |
| `fullscreen_on_one_column` | bool | `false` | If there's only one column, make it fullscreen |
| `focus_fit_method` | int (0 or 1) | `0` | When focusing a column: `0` = center it, `1` = fit it into view |
| `follow_focus` | bool | `true` | Automatically scroll to keep the focused column visible |
| `follow_debounce_ms` | int | `0` | Debounce time (ms) for follow_focus events |
| `explicit_column_widths` | string | `0.333, 0.5, 0.667, 1.0` | Comma-separated preset widths for `+conf` / `-conf` cycling |
| `collapsed_width` | int (px) | `30` | Width of a collapsed column in pixels |
| `focus_history` | bool | `true` | Enable focus history for `focusback` / `focusfwd` |
| `auto_width_rules` | string | `` | Per-class automatic width: `firefox:0.7, kitty:0.3` |

### Example

```
plugin {
    hyprscrolling {
        column_width = 0.5
        fullscreen_on_one_column = false
        focus_fit_method = 0
        follow_focus = true
        follow_debounce_ms = 0
        explicit_column_widths = 0.333, 0.5, 0.667, 1.0
        collapsed_width = 30
        focus_history = true
        auto_width_rules = firefox:0.7, kitty:0.3, code-oss:0.6
    }
}
```

---

## Layout Messages

Use `layoutmsg` dispatcher to send commands:

```
bind = SUPER, key, layoutmsg, <message>
```

### Focus

| Message | Description |
|---|---|
| `focus l/r/u/d` | Move focus in direction, wraps instead of jumping to adjacent monitor |
| `focusback` | Go back in focus history |
| `focusfwd` | Go forward in focus history |

### Window Movement

| Message | Description |
|---|---|
| `movewindowto l/r/u/d` | Move window to adjacent column/stack. Moving right at the last column promotes the window to a new column |
| `promote` | Promote window to its own new column |

### Column Operations

| Message | Params | Description |
|---|---|---|
| `swapcol l/r` | `l` or `r` | Swap current column with its left/right neighbor. Wraps around |
| `colresize` | `0.5`, `+0.2`, `-0.2`, `+conf`, `-conf`, `all 0.5` | Resize current column (or all columns) |
| `movecoltoworkspace` | `1`, `+1`, `-1`, `special`, etc. | Move entire current column to a workspace |
| `togglecollapse` | — | Fold / expand the current column |

### Scrolling

| Message | Params | Description |
|---|---|---|
| `move` | `+col`, `-col`, `+200`, `-200` | Scroll layout horizontally by columns or pixels |

### Fit Operations

| Message | Params | Description |
|---|---|---|
| `fit` | `active`, `visible`, `all`, `toend`, `tobeg` | Resize/arrange columns to fit the screen |
| `togglefit` | — | Toggle `focus_fit_method` between center (0) and fit (1) |

### View Modes

| Message | Description |
|---|---|
| `zen` | Show only the focused column (focus mode). Toggle to exit |
| `pin left` | Pin current column to the left screen edge (stays fixed while scrolling) |
| `pin right` | Pin current column to the right screen edge |
| `unpin` | Remove pin from current column |

---

## Example Keybindings

Copy from [`scrolling.conf`](./scrolling.conf) or use this as a starting point:

```
# Focus movement
bind = SUPER, H, layoutmsg, focus l
bind = SUPER, L, layoutmsg, focus r
bind = SUPER, K, layoutmsg, focus u
bind = SUPER, J, layoutmsg, focus d

# Window movement
bind = SUPER SHIFT, H, layoutmsg, movewindowto l
bind = SUPER SHIFT, L, layoutmsg, movewindowto r
bind = SUPER SHIFT, K, layoutmsg, movewindowto u
bind = SUPER SHIFT, J, layoutmsg, movewindowto d
bind = SUPER, P, layoutmsg, promote

# Column swap
bind = SUPER ALT, H, layoutmsg, swapcol l
bind = SUPER ALT, L, layoutmsg, swapcol r

# Column resize
bind = SUPER, equal, layoutmsg, colresize +0.05
bind = SUPER, minus, layoutmsg, colresize -0.05
bind = SUPER, bracketright, layoutmsg, colresize +conf
bind = SUPER, bracketleft, layoutmsg, colresize -conf
bind = SUPER SHIFT, equal, layoutmsg, colresize all 0.5

# Scrolling
bind = SUPER, period, layoutmsg, move +col
bind = SUPER, comma, layoutmsg, move -col
bind = SUPER SHIFT, period, layoutmsg, move +200
bind = SUPER SHIFT, comma, layoutmsg, move -200

# Fit
bind = SUPER, F, layoutmsg, fit active
bind = SUPER SHIFT, F, layoutmsg, fit visible
bind = SUPER CTRL, F, layoutmsg, fit all
bind = SUPER, T, layoutmsg, togglefit

# Pin
bind = SUPER CTRL, bracketleft, layoutmsg, pin left
bind = SUPER CTRL, bracketright, layoutmsg, pin right
bind = SUPER CTRL, backslash, layoutmsg, unpin

# Collapse
bind = SUPER, C, layoutmsg, togglecollapse

# Zen mode
bind = SUPER, Z, layoutmsg, zen

# Focus history
bind = SUPER ALT, bracketleft, layoutmsg, focusback
bind = SUPER ALT, bracketright, layoutmsg, focusfwd

# Move column to workspace
bind = SUPER CTRL SHIFT, 1, layoutmsg, movecoltoworkspace 1
bind = SUPER CTRL SHIFT, 2, layoutmsg, movecoltoworkspace 2
bind = SUPER CTRL SHIFT, 3, layoutmsg, movecoltoworkspace 3
bind = SUPER CTRL SHIFT, right, layoutmsg, movecoltoworkspace +1
bind = SUPER CTRL SHIFT, left, layoutmsg, movecoltoworkspace -1
bind = SUPER CTRL SHIFT, S, layoutmsg, movecoltoworkspace special
```

---

## Build Systems

Three build systems are included — pick whichever fits your setup:

| File | Command |
|---|---|
| `Makefile` | `make` |
| `CMakeLists.txt` | `cmake -B build && cmake --build build` |
| `meson.build` | `meson setup build && ninja -C build` |

---

## License

MIT
