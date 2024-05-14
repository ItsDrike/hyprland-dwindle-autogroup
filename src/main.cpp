#include "func_finder.hpp"
#include "globals.hpp"
#include "plugin.hpp"

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT void PLUGIN_EXIT()
{
    // Unhook the overridden functions and remove the hooks
    if (g_pCreateGroupHook) {
        g_pCreateGroupHook->unhook();
        HyprlandAPI::removeFunctionHook(PHANDLE, g_pCreateGroupHook);
        g_pCreateGroupHook = nullptr;
    }
    if (g_pDestroyGroupHook) {
        g_pDestroyGroupHook->unhook();
        HyprlandAPI::removeFunctionHook(PHANDLE, g_pDestroyGroupHook);
        g_pCreateGroupHook = nullptr;
    }

    // Plugin unloading was successful
    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Unloaded successfully!", s_notifyColor, 5000);
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    Debug::log(LOG, "[dwindle-autogroup] Loading Hyprland functions");

    // Find pointers to functions by name (from the Hyprland binary)
    g_pNodeFromWindow =
        (nodeFromWindowFuncT)findHyprlandFunction("getNodeFromWindow", "CHyprDwindleLayout::getNodeFromWindow(std::shared_ptr<CWindow>)");
    auto pCreateGroup = findHyprlandFunction("createGroup", "CWindow::createGroup()");
    auto pDestroyGroup = findHyprlandFunction("destroyGroup", "CWindow::destroyGroup()");

    // Return immediately if one of the functions wasn't found
    if (!g_pNodeFromWindow || !pCreateGroup || !pDestroyGroup) {
        // Set all of the global function pointers to NULL, to avoid any potential issues
        g_pNodeFromWindow = nullptr;

        return s_pluginDescription;
    }

    Debug::log(LOG, "[dwindle-autogroup] Registering function hooks");

    // Register function hooks, for overriding the original group methods
    g_pCreateGroupHook = HyprlandAPI::createFunctionHook(PHANDLE, pCreateGroup, (void*)&newCreateGroup);
    g_pDestroyGroupHook = HyprlandAPI::createFunctionHook(PHANDLE, pDestroyGroup, (void*)&newDestroyGroup);

    // Initialize the hooks, from now on, the original functions will be overridden
    g_pCreateGroupHook->hook();
    g_pDestroyGroupHook->hook();

    HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Initialized successfully!", s_notifyColor, 5000);
    return s_pluginDescription;
}
