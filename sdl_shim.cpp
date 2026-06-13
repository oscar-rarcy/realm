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
#include <cmath>
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
TTF_Font*     fontTitle = nullptr;   // decorative blackletter for A_TITLE
std::string   fontPath;
std::string   fontPathBold;         // explicit bold face; empty = synthesize
int           fontPt = 15;          // point size; mouse wheel zooms this
int           mouseDebug = 0;     // 0=off, 1=buttons, 2=all events incl. hover

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

// Translucent overlay rects (e.g. the drag-selection box), in grid-cell
// coords. Buffered during the frame, alpha-blended over the composed cells
// in refresh(), then cleared. Terminal builds never see these.
struct OverlayRect { int gx0, gy0, gx1, gy1; Uint8 r, g, b, fillA, borderA; };
std::vector<OverlayRect> overlays;

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

SDL_Texture* glyphTex(unsigned cp, int face) {   // 0 normal, 1 bold, 2 title
    unsigned long long key = ((unsigned long long)face << 32) | cp;
    auto it = glyphCache.find(key);
    if (it != glyphCache.end()) return it->second;
    TTF_Font* f = (face == 2 && fontTitle) ? fontTitle
                : (face >= 1 && fontBold)  ? fontBold : font;
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
    if (font)      { TTF_CloseFont(font); font = nullptr; }
    if (fontBold)  { TTF_CloseFont(fontBold); fontBold = nullptr; }
    if (fontTitle) { TTF_CloseFont(fontTitle); fontTitle = nullptr; }
    clearGlyphCache();
    int px = (int)(fontPt * pixelScale());
    font = TTF_OpenFont(fontPath.c_str(), px);
    if (!font) { fprintf(stderr, "TTF_OpenFont(%s) failed: %s\n", fontPath.c_str(), TTF_GetError()); exit(1); }
    // A real bold face when the family ships one; synthesized bold otherwise.
    if (!fontPathBold.empty()) fontBold = TTF_OpenFont(fontPathBold.c_str(), px);
    if (!fontBold) {
        fontBold = TTF_OpenFont(fontPath.c_str(), px);
        if (fontBold) TTF_SetFontStyle(fontBold, TTF_STYLE_BOLD);
    }
    // Light hinting: at game sizes the default (normal) hinting thickens
    // stems on rasterization — part of the "fat/distorted" look. Light
    // keeps the outlines closer to the vector shapes.
    TTF_SetFontHinting(font, TTF_HINTING_LIGHT);
    if (fontBold) TTF_SetFontHinting(fontBold, TTF_HINTING_LIGHT);
    // Blackletter display face for A_TITLE cells (headers, flourishes).
    // Same px size as the grid font so it stays cell-aligned; bold fallback.
    fontTitle = TTF_OpenFont("/System/Library/Fonts/Supplemental/Luminari.ttf", px);
    if (fontTitle) TTF_SetFontHinting(fontTitle, TTF_HINTING_LIGHT);
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

bool selfTest = false;   // defined early: pushMouse must know the input source

// The authoritative pointer position: the OS's own global pointer minus the
// window origin, both in desktop points. SDL *event* coordinates are NOT
// used for position — across SDL builds (classic, sdl2-compat/SDL3) their
// units on HiDPI macOS have not proven trustworthy, and a points-vs-pixels
// mixup is exactly a "cursor tile doesn't sit under the pointer" bug.
// Polling global state is permission-free and unambiguous.
// Self-test keeps injected coordinates (there is no real pointer to poll).
static void pointerWindowPos(int evX, int evY, int& outX, int& outY) {
    if (selfTest) { outX = evX; outY = evY; return; }
    int gx, gy, wx, wy;
    SDL_GetGlobalMouseState(&gx, &gy);
    SDL_GetWindowPosition(win, &wx, &wy);
    outX = gx - wx; outY = gy - wy;
}

void pushMouse(mmask_t bstate, int px, int py) {
    int evX = px, evY = py;
    pointerWindowPos(evX, evY, px, py);
    MEVENT me{};
    pointToCell(px, py, me.x, me.y);
    me.bstate = bstate;
    if (SDL_GetModState() & KMOD_SHIFT) me.bstate |= BUTTON_SHIFT;
    // Dedupe pure hover pushes to cell crossings (drags always go through);
    // centralised here so the event path and the poll path can't differ.
    bool isButton = (bstate & ~REPORT_MOUSE_POSITION) != 0;
    if (!isButton && !leftHeld && me.x == lastCellX && me.y == lastCellY) return;
    lastCellX = me.x; lastCellY = me.y;
    mouseQ.push_back(me);
    keyQ.push_back(KEY_MOUSE);
    // Debug level 1: buttons only. Level 2: every push — logs BOTH the SDL
    // event coords and the global-derived ones; a systematic 2x mismatch
    // between them is the HiDPI event-unit bug this design sidesteps.
    if (mouseDebug && (isButton || mouseDebug >= 2)) {
        int ww, wh, pw, ph, wx, wy;
        SDL_GetWindowSize(win, &ww, &wh);
        SDL_GetRendererOutputSize(ren, &pw, &ph);
        SDL_GetWindowPosition(win, &wx, &wy);
        fprintf(stderr, "[mouse] %s evt=(%d,%d) glob-derived=(%d,%d) winpos=(%d,%d) win=%dx%d out=%dx%d cell=%dx%d -> (%d,%d)\n",
                isButton ? "btn" : "mov",
                evX, evY, px, py, wx, wy, ww, wh, pw, ph, cellW, cellH, me.x, me.y);
    }
}

// Poll the real pointer every ~30ms and feed hover updates from it.
// This — not the SDL motion event stream — is the primary hover source:
// it keeps the cursor tile glued to the pointer even when no events
// arrive (parked pointer while the view edge-scrolls underneath), and it
// keeps edge scrolling continuous (unconditional re-push every 60ms in
// the border zone, since the game's edge logic is event-driven).
void pollPointer() {
    if (selfTest) return;   // self-test drives injected coordinates only
    static Uint32 lastPoll = 0, lastEdgePush = 0;
    Uint32 now = SDL_GetTicks();
    if (now - lastPoll < 30) return;
    lastPoll = now;
    // Gate on live window flags, not SDL_GetMouseFocus(): that one is fed by
    // enter/leave EVENTS and goes stale (warped pointers, Cmd-Tab returns).
    static bool pollAlways = getenv("REALM_POLL_ALWAYS") != nullptr;
    Uint32 wf = SDL_GetWindowFlags(win);
    if (!pollAlways && !(wf & (SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS))) return;
    int px, py;
    pointerWindowPos(0, 0, px, py);
    int ww, wh;
    SDL_GetWindowSize(win, &ww, &wh);
    if (px < 0 || py < 0 || px >= ww || py >= wh) return;
    float s = pixelScale();
    float edgeX = 2.0f * cellW / s, edgeY = 2.0f * cellH / s;
    bool inEdge = (px < edgeX || px > ww - edgeX || py < edgeY || py > wh - edgeY);
    if (inEdge && now - lastEdgePush >= 60) {
        lastEdgePush = now;
        lastCellX = lastCellY = -1;   // force this push through the cell dedupe
    }
    // Self-heal: once a second, push even without a cell change. Covers a
    // pointer parked across a match start (dedupe primed on the splash) and
    // any staleness after the cell size changes (font zoom, resize).
    static Uint32 lastForce = 0;
    if (now - lastForce >= 1000) {
        lastForce = now;
        lastCellX = lastCellY = -1;
    }
    pushMouse(REPORT_MOUSE_POSITION, px, py);
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
            // Cmd/Ctrl +/- = font zoom (reliable everywhere, gestures aside).
            if (e.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) {
                if (e.key.keysym.sym == SDLK_EQUALS || e.key.keysym.sym == SDLK_PLUS
                    || e.key.keysym.sym == SDLK_KP_PLUS)  { zoomFont(1);  break; }
                if (e.key.keysym.sym == SDLK_MINUS || e.key.keysym.sym == SDLK_KP_MINUS)
                                                          { zoomFont(-1); break; }
            }
            int k = translateKey(e.key.keysym);
            if (k >= 0) pushKey(k);
            break;
        }
        case SDL_MOUSEMOTION:
            // Cell-crossing dedupe lives in pushMouse, shared with the poll path.
            pushMouse(REPORT_MOUSE_POSITION, e.motion.x, e.motion.y);
            break;
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
            // whole grid every flick. preciseY accumulates fractional
            // deltas so trackpad zooming is smooth instead of stepping a
            // full point per event.
            if (SDL_GetModState() & (KMOD_GUI | KMOD_CTRL)) {
                static float wheelAcc = 0.0f;
                float dy = (e.wheel.preciseY != 0.0f) ? e.wheel.preciseY : (float)e.wheel.y;
                wheelAcc += dy;
                while (wheelAcc >= 0.5f)  { zoomFont(1);  wheelAcc -= 0.5f; }
                while (wheelAcc <= -0.5f) { zoomFont(-1); wheelAcc += 0.5f; }
            }
            break;
        // Pinch zoom, built from raw trackpad finger events. SDL on macOS
        // does not deliver MULTIGESTURE for trackpads (verified — pinch did
        // nothing), but it does surface the pad as an indirect touch device
        // with FINGERDOWN/MOTION/UP in normalized pad coordinates. Track two
        // fingers, measure their spread; a deadband distinguishes pinching
        // from two-finger scrolling (spread ~constant while scrolling).
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP: {
            static SDL_FingerID ids[2]; static float fx[2], fy[2];
            static int nf = 0;
            static float baseDist = -1.0f, lastDist = -1.0f;
            static bool zooming = false;
            auto upd = [&](SDL_FingerID id, float x, float y, bool remove) {
                for (int i = 0; i < nf; i++) {
                    if (ids[i] == id) {
                        if (remove) { ids[i]=ids[--nf]; fx[i]=fx[nf]; fy[i]=fy[nf]; }
                        else        { fx[i]=x; fy[i]=y; }
                        return;
                    }
                }
                if (!remove && nf < 2) { ids[nf]=id; fx[nf]=x; fy[nf]=y; nf++; }
            };
            upd(e.tfinger.fingerId, e.tfinger.x, e.tfinger.y, e.type == SDL_FINGERUP);
            if (nf == 2) {
                float dx = fx[0]-fx[1], dy = fy[0]-fy[1];
                float d = std::sqrt(dx*dx + dy*dy);
                if (baseDist < 0) { baseDist = lastDist = d; zooming = false; }
                // Deadband: ignore spread drift < 6% of pad until it's
                // clearly a pinch; then zoom 1pt per 2.5% spread change.
                if (!zooming && std::fabs(d - baseDist) > 0.06f) { zooming = true; lastDist = d; }
                if (zooming) {
                    const float STEP = 0.025f;
                    while (d - lastDist >= STEP)  { zoomFont(1);  lastDist += STEP; }
                    while (lastDist - d >= STEP)  { zoomFont(-1); lastDist -= STEP; }
                }
            } else {
                baseDist = lastDist = -1.0f; zooming = false;
            }
            break;
        }
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
// REALM_AUTOSTART=1      : just start a duel after 1.2s, then hands off —
//                          used with external pointer-warping diagnostics.
const char* dumpPath = nullptr;
const char* dumpBmp  = nullptr;
bool autoStart = false;

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
        case 0: if (t > 1200) {
                    injectText("1"); injectKey(SDLK_RETURN);
                    injectText("S");   // debug reveal: dumps show the whole map
                    phase++;
                } break;
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

void autoStartStep() {
    static bool done = false;
    static Uint32 t0 = SDL_GetTicks();
    if (done || SDL_GetTicks() - t0 < 1200) return;
    injectText("1"); injectKey(SDLK_RETURN);
    done = true;
}

void pumpEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) handleEvent(e);
    pollPointer();
    if (selfTest) selfTestStep();
    if (autoStart) autoStartStep();
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
    if (env && *env) {
        const char* envB = getenv("REALM_FONT_BOLD");
        if (envB && *envB) fontPathBold = envB;
        return env;
    }
    // Monospace only: the renderer is a strict cell grid, and the grid pitch
    // comes from one glyph's advance — a proportional face (Helvetica) makes
    // wide glyphs collide and narrow ones float, which reads as bad kerning.
    // Helvetica stays solely as a nothing-else-exists fallback.
    // First choice is SF Mono REGULAR (Medium was tried 2026-06-12 and read
    // too heavy — user asked for the lighter weight back), paired with the
    // true Bold face for A_BOLD.
    struct Cand { const char* reg; const char* bold; };
    static const Cand candidates[] = {
        { "/System/Applications/Utilities/Terminal.app/Contents/Resources/Fonts/SF-Mono-Regular.otf",
          "/System/Applications/Utilities/Terminal.app/Contents/Resources/Fonts/SF-Mono-Bold.otf" },
        { "/System/Library/Fonts/SFNSMono.ttf", nullptr },
        { "/System/Library/Fonts/Menlo.ttc", nullptr },
        { "/System/Library/Fonts/Monaco.ttf", nullptr },
        { "/Library/Fonts/Andale Mono.ttf", nullptr },
        { "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf" },
        { "/usr/share/fonts/TTF/DejaVuSansMono.ttf", nullptr },
        { "C:\\Windows\\Fonts\\consola.ttf", "C:\\Windows\\Fonts\\consolab.ttf" },
        { "C:\\Windows\\Fonts\\lucon.ttf", nullptr },
        { "/System/Library/Fonts/Helvetica.ttc", nullptr },
        { nullptr, nullptr }
    };
    for (int i = 0; candidates[i].reg; i++) {
        FILE* f = fopen(candidates[i].reg, "rb");
        if (!f) continue;
        fclose(f);
        if (candidates[i].bold) {
            FILE* fb = fopen(candidates[i].bold, "rb");
            if (fb) { fclose(fb); fontPathBold = candidates[i].bold; }
        }
        return candidates[i].reg;
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
    mouseDebug = getenv("REALM_MOUSE_DEBUG") ? atoi(getenv("REALM_MOUSE_DEBUG")) : 0;
    if (getenv("REALM_MOUSE_DEBUG") && mouseDebug == 0) mouseDebug = 1;
    dumpPath   = getenv("REALM_DUMP_GRID");
    dumpBmp    = getenv("REALM_DUMP_BMP");
    selfTest   = getenv("REALM_SELFTEST") != nullptr;
    autoStart  = getenv("REALM_AUTOSTART") != nullptr;
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

    // Hide the OS cursor: the gold cell cursor IS the pointer. This was
    // visible for a while because the cell cursor lagged the pointer (the
    // 2026-06-12 queue-desync era); now that position comes from a 30 ms
    // SDL_GetGlobalMouseState poll the cell tracks the fingertip exactly,
    // and two cursors just read as clutter. REALM_SHOW_POINTER=1 restores
    // the OS pointer if a setup ever needs it.
    const char* showPtr = getenv("REALM_SHOW_POINTER");
    SDL_ShowCursor((showPtr && *showPtr && *showPtr != '0') ? SDL_ENABLE : SDL_DISABLE);
    SDL_StartTextInput();
    stdscr = (WINDOW*)(void*)&grid; // non-null token
    return stdscr;
}

int endwin() {
    clearGlyphCache();
    if (font)      { TTF_CloseFont(font); font = nullptr; }
    if (fontBold)  { TTF_CloseFont(fontBold); fontBold = nullptr; }
    if (fontTitle) { TTF_CloseFont(fontTitle); fontTitle = nullptr; }
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

// KEY_MOUSE tokens (keyQ) and their MEVENT payloads (mouseQ) are parallel
// queues — they MUST be popped in lockstep. Staging the payload here, the
// moment its KEY_MOUSE leaves getch(), makes that structural: a consumer
// that swallows KEY_MOUSE without calling getmouse() (the splash menu, any
// modal prompt) can no longer leave an orphaned MEVENT behind. Before this,
// one mouse twitch on the splash screen desynced the queues for the whole
// match — every later getmouse() returned a stale event, so the cursor
// tile permanently trailed the real pointer by N events.
static MEVENT stagedMouse{};
static bool   hasStagedMouse = false;

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
    if (k == KEY_MOUSE) {
        if (!mouseQ.empty()) {
            stagedMouse = mouseQ.front(); mouseQ.pop_front();
            hasStagedMouse = true;
        } else {
            hasStagedMouse = false;   // defensive: never replay an old event
        }
    }
    return k;
}

int getmouse(MEVENT* event) {
    if (!hasStagedMouse) return ERR;
    *event = stagedMouse;
    hasStagedMouse = false;
    return OK;
}

int erase() {
    for (auto& c : grid) c = Cell{};
    return OK;
}

int attron(unsigned attrs)  {
    if (attrs >> 8 & 0xFFF) curPair = (short)(attrs >> 8 & 0xFFF);
    curFlags |= attrs & (A_BOLD | A_DIM | A_REVERSE | A_TITLE);
    return OK;
}
int attroff(unsigned attrs) {
    if (attrs >> 8 & 0xFFF) curPair = 0;
    curFlags &= ~(attrs & (A_BOLD | A_DIM | A_REVERSE | A_TITLE));
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
                int face = (c.flags & A_TITLE) ? 2 : (c.flags & A_BOLD) ? 1 : 0;
                SDL_Texture* t = glyphTex(c.cp, face);
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
    // Translucent overlays (drag-selection box) sit on top of the whole
    // composed grid — a soft tinted fill plus a crisp 2px border.
    if (!overlays.empty()) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        for (const auto& o : overlays) {
            SDL_Rect r = { o.gx0 * cellW, o.gy0 * cellH,
                           (o.gx1 - o.gx0 + 1) * cellW, (o.gy1 - o.gy0 + 1) * cellH };
            SDL_SetRenderDrawColor(ren, o.r, o.g, o.b, o.fillA);
            SDL_RenderFillRect(ren, &r);
            SDL_SetRenderDrawColor(ren, o.r, o.g, o.b, o.borderA);
            for (int t = 0; t < 2; t++) {
                SDL_Rect e = { r.x + t, r.y + t, r.w - 2*t, r.h - 2*t };
                if (e.w <= 0 || e.h <= 0) break;
                SDL_RenderDrawRect(ren, &e);
            }
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_NONE);
        overlays.clear();
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

// Queue a translucent overlay rect (grid-cell coords, inclusive). Drawn over
// the whole grid at the next refresh(). See sdl_shim.h.
void shimOverlayRect(int gx0, int gy0, int gx1, int gy1,
                     int r, int g, int b, int fillA, int borderA) {
    if (gx1 < gx0 || gy1 < gy0) return;
    if (gx0 < 0) gx0 = 0; if (gy0 < 0) gy0 = 0;
    if (gx1 > cols - 1) gx1 = cols - 1; if (gy1 > rows - 1) gy1 = rows - 1;
    if (gx1 < gx0 || gy1 < gy0) return;
    overlays.push_back({ gx0, gy0, gx1, gy1,
        (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)fillA, (Uint8)borderA });
}

void shimGetMaxYX(int& y, int& x) { y = rows; x = cols; }
