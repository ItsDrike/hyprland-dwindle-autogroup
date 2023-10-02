#include "plugin.hpp"
#include "globals.hpp"
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/layout/DwindleLayout.hpp>
#include <hyprland/src/managers/LayoutManager.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/decorations/CHyprGroupBarDecoration.hpp>

/*! Recursively collect all dwindle child nodes for given root node
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
 * \param[out] pDeque   deque to store the found group windows into
 * \param[in] pWindow   any window that belongs to a group (doesn't have to be the group head window)
 */
void collectGroupWindows(std::deque<CWindow*>* pDeque, CWindow* pWindow)
{
    CWindow* curr = pWindow;
    do {
        const auto PLAST = curr;
        pDeque->emplace_back(curr);
        curr = curr->m_sGroupData.pNextWindow;
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
void moveIntoGroup(CWindow* pWindow, CWindow* pGroupHeadWindow)
{
    const auto P_LAYOUT = g_pLayoutManager->getCurrentLayout();

    const auto* USE_CURR_POS = &g_pConfigManager->getConfigValuePtr("misc:group_insert_after_current")->intValue;
    CWindow* pGroupWindow = *USE_CURR_POS ? pGroupHeadWindow : pGroupHeadWindow->getGroupTail();

    // Remove the window from layout (will become a part of a group)
    P_LAYOUT->onWindowRemoved(pWindow);

    // Create a group bar decoration for the window we'll be inserting
    // (if it's not already a group, in which case it should already have it)
    if (!pWindow->m_sGroupData.pNextWindow)
        pWindow->m_dWindowDecorations.emplace_back(std::make_unique<CHyprGroupBarDecoration>(pWindow));

    pGroupWindow->insertWindowToGroup(pWindow);
    pGroupHeadWindow->updateWindowDecos();
    pWindow->setHidden(true);

    g_pLayoutManager->getCurrentLayout()->recalculateWindow(pGroupHeadWindow);
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

    // Only continue if the group really was created, as there are some pre-conditions to that.
    if (!self->m_sGroupData.pNextWindow) {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring autogroup - invalid / non-group widnow");
        return;
    }

    Debug::log(LOG, "[dwindle-autogroup] Triggered createGroup for {:x}", self);

    // Obtain an instance of the dwindle layout, also run some general pre-conditions
    // for the plugin, quit now if they're not met.
    CHyprDwindleLayout* pDwindleLayout = nullptr;
    if (!handleGroupOperation(&pDwindleLayout))
        return;

    Debug::log(LOG, "[dwindle-autogroup] Autogroupping dwindle child nodes");

    // Collect all child dwindle nodes, we'll want to add all of those into a group
    const auto P_DWINDLE_NODE = g_pNodeFromWindow(pDwindleLayout, self);
    std::deque<SDwindleNodeData*> p_dDwindleNodes;
    const auto P_SIBLING_NODE =
        P_DWINDLE_NODE->pParent->children[0] == P_DWINDLE_NODE ? P_DWINDLE_NODE->pParent->children[1] : P_DWINDLE_NODE->pParent->children[0];
    collectDwindleChildNodes(&p_dDwindleNodes, P_SIBLING_NODE);

    // Stop if one of the dwindle child nodes is already in a group
    for (auto& node : p_dDwindleNodes) {
        auto curWindow = node->pWindow;
        if (curWindow->m_sGroupData.pNextWindow) {
            Debug::log(LOG, "[dwindle-autogroup] Ignoring autogroup for nested groups: window {:x} is group", curWindow);
            return;
        }
    }

    Debug::log(LOG, "[dwindle-autogroup] Found {} dwindle nodes to autogroup", p_dDwindleNodes.size());

    // Add all of the dwindle child node widnows into the group
    for (auto& node : p_dDwindleNodes) {
        auto curWindow = node->pWindow;
        Debug::log(LOG, "[dwindle-autogroup] Moving window {:x} into group", curWindow);
        moveIntoGroup(curWindow, self);
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

    // Only continue if the window isn't in a group
    if (!self->m_sGroupData.pNextWindow) {
        Debug::log(LOG, "[dwindle-autogroup] Ignoring ungroup - invalid / non-group widnow");
        return;
    }

    Debug::log(LOG, "[dwindle-autogroup] Triggered destroyGroup for {:x}", self);

    // Obtain an instance of the dwindle layout, also run some general pre-conditions
    // for the plugin, fall back now if they're not met.
    CHyprDwindleLayout* pDwindleLayout = nullptr;
    if (!handleGroupOperation(&pDwindleLayout)) {
        ((createGroupFuncT)g_pDestroyGroupHook->m_pOriginal)(self);
        return;
    }

    // TODO: Add proper ungroupping logic instead of this fallback
    // (Tracked by github issue #1)
    Debug::log(LOG, "[dwindle-autogroup] Falling back to original ungroupping behavior (temporary)");
    ((createGroupFuncT)g_pDestroyGroupHook->m_pOriginal)(self);
}
