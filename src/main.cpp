#include "globals.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/Window.hpp>
#include <hyprland/src/layout/DwindleLayout.hpp>
#include <hyprland/src/layout/IHyprLayout.hpp>
#include <hyprland/src/managers/KeybindManager.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>

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

/// This is partially from CKeybindManager::moveIntoGroup (dispatcher) function
/// but without making the new window focused.
void moveIntoGroup(CWindow* window, CWindow* groupRootWin)
{
    // Remove this window from being shown by layout (it will become a part of a group)
    g_pLayoutManager->getCurrentLayout()->onWindowRemoved(window);

    // Create a group bar decoration for the window
    // (if it's not already a group, in which case it should already have it)
    if (!window->m_sGroupData.pNextWindow)
        window->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(window));

    groupRootWin->insertWindowToGroup(window);

    // Make sure to treat this window as hidden (will focus the group instead of this window
    // on request activate). This is the behavior CWindow::setGroupCurrent uses.
    window->setHidden(true);
}

void collectGroupWindows(std::deque<CWindow*>* pDeque, CWindow* pWindow)
{
    CWindow* curr = pWindow;
    do {
        const auto PLAST = curr;
        pDeque->emplace_back(curr);
        curr = curr->m_sGroupData.pNextWindow;
    } while (curr != pWindow);
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

    // We could just call originalToggleGroup function here, but that fucntion doesn't
    // respect the dwindle layout, and would just place the newly ungroupped windows
    // randomly throughout the worskapce, messing up the layout. So instead, we replicate
    // it's behavior here manually, taking care to disolve the groups nicely.
    Debug::log(LOG, "Dissolving group");

    std::deque<CWindow*> members;
    collectGroupWindows(&members, PWINDOW);

    // If the group head window is in fullscreen, unfullscreen it.
    // We need to have the window placed in the layout, to figure out where
    // to ungroup the rest of the windows.
    g_pCompositor->setWindowFullscreen(PWINDOW, false, FULLSCREEN_FULL);

    Debug::log(LOG, "Ungroupping Members");

    const bool GROUPSLOCKEDPREV = g_pKeybindManager->m_bGroupsLocked;

    for (auto& w : members) {
        w->m_sGroupData.pNextWindow = nullptr;
        w->setHidden(false);

        // Ask layout to create a new window for all windows that were in the group
        // except for the group head (already has a window).
        if (w->m_sGroupData.head) {
            Debug::log(LOG, "Ungroupping member head window");
            w->m_sGroupData.head = false;

            // Update the window decorations (removing group bar)
            // w->updateWindowDecos();
        }
        else {
            Debug::log(LOG, "Ungroupping member non-head window");
            g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(w);
            // Focus the window that we just spawned, so that on the next iteration
            // the window created will be it's dwindle child node.
            // This allows the original group head to remain a parent window to all
            // of the other (groupped) nodes
            g_pCompositor->focusWindow(w);
        }
    }

    g_pKeybindManager->m_bGroupsLocked = GROUPSLOCKEDPREV;

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // Leave with the focus the original group (head) window
    g_pCompositor->focusWindow(PWINDOW);
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
        moveIntoGroup(window, PWINDOW);
    }
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
