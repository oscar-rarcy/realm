#pragma once
// ============================================================
// SDL TERMINAL SHIM — implements the exact ncurses API surface that
// render.cpp / ui.cpp / input.cpp / main.cpp use, over SDL2 + SDL_ttf.
// The game compiles unchanged against either backend:
//   - default build: real ncurses, runs in the terminal
//   - -DUSE_SDL_SHIM: this shim, runs as a standalone window with
//     vector-font glyphs (crisp at any scale) and native mouse events.
//
// Only what the game actually calls is implemented. If a new ncurses
// call is added to the game, add it here too (the linker will tell you).
// ============================================================
#include <cstdarg>

// ----- core types -----
typedef unsigned int  chtype;
typedef unsigned long mmask_t;
struct WINDOW;            // opaque; only ever used as `stdscr` token
extern WINDOW* stdscr;

#define ERR (-1)
#define OK  (0)
#define COLOR_PAIRS 512   // shim pair table size; > CP_EMOJI_MAX (255)
#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

// ----- attributes -----
// Pair lives in bits 8..19; flag bits sit above. mvaddch only ever
// receives bare characters in this codebase (attrs go via attron).
#define COLOR_PAIR(n) ((unsigned)(n) << 8)
#define A_NORMAL  0u
#define A_BOLD    (1u << 20)
#define A_DIM     (1u << 21)
#define A_REVERSE (1u << 22)
// Decorative face (blackletter) for titles/headers. The SDL build renders
// these cells in a display font (Luminari); ncurses maps it to bold.
#define A_TITLE   (1u << 23)

// ----- colors -----
#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

// ----- line-drawing glyphs (ncurses provides these natively; the shim maps
// them to the Unicode codepoints the TTF renderer draws directly) -----
#define ACS_CKBOARD  ((chtype)0x2592)  // ▒ medium shade
#define ACS_BOARD    ((chtype)0x2591)  // ░ light shade
#define ACS_BLOCK    ((chtype)0x2588)  // █ full block
#define ACS_HLINE    ((chtype)0x2500)  // ─
#define ACS_VLINE    ((chtype)0x2502)  // │
#define ACS_ULCORNER ((chtype)0x250C)  // ┌
#define ACS_URCORNER ((chtype)0x2510)  // ┐
#define ACS_LLCORNER ((chtype)0x2514)  // └
#define ACS_LRCORNER ((chtype)0x2518)  // ┘
#define ACS_PLUS     ((chtype)0x253C)  // ┼
#define ACS_LTEE     ((chtype)0x251C)  // ├
#define ACS_RTEE     ((chtype)0x2524)  // ┤
#define ACS_TTEE     ((chtype)0x252C)  // ┬
#define ACS_BTEE     ((chtype)0x2534)  // ┴
#define ACS_BULLET   ((chtype)0x00B7)  // · cobble/waypoint dot
#define ACS_DIAMOND  ((chtype)0x25C6)  // ◆

// ----- keys (values are arbitrary but match ncurses where easy) -----
#define KEY_DOWN   258
#define KEY_UP     259
#define KEY_LEFT   260
#define KEY_RIGHT  261
#define KEY_HOME   262
#define KEY_F(n)   (264 + (n))
#define KEY_ENTER  343
#define KEY_SF     336   // shift+down
#define KEY_SR     337   // shift+up
#define KEY_NPAGE  338
#define KEY_PPAGE  339
#define KEY_END    360
#define KEY_SLEFT  393
#define KEY_SRIGHT 402
#define KEY_MOUSE  409

// ----- mouse -----
#define BUTTON1_RELEASED        0x00000001ul
#define BUTTON1_PRESSED         0x00000002ul
#define BUTTON1_CLICKED         0x00000004ul
#define BUTTON1_DOUBLE_CLICKED  0x00000008ul
#define BUTTON3_RELEASED        0x00010000ul
#define BUTTON3_PRESSED         0x00020000ul
#define BUTTON3_CLICKED         0x00040000ul
#define BUTTON_SHIFT            0x02000000ul
#define REPORT_MOUSE_POSITION   0x08000000ul
#define ALL_MOUSE_EVENTS        0x01fffffful

typedef struct { short id; int x, y, z; mmask_t bstate; } MEVENT;

// ----- functions -----
WINDOW* initscr();
int  endwin();
int  cbreak();
int  noecho();
int  keypad(WINDOW*, bool);
int  curs_set(int);
int  start_color();
int  use_default_colors();
int  init_pair(short pair, short fg, short bg);
mmask_t mousemask(mmask_t newmask, mmask_t* oldmask);
void timeout(int ms);
int  getch();
int  getmouse(MEVENT* event);
int  erase();
int  refresh();
int  attron(unsigned attrs);
int  attroff(unsigned attrs);
int  mvaddch(int y, int x, chtype ch);
int  mvaddstr(int y, int x, const char* str);
int  mvhline(int y, int x, chtype ch, int n);
int  mvprintw(int y, int x, const char* fmt, ...);

void shimGetMaxYX(int& y, int& x);
#define getmaxyx(win, y, x) shimGetMaxYX((y), (x))

// Queue a translucent overlay rect, in grid-cell coords (inclusive bounds),
// drawn alpha-blended over the whole grid at the next refresh(). Used for the
// drag-selection box. r/g/b 0-255; fillA = interior alpha, borderA = edge alpha.
void shimOverlayRect(int gx0, int gy0, int gx1, int gy1,
                     int r, int g, int b, int fillA, int borderA);
