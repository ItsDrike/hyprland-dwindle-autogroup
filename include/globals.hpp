#pragma once

#include <hyprland/src/desktop/DesktopTypes.hpp> // needed for DwindleLayout.hpp
#include <hyprland/src/layout/DwindleLayout.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

const CColor s_notifyColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f}; // RGBA
const PLUGIN_DESCRIPTION_INFO s_pluginDescription = {"dwindle-autogroup", "Dwindle Autogroup", "ItsDrike", "1.0"};

inline HANDLE PHANDLE = nullptr;

typedef void* (*createGroupFuncT)(CWindow*);
inline CFunctionHook* g_pCreateGroupHook = nullptr;

typedef void* (*destroyGroupFuncT)(CWindow*);
inline CFunctionHook* g_pDestroyGroupHook = nullptr;

typedef SDwindleNodeData* (*nodeFromWindowFuncT)(void*, CWindow*);
inline nodeFromWindowFuncT g_pNodeFromWindow = nullptr;
