#include "plugin.hpp"

#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/Group.hpp>
#include <hyprland/src/desktop/view/window/Window.hpp>
#include <hyprland/src/desktop/view/window/WindowGroupMembership.hpp>
#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/algorithm/tiled/dwindle/DwindleAlgorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/layout/target/WindowGroupTarget.hpp>
#include <hyprland/src/layout/target/WindowTarget.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>

#include <vector>

namespace {
using Node = Layout::Tiled::SDwindleNodeData;
using Dwindle = Layout::Tiled::CDwindleAlgorithm;

SDispatchResult failure(const std::string& message) {
    return {.success = false, .error = "dwindle-autogroup: " + message};
}

// Snapshot windows before changing the tree: adding a group member removes its node.
bool collectWindows(const SP<Node>& node, std::vector<PHLWINDOW>& windows) {
    if (!node || !node->valid)
        return false;
    if (node->isNode)
        return collectWindows(node->children[0].lock(), windows) && collectWindows(node->children[1].lock(), windows);

    const auto target = node->pTarget.lock();
    if (!target || target->type() != Layout::TARGET_TYPE_WINDOW || !target->window())
        return false;
    windows.push_back(target->window());
    return true;
}

SDispatchResult createGroup(PHLWINDOW window, Dwindle* dwindle) {
    using namespace Desktop::View;
    if (Desktop::windowState()->groupsLocked() || (window->grouping().rules() & GROUP_DENY))
        return failure("grouping is locked or denied for the focused window");

    const auto node = dwindle->getNodeFromWindow(window);
    if (!node)
        return failure("focused window has no dwindle node");
    const auto parent = node->pParent.lock();
    std::vector<PHLWINDOW> windows;
    if (parent) {
        const auto first = parent->children[0].lock();
        const auto second = parent->children[1].lock();
        if (!first || !second || (first != node && second != node) ||
            !collectWindows(first == node ? second : first, windows))
            return failure("sibling subtree contains an existing group or invalid target");
    }

    for (const auto& member : windows) {
        if (member->grouping().group() || (member->grouping().rules() & GROUP_DENY) || Fullscreen::controller()->isFullscreen(member))
            return failure("a sibling is grouped, fullscreen, or denies grouping");
    }

    const auto group = CGroup::create({window});
    // Respect the normal group lock/rule checks before changing any sibling.
    for (const auto& member : windows) {
        if (!member->grouping().canBeGroupedInto(group)) {
            group->destroy();
            return failure("a sibling cannot be added to the new group");
        }
    }
    for (const auto& member : windows) {
        group->add(member);
        group->setCurrent(window);
    }
    Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
    return {};
}

SDispatchResult destroyGroup(PHLWINDOW window, const SP<Desktop::View::CGroup>& group) {
    if (Fullscreen::controller()->isFullscreen(window))
        return failure("exit fullscreen before ungrouping");
    const auto space = group->target()->space();
    if (!space)
        return failure("group has no layout space");

    std::vector<PHLWINDOW> members;
    for (const auto& weak : group->windows()) {
        const auto member = weak.lock();
        if (!member)
            return failure("group contains an invalid window");
        if (member != window)
            members.push_back(member);
    }

    for (const auto& member : members) {
        group->remove(member);
        const auto target = member->layoutTarget();
        if (target->space() != space || target->floating())
            continue; // Hyprland may float windows whose size constraints prohibit tiling.

        // Reinsert using a focal point inside the remaining group node, regardless
        // of cursor position or dwindle:use_active_for_splits.
        const auto box = group->target()->position();
        const auto focal = box.pos() + box.size() / 2.0;
        space->remove(target);
        space->move(target, focal);
    }
    // Destroy the final one-member group through Hyprland to emit its IPC event.
    group->destroy();
    Desktop::focusState()->fullWindowFocus(window, Desktop::FOCUS_REASON_KEYBIND);
    return {};
}
} // namespace

SDispatchResult toggleAutogroup(const std::string& args) {
    if (!args.empty() && args != "x")
        return failure("this dispatcher takes no arguments (or use x as a placeholder)");
    const auto window = Desktop::focusState()->window();
    if (!window || !window->mapped())
        return failure("no focused mapped window");
    const auto target = window->layoutTarget();
    if (!target || target->floating() || !target->space() || !target->space()->algorithm())
        return failure("focused window is not tiled");
    const auto& tiled = target->space()->algorithm()->tiledAlgo();
    auto* dwindle = dynamic_cast<Dwindle*>(tiled.get());
    if (!dwindle)
        return failure("focused workspace does not use dwindle");
    if (const auto group = window->grouping().group())
        return destroyGroup(window, group);
    return createGroup(window, dwindle);
}
