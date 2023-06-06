# Dwindle-Autogroup

This plugin changes the behavior of the `togglegroup` dispatcher for dwindle layout, to automatically group all of the child windows when a new group is created.

Before Hyprland `v0.23.0beta`, this was actually the default behavior ([PR #1580](https://github.com/hyprwm/Hyprland/pull/1580)), however as that release introduced group support for other layouts, including floating windows, this dwindle specific feature was removed and `togglegroup` now only creates a group window, and requires you to move in all of the windows that should be a part of that group into it manually.

## Installation

Since Hyprland plugins don't have ABI guarantees, you should download the Hyprland source and compile it if you plan to use plugins. This ensures the compiler version is the same between the Hyprland build you're running, and the plugins you are using.

The guide on compiling and installing Hyprland manually is on the [wiki](http://wiki.hyprland.org/Getting-Started/Installation/#manual-manual-build)

Note, this plugin currently requires hyprland compiled from commit: [`4afeedbd`](https://github.com/hyprwm/Hyprland/commit/4afeedbd567ca2114f682f14a6de26d4a8188878) or later.

## Using [hyprload](https://github.com/Duckonaut/hyprload)

Add the line `"ItsDrike/hyprland-dwindle-autogroup"` to your `hyprload.toml` config, like this:

```
plugins = [
    "ItsDrike/hyprland-dwindle-autogroup",
]
```

Then update via `hyprload,update` dispatcher.

## Manual installation

1. Clone the Hyprland repository and build the plugin environment

   - `git clone --recursive https://github.com/hyprwm/Hyprland`
   - In the Hyprland directory: `make pluginenv`
   - Ideally, you should use this instance of Hyprland as your compositor, to do that, run `make install`. This is heavily recommended, as using a different Hyprland instance might cause incompatibilities.

2. Build the plugn

   - Run `make all`

3. Add this line to the bottom of your hyprland config

   - `exec-once=hyprctl plugin load <ABSOLUTE PATH TO split-monitor-workspaces.so>`

## Development

When developing, it is useful to run `make clangd`, to generate `compile_flags.txt` file, allowing Clang language server to properly recognize the imports, and give you autocompletion.

## Disclaimer

I'm very new to C++ development, and I'm also not very familiar with Hyprland's codebase. So while I will do my best to follow best practices, with so little experience, you can be pretty much certain that there will be bugs, and that the code will not be pretty. But hey, if you know about something that I did wrong, feel free to PR/make an issue about it.
