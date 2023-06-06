# compile with HYPRLAND_HEADERS=<path_to_hl> make all
# make sure that the path above is to the root hl repo directory, NOT src/
# and that you have ran `make protocols` in the hl dir.

PLUGIN_NAME=dwindle-autogroup

INSTALL_LOCATION=${HOME}/.local/share/hyprload/plugins/bin

SOURCE_FILES=$(wildcard src/*.cpp)

COMPILE_FLAGS=-g -fPIC --no-gnu-unique -std=c++23
COMPILE_FLAGS+=`pkg-config --cflags pixman-1 libdrm hyprland`
COMPILE_FLAGS+=-Iinclude

LINK_FLAGS=-shared

.PHONY: clean clangd

all: check_env $(PLUGIN_NAME).so

install: all
	mkdir -p $(INSTALL_LOCATION)
	cp $(PLUGIN_NAME).so $(INSTALL_LOCATION)

check_env:
	@if ! pkg-config --exists hyprland; then \
		echo 'Hyprland headers not available. Run `make pluginenv` in the root Hyprland directory.'; \
		exit 1; \
	fi

$(PLUGIN_NAME).so: $(SOURCE_FILES) $(INCLUDE_FILES)
	g++ $(LINK_FLAGS) $(COMPILE_FLAGS) $(SOURCE_FILES) -o $(PLUGIN_NAME).so

clean:
	rm ./${PLUGIN_NAME}.so

clangd:
	echo "$(COMPILE_FLAGS)" | \
		sed 's/--no-gnu-unique//g' | \
		sed 's/ -/\n-/g' | \
		sed 's/std=c++23/std=c++2b/g' \
		> compile_flags.txt
