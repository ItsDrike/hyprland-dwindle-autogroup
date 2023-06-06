#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <hyprland/src/Window.hpp>
#include <hyprland/src/layout/DwindleLayout.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

inline std::function<void(std::string)> originalToggleGroup = nullptr;

typedef SDwindleNodeData* (*nodeFromWindowT)(void*, CWindow*);
inline nodeFromWindowT g_pNodeFromWindow = nullptr;

void collectChildNodes(std::deque<SDwindleNodeData*>* pDeque, SDwindleNodeData* node)
{
    if (node->isNode) {
        collectChildNodes(pDeque, node->children[0]);
        collectChildNodes(pDeque, node->children[1]);
    }
    else {
        pDeque->emplace_back(node);
    }
}

void groupDissolve(const SDwindleNodeData* PNODE, CHyprDwindleLayout* layout)
{
    CWindow* PWINDOW = PNODE->pWindow;

    // This group only has this single winodw
    if (PWINDOW->m_sGroupData.pNextWindow == PWINDOW) {
        Debug::log(LOG, "Ignoring autogroup on single window dissolve");
        originalToggleGroup("");
        return;
    }

    Debug::log(LOG, "Dissolving group");
    // We currently don't need any special logic for dissolving, just call the original func
    originalToggleGroup("");
}

void groupCreate(const SDwindleNodeData* PNODE, CHyprDwindleLayout* layout)
{
    const auto PWINDOW = PNODE->pWindow;

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);
    if (PWORKSPACE->m_bHasFullscreenWindow) {
        Debug::log(LOG, "Ignoring autogroup on a fullscreen window");
        originalToggleGroup("");
        return;
    }

    if (!PNODE->pParent) {
        Debug::log(LOG, "Ignoring autogroup for a solitary window");
        originalToggleGroup("");
        return;
    }

    Debug::log(LOG, "Creating group");
    // Call the original toggleGroup function, and only do extra things afterwards
    originalToggleGroup("");

    std::deque<SDwindleNodeData*> newGroupMembers;

    collectChildNodes(&newGroupMembers, PNODE->pParent->children[0] == PNODE ? PNODE->pParent->children[1] : PNODE->pParent->children[0]);

    // Make sure one of the child nodes isn't itself a group
    for (auto& n : newGroupMembers) {
        if (n->pWindow->m_sGroupData.pNextWindow) {
            Debug::log(LOG, "Ignoring autogroup for nested groups");
            return;
        }
    }

    // Add all of the children nodes into the group
    for (auto& n : newGroupMembers) {
        auto window = n->pWindow;
        layout->onWindowRemoved(window);
        PWINDOW->insertWindowToGroup(window);
        window->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(window));
    }

    // Moving new windows into group makes them the active window in that group,
    // refocus the original window
    PWINDOW->setGroupCurrent(PWINDOW);
}

void toggleGroup(std::string args)
{
    // We only care about group creations, not group disolves
    const auto PWINDOW = g_pCompositor->m_pLastWindow;
    if (!PWINDOW || !g_pCompositor->windowValidMapped(PWINDOW))
        return;

    // Don't do anything if we're not on "dwindle" layout
    IHyprLayout* layout = g_pLayoutManager->getCurrentLayout();
    if (layout->getLayoutName() != "dwindle") {
        Debug::log(LOG, "Ignoring autogroup for non-dinwle layout");
        originalToggleGroup(args);
        return;
    }

    CHyprDwindleLayout* cur_dwindle = (CHyprDwindleLayout*)layout;

    const auto PNODE = g_pNodeFromWindow(cur_dwindle, PWINDOW);
    if (!PNODE) {
        Debug::log(LOG, "Ignoring autogroup for floating window");
        originalToggleGroup(args);
        return;
    }

    if (PWINDOW->m_sGroupData.pNextWindow) {
        groupDissolve(PNODE, cur_dwindle);
    }
    else {
        groupCreate(PNODE, cur_dwindle);
    }
}

// Do NOT change this function.
APICALL EXPORT std::string PLUGIN_API_VERSION()
{
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle)
{
    PHANDLE = handle;

    // Get address of the private CHyprDwindleLayout::getNodeFromWindow member function, we'll need it in toggleGroup
    static const auto METHODS = HyprlandAPI::findFunctionsByName(PHANDLE, "getNodeFromWindow");
    g_pNodeFromWindow = (nodeFromWindowT)METHODS[1].address;

    for (auto& method : METHODS) {
        if (method.demangled == "CHyprDwindleLayout::getNodeFromWindow(CWindow*)") {
            g_pNodeFromWindow = (nodeFromWindowT)method.address;
            break;
        }
    }

    if (g_pNodeFromWindow == nullptr) {
        Debug::log(ERR, "getNodeFromWindow funnction for dwindle layout wasn't found! This function's signature probably changed, report this");
        HyprlandAPI::addNotification(
            PHANDLE, "[dwindle-autogroup] Initialization failed!! getNodeFromWindow functio not found", s_pluginColor, 10000);
    }
    else {
        originalToggleGroup = g_pKeybindManager->m_mDispatchers["togglegroup"];
        HyprlandAPI::addDispatcher(PHANDLE, "togglegroup", toggleGroup);

        HyprlandAPI::reloadConfig();

        HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Initialized successfully!", s_pluginColor, 5000);
    }

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
