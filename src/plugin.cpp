#include "plugin.hpp"
#include "globals.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>

/*! Recursively collect all dwindle child nodes for given root node
 *
 * This function is similar to Hyprland's private addToDequeRecursive function.
 *
 * \param[out] pDeque   deque to store the found child nodes into
 * \param[in] pNode     Dwindle node which children should be collected
 */
void collectDwindleChildNodes(std::deque<SDwindleNodeData*>* pDeque, SDwindleNodeData* pNode)
{
    if (pNode->isNode) {
        collectDwindleChildNodes(pDeque, pNode->children[0]);
        collectDwindleChildNodes(pDeque, pNode->children[1]);
    }
    else {
        pDeque->emplace_back(pNode);
    }
}

/*! Collect all windows that belong to the same group
 *
 * This function is similar to a part of the logic present in
 * Hyprland's CWindow::destroyGroup function.
 *
 * \param[out] pDeque   deque to store the found group windows into
 * \param[in] pWindow   any window that belongs to a group (doesn't have to be the group head window)
 */
void collectGroupWindows(std::vector<PHLWINDOW>* pMembersVec, PHLWINDOW pWindow)
{
    PHLWINDOW curr = pWindow;
    do {
        curr = curr->m_sGroupData.pNextWindow.lock();
        pMembersVec->push_back(curr);
    } while (curr != pWindow);
}

/*! Move given window into a group
 *
 * This is almost the same as CKeybindManager::moveWindowIntoGroup (dispatcher) function,
 * but without making the new window a group head and focused.
 *
 * \param[in] pWindow       Window to be inserted into a group
 * \param[in] pGroupWindow  Window that's a part of a group to insert the pWindow into
 */
void moveIntoGroup(PHLWINDOW pWindow, PHLWINDOW pGroupHeadWindow)
{
    Debug::log(LOG, "[dwindle-autogroup] Moving window {:x} into group {:x}", pWindow, pGroupHeadWindow);

    if (pWindow->m_sGroupData.deny)
        return;

    const auto P_LAYOUT = g_pLayoutManager->getCurrentLayout();

    // remove the window from layout (will become a part of a group)
    P_LAYOUT->onWindowRemoved(pWindow);

    // Insert the new window into the group.
    // Depending on the config, the new window will either be inserted after
    // the current/head window in the group, or at the end of the group.
    const auto* USE_CURR_POS = (Hyprlang::INT* const*)g_pConfigManager->getConfigValuePtr("group:insert_after_current");
    PHLWINDOW pGroupInsertionWindow = *USE_CURR_POS ? pGroupHeadWindow : pGroupHeadWindow->getGroupTail();
    pGroupInsertionWindow->insertWindowToGroup(pWindow);

    // Since the inserted window is not becoming the group head, we need to hide it
    // (this is the difference from the original function, which would focus the window,
    // making it the new group head)
    pWindow->setHidden(true);

    pGroupHeadWindow->updateWindowDecos();
    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pGroupHeadWindow);
    if (!pWindow->getDecorationByType(DECORATION_GROUPBAR))
        pWindow->addWindowDeco(std::make_unique<CHyprGroupBarDecoration>(pWindow));
}

/*! Check common pre-conditions for group creation/deletion and perform needed initializations
 *
 * \param[out] pDwindleLayout  Pointer to dwindle layout instance
 * \return  Necessary pre-conditions succeded?
 */
bool handleGroupOperation(CHyprDwindleLayout** pDwindleLayout)
{
    const auto P_LAYOUT = g_pLayoutManager->getCurrentLayout();
    if (P_LAYOUT->getLayoutName() != "dwindle") {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring non-dwindle layout");
        return false;
    }

    *pDwindleLayout = dynamic_cast<CHyprDwindleLayout*>(P_LAYOUT);
    return true;
}

void newCreateGroup(CWindow* self)
{
    // Run the original function first, creating a "classical" group
    // with just the currently selected window in it
    ((createGroupFuncT)g_pCreateGroupHook->m_pOriginal)(self);

    // Only continue if the group really was created, as there are some pre-conditions to that
    // checked in the original function
    if (self->m_sGroupData.pNextWindow.expired()) {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring autogroup - invalid / non-group widnow");
        return;
    }

    // Skip floating windows
    if (self->m_bIsFloating) {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring autogroup for floating window");
        return;
    }

    Debug::log(LOG, "[dwindle-autogroup] Triggered createGroup for {:x}", self->m_pSelf.lock());

    // Obtain an instance of the dwindle layout, also run some general pre-conditions
    // for the plugin, quit now if they're not met.
    CHyprDwindleLayout* pDwindleLayout = nullptr;
    if (!handleGroupOperation(&pDwindleLayout))
        return;

    Debug::log(LOG, "[dwindle-autogroup] Collecting dwindle child nodes");

    // Collect all child dwindle nodes, we'll want to add all of those into a group
    const auto P_DWINDLE_NODE = g_pNodeFromWindow(pDwindleLayout, self->m_pSelf.lock());
    const auto P_PARENT_NODE = P_DWINDLE_NODE->pParent;
    if (!P_PARENT_NODE) {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring autogroup for a single window");
        return;
    }
    const auto P_SIBLING_NODE = P_PARENT_NODE->children[0] == P_DWINDLE_NODE ? P_PARENT_NODE->children[1] : P_PARENT_NODE->children[0];
    std::deque<SDwindleNodeData*> p_dDwindleNodes;
    collectDwindleChildNodes(&p_dDwindleNodes, P_SIBLING_NODE);

    // Stop if one of the dwindle child nodes is already in a group
    for (auto& node : p_dDwindleNodes) {
        auto curWindow = node->pWindow.lock();
        if (!curWindow->m_sGroupData.pNextWindow.expired()) {
            Debug::log(LOG, "[dwindle-autogroup] Ignoring autogroup for nested groups: window {:x} is group", curWindow);
            return;
        }
    }

    Debug::log(LOG, "[dwindle-autogroup] Found {} dwindle nodes to autogroup", p_dDwindleNodes.size());

    // Add all of the dwindle child node widnows into the group
    for (auto& node : p_dDwindleNodes) {
        auto curWindow = node->pWindow.lock();
        Debug::log(LOG, "[dwindle-autogroup] Moving window {:x} into group", curWindow);
        moveIntoGroup(curWindow, self->m_pSelf.lock());
    }

    Debug::log(LOG, "[dwindle-autogroup] Autogroup done, {} child nodes moved", p_dDwindleNodes.size());
}

void newDestroyGroup(CWindow* self)
{
    // We can't use the original function here (other than for falling back)
    // as it removes the group head and then creates the new windows on the
    // layout. This often messes up the user layout of the windows.
    //
    // The goal of this function is to ungroup the windows such that they
    // only continue as children from the dwindle binary tree node the group
    // head was on.

    // Only continue if the window is in a group
    if (self->m_sGroupData.pNextWindow.expired()) {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring ungroup - invalid / non-group widnow");
        return;
    }

    Debug::log(LOG, "[dwindle-autogroup] Triggered destroyGroup for {:x}", self->m_pSelf.lock());

    // Obtain an instance of the dwindle layout, also run some general pre-conditions
    // for the plugin, fall back now if they're not met.
    CHyprDwindleLayout* pDwindleLayout = nullptr;
    if (!handleGroupOperation(&pDwindleLayout)) {
        ((createGroupFuncT)g_pDestroyGroupHook->m_pOriginal)(self);
        return;
    }

    std::vector<PHLWINDOW> vGroupWindows;
    collectGroupWindows(&vGroupWindows, self->m_pSelf.lock());

    // If the group head window is in fullscreen, unfullscreen it.
    // We need to have the window placed in the layout, to figure out where
    // to ungroup the rest of the windows.
    g_pCompositor->setWindowFullscreen(self->m_pSelf.lock(), false, FULLSCREEN_FULL);

    Debug::log(LOG, "[dwindle-autogroup] Ungroupping {} windows", vGroupWindows.size());

    // Set a groups lock flag
    const bool GROUPS_LOCKED_PREV = g_pKeybindManager->m_bGroupsLocked;
    g_pKeybindManager->m_bGroupsLocked = true;

    for (PHLWINDOW pWindow : vGroupWindows) {
        Debug::log(LOG, "[dwindle-autogroup] Ungroupping window {:x}", pWindow);
        pWindow->m_sGroupData.pNextWindow.reset();
        pWindow->m_sGroupData.head = false;

        // Current / Visible window (this isn't always the head)
        if (!pWindow->isHidden()) {
            Debug::log(LOG, "[dwindle-autogroup] -> Visible window ungroup");

            // This window is already visible in the layout, we don't need to create
            // a new layout window for it.
            //
            // The original destroyGroup removes the window from the layout here,
            // which is what causes the weird ungroupping behavior as this window
            // is then recreated, which spawns it in a potentially unexpected place
            // (often determined by the cursor position).

            // Update the window decorations (removing group bar)
            pWindow->updateWindowDecos();
        }
        else {
            pWindow->setHidden(false);

            g_pLayoutManager->getCurrentLayout()->onWindowCreatedTiling(pWindow);
            pWindow->updateWindowDecos();

            // Focus the window that we just spawned, so that on the next iteration
            // the window created will be it's dwindle child node.
            // This allows the original group head to remain a parent window to all
            // of the other (groupped) nodes.
            //
            // Note that this won't preserve the exact original layout of the group
            // but it will make sure all of the groupped windows will extend from
            // the dwindle node of the group head window. Preserving the original
            // layout isn't really possible, since new windows can be added into
            // groups after they were created.
            g_pCompositor->focusWindow(pWindow);
        }
    }

    g_pKeybindManager->m_bGroupsLocked = GROUPS_LOCKED_PREV;

    Debug::log(LOG, "[dwindle-autogroup] All windows ungroupped");

    g_pCompositor->updateAllWindowsAnimatedDecorationValues();

    // Leave with the focus the original (main) window
    g_pCompositor->focusWindow(self->m_pSelf.lock());

    Debug::log(LOG, "[dwindle-autogroup] Ungroupping done");
}
