# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

PLUGIN_NAME=dwindle-autogroup

SOURCE_FILES=$(wildcard src/*.cpp)

.PHONY: clean clangd

all: check_env $(PLUGIN_NAME).so

install: all
	cp $(PLUGIN_NAME).so ${HOME}/.local/share/hyprload/plugins/bin

check_env:
	@if ! pkg-config --exists hyprland; then \
		echo 'Hyprland headers not available. Run `make pluginenv` in the root Hyprland directory.'; \
		exit 1; \
	fi

$(PLUGIN_NAME).so: $(SOURCE_FILES) $(INCLUDE_FILES)
	g++ -shared -fPIC --no-gnu-unique $(SOURCE_FILES) -o $(PLUGIN_NAME).so -g `pkg-config --cflags pixman-1 libdrm hyprland` -Iinclude -std=c++23

clean:
	rm ./${PLUGIN_NAME}.so

clangd:
	printf "%b" "`pkg-config --cflags pixman-1 libdrm hyprland | sed 's/ -/\n-/g'`\n-Iinclude\n-std=c++2b" > compile_flags.txt
