PLUGIN_NAME := dwindle-autogroup
SOURCE_FILES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.hpp)

CXX ?= g++
CPPFLAGS += -Iinclude $(shell pkg-config --cflags hyprland luajit)
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++23 -fPIC -fno-gnu-unique
LDLIBS += $(shell pkg-config --libs luajit)

.PHONY: all clean clangd

all: $(PLUGIN_NAME).so

$(PLUGIN_NAME).so: $(SOURCE_FILES) $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -shared $(SOURCE_FILES) $(LDLIBS) -o $@

clean:
	rm -f $(PLUGIN_NAME).so

clangd:
	printf '%s\n' $(CPPFLAGS) $(CXXFLAGS) > compile_flags.txt
