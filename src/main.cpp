#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <src/layout/DwindleLayout.hpp>
#include <src/managers/KeybindManager.hpp>
#include <src/managers/LayoutManager.hpp>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

inline std::function<void(std::string)> originalToggleGroup = nullptr;


void toggleGroup(std::string args)
{
    // Run the original function, creating the group
    originalToggleGroup(args);

    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Group toggeld!", s_pluginColor, 5000);
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    originalToggleGroup = g_pKeybindManager->m_mDispatchers["togglegroup"];
    HyprlandAPI::addDispatcher(PHANDLE, "togglegroup", toggleGroup);

    HyprlandAPI::reloadConfig();

    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Initialized successfully!", s_pluginColor, 5000);

    return {"dwindle-autogroup", "Dwindle Autogroup", "ItsDrike", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT()
{
    // Since we added the "togglegroup" dispatcher ourselves, by default, the cleanup would just remove it
    // but we want to restore it back to the original function instead
    HyprlandAPI::removeDispatcher(PHANDLE, "togglegroup");
    g_pKeybindManager->m_mDispatchers["togglegroup"] = originalToggleGroup;

    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Unloaded successfully!", s_pluginColor, 5000);
}
