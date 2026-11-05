#include "plugin.hpp"

#include <stdexcept>

extern "C" {
#include <lua.h>
}

static int luaToggle(lua_State* lua) {
    const auto result = toggleAutogroup("");
    lua_pushboolean(lua, result.success);
    if (!result.success) {
        lua_pushstring(lua, result.error.c_str());
        return 2;
    }
    return 1;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    if (std::string{__hyprland_api_get_hash()} != __hyprland_api_get_client_hash()) {
        HyprlandAPI::addNotification(handle, "[dwindle-autogroup] Hyprland version mismatch", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("dwindle-autogroup: Hyprland version mismatch");
    }

    if (!HyprlandAPI::addLuaFunction(handle, "dwindle_autogroup", "toggle", luaToggle))
        throw std::runtime_error("dwindle-autogroup: could not register Lua function (Lua configuration required)");

    return {"dwindle-autogroup", "Group and ungroup dwindle subtrees", "ItsDrike", "2.0"};
}
