// ============================================================
// SDL TERMINAL SHIM — see sdl_shim.h for the contract.
//
// Model: a cols×rows grid of cells (codepoint + pair + flags), exactly
// like a terminal. The game writes cells through the ncurses-style calls;
// refresh() draws the whole grid with SDL: a background rect per cell and
// a glyph texture rendered from a monospace TTF at the window's device
// pixel scale — so glyphs are rasterized from vector outlines at native
// resolution and stay crisp on any display, including retina.
// ============================================================
#include "sdl_shim.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

WINDOW* stdscr = nullptr;  // token only; never dereferenced

namespace {

struct Cell { unsigned cp = ' '; short pair = 0; unsigned flags = 0; };
struct Pair { short fg = -1, bg = -1; };

SDL_Window*   win = nullptr;
SDL_Renderer* ren = nullptr;
TTF_Font*     font = nullptr;
TTF_Font*     fontBold = nullptr;

int cellW = 10, cellH = 20;      // device pixels
int cols = 80, rows = 24;
float scaleX = 1.0f, scaleY = 1.0f;  // window points -> device pixels

std::vector<Cell> grid;
Pair  pairs[512];
short curPair = 0;
unsigned curFlags = 0;
int   timeoutMs = 0;

std::deque<int>    keyQ;
std::deque<MEVENT> mouseQ;
int lastCellX = -1, lastCellY = -1;
bool leftHeld = false;

Cell& at(int y, int x) { return grid[(size_t)y * cols + x]; }
bool inGrid(int y, int x) { return y >= 0 && y < rows && x >= 0 && x < cols; }

// xterm-256 palette -> RGB. The game's initColors() speaks 256-color
// indexes; this is the same table every terminal emulator ships.
void color256(int v, Uint8& r, Uint8& g, Uint8& b) {
    static const Uint8 base[16][3] = {
        {0,0,0},{205,49,49},{13,188,121},{229,229,16},{36,114,200},{188,63,188},{17,168,205},{229,229,229},
        {102,102,102},{241,76,76},{35,209,139},{245,245,67},{59,142,234},{214,112,214},{41,184,219},{255,255,255}};
    if (v < 0) { r = 200; g = 200; b = 200; return; }
    if (v < 16) { r = base[v][0]; g = base[v][1]; b = base[v][2]; return; }
    if (v < 232) {
        int c = v - 16;
        static const Uint8 lv[6] = {0, 95, 135, 175, 215, 255};
        r = lv[c / 36]; g = lv[(c / 6) % 6]; b = lv[c % 6]; return;
    }
    Uint8 gray = (Uint8)(8 + 10 * (v - 232));
    r = g = b = gray;
}

void pairColors(short pair, unsigned flags, SDL_Color& fg, SDL_Color& bg) {
    Pair p = (pair >= 0 && pair < 512) ? pairs[pair] : Pair{};
    if (p.fg < 0) fg = {200, 200, 200, 255}; else color256(p.fg, fg.r, fg.g, fg.b), fg.a = 255;
    if (p.bg < 0) bg = {14, 14, 16, 255};    else color256(p.bg, bg.r, bg.g, bg.b), bg.a = 255;
    if (flags & A_REVERSE) std::swap(fg, bg);
    if (flags & A_DIM) { fg.r /= 2; fg.g /= 2; fg.b /= 2; }
}

// Glyph atlas: codepoint+bold -> texture rendered white, tinted at draw
// time via color mod. Bounded in practice (ASCII + a few dozen unicode).
std::unordered_map<unsigned long long, SDL_Texture*> glyphCache;

SDL_Texture* glyphTex(unsigned cp, bool bold) {
    unsigned long long key = ((unsigned long long)bold << 32) | cp;
    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) return it->second;
    TTF_Font* f = bold && fontBold ? fontBold : font;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface* s = TTF_RenderGlyph32_Blended(f, cp, white);
    if (!s && cp > 127) s = TTF_RenderGlyph32_Blended(f, '?', white);
    SDL_Texture* t = s ? SDL_CreateTextureFromSurface(ren, s) : nullptr;
    if (s) SDL_FreeSurface(s);
    glyphCache[key] = t;
    return t;
}

void recomputeGrid() {
    int pw, ph, ww, wh;
    SDL_GetRendererOutputSize(ren, &pw, &ph);
    SDL_GetWindowSize(win, &ww, &wh);
    scaleX = ww > 0 ? (float)pw / ww : 1.0f;
    scaleY = wh > 0 ? (float)ph / wh : 1.0f;
    cols = pw / cellW > 20 ? pw / cellW : 20;
    rows = ph / cellH > 10 ? ph / cellH : 10;
    grid.assign((size_t)cols * rows, Cell{});
}

void pushKey(int k) { keyQ.push_back(k); }
void pushMouse(mmask_t bstate, int px, int py) {
    MEVENT me{};
    me.x = (int)(px * scaleX) / cellW;
    me.y = (int)(py * scaleY) / cellH;
    me.bstate = bstate;
    if (SDL_GetModState() & KMOD_SHIFT) me.bstate |= BUTTON_SHIFT;
    mouseQ.push_back(me);
    keyQ.push_back(KEY_MOUSE);
}

int translateKey(const SDL_Keysym& k) {
    bool shift = (k.mod & KMOD_SHIFT) != 0;
    switch (k.sym) {
        case SDLK_UP:       return shift ? KEY_SR : KEY_UP;
        case SDLK_DOWN:     return shift ? KEY_SF : KEY_DOWN;
        case SDLK_LEFT:     return shift ? KEY_SLEFT : KEY_LEFT;
        case SDLK_RIGHT:    return shift ? KEY_SRIGHT : KEY_RIGHT;
        case SDLK_PAGEUP:   return KEY_PPAGE;
        case SDLK_PAGEDOWN: return KEY_NPAGE;
        case SDLK_HOME:     return KEY_HOME;
        case SDLK_END:      return KEY_END;
        case SDLK_RETURN: case SDLK_RETURN2: case SDLK_KP_ENTER: return '\n';
        case SDLK_ESCAPE:   return 27;
        case SDLK_TAB:      return '\t';
        case SDLK_F5:  return KEY_F(5);
        case SDLK_F6:  return KEY_F(6);
        case SDLK_F7:  return KEY_F(7);
        case SDLK_F8:  return KEY_F(8);
        case SDLK_F9:  return KEY_F(9);
        case SDLK_F10: return KEY_F(10);
        case SDLK_F11: return KEY_F(11);
        case SDLK_F12: return KEY_F(12);
        default: return -1;
    }
}

void handleEvent(const SDL_Event& e) {
    switch (e.type) {
        case SDL_QUIT:
            endwin();
            exit(0);
        case SDL_TEXTINPUT:
            // Printable input (shift-correct). Specials come via KEYDOWN.
            for (const char* c = e.text.text; *c; c++)
                if ((unsigned char)*c < 128) pushKey(*c);
            break;
        case SDL_KEYDOWN: {
            int k = translateKey(e.key.keysym);
            if (k >= 0) pushKey(k);
            break;
        }
        case SDL_MOUSEMOTION: {
            int cx = (int)(e.motion.x * scaleX) / cellW;
            int cy = (int)(e.motion.y * scaleY) / cellH;
            // Hover events only when the cell changes (or mid-drag) so the
            // queue doesn't flood at device report rate.
            if (cx != lastCellX || cy != lastCellY || leftHeld) {
                lastCellX = cx; lastCellY = cy;
                pushMouse(REPORT_MOUSE_POSITION, e.motion.x, e.motion.y);
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
            if (e.button.button == SDL_BUTTON_LEFT) {
                leftHeld = true;
                pushMouse(e.button.clicks >= 2 ? BUTTON1_DOUBLE_CLICKED
                                               : BUTTON1_PRESSED,
                          e.button.x, e.button.y);
            } else if (e.button.button == SDL_BUTTON_RIGHT) {
                pushMouse(BUTTON3_PRESSED, e.button.x, e.button.y);
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if (e.button.button == SDL_BUTTON_LEFT) {
                leftHeld = false;
                pushMouse(BUTTON1_RELEASED, e.button.x, e.button.y);
            }
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) recomputeGrid();
            break;
    }
}

void pumpEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);
}

// Decode one UTF-8 codepoint; advances p.
unsigned utf8Next(const char*& p) {
    unsigned char c = (unsigned char)*p;
    if (c < 0x80) { p += 1; return c; }
    if ((c >> 5) == 0x6 && p[1]) { unsigned cp = ((c & 0x1F) << 6) | (p[1] & 0x3F); p += 2; return cp; }
    if ((c >> 4) == 0xE && p[1] && p[2]) {
        unsigned cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3; return cp;
    }
    if ((c >> 3) == 0x1E && p[1] && p[2] && p[3]) {
        unsigned cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        p += 4; return cp;
    }
    p += 1; return '?';
}

void putCells(int y, int x, const char* s) {
    if (y < 0 || y >= rows) return;
    const char* p = s;
    while (*p && x < cols) {
        unsigned cp = utf8Next(p);
        if (x >= 0) { Cell& c = at(y, x); c.cp = cp; c.pair = curPair; c.flags = curFlags; }
        x++;
    }
}

const char* findFont() {
    const char* env = getenv("REALM_FONT");
    if (env && *env) return env;
    static const char* candidates[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.ttf",
        "/Library/Fonts/Andale Mono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
        nullptr
    };
    for (int i = 0; candidates[i]; i++) {
        FILE* f = fopen(candidates[i], "rb");
        if (f) { fclose(f); return candidates[i]; }
    }
    return nullptr;
}

} // namespace

// ============================================================
// API
// ============================================================
WINDOW* initscr() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }
    TTF_Init();
    win = SDL_CreateWindow("REALM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           1440, 860,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL window/renderer failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Device pixel scale, so the font rasterizes at native resolution.
    int pw, ww, ph, wh;
    SDL_GetRendererOutputSize(ren, &pw, &ph);
    SDL_GetWindowSize(win, &ww, &wh);
    float scale = ww > 0 ? (float)pw / ww : 1.0f;

    const char* fontPath = findFont();
    if (!fontPath) { fprintf(stderr, "No monospace font found; set REALM_FONT=/path/to/font.ttf\n"); exit(1); }
    int pt = 15;
    if (const char* fp = getenv("REALM_FONT_PT")) { int v = atoi(fp); if (v >= 6 && v <= 72) pt = v; }
    font     = TTF_OpenFont(fontPath, (int)(pt * scale));
    fontBold = TTF_OpenFont(fontPath, (int)(pt * scale));
    if (!font) { fprintf(stderr, "TTF_OpenFont failed: %s\n", TTF_GetError()); exit(1); }
    if (fontBold) TTF_SetFontStyle(fontBold, TTF_STYLE_BOLD);

    int adv = 0, minx, maxx, miny, maxy;
    if (TTF_GlyphMetrics32(font, 'M', &minx, &maxx, &miny, &maxy, &adv) != 0 || adv <= 0)
        TTF_SizeUTF8(font, "M", &adv, nullptr);
    cellW = adv > 0 ? adv : 10;
    cellH = TTF_FontHeight(font);

    recomputeGrid();
    SDL_StartTextInput();
    stdscr = (WINDOW*)(void*)&grid; // non-null token
    return stdscr;
}

int endwin() {
    for (auto& kv : glyphCache) if (kv.second) SDL_DestroyTexture(kv.second);
    glyphCache.clear();
    if (font)     { TTF_CloseFont(font); font = nullptr; }
    if (fontBold) { TTF_CloseFont(fontBold); fontBold = nullptr; }
    if (ren) { SDL_DestroyRenderer(ren); ren = nullptr; }
    if (win) { SDL_DestroyWindow(win); win = nullptr; }
    if (TTF_WasInit()) TTF_Quit();
    SDL_Quit();
    return OK;
}

int  cbreak()  { return OK; }
int  noecho()  { return OK; }
int  keypad(WINDOW*, bool) { return OK; }
int  curs_set(int) { return OK; }
int  start_color() { return OK; }
int  use_default_colors() { return OK; }
mmask_t mousemask(mmask_t, mmask_t*) { return ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION; }

int init_pair(short pair, short fg, short bg) {
    if (pair >= 0 && pair < 512) pairs[pair] = {fg, bg};
    return OK;
}

void timeout(int ms) { timeoutMs = ms; }

int getch() {
    pumpEvents();
    if (keyQ.empty() && timeoutMs > 0) {
        Uint32 deadline = SDL_GetTicks() + timeoutMs;
        while (keyQ.empty()) {
            Uint32 now = SDL_GetTicks();
            if (now >= deadline) break;
            SDL_Event e;
            if (SDL_WaitEventTimeout(&e, deadline - now)) {
                handleEvent(e);
                pumpEvents();
            }
        }
    }
    if (keyQ.empty()) return ERR;
    int k = keyQ.front(); keyQ.pop_front();
    return k;
}

int getmouse(MEVENT* event) {
    if (mouseQ.empty()) return ERR;
    *event = mouseQ.front(); mouseQ.pop_front();
    return OK;
}

int erase() {
    for (auto& c : grid) c = Cell{};
    return OK;
}

int attron(unsigned attrs)  {
    if (attrs >> 8 & 0xFFF) curPair = (short)(attrs >> 8 & 0xFFF);
    curFlags |= attrs & (A_BOLD | A_DIM | A_REVERSE);
    return OK;
}
int attroff(unsigned attrs) {
    if (attrs >> 8 & 0xFFF) curPair = 0;
    curFlags &= ~(attrs & (A_BOLD | A_DIM | A_REVERSE));
    return OK;
}

int mvaddch(int y, int x, chtype ch) {
    if (!inGrid(y, x)) return ERR;
    Cell& c = at(y, x);
    c.cp = (unsigned)ch; c.pair = curPair; c.flags = curFlags;
    return OK;
}

int mvaddstr(int y, int x, const char* str) { putCells(y, x, str); return OK; }

int mvhline(int y, int x, chtype ch, int n) {
    if (ch == 0) ch = ' ';
    for (int i = 0; i < n; i++) mvaddch(y, x + i, ch);
    return OK;
}

int mvprintw(int y, int x, const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    putCells(y, x, buf);
    return OK;
}

int refresh() {
    SDL_SetRenderDrawColor(ren, 14, 14, 16, 255);
    SDL_RenderClear(ren);
    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            Cell& c = at(y, x);
            SDL_Color fg, bg;
            pairColors(c.pair, c.flags, fg, bg);
            SDL_Rect r = {x * cellW, y * cellH, cellW, cellH};
            SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 255);
            SDL_RenderFillRect(ren, &r);
            if (c.cp != ' ' && c.cp != 0) {
                SDL_Texture* t = glyphTex(c.cp, (c.flags & A_BOLD) != 0);
                if (t) {
                    int tw, th;
                    SDL_QueryTexture(t, nullptr, nullptr, &tw, &th);
                    SDL_SetTextureColorMod(t, fg.r, fg.g, fg.b);
                    // Center horizontally; align baseline by top (cellH == font height).
                    SDL_Rect dst = {x * cellW + (cellW - tw) / 2, y * cellH + (cellH - th) / 2, tw, th};
                    SDL_RenderCopy(ren, t, nullptr, &dst);
                }
            }
        }
    }
    SDL_RenderPresent(ren);
    return OK;
}

void shimGetMaxYX(int& y, int& x) { y = rows; x = cols; }
