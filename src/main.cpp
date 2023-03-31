#define WLR_USE_UNSTABLE

#include "globals.hpp"

#include <src/Window.hpp>
#include <src/layout/DwindleLayout.hpp>
#include <src/managers/KeybindManager.hpp>
#include <src/managers/LayoutManager.hpp>

const CColor s_pluginColor = {0x61 / 255.0f, 0xAF / 255.0f, 0xEF / 255.0f, 1.0f};

inline std::function<void(std::string)> originalToggleGroup = nullptr;

typedef SDwindleNodeData* (*nodeFromWindowT)(void*, CWindow*);
inline nodeFromWindowT g_pNodeFromWindow = nullptr;

void addChildNodesToDequeRecursive(std::deque<SDwindleNodeData*>* pDeque, std::deque<SDwindleNodeData*>* pParents, SDwindleNodeData* node)
{
    if (node->isNode) {
        pParents->push_back(node);
        addChildNodesToDequeRecursive(pDeque, pParents, node->children[0]);
        addChildNodesToDequeRecursive(pDeque, pParents, node->children[1]);
    }
    else {
        pDeque->emplace_back(node);
    }
}

void toggleGroup(std::string args)
{
    // Run the original function, creating the group
    originalToggleGroup(args);

    // We only care about group creations, not group disolves
    const auto PWINDOW = g_pCompositor->m_pLastWindow;

    if (!PWINDOW || !g_pCompositor->windowValidMapped(PWINDOW))
        return;

    if (!PWINDOW->m_sGroupData.pNextWindow) {
        Debug::log(LOG, "Ignoring autogroup for group disolve");
        return;
    }

    // Don't do anything if we're not on "dwindle" layout
    IHyprLayout* layout = g_pLayoutManager->getCurrentLayout();
    if (layout->getLayoutName() != "dwindle") {
        return;
    }
    CHyprDwindleLayout* cur_dwindle = (CHyprDwindleLayout*)layout;

    const auto PNODE = g_pNodeFromWindow(cur_dwindle, PWINDOW);
    if (!PNODE) {
        Debug::log(LOG, "Ignoring autogroup for floating window");
        return;
    }

    const auto PWORKSPACE = g_pCompositor->getWorkspaceByID(PNODE->workspaceID);
    if (PWORKSPACE->m_bHasFullscreenWindow) {
        Debug::log(ERR, "Ignoring autogroupgroup on a fullscreen window");
        return;
    }

    if (!PNODE->pParent) {
        Debug::log(LOG, "Ignoring autogroup for a solitary window");
    }

    std::deque<SDwindleNodeData*> newGroupMembers;
    std::deque<SDwindleNodeData*> parentNodes;

    addChildNodesToDequeRecursive(
        &newGroupMembers, &parentNodes, PNODE->pParent->children[0] == PNODE ? PNODE->pParent->children[1] : PNODE->pParent->children[0]);

    for (auto& n : newGroupMembers) {
        if (n->pWindow->m_sGroupData.pNextWindow) {
            Debug::log(LOG, "Ignoring autogroup for nested groups");
        }
    }

    // Add all of the children nodes into the group
    for (auto& n : newGroupMembers) {
        auto window = n->pWindow;
        layout->onWindowRemoved(window);
        PWINDOW->insertWindowToGroup(window);
        window->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(window));
        PWINDOW->setGroupCurrent(PWINDOW);
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
