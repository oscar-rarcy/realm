CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
PKG_CONFIG ?= pkg-config

TARGET := realm
OBJS := main.o globals.o mapgen.o entity.o combat.o world.o ai.o render.o ui.o input.o display.o save.o commands.o

# Use wide ncurses for UTF-8/Unicode glyph output. Falls back to -lncursesw
# if pkg-config is unavailable.
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null)
NCURSES_LIBS   := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo -lncursesw)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(NCURSES_LIBS)

%.o: %.cpp realm.h
	$(CXX) $(CXXFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

# --- Standalone GUI build: same sources, SDL shim backend ---
GUI_TARGET := realm-gui
GUI_OBJS   := $(addprefix gui/,$(OBJS)) gui/sdl_shim.o
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf 2>/dev/null)

gui-build: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(GUI_OBJS) $(SDL_LIBS)

gui/%.o: %.cpp realm.h sdl_shim.h | gui
	$(CXX) $(CXXFLAGS) -DUSE_SDL_SHIM $(SDL_CFLAGS) -c -o $@ $<

gui:
	mkdir -p gui

clean:
	rm -f $(OBJS) $(TARGET) $(GUI_OBJS) $(GUI_TARGET)

.PHONY: all clean gui-build
