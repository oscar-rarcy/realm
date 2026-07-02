CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
PKG_CONFIG ?= pkg-config

TARGET := realm
# Object basenames, shared by both backends. Terminal objects build into
# obj/, the SDL-shim objects into gui/ — so the repo root stays clean.
OBJ_NAMES := main.o globals.o mapgen.o entity.o pathfind.o combat.o world.o ai.o render.o ui.o input.o display.o save.o commands.o net.o
OBJS := $(addprefix obj/,$(OBJ_NAMES))

# Use wide ncurses for UTF-8/Unicode glyph output. Falls back to -lncursesw
# if pkg-config is unavailable.
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null)
NCURSES_LIBS   := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo -lncursesw)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(NCURSES_LIBS)

obj/%.o: %.cpp realm.h | obj
	$(CXX) $(CXXFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

obj:
	mkdir -p obj

# --- Standalone GUI build: same sources, SDL shim backend ---
GUI_TARGET := realm-gui
GUI_OBJS   := $(addprefix gui/,$(OBJ_NAMES)) gui/sdl_shim.o
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2 SDL2_ttf SDL2_mixer 2>/dev/null)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2 SDL2_ttf SDL2_mixer 2>/dev/null)

gui-build: $(GUI_TARGET)

$(GUI_TARGET): $(GUI_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(GUI_OBJS) $(SDL_LIBS)

gui/%.o: %.cpp realm.h sdl_shim.h | gui
	$(CXX) $(CXXFLAGS) -DUSE_SDL_SHIM $(SDL_CFLAGS) -c -o $@ $<

gui:
	mkdir -p gui

# Self-contained Realm.app (Apple Silicon) — bundles the SDL dylibs so the app
# runs on a Mac without Homebrew. See make-app.sh.
app:
	./make-app.sh

# Friend-ready zip on the Desktop: Realm/ = Realm.app + the Gatekeeper /
# multiplayer READ ME (share/READ ME FIRST.txt is the source of truth).
share: app
	rm -rf /tmp/RealmShare && mkdir -p /tmp/RealmShare/Realm
	ditto Realm.app /tmp/RealmShare/Realm/Realm.app
	cp "share/READ ME FIRST.txt" "/tmp/RealmShare/Realm/READ ME FIRST.txt"
	ditto -c -k --keepParent /tmp/RealmShare/Realm ~/Desktop/Realm-mac.zip
	@echo "==> ~/Desktop/Realm-mac.zip refreshed."

clean:
	rm -f $(OBJS) $(TARGET) $(GUI_OBJS) $(GUI_TARGET)
	rm -rf obj gui Realm.app Realm.zip

.PHONY: all clean gui-build app share
