#include "globals.hpp"

/* Find function pointer from the Hyprland binary by it's name and demangledName
 *
 * \param[in] name              Simple name of the function (i.e. createGroup)
 * \param[in] demangledName     Demangled name of the function (i.e. CWindow::createGroup())
 * \return  Raw pointer to the found function
 */
void* findHyprlandFunction(const std::string& name, const std::string& demangledName);
