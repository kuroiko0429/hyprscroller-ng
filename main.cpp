#include <unistd.h>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/desktop/DesktopTypes.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/values/ConfigValues.hpp>
#include <hyprland/src/helpers/Color.hpp>

#include "globals.hpp"
#include "Scrolling.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

//

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string HASH        = __hyprland_api_get_hash();
    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();

    if (HASH != CLIENT_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Failure in initialization: Version mismatch (headers ver is not equal to running hyprland ver)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hs] Version mismatch");
    }

    // Register config values (V2 API)
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:hyprscrolling:fullscreen_on_one_column", "fullscreen single column", 0));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Float>("plugin:hyprscrolling:column_width", "default column width", 0.5F));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:hyprscrolling:focus_fit_method", "focus fit method", 0));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:hyprscrolling:follow_focus", "follow focus", 1));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:hyprscrolling:follow_debounce_ms", "follow debounce ms", 0));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::String>("plugin:hyprscrolling:explicit_column_widths", "explicit column widths", std::string{"0.333, 0.5, 0.667, 1.0"}));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:hyprscrolling:collapsed_width", "collapsed column width in px", 30));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::Int>("plugin:hyprscrolling:focus_history", "enable focus history", 1));
    HyprlandAPI::addConfigValueV2(PHANDLE, makeShared<Config::Values::String>("plugin:hyprscrolling:auto_width_rules", "auto width rules per class", std::string{""}));

    // Register the tiled algorithm using factory pattern
    bool success = HyprlandAPI::addTiledAlgo(PHANDLE, "hyprscrolling", &typeid(CScrollingLayout), []() -> UP<Layout::ITiledAlgorithm> {
        return makeUnique<CScrollingLayout>();
    });

    if (!success) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Failure in initialization: failed to register tiled algorithm (name collision with built-in?)",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hs] Algorithm registration failed");
    }

    HyprlandAPI::addNotification(PHANDLE, "[hyprscrolling] Initialized successfully!", CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    return {"hyprscrolling", "A plugin to add a scrolling layout to hyprland", "Vaxry", "2.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::removeAlgo(PHANDLE, "hyprscrolling");
}
