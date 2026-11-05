# Dwindle-Autogroup

This plugin adds an alternative to the toggle group dispatcher for dwindle
layout, to automatically group all of the child windows when a new group is
created.

Before Hyprland `v0.23.0beta`, this groupping behavior was actually the default
behavior, however as that release introduced group support for other layouts,
including floating windows, this dwindle specific feature was removed (in [PR
#1580][hypr-pr-1580]) and group toggling now only creates a group window, and
requires you to move in all of the windows that should be a part of that group
into it manually.

[hypr-pr-1580]: https://github.com/hyprwm/Hyprland/pull/1580

# Showcase

https://github.com/ItsDrike/hyprland-dwindle-autogroup/assets/20902250/31e60fcf-b741-4501-b20e-0e2bf0c65209

## Installation

### Hyprpm

The recommended way to install this plugin is via `hyprpm`:

- `hyprpm add https://github.com/ItsDrike/hyprland-dwindle-autogroup`
- `hyprpm enable dwindle-autogroup`

Load enabled plugins in the current session with `hyprpm reload`. To load them
automatically when Hyprland starts, add this to `hyprland.lua`:

```lua
hl.on("hyprland.start", function()
    hl.exec_cmd("hyprpm reload -n")
end)
```

The `-n` flag adds a success notification, remove it if you don't like that.
Run `hyprpm update` after upgrading Hyprland or the plugin. See the [Hyprland
plugin documentation][hypr-plugin-docs] for more information, including
permissions if enabled in your configuration.

[hypr-plugin-docs]: https://wiki.hypr.land/Plugins/Using-Plugins/

## Manual installation

1. **Set up the Hyprland headers.** The headers are the Hyprland source files
   needed to compile the plugin. They must come from the **same Hyprland
   commit** as the compositor you run: even two builds with the same version
   number can differ. Check your running version and commit with:

   ```sh
   hyprctl version
   ```

   If your distribution provides Hyprland development headers for that exact
   build, install them using its package manager. You can check whether
   `pkg-config` finds the headers and their dependencies with:

   ```sh
   pkg-config --modversion hyprland
   pkg-config --cflags hyprland
   ```

   If the headers aren't available as a package, build and install them from
   Hyprland's source.

   ```sh
   cd /whatever/dir/you/like
   git clone --recursive https://github.com/hyprwm/Hyprland.git
   cd Hyprland
   git checkout "$(hyprctl version -j | jq -r '.commit')"
   git submodule update --init --recursive
   make all
   sudo make installheaders
   ```

   This requires Hyprland's build dependencies (see [build
   instructions][hypr-build-instructions]).

2. **Install build dependencies.** Requires LuaJIT development files, `pkg-config`, and a
   C++23 compiler, as well as any missing Hyprland dependency development
   packages. Use a compiler and C++ standard library compatible with the ones
   used to build your running Hyprland.

3. **Build the plugin.** In this plugin's repository, run:

   ```sh
   make all
   ```

4. **Load the plugin.** Use the absolute path to the resulting shared object:

   ```sh
   hyprctl plugin load /absolute/path/to/dwindle-autogroup.so
   ```

   The plugin checks for a Hyprland version mismatch at load time. Rebuild it
   with matching headers after updating Hyprland.

[hypr-build-instructions]: https://wiki.hypr.land/Getting-Started/Installation/

## Usage

Add a bind to `hyprland.lua` (instead of the native `hl.dsp.group.toggle()`):

```lua
hl.bind("SUPER + G", function()
    local plugin = hl.plugin.dwindle_autogroup
    if not plugin then return end
    local ok, err = plugin.toggle()
    if not ok then
        hl.notification.create({ text = err, duration = 3000 })
    end
end)
```

The function returns `true` on success or `false, error_message` on failure. It
works on tiled dwindle windows, refuses groupping if the dwindle subtree
contain existing groups, and requires exiting fullscreen or maximized mode
before ungroupping.

## Development

When developing, it is useful to run `make clangd`, to generate `compile_flags.txt` file, allowing Clang language server to properly recognize the imports, and give you autocompletion.

## Disclaimer

I'm very new to C++ development, and I'm also not very familiar with Hyprland's codebase. So while I will do my best to follow best practices, with so little experience, you can be pretty much certain that there will be bugs, and that the code will not be pretty. But hey, if you know about something that I did wrong, feel free to PR/make an issue about it.
