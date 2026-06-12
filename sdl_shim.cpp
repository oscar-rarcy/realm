// ============================================================
// SDL TERMINAL SHIM — see sdl_shim.h for the contract.
//
// Model: a cols×rows grid of cells (codepoint + pair + flags), exactly
// like a terminal. The game writes cells through the ncurses-style calls;
// refresh() draws the whole grid with SDL: a background rect per cell and
// a glyph texture rendered from a TTF at the window's device pixel scale —
// so glyphs are rasterized from vector outlines at native resolution and
// stay crisp on any display, including retina.
//
// Coordinate spaces: SDL mouse events arrive in window points; all
// rendering happens in device pixels. pointToCell() converts with a
// live-queried window/output ratio — never cached, so DPI changes,
// display moves, and resizes can't desynchronize clicks from drawing.
// The OS cursor stays VISIBLE: with macOS pointer acceleration the user
// has no idea where a hidden pointer is, so the game's cell cursor
// looked like it moved on its own. Both cursors share pointToCell, so
// the cell cursor always sits exactly under the OS pointer tip.
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
std::string   fontPath;
int           fontPt = 15;          // point size; mouse wheel zooms this
bool          mouseDebug = false;

int cellW = 10, cellH = 20;         // device pixels
int cols = 80, rows = 24;

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
// indexes; this is the same table every terminal emulator ships, so the
// palette tuned in the terminal carries over to the GUI exactly.
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

void clearGlyphCache() {
    for (auto& kv : glyphCache) if (kv.second) SDL_DestroyTexture(kv.second);
    glyphCache.clear();
}

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

// Live window-point -> device-pixel ratio. Queried per call, never cached.
float pixelScale() {
    int ww = 0, pw = 0, h;
    SDL_GetWindowSize(win, &ww, &h);
    SDL_GetRendererOutputSize(ren, &pw, &h);
    return (ww > 0 && pw > 0) ? (float)pw / ww : 1.0f;
}

void pointToCell(int px, int py, int& cx, int& cy) {
    float s = pixelScale();
    cx = (int)(px * s) / cellW;
    cy = (int)(py * s) / cellH;
}

void openFonts() {
    if (font)     { TTF_CloseFont(font); font = nullptr; }
    if (fontBold) { TTF_CloseFont(fontBold); fontBold = nullptr; }
    clearGlyphCache();
    int px = (int)(fontPt * pixelScale());
    font     = TTF_OpenFont(fontPath.c_str(), px);
    fontBold = TTF_OpenFont(fontPath.c_str(), px);
    if (!font) { fprintf(stderr, "TTF_OpenFont(%s) failed: %s\n", fontPath.c_str(), TTF_GetError()); exit(1); }
    if (fontBold) TTF_SetFontStyle(fontBold, TTF_STYLE_BOLD);
    // Cell advance from '0': digits share one width even in proportional
    // faces like Helvetica, giving a stable grid; glyphs are centered per
    // cell so wider letters just spill symmetrically.
    int adv = 0, minx, maxx, miny, maxy;
    if (TTF_GlyphMetrics32(font, '0', &minx, &maxx, &miny, &maxy, &adv) != 0 || adv <= 0)
        TTF_SizeUTF8(font, "0", &adv, nullptr);
    cellW = adv > 0 ? adv : 10;
    cellH = TTF_FontHeight(font);
}

void recomputeGrid() {
    int pw, ph;
    SDL_GetRendererOutputSize(ren, &pw, &ph);
    cols = pw / cellW > 20 ? pw / cellW : 20;
    rows = ph / cellH > 10 ? ph / cellH : 10;
    grid.assign((size_t)cols * rows, Cell{});
}

void zoomFont(int dir) {
    int np = fontPt + dir;
    if (np < 8) np = 8;
    if (np > 32) np = 32;
    if (np == fontPt) return;
    fontPt = np;
    openFonts();
    recomputeGrid();
}

void pushKey(int k) { keyQ.push_back(k); }

void pushMouse(mmask_t bstate, int px, int py) {
    MEVENT me{};
    pointToCell(px, py, me.x, me.y);
    me.bstate = bstate;
    if (SDL_GetModState() & KMOD_SHIFT) me.bstate |= BUTTON_SHIFT;
    mouseQ.push_back(me);
    keyQ.push_back(KEY_MOUSE);
    if (mouseDebug && (bstate & ~REPORT_MOUSE_POSITION)) {
        int ww, wh, pw, ph;
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetRendererOutputSize(ren, &pw, &ph);
        fprintf(stderr, "[mouse] pt=(%d,%d) win=%dx%d out=%dx%d cell=%dx%d -> (%d,%d)\n",
                px, py, ww, wh, pw, ph, cellW, cellH, me.x, me.y);
    }
}

// AoE-style continuous edge scroll: the game's edge logic is driven by
// motion events, which stop when the mouse parks. While the pointer sits
// in the edge zone, feed it synthetic position reports so scrolling
// continues. Never fires elsewhere, so keyboard cursor control is safe.
void maybeSynthEdgeMotion() {
    static Uint32 lastSynth = 0;
    Uint32 now = SDL_GetTicks();
    if (now - lastSynth < 60) return;
    if (SDL_GetMouseFocus() != win) return;
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
    float s = pixelScale();
    float edgeX = 2.0f * cellW / s, edgeY = 2.0f * cellH / s;
    if (mx < edgeX || mx > ww - edgeX || my < edgeY || my > wh - edgeY) {
        lastSynth = now;
        pushMouse(REPORT_MOUSE_POSITION, mx, my);
    }
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
            // Ctrl/Cmd + digit = assign control group (RTS standard).
            // Routed through the game's existing G-then-digit flow.
            if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_9
                && (e.key.keysym.mod & (KMOD_CTRL | KMOD_GUI))) {
                pushKey('G');
                pushKey('1' + (e.key.keysym.sym - SDLK_1));
                break;
            }
            int k = translateKey(e.key.keysym);
            if (k >= 0) pushKey(k);
            break;
        }
        case SDL_MOUSEMOTION: {
            int cx, cy;
            pointToCell(e.motion.x, e.motion.y, cx, cy);
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
        case SDL_MOUSEWHEEL:
            // Zoom only with Cmd/Ctrl held. Trackpads stream wheel events
            // (two-finger scroll, momentum); bare-wheel zoom resized the
            // whole grid every flick.
            if (SDL_GetModState() & (KMOD_GUI | KMOD_CTRL))
                zoomFont(e.wheel.y > 0 ? 1 : e.wheel.y < 0 ? -1 : 0);
            break;
        case SDL_WINDOWEVENT:
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                openFonts();      // DPI may have changed (display move)
                recomputeGrid();
            }
            break;
    }
}

// ---- optional instrumentation (env vars; zero cost when unset) ----
// REALM_DUMP_GRID=/path  : write the cell grid every refresh (debug/CI)
// REALM_DUMP_BMP=/path   : save the rendered framebuffer as BMP every ~2s.
//                          Reads back our own renderer, so it works without
//                          macOS Screen Recording permission.
// REALM_SELFTEST=1       : scripted input — start a duel, then perform a
//                          slow box-select drag and exit. Combined with
//                          the grid dump this proves the whole pipeline
//                          (events -> cells -> drag box render) headlessly.
const char* dumpPath = nullptr;
const char* dumpBmp  = nullptr;
bool selfTest = false;

void maybeDumpFrame() {
    if (!dumpBmp) return;
    static Uint32 last = 0; static int n = 0;
    Uint32 now = SDL_GetTicks();
    if (now - last < 2000) return;
    last = now;
    int pw, ph;
    SDL_GetRendererOutputSize(ren, &pw, &ph);
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, pw, ph, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) {
        char path[256];
        snprintf(path, sizeof path, "%s.%d.bmp", dumpBmp, ++n);
        SDL_SaveBMP(s, path);
    }
    SDL_FreeSurface(s);
}

void injectMotion(int x, int y) {
    SDL_Event e{}; e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y; handleEvent(e);
}
void injectButton(bool down, int x, int y) {
    SDL_Event e{}; e.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    e.button.button = SDL_BUTTON_LEFT; e.button.clicks = 1;
    e.button.x = x; e.button.y = y; handleEvent(e);
}
void injectText(const char* s) {
    SDL_Event e{}; e.type = SDL_TEXTINPUT;
    strncpy(e.text.text, s, sizeof(e.text.text) - 1); handleEvent(e);
}
void injectKey(SDL_Keycode k) {
    SDL_Event e{}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; handleEvent(e);
}

void selfTestStep() {
    static int phase = 0;
    static Uint32 t0 = SDL_GetTicks();
    Uint32 t = SDL_GetTicks() - t0;
    switch (phase) {
        case 0: if (t > 1200) { injectText("1"); injectKey(SDLK_RETURN); phase++; } break;
        case 1: if (t > 3500) { injectMotion(400, 300); phase++; } break;
        case 2: if (t > 3600) { injectButton(true, 400, 300); phase++; } break;
        case 3: if (t > 3700 && t < 5200) {
                    int step = (int)((t - 3700) / 100);
                    injectMotion(400 + step * 20, 300 + step * 12);
                } else if (t >= 5200) phase++;
                break;  // keep holding: drag box should be on screen now
        case 4: if (t > 6800) { injectButton(false, 700, 480); phase++; } break;
        case 5: // Hover the bottom window edge (over the hotkey bar): must
                // edge-scroll calmly (time-throttled) and must NOT move the
                // cell cursor — that area is UI, not map. Jitter between two
                // cells so motion events keep flowing like a real trackpad.
                if (t > 7600 && t < 8000) {
                    static bool flip = false; flip = !flip;
                    injectMotion(flip ? 720 : 740, 855);
                } else if (t >= 8000) phase++;
                break;
        case 6: if (t > 8600) { fprintf(stderr, "[selftest] done\n"); exit(0); } break;
    }
}

void pumpEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);
    maybeSynthEdgeMotion();
    if (selfTest) selfTestStep();
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
    // Monospace only: the renderer is a strict cell grid, and the grid pitch
    // comes from one glyph's advance — a proportional face (Helvetica) makes
    // wide glyphs collide and narrow ones float, which reads as bad kerning.
    // Helvetica stays solely as a nothing-else-exists fallback.
    static const char* candidates[] = {
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/SFNSMono.ttf",
        "/System/Library/Fonts/Monaco.ttf",
        "/Library/Fonts/Andale Mono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "C:\\Windows\\Fonts\\consola.ttf",
        "C:\\Windows\\Fonts\\lucon.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
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
    mouseDebug = getenv("REALM_MOUSE_DEBUG") != nullptr;
    dumpPath   = getenv("REALM_DUMP_GRID");
    dumpBmp    = getenv("REALM_DUMP_BMP");
    selfTest   = getenv("REALM_SELFTEST") != nullptr;
    win = SDL_CreateWindow("REALM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           1440, 860,
                           SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!win || !ren) {
        fprintf(stderr, "SDL window/renderer failed: %s\n", SDL_GetError());
        exit(1);
    }

    const char* fp = findFont();
    if (!fp) { fprintf(stderr, "No usable font found; set REALM_FONT=/path/to/font.ttf\n"); exit(1); }
    fontPath = fp;
    if (const char* e = getenv("REALM_FONT_PT")) { int v = atoi(e); if (v >= 8 && v <= 32) fontPt = v; }
    openFonts();
    recomputeGrid();

    // Keep the OS cursor visible. It was hidden at first ("the cell cursor
    // is the pointer"), but with trackpad acceleration an invisible pointer
    // means the player can't tell why the cell cursor moves the way it does.
    // Visible pointer + cell cursor snapped to the cell under its tip reads
    // as one coherent cursor.
    SDL_ShowCursor(SDL_ENABLE);
    SDL_StartTextInput();
    stdscr = (WINDOW*)(void*)&grid; // non-null token
    return stdscr;
}

int endwin() {
    clearGlyphCache();
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
                    SDL_Rect dst = {x * cellW + (cellW - tw) / 2, y * cellH + (cellH - th) / 2, tw, th};
                    SDL_RenderCopy(ren, t, nullptr, &dst);
                }
            }
        }
    }
    maybeDumpFrame();   // before present: read back the frame we just composed
    SDL_RenderPresent(ren);
    if (dumpPath) {
        static int frame = 0;
        FILE* f = fopen(dumpPath, "w");
        if (f) {
            fprintf(f, "frame %d cols %d rows %d\n", ++frame, cols, rows);
            for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
                Cell& c = at(y, x);
                if (c.cp != ' ' || c.pair != 0)
                    fprintf(f, "%d %d cp=%u pair=%d fl=%u\n", y, x, c.cp, c.pair, c.flags);
            }
            fclose(f);
        }
    }
    return OK;
}

void shimGetMaxYX(int& y, int& x) { y = rows; x = cols; }
