# compile with `make all`
# make sure you've ran `make pluginenv` in the clonned Hyprland repo

PLUGIN_NAME=dwindle-autogroup

INSTALL_LOCATION=${HOME}/.local/share/hyprload/plugins/bin

SOURCE_FILES=$(wildcard src/*.cpp)

COMPILE_DEFINES=-DWLR_USE_UNSTABLE

COMPILE_FLAGS=-g -fPIC --no-gnu-unique -std=c++23
COMPILE_FLAGS+=`pkg-config --cflags pixman-1 libdrm hyprland`
COMPILE_FLAGS+=-Iinclude

LINK_FLAGS=-shared

# Phony targets (i.e. targets that don't actually build anything, and don't track dependencies)
# These will always be run when called
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
	g++ $(LINK_FLAGS) $(COMPILE_FLAGS) $(COMPILE_DEFINES) $(SOURCE_FILES) -o $(PLUGIN_NAME).so

clean:
	rm ./${PLUGIN_NAME}.so

clangd:
	echo "$(COMPILE_FLAGS) $(COMPILE_DEFINES)" | \
		sed 's/--no-gnu-unique//g' | \
		sed 's/ -/\n-/g' | \
		sed 's/std=c++23/std=c++2b/g' \
		> compile_flags.txt
