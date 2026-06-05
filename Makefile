CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
PKG_CONFIG ?= pkg-config

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := bin

ifeq ($(OS),Windows_NT)
  NATIVE_WINDOWS := 1
  EXEEXT := .exe
else
  NATIVE_WINDOWS := 0
  EXEEXT :=
endif

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

ifeq ($(NATIVE_WINDOWS),1)
  TERM_TARGET := $(BIN_DIR)/realm-terminal$(EXEEXT)
  GFX_TARGET := $(BIN_DIR)/realm$(EXEEXT)
  LAB_TARGET := $(BIN_DIR)/realm-lab$(EXEEXT)
else
  TERM_TARGET := $(BIN_DIR)/realm$(EXEEXT)
  GFX_TARGET := $(BIN_DIR)/realm-gfx$(EXEEXT)
  LAB_TARGET := $(BIN_DIR)/realm-lab$(EXEEXT)
endif
TEST_TARGET := $(BIN_DIR)/realm_headless_tests$(EXEEXT)

CORE_SRCS := $(wildcard $(SRC_DIR)/core/*.cpp)
SIM_SRCS := $(wildcard $(SRC_DIR)/sim/*.cpp) $(wildcard $(SRC_DIR)/sim/migrations/*.cpp)
COMMAND_SRCS := $(wildcard $(SRC_DIR)/commands/*.cpp)
AI_SRCS := $(wildcard $(SRC_DIR)/ai/*.cpp)
MAP_SRCS := $(wildcard $(SRC_DIR)/map/*.cpp)
PLATFORM_COMMON_SRCS := $(SRC_DIR)/platform/app_config.cpp $(SRC_DIR)/platform/game_init.cpp $(SRC_DIR)/platform/user_settings.cpp $(SRC_DIR)/platform/view_state.cpp
GAME_SRCS := $(CORE_SRCS) $(SIM_SRCS) $(COMMAND_SRCS) $(AI_SRCS) $(MAP_SRCS) $(PLATFORM_COMMON_SRCS)

RENDER_MODEL_SRCS := $(SRC_DIR)/render/display_model.cpp $(SRC_DIR)/render/entity_visual_defs.cpp $(SRC_DIR)/render/render_model.cpp
ASCII_RENDER_SRCS := $(RENDER_MODEL_SRCS) $(wildcard $(SRC_DIR)/render/ascii/*.cpp)
SDL_RENDER_SRCS := $(RENDER_MODEL_SRCS) $(wildcard $(SRC_DIR)/render/sdl/*.cpp)

TERM_SRCS := $(SRC_DIR)/platform/main_terminal.cpp $(GAME_SRCS) $(ASCII_RENDER_SRCS)
GFX_SRCS := $(SRC_DIR)/platform/main_sdl.cpp $(GAME_SRCS) $(SDL_RENDER_SRCS)
LAB_SRCS := $(SRC_DIR)/platform/main_lab.cpp $(GAME_SRCS) $(SDL_RENDER_SRCS)
TEST_SRCS := tests/realm_headless_tests.cpp $(GAME_SRCS) $(SRC_DIR)/render/render_model.cpp
WEB_SRCS := $(SRC_DIR)/platform/main_web.cpp $(GAME_SRCS) $(SDL_RENDER_SRCS)

NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null)
ifeq ($(UNAME_S),Darwin)
  NCURSES_FALLBACK_LIBS := -lncurses
else
  NCURSES_FALLBACK_LIBS := -lncursesw
endif
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo $(NCURSES_FALLBACK_LIBS))

SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf 2>/dev/null)
SDL_LIBS := $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf 2>/dev/null)
PNG_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpng 2>/dev/null)
PNG_LIBS := $(shell $(PKG_CONFIG) --libs libpng 2>/dev/null || echo -lpng)

ifeq ($(NATIVE_WINDOWS),1)
WINDOWS_RUNTIME_DLLS := SDL2.dll SDL2_ttf.dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libfreetype-6.dll libharfbuzz-0.dll libbz2-1.dll zlib1.dll libbrotlidec.dll libbrotlicommon.dll libpng16-16.dll libgraphite2.dll libglib-2.0-0.dll libintl-8.dll libiconv-2.dll libpcre2-8-0.dll
endif

DEFAULT_INCLUDES := -I$(INC_DIR) -I$(SRC_DIR)

define objs_for
$(patsubst %.cpp,$(OBJ_DIR)/$(1)/%.o,$(filter %.cpp,$(2)))
endef

TERM_OBJS := $(call objs_for,term,$(TERM_SRCS))
GFX_OBJS := $(call objs_for,gfx,$(GFX_SRCS))
LAB_OBJS := $(call objs_for,lab,$(LAB_SRCS))
TEST_OBJS := $(call objs_for,test,$(TEST_SRCS))

.DEFAULT_GOAL := gui
.PHONY: all gui gfx lab terminal term run run-gui run-lab run-terminal web test ui-test ascii-compare debug sanitize package clean check-sdl check-ncurses copy-windows-runtime help

all: gui

ifeq ($(NATIVE_WINDOWS),1)
gui: check-sdl $(GFX_TARGET) copy-windows-runtime
else
gui: check-sdl $(GFX_TARGET)
endif

gfx: gui

ifeq ($(NATIVE_WINDOWS),1)
lab: check-sdl $(LAB_TARGET) copy-windows-runtime
else
lab: check-sdl $(LAB_TARGET)
endif

run: run-gui
run-gui: gui
	./$(GFX_TARGET)
run-lab: lab
	./$(LAB_TARGET)

ifeq ($(NATIVE_WINDOWS),1)
terminal term run-terminal:
	@echo "Native Windows builds GUI only. Use 'mingw32-make' or 'mingw32-make gui'."
	@echo "For the ncurses terminal build, use WSL/Linux/macOS and run 'make terminal'."
	@exit 1
else
terminal term: check-ncurses $(TERM_TARGET)
run-terminal: terminal
	./$(TERM_TARGET)
endif

help:
	@echo "Realm build targets:"
	@echo "  make / make gui      Build GUI renderer (default, all platforms)"
	@echo "  make web            Build WebAssembly/Netlify output in dist/netlify"
	@echo "  make run            Build and run GUI renderer"
	@echo "  make lab            Build local tileset lab"
	@echo "  make run-lab        Build and run local tileset lab"
	@echo "  make test           Build and run headless simulation tests"
	@echo "  make ui-test        Build GUI and write UI screenshots to build/ui-screenshots"
	@echo "  make ascii-compare  Build GUI and write ASCII terminal/GUI comparison captures"
	@echo "  make debug          Build tests with debug flags"
ifeq ($(NATIVE_WINDOWS),1)
	@echo "  make terminal       Not supported on native Windows; use WSL"
	@echo "  make sanitize       Not supported on native Windows/MSYS2 by this Makefile"
else
	@echo "  make terminal       Build terminal/ncurses renderer"
	@echo "  make run-terminal   Build and run terminal/ncurses renderer"
	@echo "  make sanitize       Build tests with address/undefined sanitizers"
endif
	@echo "  make package        Create a zip from bin/ after GUI build"
	@echo "  make clean          Remove build outputs"

web:
	bash scripts/build-web.sh

check-sdl:
	@$(PKG_CONFIG) --exists sdl2 SDL2_ttf libpng || ( \
		echo "Missing SDL2/SDL2_ttf/libpng development packages."; \
		echo "Windows/MSYS2 UCRT64:"; \
		echo "  pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-libpng"; \
		echo "Linux/WSL:"; \
		echo "  sudo apt install build-essential pkg-config libsdl2-dev libsdl2-ttf-dev libpng-dev"; \
		echo "macOS/Homebrew:"; \
		echo "  brew install sdl2 sdl2_ttf libpng pkg-config"; \
		exit 1 )

check-ncurses:
	@$(PKG_CONFIG) --exists ncursesw 2>/dev/null || true

$(GFX_TARGET): $(GFX_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(GFX_OBJS) $(SDL_LIBS) $(PNG_LIBS)

$(LAB_TARGET): $(LAB_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LAB_OBJS) $(SDL_LIBS) $(PNG_LIBS)

$(TERM_TARGET): $(TERM_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TERM_OBJS) $(NCURSES_LIBS)

$(TEST_TARGET): $(TEST_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS)

$(OBJ_DIR)/term/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DUSE_NCURSES_RENDERER $(DEFAULT_INCLUDES) $(NCURSES_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/gfx/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER $(DEFAULT_INCLUDES) $(SDL_CFLAGS) $(PNG_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/lab/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER $(DEFAULT_INCLUDES) $(SDL_CFLAGS) $(PNG_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/test/%.o: %.cpp | $(OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -DREALM_ENABLE_WORLD_INDEX_STATS $(DEFAULT_INCLUDES) -c -o $@ $<

test: $(TEST_TARGET)
	./$(TEST_TARGET)

architecture-check:
	python scripts/check_architecture.py

ui-test: gui
	REALM_UI_TEST=1 ./$(GFX_TARGET)

ascii-compare: gui
	REALM_ASCII_COMPARE=1 ./$(GFX_TARGET)

debug:
	$(MAKE) clean
	$(MAKE) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS) -O0 -g -D_GLIBCXX_ASSERTIONS -DREALM_DEBUG_ASSERTS" test

ifeq ($(NATIVE_WINDOWS),1)
sanitize:
	@echo "Sanitizer target is intentionally disabled on native Windows/MSYS2; use WSL/Linux/macOS."
	@exit 1
else
sanitize:
	$(MAKE) clean
	REALM_TEST_LONG_TICKS=2000 $(MAKE) CXX="$(CXX)" CXXFLAGS="$(CXXFLAGS) -O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer" test
endif

package: gui
	/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/package-windows.ps1

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

ifeq ($(NATIVE_WINDOWS),1)
copy-windows-runtime: | $(BIN_DIR)
	@for dll in $(WINDOWS_RUNTIME_DLLS); do \
		cp -u "/ucrt64/bin/$$dll" "$(BIN_DIR)/" || exit 1; \
	done
else
copy-windows-runtime:
endif

clean:
ifeq ($(NATIVE_WINDOWS),1)
	@rm -rf $(BIN_DIR)
	@clean_err="$$(mktemp)"; \
	if rm -rf $(BUILD_DIR) 2>"$$clean_err"; then \
		rm -f "$$clean_err"; \
	else \
		status=$$?; \
		if [ -d "$(BUILD_DIR)" ] && ! find "$(BUILD_DIR)" -mindepth 1 ! \( -name 'web-server*.log' -o -name 'web-server*.err' -o -name 'web-fallback-server*.log' -o -name 'web-fallback-server*.err' \) -print -quit | grep -q .; then \
			echo "warning: preserving locked $(BUILD_DIR) web server logs during Windows clean"; \
			rm -f "$$clean_err"; \
		else \
			cat "$$clean_err"; \
			rm -f "$$clean_err"; \
			exit $$status; \
		fi; \
	fi
else
	rm -rf $(BUILD_DIR) $(BIN_DIR)
endif
