# Example Configuration for Hyprland v0.55+ (Lua Config)

This guide explains how to load, configure, and bind keys for the `hyprscrolling` (`hyprscroller-ng`) plugin in Hyprland's new Lua configuration environment (`v0.55.0` or later).

---

## 📂 Directory Structure

We recommend splitting your configuration into modules under `~/.config/hypr/config/`:

```text
~/.config/hypr/
├── hyprland.lua
└── config/
    ├── autostart.lua      # Plugin auto-loading
    ├── general.lua        # Default layout & plugin settings
    └── binds.lua          # Layout keybindings
```

---

## 🚀 1. Load the Plugin on Startup

To load the plugin automatically, call `hyprctl plugin load` inside your autostart script. 
We recommend adding a **1-second delay** (`sleep 1`) to ensure the Hyprland IPC socket is fully ready.

In your **`~/.config/hypr/config/autostart.lua`**:

```lua
hl.on("hyprland.start", function()
    -- [Other startup commands...]

    -- Load hyprscrolling plugin with delay to avoid connection issues on startup
    hl.exec_cmd("sleep 1 && hyprctl plugin load /absolute/path/to/hyprscrolling.so")
end)
```

> [!NOTE]
> Make sure to replace `/absolute/path/to/hyprscrolling.so` with the actual path to your compiled plugin.

---

## ⚙️ 2. Configure Layout & Parameters

Set the default layout to `"hyprscrolling"` and specify the parameters in the `plugin.hyprscrolling` table.

In your **`~/.config/hypr/config/general.lua`**:

```lua
hl.config({
    general = {
        -- Set default tiling layout to hyprscrolling
        layout = "hyprscrolling",
        
        -- [Other general settings...]
    },

    -- Plugin parameters
    plugin = {
        hyprscrolling = {
            -- If there is only one column, make it fullscreen
            fullscreen_on_one_column = false,

            -- Default column width as a fraction of monitor width (0.1 ~ 1.0)
            column_width = 0.5,

            -- How to bring a focused column into view (0 = center, 1 = fit into view)
            focus_fit_method = 0,

            -- Automatically scroll to keep the focused column visible
            follow_focus = true,

            -- Debounce time (ms) for follow_focus events
            follow_debounce_ms = 0,

            -- Preset widths to cycle through with +conf / -conf layout messages
            explicit_column_widths = "0.333, 0.5, 0.667, 1.0",

            -- Width of a collapsed column in pixels
            collapsed_width = 30,

            -- Enable/disable focus history (true = enabled)
            focus_history = true,

            -- Automatic column width per window class (e.g. "firefox:0.7,kitty:0.3")
            auto_width_rules = "",
        },
    },
})
```

---

## ⌨️ 3. Keybindings

Use `hl.dsp.layout("COMMAND")` to trigger layout messages.

In your **`~/.config/hypr/config/binds.lua`**:

```lua
local mainMod = "SUPER"

-- =====================================================================
-- Focus Movement
-- =====================================================================
hl.bind(mainMod .. " + H", hl.dsp.layout("focus l"))
hl.bind(mainMod .. " + L", hl.dsp.layout("focus r"))
hl.bind(mainMod .. " + K", hl.dsp.layout("focus u"))
hl.bind(mainMod .. " + J", hl.dsp.layout("focus d"))

-- Cycle through windows with mouse wheel
hl.bind(mainMod .. " + CTRL + mouse_down", hl.dsp.layout("cyclenext"))
hl.bind(mainMod .. " + CTRL + mouse_up", hl.dsp.layout("cyclenext prev"))

-- =====================================================================
-- Window Movement
-- =====================================================================
-- Move window into adjacent column/stack
hl.bind(mainMod .. " + SHIFT + H", hl.dsp.layout("movewindowto l"))
hl.bind(mainMod .. " + SHIFT + L", hl.dsp.layout("movewindowto r"))
hl.bind(mainMod .. " + SHIFT + K", hl.dsp.layout("movewindowto u"))
hl.bind(mainMod .. " + SHIFT + J", hl.dsp.layout("movewindowto d"))

-- Promote window to its own new column
hl.bind(mainMod .. " + P", hl.dsp.layout("promote"))

-- =====================================================================
-- Column Operations
-- =====================================================================
-- Swap column with its neighbor
hl.bind(mainMod .. " + ALT + H", hl.dsp.layout("swapcol l"))
hl.bind(mainMod .. " + ALT + L", hl.dsp.layout("swapcol r"))

-- Resize column width manually
hl.bind(mainMod .. " + equal", hl.dsp.layout("colresize +0.05"))
hl.bind(mainMod .. " + minus", hl.dsp.layout("colresize -0.05"))

-- Cycle preset widths (e.g. 33%, 50%, 67%, 100%)
hl.bind(mainMod .. " + bracketright", hl.dsp.layout("colresize +conf"))
hl.bind(mainMod .. " + bracketleft", hl.dsp.layout("colresize -conf"))

-- Reset all columns to equal width
hl.bind(mainMod .. " + SHIFT + equal", hl.dsp.layout("colresize all 0.5"))

-- =====================================================================
-- View Scrolling (without changing focus)
-- =====================================================================
-- Scroll by column
hl.bind(mainMod .. " + period", hl.dsp.layout("move +col"))
hl.bind(mainMod .. " + comma", hl.dsp.layout("move -col"))

-- Scroll by pixels
hl.bind(mainMod .. " + SHIFT + period", hl.dsp.layout("move +200"))
hl.bind(mainMod .. " + SHIFT + comma", hl.dsp.layout("move -200"))

-- =====================================================================
-- Fit Operations
-- =====================================================================
-- Fit active/visible columns to screen
hl.bind(mainMod .. " + F", hl.dsp.layout("fit active"))
hl.bind(mainMod .. " + SHIFT + F", hl.dsp.layout("fit visible"))
hl.bind(mainMod .. " + CTRL + F", hl.dsp.layout("fit all"))
hl.bind(mainMod .. " + T", hl.dsp.layout("togglefit"))

-- =====================================================================
-- Column Pinning
-- =====================================================================
-- Pin column to left/right edge
hl.bind(mainMod .. " + CTRL + bracketleft", hl.dsp.layout("pin left"))
hl.bind(mainMod .. " + CTRL + bracketright", hl.dsp.layout("pin right"))
hl.bind(mainMod .. " + CTRL + backslash", hl.dsp.layout("unpin"))

-- =====================================================================
-- Column Workspace Movement
-- =====================================================================
-- Move entire column to workspace 1-5
for i = 1, 5 do
    hl.bind(mainMod .. " + CTRL + SHIFT + " .. i, hl.dsp.layout("movecoltoworkspace " .. i))
end

-- Move column to relative workspace or special scratchpad
hl.bind(mainMod .. " + CTRL + SHIFT + right", hl.dsp.layout("movecoltoworkspace +1"))
hl.bind(mainMod .. " + CTRL + SHIFT + left", hl.dsp.layout("movecoltoworkspace -1"))
hl.bind(mainMod .. " + CTRL + SHIFT + S", hl.dsp.layout("movecoltoworkspace special"))

-- =====================================================================
-- Column Collapse & Focus History
-- =====================================================================
-- Toggle fold / expand current column (minimized to collapsed_width)
hl.bind(mainMod .. " + C", hl.dsp.layout("togglecollapse"))

-- Zen mode (focus current column fullscreen)
hl.bind(mainMod .. " + Z", hl.dsp.layout("zen"))

-- Back/Forward in focus history (similar to alt-tab)
hl.bind(mainMod .. " + ALT + bracketleft", hl.dsp.layout("focusback"))
hl.bind(mainMod .. " + ALT + bracketright", hl.dsp.layout("focusfwd"))
