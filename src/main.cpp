#define WLR_USE_UNSTABLE

#include "globals.hpp"

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Initialized successfully!", s_pluginColor, 5000);

    HyprlandAPI::reloadConfig();

    return {"dwindle-autogroup", "Dwindle Autogroup", "ItsDrike", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT()
{
    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Unloaded successfully!", s_pluginColor, 5000);
}
