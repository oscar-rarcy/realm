#pragma once

#if defined(USE_NCURSES_RENDERER)
#include <ncurses.h>
#else
constexpr int ERR = -1;
constexpr int OK = 0;
constexpr int KEY_DOWN = 1001;
constexpr int KEY_UP = 1002;
constexpr int KEY_LEFT = 1003;
constexpr int KEY_RIGHT = 1004;
constexpr int KEY_HOME = 1005;
constexpr int KEY_END = 1006;
constexpr int KEY_NPAGE = 1007;
constexpr int KEY_PPAGE = 1008;
constexpr int KEY_ENTER = 1009;
constexpr int KEY_MOUSE = 1010;
constexpr int KEY_SR = 1011;
constexpr int KEY_SF = 1012;
constexpr int KEY_SLEFT = 1013;
constexpr int KEY_SRIGHT = 1014;
constexpr int KEY_F0 = 1100;
constexpr int KEY_F(int n) { return KEY_F0 + n; }

struct MEVENT {
    int x = 0;
    int y = 0;
    unsigned long bstate = 0;
};

constexpr unsigned long BUTTON1_PRESSED = 1ul << 0;
constexpr unsigned long BUTTON1_RELEASED = 1ul << 1;
constexpr unsigned long BUTTON1_CLICKED = 1ul << 2;
constexpr unsigned long BUTTON1_DOUBLE_CLICKED = 1ul << 3;
constexpr unsigned long BUTTON3_PRESSED = 1ul << 4;
constexpr unsigned long BUTTON3_CLICKED = 1ul << 5;

inline int getmouse(MEVENT*) { return ERR; }
inline void endwin() {}
inline void* stdscr = nullptr;
inline void getmaxyx(void*, int& y, int& x) { y = 0; x = 0; }
#endif
