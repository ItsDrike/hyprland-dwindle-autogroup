#include "func_finder.hpp"

/* Debug function for converting vector of strings to pretty-printed representation
 *
 * \param[in]   Vector of strings
 * \return      Pretty printed single-line representation of the vector
 */
std::string vectorToString(const std::vector<std::string>& vec)
{
    std::ostringstream oss;
    oss << "[";

    for (size_t i = 0; i < vec.size(); ++i) {
        oss << std::quoted(vec[i]);

        // Add a comma after each element except the last one
        if (i < vec.size() - 1) {
            oss << ", ";
        }
    }

    oss << "]";
    return oss.str();
}

void* findHyprlandFunction(const std::string& name, const std::string& demangledName)
{
    const auto METHODS = HyprlandAPI::findFunctionsByName(PHANDLE, name);

    if (METHODS.size() == 0) {
        Debug::log(
            ERR, "[dwindle-autogroup] Function {} wasn't found in Hyprland binary! This function's signature probably changed, report this", name);
        HyprlandAPI::addNotification(PHANDLE, "[dwindle-autogroup] Initialization failed! " + name + " function wasn't found", s_notifyColor, 10000);
        return nullptr;
    }

    void* res = nullptr;
    for (auto& method : METHODS) {
        if (method.demangled == demangledName) {
            res = method.address;
            break;
        }
    }
    // Use std::transform to extract the demangled strings from function matches
    std::vector<std::string> demangledStrings;
    std::transform(METHODS.begin(), METHODS.end(), std::back_inserter(demangledStrings), [](const SFunctionMatch& match) { return match.demangled; });

    if (res == nullptr) {
        Debug::log(ERR,
                   "[dwindle-autogroup] Demangled function {} wasn't found in matches for function name {} in Hyprland binary! This function's "
                   "signature probably "
                   "changed, report this. Found matches: {}",
                   demangledName,
                   name,
                   vectorToString(demangledStrings));
        HyprlandAPI::addNotification(
            PHANDLE, "[dwindle-autogroup] Initialization failed! " + name + " function (demangled) wasn't found", s_notifyColor, 10000);
        return nullptr;
    }

    return res;
}
