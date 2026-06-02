CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
PKG_CONFIG ?= pkg-config

SRC_DIR := src
INC_DIR := include
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := bin

# -------------------------------------------------------------------
# Platform detection
# -------------------------------------------------------------------
ifeq ($(OS),Windows_NT)
  NATIVE_WINDOWS := 1
  EXEEXT := .exe
else
  NATIVE_WINDOWS := 0
  EXEEXT :=
endif

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

# -------------------------------------------------------------------
# Targets / objects
# -------------------------------------------------------------------
ifeq ($(NATIVE_WINDOWS),1)
  TERM_TARGET := $(BIN_DIR)/realm-terminal$(EXEEXT)
  GFX_TARGET := $(BIN_DIR)/realm$(EXEEXT)
  LAB_TARGET := $(BIN_DIR)/realm-lab$(EXEEXT)
else
  TERM_TARGET := $(BIN_DIR)/realm$(EXEEXT)
  GFX_TARGET := $(BIN_DIR)/realm-gfx$(EXEEXT)
  LAB_TARGET := $(BIN_DIR)/realm-lab$(EXEEXT)
endif

TERM_OBJS := \
	$(OBJ_DIR)/main.o \
	$(OBJ_DIR)/env_config.o \
	$(OBJ_DIR)/entity_animation.o \
	$(OBJ_DIR)/globals.o \
	$(OBJ_DIR)/mapgen.o \
	$(OBJ_DIR)/entity.o \
	$(OBJ_DIR)/orders.o \
	$(OBJ_DIR)/simulation.o \
	$(OBJ_DIR)/ai.o \
	$(OBJ_DIR)/render.o \
	$(OBJ_DIR)/input.o \
	$(OBJ_DIR)/display.o

GFX_OBJS := \
	$(OBJ_DIR)/main_gfx.o \
	$(OBJ_DIR)/main_gfx_init.o \
	$(OBJ_DIR)/env_config_gfx.o \
	$(OBJ_DIR)/entity_animation_gfx.o \
	$(OBJ_DIR)/globals_gfx.o \
	$(OBJ_DIR)/mapgen_gfx.o \
	$(OBJ_DIR)/entity_gfx.o \
	$(OBJ_DIR)/orders_gfx.o \
	$(OBJ_DIR)/simulation_gfx.o \
	$(OBJ_DIR)/ai_gfx.o \
	$(OBJ_DIR)/input_gfx.o \
	$(OBJ_DIR)/display_gfx.o \
	$(OBJ_DIR)/tileset_assets_gfx.o \
	$(OBJ_DIR)/gfx_renderer.o

LAB_OBJS := \
	$(OBJ_DIR)/main_lab.o \
	$(OBJ_DIR)/main_lab_init.o \
	$(OBJ_DIR)/env_config_lab.o \
	$(OBJ_DIR)/entity_animation_lab.o \
	$(OBJ_DIR)/globals_lab.o \
	$(OBJ_DIR)/mapgen_lab.o \
	$(OBJ_DIR)/entity_lab.o \
	$(OBJ_DIR)/orders_lab.o \
	$(OBJ_DIR)/simulation_lab.o \
	$(OBJ_DIR)/ai_lab.o \
	$(OBJ_DIR)/input_lab.o \
	$(OBJ_DIR)/display_lab.o \
	$(OBJ_DIR)/tileset_assets_lab.o \
	$(OBJ_DIR)/gfx_renderer_lab.o

TEST_TARGET := $(BIN_DIR)/realm_headless_tests$(EXEEXT)
TEST_OBJS := \
	$(OBJ_DIR)/tests/realm_headless_tests.o \
	$(OBJ_DIR)/main_headless.o \
	$(OBJ_DIR)/entity_animation_headless.o \
	$(OBJ_DIR)/globals_headless.o \
	$(OBJ_DIR)/mapgen_headless.o \
	$(OBJ_DIR)/entity_headless.o \
	$(OBJ_DIR)/orders_headless.o \
	$(OBJ_DIR)/simulation_headless.o \
	$(OBJ_DIR)/ai_headless.o \
	$(OBJ_DIR)/input_headless.o

.DEFAULT_GOAL := gui

# -------------------------------------------------------------------
# Libraries
# -------------------------------------------------------------------
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

# -------------------------------------------------------------------
# Public build commands
# -------------------------------------------------------------------
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

# -------------------------------------------------------------------
# Dependency checks
# -------------------------------------------------------------------
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

# -------------------------------------------------------------------
# Link targets
# -------------------------------------------------------------------
$(GFX_TARGET): $(GFX_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(GFX_OBJS) $(SDL_LIBS) $(PNG_LIBS)

$(LAB_TARGET): $(LAB_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(LAB_OBJS) $(SDL_LIBS) $(PNG_LIBS)

$(TERM_TARGET): $(TERM_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TERM_OBJS) $(NCURSES_LIBS)

$(TEST_TARGET): $(TEST_OBJS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(TEST_OBJS)

# -------------------------------------------------------------------
# Compile rules
# -------------------------------------------------------------------
$(OBJ_DIR)/env_config.o: $(SRC_DIR)/env_config.cpp $(INC_DIR)/env_config.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -c -o $@ $(SRC_DIR)/env_config.cpp

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) $(NCURSES_CFLAGS) -c -o $@ $<

$(OBJ_DIR)/main_gfx.o: $(SRC_DIR)/main_gfx.cpp $(INC_DIR)/realm.h $(INC_DIR)/gfx_renderer.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) $(SDL_CFLAGS) -c -o $@ $(SRC_DIR)/main_gfx.cpp

$(OBJ_DIR)/main_gfx_init.o: $(SRC_DIR)/main.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/main.cpp

$(OBJ_DIR)/env_config_gfx.o: $(SRC_DIR)/env_config.cpp $(INC_DIR)/env_config.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/env_config.cpp

$(OBJ_DIR)/entity_animation_gfx.o: $(SRC_DIR)/entity_animation.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/entity_animation.cpp

$(OBJ_DIR)/globals_gfx.o: $(SRC_DIR)/globals.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/globals.cpp

$(OBJ_DIR)/mapgen_gfx.o: $(SRC_DIR)/mapgen.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/mapgen.cpp

$(OBJ_DIR)/entity_gfx.o: $(SRC_DIR)/entity.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/entity.cpp

$(OBJ_DIR)/orders_gfx.o: $(SRC_DIR)/orders.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/orders.cpp

$(OBJ_DIR)/simulation_gfx.o: $(SRC_DIR)/simulation.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/simulation.cpp

$(OBJ_DIR)/ai_gfx.o: $(SRC_DIR)/ai.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/ai.cpp

$(OBJ_DIR)/input_gfx.o: $(SRC_DIR)/input.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/input.cpp

$(OBJ_DIR)/display_gfx.o: $(SRC_DIR)/display.cpp $(INC_DIR)/realm.h $(INC_DIR)/display.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/display.cpp

$(OBJ_DIR)/gfx_renderer.o: $(SRC_DIR)/gfx_renderer.cpp $(INC_DIR)/realm.h $(INC_DIR)/gfx_renderer.h $(INC_DIR)/entity_animation.h $(INC_DIR)/tileset_assets.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) $(SDL_CFLAGS) -c -o $@ $(SRC_DIR)/gfx_renderer.cpp

$(OBJ_DIR)/tileset_assets_gfx.o: $(SRC_DIR)/tileset_assets.cpp $(INC_DIR)/tileset_assets.h $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) $(SDL_CFLAGS) $(PNG_CFLAGS) -c -o $@ $(SRC_DIR)/tileset_assets.cpp

$(OBJ_DIR)/main_lab.o: $(SRC_DIR)/main_lab.cpp $(INC_DIR)/realm.h $(INC_DIR)/gfx_renderer.h $(INC_DIR)/env_config.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) $(SDL_CFLAGS) -c -o $@ $(SRC_DIR)/main_lab.cpp

$(OBJ_DIR)/main_lab_init.o: $(SRC_DIR)/main.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/main.cpp

$(OBJ_DIR)/env_config_lab.o: $(SRC_DIR)/env_config.cpp $(INC_DIR)/env_config.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/env_config.cpp

$(OBJ_DIR)/entity_animation_lab.o: $(SRC_DIR)/entity_animation.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/entity_animation.cpp

$(OBJ_DIR)/globals_lab.o: $(SRC_DIR)/globals.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/globals.cpp

$(OBJ_DIR)/mapgen_lab.o: $(SRC_DIR)/mapgen.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/mapgen.cpp

$(OBJ_DIR)/entity_lab.o: $(SRC_DIR)/entity.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/entity.cpp

$(OBJ_DIR)/orders_lab.o: $(SRC_DIR)/orders.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/orders.cpp

$(OBJ_DIR)/simulation_lab.o: $(SRC_DIR)/simulation.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/simulation.cpp

$(OBJ_DIR)/ai_lab.o: $(SRC_DIR)/ai.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/ai.cpp

$(OBJ_DIR)/input_lab.o: $(SRC_DIR)/input.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/input.cpp

$(OBJ_DIR)/display_lab.o: $(SRC_DIR)/display.cpp $(INC_DIR)/realm.h $(INC_DIR)/display.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/display.cpp

$(OBJ_DIR)/tileset_assets_lab.o: $(SRC_DIR)/tileset_assets.cpp $(INC_DIR)/tileset_assets.h $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) $(SDL_CFLAGS) $(PNG_CFLAGS) -c -o $@ $(SRC_DIR)/tileset_assets.cpp

$(OBJ_DIR)/gfx_renderer_lab.o: $(SRC_DIR)/gfx_renderer.cpp $(INC_DIR)/realm.h $(INC_DIR)/gfx_renderer.h $(INC_DIR)/entity_animation.h $(INC_DIR)/tileset_assets.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) $(SDL_CFLAGS) -c -o $@ $(SRC_DIR)/gfx_renderer.cpp

$(OBJ_DIR)/tests/realm_headless_tests.o: tests/realm_headless_tests.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)/tests
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ tests/realm_headless_tests.cpp

$(OBJ_DIR)/main_headless.o: $(SRC_DIR)/main.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/main.cpp

$(OBJ_DIR)/entity_animation_headless.o: $(SRC_DIR)/entity_animation.cpp $(INC_DIR)/realm.h $(INC_DIR)/entity_animation.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/entity_animation.cpp

$(OBJ_DIR)/globals_headless.o: $(SRC_DIR)/globals.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/globals.cpp

$(OBJ_DIR)/mapgen_headless.o: $(SRC_DIR)/mapgen.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/mapgen.cpp

$(OBJ_DIR)/entity_headless.o: $(SRC_DIR)/entity.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/entity.cpp

$(OBJ_DIR)/orders_headless.o: $(SRC_DIR)/orders.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/orders.cpp

$(OBJ_DIR)/simulation_headless.o: $(SRC_DIR)/simulation.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/simulation.cpp

$(OBJ_DIR)/ai_headless.o: $(SRC_DIR)/ai.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/ai.cpp

$(OBJ_DIR)/input_headless.o: $(SRC_DIR)/input.cpp $(INC_DIR)/realm.h | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -DUSE_SDL_RENDERER -I$(INC_DIR) -c -o $@ $(SRC_DIR)/input.cpp

test: $(TEST_TARGET)
	./$(TEST_TARGET)

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

$(OBJ_DIR) $(OBJ_DIR)/tests $(BIN_DIR):
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
		if [ -d "$(BUILD_DIR)" ] && ! find "$(BUILD_DIR)" -mindepth 1 ! \( -name web-server.log -o -name web-server.err \) -print -quit | grep -q .; then \
			echo "warning: preserving locked $(BUILD_DIR)/web-server logs during Windows clean"; \
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
