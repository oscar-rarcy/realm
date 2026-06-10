CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -finput-charset=UTF-8 -fexec-charset=UTF-8
PKG_CONFIG ?= pkg-config

TARGET := realm
OBJS := main.o globals.o mapgen.o entity.o combat.o world.o ai.o render.o ui.o input.o display.o save.o

# Use wide ncurses for UTF-8/Unicode glyph output. Falls back to -lncursesw
# if pkg-config is unavailable.
NCURSES_CFLAGS := $(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null)
NCURSES_LIBS   := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null || echo -lncursesw)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(NCURSES_LIBS)

%.o: %.cpp realm.h
	$(CXX) $(CXXFLAGS) $(NCURSES_CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
