#include "realm.h"
#include "gfx_renderer.h"
#include "entity_animation.h"
#include "tileset_assets.h"

#include <SDL.h>
#include <SDL_ttf.h>
#if defined(REALM_WEB)
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace {

struct Color { Uint8 r, g, b, a; };

struct MobileButton {
    SDL_Rect r;
    std::string id;
    std::string label;
};

struct KeyHit {
    SDL_Rect r;
    int ch = 0;
};

struct Gfx {
    SDL_Window*   win = nullptr;
    SDL_Renderer* ren = nullptr;
    TTF_Font* mono = nullptr;
    TTF_Font* monoSmall = nullptr;
    TTF_Font* emoji = nullptr;
    bool emojiFontLoaded = false;
    std::string monoPath;
    std::string emojiPath;

    int winW = 1280;
    int winH = 800;
    int tile = 24;
    int topH = 32;
    int bottomH = 48;
    int panelW = 286;

    // Internal projection flag. Tileset mode forces this to true; ASCII desktop
    // uses the terminal grid renderer instead.
    bool isometric = true;
    bool asciiOnly = false;
    bool fullscreen = false;

    bool leftDown = false;
    bool middleDown = false;
    bool miniMapDown = false;
    int dragStartX = 0, dragStartY = 0;
    int panStartMouseX = 0, panStartMouseY = 0;
    int panStartViewX = 0, panStartViewY = 0;
    int lastMouseMapX = -9999, lastMouseMapY = -9999;
    int mouseX = -10000, mouseY = -10000;
    std::vector<KeyHit> keyHits;

    int mobileOrientation = 0; // 0 auto, 1 portrait, 2 landscape.
    float mobileUiScale = 1.0f;
    bool mobileMinimapTap = true;
    bool mobileConfirmCommands = false;
    bool mobileEdgeScroll = false;
    bool mobileSplashSettings = false;
    bool mobileSplashHelp = false;
    bool loadGameRequested = false;
    EntityType mobileBuildType = E_NONE;
    int mobileBuildPage = 0;

    bool touchDown = false;
    bool touchOnMap = false;
    bool touchPanning = false;
    bool suppressNextMouse = false;
    Uint32 touchDownTicks = 0;
    int touchStartX = 0, touchStartY = 0;
    int touchLastX = 0, touchLastY = 0;

    std::unordered_map<std::string, SDL_Texture*> textCache;
    std::unordered_set<std::string> missingTileKeys;
    bool missingTileLogStarted = false;
} s;

struct LabLightOverride {
    bool enabled = false;
    int x = 0;
    int y = 0;
    float strength = 0.0f;
    float radius = 0.0f;
};

static bool labForcesImageTileset = false;
static LabLightOverride labLightOverride;

struct TerminalCell {
    char ch;
    Color fg;
    Color bg;
};

struct TerminalFrame {
    int cols = 0;
    int rows = 0;
    int cellW = 0;
    int cellH = 0;
    std::vector<TerminalCell> cells;

    TerminalCell& at(int x, int y) {
        return cells[(size_t)y * (size_t)cols + (size_t)x];
    }

    const TerminalCell& at(int x, int y) const {
        return cells[(size_t)y * (size_t)cols + (size_t)x];
    }
};

static TerminalFrame makeBlankTerminalFrame();
static void clampTerminalView();
static void terminalMapCellMetrics(int& cellW, int& cellH);
static SDL_Rect terminalMapPixelRect(const TerminalFrame& frame);
static void updateTerminalCamera(int cols, int rows, bool keepCursor = true);

static Color rgb(int r, int g, int b, int a = 255) {
    return Color{(Uint8)std::max(0,std::min(255,r)),
                 (Uint8)std::max(0,std::min(255,g)),
                 (Uint8)std::max(0,std::min(255,b)),
                 (Uint8)std::max(0,std::min(255,a))};
}

static Color scale(Color c, float f) {
    return rgb((int)(c.r*f), (int)(c.g*f), (int)(c.b*f), c.a);
}

static Color blend(Color a, Color b, float t) {
    return rgb((int)(a.r + (b.r-a.r)*t), (int)(a.g + (b.g-a.g)*t), (int)(a.b + (b.b-a.b)*t),
               (int)(a.a + (b.a-a.a)*t));
}

static std::string lowerSlug(const std::string& text) {
    std::string out;
    bool lastUnderscore = false;
    for (unsigned char raw : text) {
        char ch = (char)std::tolower(raw);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            out.push_back(ch);
            lastUnderscore = false;
        } else if (!lastUnderscore && !out.empty()) {
            out.push_back('_');
            lastUnderscore = true;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "unknown" : out;
}

static std::string quoteLogValue(const std::string& text) {
    std::string out = "\"";
    for (char ch : text) {
        if (ch == '\\' || ch == '"') out.push_back('\\');
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

static bool envFlagEnabled(const char* name, bool fallback) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw) return fallback;
    std::string value(raw);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return (char)std::tolower(ch); });
    return !(value == "0" || value == "false" || value == "off" || value == "no");
}

static bool localTilesetAuditEnabled() {
    if (!envFlagEnabled("REALM_TILESET_AUDIT", true)) return false;
#if defined(REALM_WEB)
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.location) return 0;
        var host = String(window.location.hostname || '').toLowerCase();
        return (host === 'localhost' || host === '127.0.0.1' || host === '::1' || host === '') ? 1 : 0;
    }) != 0;
#else
    return true;
#endif
}

static void appendMissingTileLocalhostLog(const std::string& line) {
#if defined(REALM_WEB)
    EM_ASM({
        if (typeof window === 'undefined' || !window.location) return;
        var host = String(window.location.hostname || '').toLowerCase();
        if (!(host === 'localhost' || host === '127.0.0.1' || host === '::1' || host === '')) return;
        var line = UTF8ToString($0);
        var storageKey = 'realm.missingTilesLog';
        var header = '# Realm missing tiles\n'
            + '# Local browser run only. Add real assets for these keys; current renderer used placeholders.\n';
        var existing = '';
        try { existing = window.localStorage.getItem(storageKey) || ''; } catch (error) {}
        if (existing.indexOf('# Realm missing tiles') !== 0) existing = header;
        if (existing.indexOf(line) === -1) {
            existing += line + '\n';
            try { window.localStorage.setItem(storageKey, existing); } catch (error) {}
        }
        if (!window.realmMissingTiles) window.realmMissingTiles = [];
        if (window.realmMissingTiles.indexOf(line) === -1) window.realmMissingTiles.push(line);
        console.info('[Realm missing tile]', line);
    }, line.c_str());
#else
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories("build", ec);
    const fs::path path = fs::path("build") / "missing-tiles.log";
    if (!s.missingTileLogStarted) {
        std::ofstream reset(path, std::ios::trunc);
        if (reset) {
            reset << "# Realm missing tiles\n"
                  << "# Local GUI run only. Add real assets for these keys; current renderer used placeholders.\n"
                  << "# Format: kind | key | name | fallback | suggested_asset\n";
        }
        s.missingTileLogStarted = true;
    }
    std::ofstream out(path, std::ios::app);
    if (out) out << line << '\n';
#endif
}

static void logMissingTile(const std::string& kind, const std::string& key,
                           const std::string& name, const std::string& fallback,
                           const std::string& suggestedAsset) {
    if (!localTilesetAuditEnabled()) return;
    if (!s.missingTileKeys.insert(kind + ":" + key).second) return;
    std::ostringstream line;
    line << kind << " | " << key
         << " | name=" << quoteLogValue(name)
         << " | fallback=" << quoteLogValue(fallback)
         << " | suggested_asset=" << suggestedAsset;
    appendMissingTileLocalhostLog(line.str());
}

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

static void setDraw(Color c) {
    SDL_SetRenderDrawColor(s.ren, c.r, c.g, c.b, c.a);
}

static void applyRendererOutputScale() {
#if defined(REALM_WEB)
    int outW = 0;
    int outH = 0;
    SDL_GetRendererOutputSize(s.ren, &outW, &outH);
    float sx = (s.winW > 0 && outW > 0) ? (float)outW / (float)s.winW : 1.0f;
    float sy = (s.winH > 0 && outH > 0) ? (float)outH / (float)s.winH : 1.0f;
    if (!std::isfinite(sx) || sx <= 0.0f) sx = 1.0f;
    if (!std::isfinite(sy) || sy <= 0.0f) sy = 1.0f;
    SDL_RenderSetScale(s.ren, sx, sy);
#endif
}

static unsigned hash2(int x, int y, unsigned salt) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + salt * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float noisePatch(int x, int y, unsigned salt) {
    // Coarse noise: gives soft painted patches, not salt-and-pepper.
    unsigned a = hash2(x/5,  y/4,  salt);
    unsigned b = hash2(x/12, y/9,  salt+11u);
    unsigned c = hash2(x/23, y/17, salt+29u);
    return ((a & 255) + ((b & 255) * 0.65f) + ((c & 255) * 0.35f)) / (255.0f * 2.0f);
}

static int terrainFamily(Terrain t) {
    switch (t) {
        case T_WATER: case T_SHALLOWS: case T_FISH: case T_ICE: return 1;
        case T_FOREST: case T_PINE: case T_PALM: case T_DEAD_TREE: return 2;
        case T_MOUNTAIN: case T_HILLS: case T_STONE: return 3;
        case T_GOLD: case T_WHEAT: case T_BERRY: return 4;
        case T_SAND: case T_DUNES: case T_DIRT: case T_MUD: case T_GRAVEL: return 5;
        case T_LAVA: case T_ASH: return 6;
        case T_CASTLE_WALL: case T_CASTLE_FLOOR: case T_CASTLE_GATE: case T_RUINS: return 7;
        default: return 0;
    }
}

static int boundaryStrength(int x, int y) {
    if (!inBounds(x,y)) return 0;
    const Tile& c = g.map[y][x];
    int cf = terrainFamily(c.terrain), b = 0;
    const int dx[4] = {1,-1,0,0};
    const int dy[4] = {0,0,1,-1};
    for (int i=0;i<4;i++) {
        int nx=x+dx[i], ny=y+dy[i];
        if (!inBounds(nx,ny)) continue;
        const Tile& n = g.map[ny][nx];
        if (n.biome != c.biome || terrainFamily(n.terrain) != cf) b++;
    }
    return b;
}

static Color seasonTint(Color base) {
    switch (getSeason()) {
        case SPRING: return blend(base, rgb(120,190,105), 0.12f);
        case SUMMER: return blend(base, rgb(205,170,80), 0.07f);
        case AUTUMN: return blend(base, rgb(190,115,55), 0.18f);
        case WINTER: return blend(base, rgb(205,215,225), 0.24f);
    }
    return base;
}

static Color timeTint(Color base) {
    float b = clamp01(getBrightness());
    float darkness = clamp01((0.86f - b) / 0.86f);
    float twilight = clamp01(1.0f - std::abs(b - 0.42f) / 0.30f);

    Color c = scale(base, 1.0f - darkness * 0.46f);
    c = blend(c, rgb(18, 32, 66), darkness * 0.30f);

    if (g.dayPhase < 0.5f) {
        c = blend(c, rgb(255, 174, 92), twilight * 0.13f);
    } else {
        c = blend(c, rgb(92, 82, 158), twilight * 0.15f);
        c = blend(c, rgb(255, 122, 72), twilight * 0.05f);
    }
    return c;
}

static Color biomeBase(Biome b) {
    switch (b) {
        case B_DESERT:   return rgb(154,126,73);
        case B_SNOW:     return rgb(190,202,210);
        case B_SWAMP:    return rgb(45,76,52);
        case B_FOREST:   return rgb(28,82,42);
        case B_VOLCANIC: return rgb(58,50,48);
        case B_OCEAN:    return rgb(30,74,105);
        case B_TEMPERATE:
        default:         return rgb(45,105,48);
    }
}

static Color terrainBg(const Tile& t, int x, int y) {
    Color c = biomeBase(t.biome);
    switch (t.terrain) {
        case T_WATER:    c = rgb(22, 74, 118); break;
        case T_SHALLOWS: c = rgb(43, 115, 128); break;
        case T_FISH:     c = rgb(28, 88, 130); break;
        case T_ICE:      c = rgb(135, 178, 196); break;
        case T_SNOW:     c = rgb(200, 211, 218); break;
        case T_SAND:     c = rgb(165, 134, 78); break;
        case T_DUNES:    c = rgb(181, 151, 90); break;
        case T_DIRT:     c = rgb(104, 73, 43); break;
        case T_ROAD:     c = rgb(83, 76, 63); break;
        case T_MUD:      c = rgb(72, 61, 42); break;
        case T_LAVA:     c = rgb(114, 40, 24); break;
        case T_ASH:      c = rgb(56, 54, 52); break;
        case T_CASTLE_WALL: c = rgb(72,72,74); break;
        case T_CASTLE_FLOOR: c = rgb(91,74,53); break;
        case T_CASTLE_GATE: c = rgb(62,55,49); break;
        default: break;
    }

    c = seasonTint(c);
    float n = noisePatch(x, y, 811u + (unsigned)t.biome*17u + (unsigned)getSeason()*37u);
    float shade = 0.90f + n * 0.22f;
    int edge = boundaryStrength(x,y);
    if (edge) shade += 0.04f * edge;
    c = scale(c, shade);
    return timeTint(c);
}

static Color ownerBg(int owner) {
    switch (owner % MAX_PLAYERS) {
        case 0: return rgb(35, 150, 220); // human cyan/blue
        case 1: return rgb(190, 42, 45);  // opponent red
        case 2: return rgb(205, 125, 40); // orange
        default:return rgb(125, 67, 158); // purple
    }
}

static std::string firstExisting(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        FILE* f = std::fopen(p.c_str(), "rb");
        if (f) { std::fclose(f); return p; }
    }
    return "";
}

static TTF_Font* openFont(const std::vector<std::string>& paths, int size, std::string* usedPath = nullptr) {
    std::string p = firstExisting(paths);
    if (usedPath) *usedPath = p;
    if (p.empty()) return nullptr;
    TTF_Font* f = TTF_OpenFont(p.c_str(), size);
    if (f) TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    return f;
}

static std::string emojiFallbackGlyph(const std::string& text) {
    // Used only if no emoji-capable font can be loaded. Avoids tofu boxes.
    if (text == u8"🪨") return u8"◆";
    if (text == u8"🌳" || text == u8"🌲" || text == u8"🌴") return u8"♣";
    if (text == u8"🪵") return u8"▬";
    if (text == u8"🌾") return u8"§";
    if (text == u8"🫐") return u8":";
    if (text == u8"🐟") return u8"≈";
    if (text == u8"🧍") return u8"p";
    if (text == u8"🚶") return u8"p";
    if (text == u8"🧎") return u8"p";
    if (text == u8"🏌") return u8"p";
    if (text == u8"🤺") return u8"m";
    if (text == u8"🏹") return u8"a";
    if (text == u8"🐎") return u8"k";
    if (text == u8"🛞") return u8"c";
    if (text == u8"🦌") return u8">";
    if (text == u8"🐑") return u8"o";
    if (text == u8"🐺") return u8"<";
    if (text == u8"🐗") return u8"@";
    if (text == u8"🏛" || text == u8"🏠" || text == u8"🏕" || text == u8"🏰") return u8"▣";
    if (text == u8"🧱") return u8"■";
    if (text == u8"🚪") return u8"▣";
    if (text == u8"⚒") return u8"△";
    if (text == u8"⛪") return u8"✚";
    if (text == u8"🏪") return u8"◆";
    if (text == u8"🐴") return u8"♘";
    if (text == u8"🗼") return u8"▣";
    if (text == u8"⚙") return u8"○";
    if (text == u8"⚓") return u8"∩";
    return text;
}

static SDL_Texture* cachedText(TTF_Font* font, const std::string& text, Color col, bool blended = true) {
    if (!font) return nullptr;
    std::ostringstream k;
    k << (void*)font << '|' << (int)col.r << ',' << (int)col.g << ',' << (int)col.b << ',' << (int)col.a << '|' << text;
    std::string key = k.str();
    auto it = s.textCache.find(key);
    if (it != s.textCache.end()) return it->second;
    SDL_Color sc{col.r,col.g,col.b,col.a};
    SDL_Surface* surf = blended ? TTF_RenderUTF8_Blended(font, text.c_str(), sc)
                                : TTF_RenderUTF8_Solid(font, text.c_str(), sc);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(s.ren, surf);
    SDL_FreeSurface(surf);
    if (tex) {
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        s.textCache[key] = tex;
    }
    return tex;
}

static void drawText(int x, int y, const std::string& text, Color col, TTF_Font* font = nullptr) {
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, col);
    if (!tex) return;
    int w=0,h=0; SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{x,y,w,h};
    SDL_SetTextureColorMod(tex, 255,255,255);
    SDL_SetTextureAlphaMod(tex, col.a);
    SDL_RenderCopy(s.ren, tex, nullptr, &dst);
}

static void drawTextFit(int x, int y, const std::string& text, Color col, int maxW, TTF_Font* font);

static int textWidth(const std::string& text, TTF_Font* font = nullptr) {
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, rgb(255,255,255));
    if (!tex) return 0;
    int w=0,h=0; SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    return w;
}

static int textLineHeight(TTF_Font* font = nullptr) {
    TTF_Font* f = font ? font : s.mono;
    return std::max(16, f ? TTF_FontLineSkip(f) : 18);
}

static bool rectHovered(SDL_Rect r) {
    return s.mouseX >= r.x && s.mouseY >= r.y && s.mouseX < r.x + r.w && s.mouseY < r.y + r.h;
}

static void registerKeyHit(SDL_Rect r, int ch) {
    if (ch == 0 || r.w <= 0 || r.h <= 0) return;
    s.keyHits.push_back({r, ch});
}

static void drawHoverMark(SDL_Rect r, Color color) {
    if (!rectHovered(r)) return;
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(color);
    SDL_RenderDrawLine(s.ren, r.x, r.y + r.h - 2, r.x + r.w, r.y + r.h - 2);
    SDL_RenderDrawLine(s.ren, r.x, r.y + r.h - 1, r.x + r.w, r.y + r.h - 1);
}

static void drawKeyOptionText(int x, int y, const std::string& text, int ch,
                              Color color, int maxW, TTF_Font* font = nullptr) {
    TTF_Font* f = font ? font : s.mono;
    drawTextFit(x, y, text, color, maxW, f);
    SDL_Rect r{x, y, std::min(std::max(1, maxW), std::max(1, textWidth(text, f))), textLineHeight(f)};
    registerKeyHit(r, ch);
    drawHoverMark(r, color);
}

static void drawKeyTokensInText(int x, int y, const std::string& text,
                                const std::vector<std::pair<std::string, int>>& tokens,
                                Color color, int maxW, TTF_Font* font = nullptr) {
    TTF_Font* f = font ? font : s.mono;
    drawTextFit(x, y, text, color, maxW, f);
    size_t searchFrom = 0;
    for (const auto& token : tokens) {
        size_t pos = text.find(token.first, searchFrom);
        if (pos == std::string::npos) pos = text.find(token.first);
        if (pos == std::string::npos) continue;
        int tx = x + textWidth(text.substr(0, pos), f);
        SDL_Rect r{tx, y, std::max(1, textWidth(token.first, f)), textLineHeight(f)};
        if (r.x < x + maxW) {
            if (r.x + r.w > x + maxW) r.w = std::max(1, x + maxW - r.x);
            registerKeyHit(r, token.second);
            drawHoverMark(r, color);
        }
        searchFrom = pos + token.first.size();
    }
}

static void drawTextFit(int x, int y, const std::string& text, Color col, int maxW, TTF_Font* font = nullptr) {
    if (maxW <= 0) return;
    std::string out = text;
    TTF_Font* f = font ? font : s.mono;
    if (textWidth(out, f) > maxW) {
        while (out.size() > 1 && textWidth(out + "~", f) > maxW) out.pop_back();
        if (!out.empty()) out += "~";
    }
    drawText(x, y, out, col, f);
}

static void drawCentered(const std::string& text, SDL_Rect rect, Color col, bool emoji, bool tint = false) {
    std::string drawTextValue = text;
    TTF_Font* font = emoji ? s.emoji : s.mono;
    if (emoji && !s.emojiFontLoaded) {
        drawTextValue = emojiFallbackGlyph(text);
        font = s.mono;
    }
    Color renderCol = tint ? rgb(255, 255, 255, col.a) : col;
    SDL_Texture* tex = cachedText(font, drawTextValue, renderCol);
    if (!tex && emoji) tex = cachedText(s.mono, emojiFallbackGlyph(text), renderCol);
    if (!tex) return;
    int tw=0, th=0; SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) return;
    float maxW = rect.w * 0.92f;
    float maxH = rect.h * 0.92f;
    float scaleF = std::min(maxW / tw, maxH / th);
    int dw = std::max(1, (int)(tw * scaleF));
    int dh = std::max(1, (int)(th * scaleF));
    SDL_Rect dst{rect.x + (rect.w-dw)/2, rect.y + (rect.h-dh)/2, dw, dh};
    if (tint) SDL_SetTextureColorMod(tex, col.r, col.g, col.b);
    else      SDL_SetTextureColorMod(tex, 255,255,255);
    SDL_SetTextureAlphaMod(tex, col.a);
    SDL_RenderCopy(s.ren, tex, nullptr, &dst);
    SDL_SetTextureColorMod(tex, 255,255,255);
}

static char terrainAscii(Terrain t) {
    switch (t) {
        case T_GRASS: return '.';
        case T_TALL_GRASS: return '"';
        case T_FLOWERS: return '*';
        case T_MEADOW: return ',';
        case T_FOREST: return 'T';
        case T_PINE: return 'Y';
        case T_PALM: return 'y';
        case T_DEAD_TREE: return 't';
        case T_MOUNTAIN: return '^';
        case T_HILLS: return 'n';
        case T_STONE: return 'o';
        case T_WATER: return '~';
        case T_SHALLOWS: return '~';
        case T_MARSH: return '=';
        case T_REEDS: return '|';
        case T_GOLD: return '$';
        case T_SAND: return '.';
        case T_DUNES: return ',';
        case T_SNOW: return '*';
        case T_ICE: return '=';
        case T_DIRT: return '.';
        case T_ROAD: return '#';
        case T_MUD: return ',';
        case T_WHEAT: return '%';
        case T_BERRY: return ':';
        case T_FISH: return '~';
        case T_RUINS: return '&';
        case T_GRAVEL: return ':';
        case T_LAVA: return '~';
        case T_ASH: return '.';
        case T_CASTLE_WALL: return '#';
        case T_CASTLE_FLOOR: return '.';
        case T_CASTLE_GATE: return '|';
    }
    return '?';
}

static const char* terrainGlyph(const Tile& t, int x, int y) {
    unsigned h = hash2(x, y, 1200u + (unsigned)g.tick/16u);
    switch (t.terrain) {
        // Interactable / meaningful resources use actual emojis.
        case T_GOLD:      return u8"🪨"; // tinted yellow by renderer, background stays biome
        case T_FOREST:    return (h&1u) ? u8"🌳" : u8"🌲";
        case T_PINE:      return u8"🌲";
        case T_PALM:      return u8"🌴";
        case T_DEAD_TREE: return u8"🪵";
        case T_WHEAT:     return u8"🌾";
        case T_BERRY:     return u8"🫐";
        case T_FISH:      return u8"🐟";

        // Decoration: non-emoji marks, designed to let colour/texture carry the tile.
        case T_GRASS: {
            static const char* a[] = {u8"·",u8"∙",u8"˙",u8"˖"}; return a[h&3u];
        }
        case T_TALL_GRASS: {
            static const char* a[] = {u8"⁝",u8"╵",u8"╷",u8"┆"}; return a[(h+(unsigned)g.tick/18u)&3u];
        }
        case T_FLOWERS: {
            static const char* a[] = {u8"✿",u8"✣",u8"✽",u8"·"}; return a[h&3u];
        }
        case T_MEADOW:    return (h&1u) ? u8"∙" : u8"ˑ";
        case T_MOUNTAIN:  return (h&1u) ? u8"▲" : u8"▴";
        case T_HILLS:     return (h&1u) ? u8"⌒" : u8"∩";
        case T_STONE:     return (h&1u) ? u8"▪" : u8"▫";
        case T_WATER:     return (h&1u) ? u8"≈" : u8"∼";
        case T_SHALLOWS:  return (h&1u) ? u8"≈" : u8"⌁";
        case T_MARSH:     return (h&1u) ? u8"≋" : u8"⌇";
        case T_REEDS:     return (h&1u) ? u8"╿" : u8"┆";
        case T_SAND:      return (h&1u) ? u8"·" : u8"˙";
        case T_DUNES:     return (h&1u) ? u8"∿" : u8"⌁";
        case T_SNOW:      return (h&1u) ? u8"·" : u8"˙";
        case T_ICE:       return (h&1u) ? u8"═" : u8"─";
        case T_DIRT:      return (h&1u) ? u8"∙" : u8"·";
        case T_ROAD:      return (h&1u) ? u8"─" : u8"═";
        case T_MUD:       return (h&1u) ? u8"∙" : u8"·";
        case T_RUINS:     return (h&1u) ? u8"⌂" : u8"⌐";
        case T_GRAVEL:    return (h&1u) ? u8"⁘" : u8"∴";
        case T_LAVA:      return (h&1u) ? u8"≈" : u8"✦";
        case T_ASH:       return (h&1u) ? u8"░" : u8"·";
        case T_CASTLE_WALL:  return u8"▓";
        case T_CASTLE_FLOOR: return (h&1u) ? u8"·" : u8"∙";
        case T_CASTLE_GATE:  return u8"▣";
    }
    return "?";
}

static const char* peasantGlyph(const Entity& e) {
    if (e.state == S_MOVING || e.state == S_RETURNING || e.pathIdx < (int)e.path.size()) return u8"🚶";
    if (e.state == S_GATHERING && (e.cargo.type == CR_FISH || e.cargo.type == CR_FOOD)) return u8"🧎";
    if (e.state == S_GATHERING || e.state == S_BUILDING || e.state == S_ATTACKING) return u8"🏌";
    return u8"🧍";
}

static const char* tilesetEntityGlyph(const Entity& e, bool& hasTile) {
    hasTile = true;
    switch (e.type) {
        case E_PEASANT: return peasantGlyph(e);
        case E_MILITIA: return u8"🤺";
        case E_ARCHER: return u8"🏹";
        case E_KNIGHT: return u8"🐎";
        case E_SPEARMAN: return u8"🗡";
        case E_CATAPULT: return u8"🛞";
        case E_TREBUCHET: return u8"🎯";
        case E_FISHING_BOAT: return u8"🛶";
        case E_WARSHIP: return u8"🚢";
        case E_TRANSPORT: return u8"⛴";
        case E_RAM: return u8"🪵";
        case E_TOWNHALL: return u8"🏛";
        case E_HOUSE: return u8"🏠";
        case E_BARRACKS: return u8"🏕";
        case E_STABLE: return u8"🐴";
        case E_TOWER: return u8"🗼";
        case E_FARM: return u8"🌾";
        case E_BLACKSMITH: return u8"⚒";
        case E_CHURCH: return u8"⛪";
        case E_MARKET: return u8"🏪";
        case E_WALL: return u8"🧱";
        case E_GATE: return e.gateOpen ? u8"🚪" : u8"🧱";
        case E_CASTLE: return u8"🏰";
        case E_LUMBER_CAMP: return u8"🪵";
        case E_MINING_CAMP: return u8"⛏";
        case E_MILL: return u8"⚙";
        case E_DOCK: return u8"⚓";
        case E_DEER: return u8"🦌";
        case E_WOLF: return u8"🐺";
        case E_SHEEP: return u8"🐑";
        case E_BOAR: return u8"🐗";
        default: break;
    }
    hasTile = false;
    return nullptr;
}

struct EntitySpriteSpec {
    std::string key;
    std::string displayName;
    std::string suggestedAsset;
    std::string description;
    int frameMs = 250;
    int frames = 2;
    bool loop = true;
    bool holdLast = false;
};

static int displayFrameMs(const EntityActionAnimationSpec& anim) {
    if (anim.frameCount <= 0) return 250;
    for (int i = 0; i < anim.frameCount; ++i) {
        if (anim.frames[i].durationMs > 0) return anim.frames[i].durationMs;
    }
    return anim.transitionAfterMs > 0 ? anim.transitionAfterMs : 250;
}

static EntitySpriteSpec entitySpriteSpec(const Entity& e) {
    EntitySpriteSpec spec;
    std::string name = STATS[e.type].name ? STATS[e.type].name : "Unknown";
    std::string slug = lowerSlug(name);
    if (const EntityActionAnimationSpec* anim = entityActionAnimationSpecFor(e)) {
        std::string action = anim->action;
        std::string direction = entityAnimationDirectionBucket(e);
        spec.frameMs = displayFrameMs(*anim);
        spec.frames = anim->frameCount;
        spec.loop = anim->loop;
        spec.holdLast = anim->holdLast;
        spec.description = anim->description;
        spec.key = slug + "/" + action + "/" + direction;
        spec.displayName = name + " " + action + " " + direction;
        spec.suggestedAsset = "assets/tiles/entities/" + spec.key + "/frame_00_base.png";
        return spec;
    }
    spec.key = slug;
    spec.displayName = name;
    spec.suggestedAsset = "assets/tiles/entities/" + slug + ".png";
    return spec;
}

static bool hasEntityImageTile(EntityType type) {
#if !defined(REALM_WEB)
    return tilesetEntityFrameExists(type, "idle", "front", 0);
#else
    (void)type;
    return false;
#endif
}

static bool hasTerrainImageTile(Terrain) {
    return false;
}

static void logMissingEntityImageTile(const Entity& e) {
    if (hasEntityImageTile(e.type)) return;
    EntitySpriteSpec spec = entitySpriteSpec(e);
    logMissingTile("entity", "entity." + spec.key, spec.displayName,
                   std::string(1, STATS[e.type].glyph),
                   spec.suggestedAsset);
}

static void logMissingTerrainImageTile(Terrain t) {
    if (hasTerrainImageTile(t)) return;
    std::string name = terrainName(t);
    std::string slug = lowerSlug(name);
    logMissingTile("terrain", "terrain." + slug, name,
                   std::string(1, terrainAscii(t)),
                   "assets/tiles/terrain/" + slug + ".png");
}

static void logMissingVisualTileParts(const Tile& tile) {
    VisualTileParts parts = visualPartsForTile(tile);
    std::string ground = groundTypeName(parts.ground);
    logMissingTile("ground", "ground." + ground, ground,
                   std::string(1, terrainAscii(tile.terrain)),
                   "assets/tiles/grounds/" + ground + ".png");
    if (parts.feature != F_NONE) {
        std::string feature = featureTypeName(parts.feature);
        std::string fallback = featureConceals(parts.feature) ? "front/back symbolic occluder" : terrainGlyph(tile, 0, 0);
        logMissingTile("feature", "feature." + feature, feature, fallback,
                       "assets/tiles/features/" + feature + "/manifest.json");
    }
    for (VisualDecalType decal : parts.decals) {
        std::string name = visualDecalName(decal);
        logMissingTile("decal", "decal." + name, name, "procedural wear/decal fallback",
                       "assets/tiles/decals/" + name + ".png");
    }
}

static const char* featureOccluderGlyph(FeatureType feature) {
    switch (feature) {
        case F_FOREST: return u8"♣";
        case F_PINE: return u8"♠";
        case F_REEDS: return u8"╿";
        default: return "";
    }
}

static void drawFeatureOccluderIfNeeded(int mx, int my, SDL_Rect rect) {
    if (!inBounds(mx, my) || !g.map[my][mx].visible[0]) return;
    Entity* ent = entityAt(mx, my);
    if (!ent || !ent->alive || isBuilding(ent->type)) return;
    VisualTileParts parts = visualPartsForTile(g.map[my][mx]);
    if (!featureConceals(parts.feature)) return;
    const char* glyph = featureOccluderGlyph(parts.feature);
    if (!glyph || !*glyph) return;
    Color col = rgb(105, 180, 95, 185);
    SDL_Rect top{rect.x + rect.w / 8, rect.y, rect.w * 3 / 4, rect.h * 3 / 4};
    drawCentered(glyph, top, col, false, false);
}

static std::string tilesetEntityVisual(const Entity& e, bool& usesSymbolFont) {
    if (!e.alive || e.state == S_DEAD) {
        usesSymbolFont = true;
        if (e.type == E_DEER || e.type == E_SHEEP || e.type == E_BOAR || e.type == E_WOLF) {
            switch (animalCarcassVisualState(e)) {
                case ACVS_DEAD_UNHARVESTED: return u8"◼";
                case ACVS_PARTLY_HARVESTED: return u8"◧";
                case ACVS_MOSTLY_HARVESTED: return u8"◌";
                case ACVS_DEPLETED_SKELETON:
                case ACVS_ALIVE: return u8"☠";
            }
        }
        return e.deathTicks >= DEATH_DECAY_TICKS ? u8"☠" : u8"†";
    }
    logMissingEntityImageTile(e);
    bool hasTile = false;
    const char* glyph = tilesetEntityGlyph(e, hasTile);
    if (hasTile && glyph) {
        usesSymbolFont = true;
        return glyph;
    }
    usesSymbolFont = false;
    return std::string(1, STATS[e.type].glyph);
}

static bool imageTilesetEnabled() {
#if defined(REALM_WEB)
    return false;
#else
    return labForcesImageTileset || envFlagEnabled("REALM_IMAGE_TILESET", false);
#endif
}

static SDL_Color toSdlColor(Color c) {
    return SDL_Color{c.r, c.g, c.b, c.a};
}

static int animationFrameFor(const EntityActionAnimationSpec* anim, int explicitFrame = -1, int speedPercent = 100) {
    if (!anim || anim->frameCount <= 0) return 0;
    if (explicitFrame >= 0) return explicitFrame % std::max(1, anim->frameCount);
    int frameMs = std::max(1, displayFrameMs(*anim));
    int speed = std::max(10, speedPercent);
    int elapsedMs = (g.tick * TICK_MS * speed) / 100;
    if (!anim->loop && anim->holdLast) {
        int total = 0;
        for (int i = 0; i < anim->frameCount; ++i) total += std::max(1, anim->frames[i].durationMs);
        if (elapsedMs >= total) return anim->frameCount - 1;
    }
    return (elapsedMs / frameMs) % std::max(1, anim->frameCount);
}

static int animationFrameForEntity(const Entity& e, const EntityActionAnimationSpec* anim, int explicitFrame = -1) {
    if (explicitFrame >= 0) return animationFrameFor(anim, explicitFrame);
    if (anim && e.state == S_DEAD && std::strcmp(anim->action, "death") == 0 && anim->frameCount > 1) {
        return e.deathTicks >= DEATH_DECAY_TICKS ? 1 : 0;
    }
    return animationFrameFor(anim);
}

static bool drawEntityImageTile(const Entity& e, SDL_Rect dst, Color modulation,
                                const char* forcedAction = nullptr,
                                const char* forcedDirection = nullptr,
                                int explicitFrame = -1,
                                SDL_Color teamColor = SDL_Color{0,0,0,0},
                                TilesetAssetFrame* outFrame = nullptr) {
#if defined(REALM_WEB)
    (void)e; (void)dst; (void)modulation; (void)forcedAction; (void)forcedDirection;
    (void)explicitFrame; (void)teamColor; (void)outFrame;
    return false;
#else
    if (!imageTilesetEnabled()) return false;
    const char* action = forcedAction;
    const EntityActionAnimationSpec* anim = nullptr;
    if (action && *action) {
        anim = findEntityActionAnimationSpec(e.type, action);
    } else {
        anim = entityActionAnimationSpecFor(e);
        action = anim ? anim->action : "idle";
    }
    const char* direction = (forcedDirection && *forcedDirection) ? forcedDirection : entityAnimationDirectionBucket(e);
    int frameIndex = animationFrameForEntity(e, anim, explicitFrame);
    if (!tilesetEntityFrameExists(e.type, action ? action : "idle", direction ? direction : "front", frameIndex)) {
        return false;
    }
    if (teamColor.a == 0) teamColor = toSdlColor(ownerBg(e.owner == OWNER_NATURE ? 0 : e.owner));
    TilesetAssetRequest request{e.type, action ? action : "idle", direction ? direction : "front", frameIndex, teamColor};
    TilesetAssetFrame frame = tilesetLoadEntityFrame(s.ren, request);
    if (outFrame) *outFrame = frame;
    if (!frame.texture) return false;
    bool mirrorHorizontal = !forcedDirection && entityAnimationMirrorHorizontal(e);

    SDL_SetTextureColorMod(frame.texture, modulation.r, modulation.g, modulation.b);
    SDL_SetTextureAlphaMod(frame.texture, modulation.a);
    SDL_RenderCopyEx(s.ren, frame.texture, nullptr, &dst, 0.0, nullptr,
                     mirrorHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
#endif
}

static bool isResourceEmojiTerrain(Terrain t) {
    return t == T_GOLD || t == T_FOREST || t == T_PINE || t == T_PALM || t == T_DEAD_TREE
        || t == T_WHEAT || t == T_BERRY || t == T_FISH;
}

static Color glyphColorForTerrain(const Tile& t, int x, int y) {
    (void)x; (void)y;
    switch (t.terrain) {
        case T_GOLD: return rgb(255, 218, 78); // yellow tint applied to rock emoji
        case T_WATER: case T_SHALLOWS: return rgb(175, 225, 238);
        case T_SNOW: case T_ICE: return rgb(245,245,245);
        case T_LAVA: return rgb(255, 190, 76);
        case T_ASH: return rgb(120,120,120);
        default: break;
    }
    Color bg = terrainBg(t, x, y);
    return blend(scale(bg, 1.45f), rgb(235,235,220), 0.25f);
}

static bool isSelected(const Entity* e) {
    if (!e) return false;
    if (e->id == g.selectedId) return true;
    return std::find(g.selectedIds.begin(), g.selectedIds.end(), e->id) != g.selectedIds.end();
}

static float visibleFadeAt(int x, int y) {
    if (!inBounds(x, y) || !g.map[y][x].explored[0]) return 0.0f;
    if (!g.map[y][x].visible[0]) return 0.34f;

    float nearest = 99.0f;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (!inBounds(nx, ny)) continue;
            if (g.map[ny][nx].visible[0]) continue;
            nearest = std::min(nearest, std::sqrt((float)(dx * dx + dy * dy)));
        }
    }

    if (nearest <= 1.01f) return 0.76f;
    if (nearest <= 1.45f) return 0.82f;
    if (nearest <= 2.05f) return 0.91f;
    return 1.0f;
}

static float torchStrength(EntityType type) {
    switch (type) {
        case E_TOWNHALL:    return 0.44f;
        case E_CASTLE:      return 0.48f;
        case E_TOWER:       return 0.42f;
        case E_CHURCH:      return 0.38f;
        case E_MARKET:      return 0.34f;
        case E_BARRACKS:
        case E_STABLE:
        case E_BLACKSMITH:  return 0.30f;
        case E_HOUSE:
        case E_LUMBER_CAMP:
        case E_MINING_CAMP:
        case E_MILL:
        case E_DOCK:        return 0.23f;
        default:            return 0.0f;
    }
}

static float torchRadius(EntityType type) {
    switch (type) {
        case E_CASTLE:      return 5.6f;
        case E_TOWNHALL:    return 5.1f;
        case E_TOWER:
        case E_CHURCH:      return 4.6f;
        case E_MARKET:      return 4.0f;
        case E_BARRACKS:
        case E_STABLE:
        case E_BLACKSMITH:  return 3.7f;
        case E_DOCK:        return 3.5f;
        case E_HOUSE:
        case E_LUMBER_CAMP:
        case E_MINING_CAMP:
        case E_MILL:        return 2.9f;
        default:            return 0.0f;
    }
}

static float torchLightAt(int x, int y) {
    if (!inBounds(x, y) || !g.map[y][x].explored[0]) return 0.0f;
    float nightNeed = clamp01((0.88f - getBrightness()) / 0.88f);
    if (nightNeed <= 0.02f) return 0.0f;

    float light = 0.0f;
    for (const auto& e : g.entities) {
        if (!e.alive || e.underConstruction || !isBuilding(e.type)) continue;
        float strength = torchStrength(e.type);
        float radius = torchRadius(e.type);
        if (strength <= 0.0f || radius <= 0.0f) continue;

        const auto& st = STATS[e.type];
        int left = e.x, right = e.x + std::max(1, st.sizeW) - 1;
        int top = e.y, bottom = e.y + std::max(1, st.sizeH) - 1;
        int cx = e.x + st.sizeW / 2, cy = e.y + st.sizeH / 2;
        if (e.owner != 0 && (!inBounds(cx, cy) || !g.map[cy][cx].visible[0])) continue;

        int dx = 0;
        if (x < left) dx = left - x;
        else if (x > right) dx = x - right;
        int dy = 0;
        if (y < top) dy = top - y;
        else if (y > bottom) dy = y - bottom;
        float dist = std::sqrt((float)(dx * dx + dy * dy));
        if (dist > radius) continue;

        float local = strength * std::pow(clamp01(1.0f - dist / radius), 1.85f);
        if (dx == 0 && dy == 0) local *= 0.40f;
        light = std::max(light, local);
    }
    if (labLightOverride.enabled && labLightOverride.radius > 0.0f && labLightOverride.strength > 0.0f) {
        float dx = (float)(x - labLightOverride.x);
        float dy = (float)(y - labLightOverride.y);
        float d = std::sqrt(dx * dx + dy * dy);
        if (d <= labLightOverride.radius) {
            float local = labLightOverride.strength
                * std::pow(clamp01(1.0f - d / labLightOverride.radius), 1.85f);
            light = std::max(light, local);
        }
    }
    return clamp01(light * nightNeed);
}

static Color applyVisionAndLight(Color c, int x, int y) {
    float vis = visibleFadeAt(x, y);
    if (vis <= 0.0f) return rgb(8, 9, 12);
    if (vis < 1.0f) c = blend(scale(c, 0.34f + 0.66f * vis), rgb(5, 7, 12), (1.0f - vis) * 0.22f);

    float torch = torchLightAt(x, y);
    if (torch > 0.0f) {
        c = blend(c, rgb(238, 122, 52), torch * 0.14f);
        c = scale(c, 1.0f + torch * 0.12f);
    }
    return c;
}

static Color applyVisionToGlyph(Color c, int x, int y) {
    c = timeTint(c);
    float vis = visibleFadeAt(x, y);
    if (vis < 1.0f) c = scale(c, 0.50f + 0.50f * vis);
    float torch = torchLightAt(x, y);
    if (torch > 0.0f) c = blend(c, rgb(238, 150, 82), torch * 0.10f);
    return c;
}

static void hatch(SDL_Rect r, Color c, int step, bool diagonal) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    if (diagonal) {
        for (int x = r.x - r.h; x < r.x + r.w; x += step)
            SDL_RenderDrawLine(s.ren, x, r.y + r.h, x + r.h, r.y);
    } else {
        for (int y = r.y; y < r.y + r.h; y += step)
            SDL_RenderDrawLine(s.ren, r.x, y, r.x + r.w, y);
    }
}

static void applyTerrainTexture(SDL_Rect r, const Tile& t, int x, int y) {
    unsigned h = hash2(x,y,1900u);
    switch (t.terrain) {
        case T_TALL_GRASS:
        case T_REEDS:
            hatch(r, rgb(220,255,210,42), std::max(5, s.tile/3), false); break;
        case T_FOREST:
        case T_PINE:
            hatch(r, rgb(8,35,12,45), std::max(6, s.tile/3), true); break;
        case T_WATER:
        case T_SHALLOWS:
            hatch(r, rgb(190,235,255,38), std::max(6, s.tile/3), false); break;
        case T_DUNES:
        case T_SAND:
            hatch(r, rgb(255,230,160,32), std::max(7, s.tile/2), false); break;
        case T_GRAVEL:
        case T_STONE:
        case T_MOUNTAIN:
            if (h & 1u) {
                hatch(r, rgb(255,255,255,26), std::max(5, s.tile/3), true);
            }
            break;
        case T_LAVA:
            hatch(r, rgb(255,160,60,55), std::max(5, s.tile/3), true); break;
        default: break;
    }
}

static bool mobileForcedByEnv(bool& value) {
    const char* env = std::getenv("REALM_MOBILE_GUI");
    if (!env || !*env) return false;
    std::string v(env);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
    value = !(v == "0" || v == "false" || v == "off" || v == "no");
    return true;
}

static bool isMobileGui() {
    bool forced = false;
    if (mobileForcedByEnv(forced)) return forced;
    int shortSide = std::min(s.winW, s.winH);
    return s.winW < 760 || s.winH < 560 || (s.winH > s.winW && s.winW <= 900) || shortSide <= 520;
}

static bool mobilePortrait() {
    if (!isMobileGui()) return false;
    if (s.mobileOrientation == 1) return true;
    if (s.mobileOrientation == 2) return false;
    return s.winH >= s.winW;
}

static bool isAsciiMobileGui() {
    return displayMode == DM_ASCII && isMobileGui();
}

static void asciiMobileCellMetrics(int& cellW, int& cellH) {
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    int w = 0, h = 0;
    if (font) TTF_SizeText(font, "M", &w, &h);
    cellW = std::max(8, w);
    cellH = std::max(15, font ? TTF_FontLineSkip(font) : h);
}

static int mobileSafePad() {
    return std::max(8, (int)std::lround(10.0f * s.mobileUiScale));
}

static int mobileHudExtent() {
    if (mobilePortrait()) {
        int preferred = (int)std::lround(s.winH * 0.42f);
        int maxHud = std::max(250, s.winH - 220);
        return std::max(250, std::min(preferred, maxHud));
    }
    int preferred = (int)std::lround(s.winW * 0.34f);
    int maxHud = std::max(280, s.winW - 320);
    return std::max(280, std::min(preferred, maxHud));
}

static SDL_Rect mapRect() {
    if (isMobileGui()) {
        int hud = mobileHudExtent();
        if (mobilePortrait()) {
            return SDL_Rect{0, 0, s.winW, std::max(1, s.winH - hud)};
        }
        return SDL_Rect{0, 0, std::max(1, s.winW - hud), s.winH};
    }
    return SDL_Rect{0, s.topH, std::max(1, s.winW - s.panelW), std::max(1, s.winH - s.topH - s.bottomH)};
}

static int mapSafeMargin() {
    return std::max(24, std::min(56, s.tile + 10));
}

static SDL_Rect insetRect(SDL_Rect r, int inset) {
    inset = std::max(0, std::min(inset, std::min(r.w, r.h) / 3));
    return SDL_Rect{r.x + inset, r.y + inset,
                    std::max(1, r.w - inset * 2),
                    std::max(1, r.h - inset * 2)};
}

static SDL_Rect mapSafeRect() {
    return insetRect(mapRect(), mapSafeMargin());
}

static SDL_Rect panelRect() {
    if (isMobileGui()) {
        int hud = mobileHudExtent();
        if (mobilePortrait()) return SDL_Rect{0, std::max(1, s.winH - hud), s.winW, hud};
        return SDL_Rect{std::max(1, s.winW - hud), 0, hud, s.winH};
    }
    return SDL_Rect{s.winW - s.panelW, 0, s.panelW, s.winH};
}

static SDL_Rect miniMapRect() {
    SDL_Rect pr = panelRect();
    if (isMobileGui()) {
        int pad = mobileSafePad();
        if (mobilePortrait()) {
            int w = std::max(118, std::min(pr.w / 3, 180));
            int h = std::max(74, std::min(110, pr.h / 3));
            return SDL_Rect{pr.x + pr.w - pad - w, pr.y + pad + 36, w, h};
        }
        int w = std::max(1, pr.w - pad * 2);
        int h = std::max(88, std::min(132, pr.h / 4));
        return SDL_Rect{pr.x + pad, pr.y + pad + 42, w, h};
    }
    return SDL_Rect{pr.x + 14, 12, std::max(1, pr.w - 28), 110};
}

static int isoHalfW() { return std::max(8, s.tile); }
static int isoHalfH() { return std::max(5, s.tile / 2); }

static void isoOrigin(int& ox, int& oy) {
    SDL_Rect mr = mapRect();
    int hw = isoHalfW();
    int hh = isoHalfH();
    int bboxW = std::max(1, (g.viewW + g.viewH) * hw);
    int bboxH = std::max(1, (g.viewW + g.viewH) * hh + hh);
    // Centre the diamond block inside the map pane.  The x-origin is the
    // screen centre of map tile (viewX, viewY).
    ox = mr.x + mr.w / 2 - ((g.viewW - g.viewH) * hw) / 2;
    oy = mr.y + std::max(0, (mr.h - bboxH) / 2);
    (void)bboxW;
}

static void isoTileCenterFromScreenOffset(int sx, int sy, int& cx, int& cy) {
    int ox, oy; isoOrigin(ox, oy);
    int hw = isoHalfW();
    int hh = isoHalfH();
    cx = ox + (sx - sy) * hw;
    cy = oy + (sx + sy) * hh + hh;
}

static void isoScreenToOffsetFloat(int px, int py, float& sx, float& sy) {
    int ox, oy; isoOrigin(ox, oy);
    int hw = isoHalfW();
    int hh = isoHalfH();
    float fx = (px - ox) / (float)hw;
    float fy = (py - oy - hh) / (float)hh;
    sx = (fy + fx) * 0.5f;
    sy = (fy - fx) * 0.5f;
}

struct IsoOffsetBounds {
    int minSx = 0, maxSx = 0;
    int minSy = 0, maxSy = 0;
};

static IsoOffsetBounds isoOffsetBoundsForRect(SDL_Rect mr, int expand) {
    float minSx = 1e9f, minSy = 1e9f;
    float maxSx = -1e9f, maxSy = -1e9f;
    const int px[4] = {mr.x, mr.x + mr.w - 1, mr.x, mr.x + mr.w - 1};
    const int py[4] = {mr.y, mr.y, mr.y + mr.h - 1, mr.y + mr.h - 1};
    for (int i = 0; i < 4; ++i) {
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(px[i], py[i], sx, sy);
        minSx = std::min(minSx, sx); maxSx = std::max(maxSx, sx);
        minSy = std::min(minSy, sy); maxSy = std::max(maxSy, sy);
    }

    return IsoOffsetBounds{
        (int)std::floor(minSx) - expand,
        (int)std::ceil(maxSx) + expand,
        (int)std::floor(minSy) - expand,
        (int)std::ceil(maxSy) + expand
    };
}

static IsoOffsetBounds isoVisibleOffsetBounds() {
    // Expand by a small border so partially visible diamonds at pane edges draw.
    return isoOffsetBoundsForRect(mapRect(), 3);
}

static IsoOffsetBounds isoSafeOffsetBounds() {
    return isoOffsetBoundsForRect(mapSafeRect(), 0);
}

static int topDownFullColumnsForRect(SDL_Rect mr) {
    return std::max(1, std::min(MAP_W, mr.w / std::max(1, s.tile)));
}

static int topDownFullRowsForRect(SDL_Rect mr) {
    return std::max(1, std::min(MAP_H, mr.h / std::max(1, s.tile)));
}

static int topDownSafeColumns() { return topDownFullColumnsForRect(mapSafeRect()); }
static int topDownSafeRows() { return topDownFullRowsForRect(mapSafeRect()); }

static void cameraBounds(int& minX, int& maxX, int& minY, int& maxY) {
    if (s.isometric) {
        IsoOffsetBounds b = isoSafeOffsetBounds();
        minX = -b.maxSx;
        maxX = MAP_W - 1 - b.minSx;
        minY = -b.maxSy;
        maxY = MAP_H - 1 - b.minSy;
        return;
    }

    int safeW = topDownSafeColumns();
    int safeH = topDownSafeRows();
    int insetTiles = std::max(1, mapSafeMargin() / std::max(1, s.tile));
    minX = -insetTiles;
    minY = -insetTiles;
    maxX = MAP_W - safeW + insetTiles;
    maxY = MAP_H - safeH + insetTiles;
}

static bool pointInDiamond(int px, int py, int cx, int cy, int hw, int hh) {
    if (hw <= 0 || hh <= 0) return false;
    float dx = std::abs(px - cx) / (float)hw;
    float dy = std::abs(py - cy) / (float)hh;
    return dx + dy <= 1.0f;
}

static void fillDiamond(int cx, int cy, int hw, int hh, Color c) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    for (int dy = -hh; dy <= hh; ++dy) {
        float t = 1.0f - std::abs(dy) / (float)std::max(1, hh);
        int span = std::max(0, (int)std::round(hw * t));
        SDL_RenderDrawLine(s.ren, cx - span, cy + dy, cx + span, cy + dy);
    }
}

static void drawDiamondOutline(int cx, int cy, int hw, int hh, Color c) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    SDL_RenderDrawLine(s.ren, cx, cy - hh, cx + hw, cy);
    SDL_RenderDrawLine(s.ren, cx + hw, cy, cx, cy + hh);
    SDL_RenderDrawLine(s.ren, cx, cy + hh, cx - hw, cy);
    SDL_RenderDrawLine(s.ren, cx - hw, cy, cx, cy - hh);
}

static void hatchDiamond(int cx, int cy, int hw, int hh, Color c, int step) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    step = std::max(3, step);
    for (int dy = -hh + step; dy < hh; dy += step) {
        float t = 1.0f - std::abs(dy) / (float)std::max(1, hh);
        int span = std::max(0, (int)std::round(hw * t));
        SDL_RenderDrawLine(s.ren, cx - span, cy + dy, cx + span, cy + dy);
    }
}

static void sparkleDiamond(int cx, int cy, int hw, int hh, Color c, int x, int y) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    unsigned h = hash2(x, y, 7811u);
    int count = 1 + (h & 3u);
    for (int i = 0; i < count; ++i) {
        int lx = (int)((hash2(x, y, 7900u + i) % (unsigned)(hw * 2 + 1)) - hw);
        int maxY = std::max(1, (int)(hh * (1.0f - std::abs(lx)/(float)std::max(1, hw))));
        int ly = (int)((hash2(x, y, 8000u + i) % (unsigned)(maxY * 2 + 1)) - maxY);
        SDL_RenderDrawPoint(s.ren, cx + lx, cy + ly);
        if (s.tile >= 28) SDL_RenderDrawPoint(s.ren, cx + lx + 1, cy + ly);
    }
}

static void applyTerrainTextureIso(int cx, int cy, int hw, int hh, const Tile& t, int x, int y) {
    switch (t.terrain) {
        case T_TALL_GRASS:
        case T_REEDS:
            hatchDiamond(cx, cy, hw, hh, rgb(225,255,210,48), std::max(4, s.tile/5)); break;
        case T_FOREST:
        case T_PINE:
            hatchDiamond(cx, cy, hw, hh, rgb(5,30,10,58), std::max(5, s.tile/4)); break;
        case T_WATER:
        case T_SHALLOWS:
            hatchDiamond(cx, cy, hw, hh, rgb(200,240,255,42), std::max(5, s.tile/4)); break;
        case T_SAND:
        case T_DUNES:
            hatchDiamond(cx, cy, hw, hh, rgb(255,230,165,36), std::max(5, s.tile/4)); break;
        case T_STONE:
        case T_GRAVEL:
        case T_MOUNTAIN:
            hatchDiamond(cx, cy, hw, hh, rgb(255,255,255,24), std::max(5, s.tile/4)); break;
        case T_LAVA:
            hatchDiamond(cx, cy, hw, hh, rgb(255,160,60,58), std::max(4, s.tile/5)); break;
        case T_FLOWERS:
            sparkleDiamond(cx, cy, hw, hh, rgb(255,220,250,70), x, y); break;
        default:
            if (terrainFamily(t.terrain) == 0) sparkleDiamond(cx, cy, hw, hh, rgb(230,255,210,32), x, y);
            break;
    }
}

static void updateViewMetrics(bool keepCursor = true) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    SDL_Rect mr = mapRect();

    if (s.isometric) {
        int hw = isoHalfW();
        int hh = isoHalfH();
        int sumByWidth  = (mr.w + hw - 1) / std::max(1, hw) + 4;
        int sumByHeight = (mr.h + hh - 1) / std::max(1, hh) + 4;
        int targetSum = std::max(12, std::max(sumByWidth, sumByHeight));
        int aspectW = std::max(6, (targetSum * 3) / 5);
        int aspectH = std::max(6, targetSum - aspectW);
        if (aspectW > MAP_W) {
            aspectW = MAP_W;
            aspectH = std::max(6, targetSum - aspectW);
        }
        if (aspectH > MAP_H) {
            aspectH = MAP_H;
            aspectW = std::max(6, targetSum - aspectH);
        }
        g.viewW = std::max(1, std::min(MAP_W, aspectW));
        g.viewH = std::max(1, std::min(MAP_H, aspectH));
    } else {
        int tile = std::max(8, s.tile);
        g.viewW = std::max(1, std::min(MAP_W, (mr.w + tile - 1) / tile));
        g.viewH = std::max(1, std::min(MAP_H, (mr.h + tile - 1) / tile));
    }

    if (keepCursor) {
        if (s.isometric) {
            IsoOffsetBounds b = isoSafeOffsetBounds();
            int minOffsetX = b.minSx;
            int maxOffsetX = b.maxSx;
            int minOffsetY = b.minSy;
            int maxOffsetY = b.maxSy;
            if (minOffsetX > maxOffsetX) { minOffsetX = b.minSx; maxOffsetX = b.maxSx; }
            if (minOffsetY > maxOffsetY) { minOffsetY = b.minSy; maxOffsetY = b.maxSy; }

            int offsetX = g.cursorX - g.viewX;
            int offsetY = g.cursorY - g.viewY;
            if (offsetX < minOffsetX) g.viewX = g.cursorX - minOffsetX;
            if (offsetX > maxOffsetX) g.viewX = g.cursorX - maxOffsetX;
            if (offsetY < minOffsetY) g.viewY = g.cursorY - minOffsetY;
            if (offsetY > maxOffsetY) g.viewY = g.cursorY - maxOffsetY;
        } else {
            int fullW = topDownSafeColumns();
            int fullH = topDownSafeRows();
            int insetTiles = std::max(1, mapSafeMargin() / std::max(1, s.tile));
            if (g.cursorX < g.viewX + insetTiles) g.viewX = g.cursorX - insetTiles;
            if (g.cursorY < g.viewY + insetTiles) g.viewY = g.cursorY - insetTiles;
            if (g.cursorX >= g.viewX + fullW) g.viewX = g.cursorX - fullW + 1 + insetTiles;
            if (g.cursorY >= g.viewY + fullH) g.viewY = g.cursorY - fullH + 1 + insetTiles;
        }
    }
    int minX, maxX, minY, maxY;
    cameraBounds(minX, maxX, minY, maxY);
    g.viewX = std::max(minX, std::min(g.viewX, maxX));
    g.viewY = std::max(minY, std::min(g.viewY, maxY));
}

static void clampView() {
    int minX, maxX, minY, maxY;
    cameraBounds(minX, maxX, minY, maxY);
    g.viewX = std::max(minX, std::min(g.viewX, maxX));
    g.viewY = std::max(minY, std::min(g.viewY, maxY));
}

static void centerViewOnTile(int mx, int my) {
    updateViewMetrics(false);
    mx = std::max(0, std::min(mx, MAP_W - 1));
    my = std::max(0, std::min(my, MAP_H - 1));

    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        g.viewX = mx - g.viewW / 2;
        g.viewY = my - g.viewH / 2;
        clampTerminalView();
        return;
    }

    if (s.isometric) {
        SDL_Rect safe = mapSafeRect();
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(safe.x + safe.w / 2, safe.y + safe.h / 2, sx, sy);
        g.viewX = mx - (int)std::lround(sx);
        g.viewY = my - (int)std::lround(sy);
    } else {
        g.viewX = mx - topDownSafeColumns() / 2;
        g.viewY = my - topDownSafeRows() / 2;
    }
    clampView();
}

static bool screenToMiniMapTile(int px, int py, int& mx, int& my, bool clampToMiniMap = false) {
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int panelW = 24;
        int panelX = frame.cols - panelW;
        if (panelX < 1) return false;
        int mmW = panelW - 2;
        int mmH = std::max(1, std::min(g.viewH / 3, 14));
        SDL_Rect r{(panelX + 1) * frame.cellW, frame.cellH,
                   mmW * frame.cellW, mmH * frame.cellH};
        if (clampToMiniMap) {
            px = std::max(r.x, std::min(px, r.x + r.w - 1));
            py = std::max(r.y, std::min(py, r.y + r.h - 1));
        } else if (px < r.x || py < r.y || px >= r.x + r.w || py >= r.y + r.h) {
            return false;
        }

        int lx = std::max(0, std::min(px - r.x, r.w - 1));
        int ly = std::max(0, std::min(py - r.y, r.h - 1));
        mx = std::max(0, std::min(lx * MAP_W / std::max(1, r.w), MAP_W - 1));
        my = std::max(0, std::min(ly * MAP_H / std::max(1, r.h), MAP_H - 1));
        return true;
    }

    SDL_Rect r = miniMapRect();
    if (clampToMiniMap) {
        px = std::max(r.x, std::min(px, r.x + r.w - 1));
        py = std::max(r.y, std::min(py, r.y + r.h - 1));
    } else if (px < r.x || py < r.y || px >= r.x + r.w || py >= r.y + r.h) {
        return false;
    }

    int lx = std::max(0, std::min(px - r.x, r.w - 1));
    int ly = std::max(0, std::min(py - r.y, r.h - 1));
    mx = std::max(0, std::min(lx * MAP_W / std::max(1, r.w), MAP_W - 1));
    my = std::max(0, std::min(ly * MAP_H / std::max(1, r.h), MAP_H - 1));
    return true;
}

static bool moveViewFromMiniMap(int px, int py, bool clampToMiniMap = false) {
    int mx = 0, my = 0;
    if (!screenToMiniMapTile(px, py, mx, my, clampToMiniMap)) return false;
    g.cursorX = mx;
    g.cursorY = my;
    centerViewOnTile(mx, my);
    return true;
}

static bool screenToMap(int px, int py, int& mx, int& my) {
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        SDL_Rect mr = terminalMapPixelRect(frame);
        if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;
        int sx = (px - mr.x) / std::max(1, mapCellW);
        int sy = (py - mr.y) / std::max(1, mapCellH);
        if (sx < 0 || sy < 0 || sx >= g.viewW || sy >= g.viewH) return false;
        mx = g.viewX + sx;
        my = g.viewY + sy;
        return inBounds(mx, my);
    }

    SDL_Rect mr = mapRect();
    if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;

    if (isAsciiMobileGui()) {
        int cellW = 8, cellH = 15;
        asciiMobileCellMetrics(cellW, cellH);
        int sx = (px - mr.x) / std::max(1, cellW);
        int sy = (py - mr.y) / std::max(1, cellH);
        mx = g.viewX + sx;
        my = g.viewY + sy;
        int cols = std::max(1, std::min(MAP_W, mr.w / std::max(1, cellW)));
        int rows = std::max(1, std::min(MAP_H, mr.h / std::max(1, cellH)));
        return inBounds(mx, my) && sx < cols && sy < rows;
    }

    if (!s.isometric) {
        int sx = (px - mr.x) / s.tile;
        int sy = (py - mr.y) / s.tile;
        mx = g.viewX + sx;
        my = g.viewY + sy;
        return inBounds(mx,my) && sx < g.viewW && sy < g.viewH;
    }

    int hw = isoHalfW();
    int hh = isoHalfH();
    float sxF = 0.0f, syF = 0.0f;
    isoScreenToOffsetFloat(px, py, sxF, syF);
    int baseX = (int)std::floor(sxF);
    int baseY = (int)std::floor(syF);
    IsoOffsetBounds b = isoVisibleOffsetBounds();

    for (int dy = -1; dy <= 2; ++dy) {
        for (int dx = -1; dx <= 2; ++dx) {
            int sx = baseX + dx, sy = baseY + dy;
            if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) continue;
            int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
            if (!pointInDiamond(px, py, cx, cy, hw, hh)) continue;
            mx = g.viewX + sx;
            my = g.viewY + sy;
            return inBounds(mx,my);
        }
    }
    return false;
}

static bool screenToMapOffset(int px, int py, int& sxOut, int& syOut) {
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        SDL_Rect mr = terminalMapPixelRect(frame);
        if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;
        sxOut = (px - mr.x) / std::max(1, mapCellW);
        syOut = (py - mr.y) / std::max(1, mapCellH);
        return sxOut >= 0 && syOut >= 0 && sxOut < g.viewW && syOut < g.viewH;
    }

    SDL_Rect mr = mapRect();
    if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;

    if (isAsciiMobileGui()) {
        int cellW = 8, cellH = 15;
        asciiMobileCellMetrics(cellW, cellH);
        sxOut = (px - mr.x) / std::max(1, cellW);
        syOut = (py - mr.y) / std::max(1, cellH);
        return true;
    }

    if (!s.isometric) {
        sxOut = (px - mr.x) / std::max(1, s.tile);
        syOut = (py - mr.y) / std::max(1, s.tile);
        return true;
    }

    int hw = isoHalfW();
    int hh = isoHalfH();
    float sxF = 0.0f, syF = 0.0f;
    isoScreenToOffsetFloat(px, py, sxF, syF);
    int baseX = (int)std::floor(sxF);
    int baseY = (int)std::floor(syF);
    IsoOffsetBounds b = isoVisibleOffsetBounds();

    for (int dy = -1; dy <= 2; ++dy) {
        for (int dx = -1; dx <= 2; ++dx) {
            int sx = baseX + dx, sy = baseY + dy;
            if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) continue;
            int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
            if (!pointInDiamond(px, py, cx, cy, hw, hh)) continue;
            sxOut = sx;
            syOut = sy;
            return true;
        }
    }

    sxOut = baseX;
    syOut = baseY;
    return true;
}

static bool mapTileScreenCenter(int mx, int my, int& px, int& py) {
    if (!inBounds(mx, my)) return false;
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        SDL_Rect mr = terminalMapPixelRect(frame);
        int sx = mx - g.viewX;
        int sy = my - g.viewY;
        if (sx < 0 || sy < 0 || sx >= g.viewW || sy >= g.viewH) return false;
        px = mr.x + sx * mapCellW + mapCellW / 2;
        py = mr.y + sy * mapCellH + mapCellH / 2;
        return px >= 0 && py >= 0 && px < s.winW && py < s.winH;
    }

    SDL_Rect mr = mapRect();
    int sx = mx - g.viewX;
    int sy = my - g.viewY;

    if (!s.isometric) {
        if (sx < 0 || sy < 0 || sx >= g.viewW || sy >= g.viewH) return false;
        px = mr.x + sx * s.tile + s.tile / 2;
        py = mr.y + sy * s.tile + s.tile / 2;
    } else {
        IsoOffsetBounds b = isoVisibleOffsetBounds();
        if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) return false;
        isoTileCenterFromScreenOffset(sx, sy, px, py);
    }

    return px >= mr.x && py >= mr.y && px < mr.x + mr.w && py < mr.y + mr.h;
}

static bool mapTileAtViewportCenter(int& mx, int& my, int& px, int& py) {
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        mx = g.viewX + g.viewW / 2;
        my = g.viewY + g.viewH / 2;
        mx = std::max(0, std::min(mx, MAP_W - 1));
        my = std::max(0, std::min(my, MAP_H - 1));
        return mapTileScreenCenter(mx, my, px, py);
    }

    SDL_Rect safe = mapSafeRect();
    px = safe.x + safe.w / 2;
    py = safe.y + safe.h / 2;
    if (screenToMap(px, py, mx, my)) return true;

    if (s.isometric) {
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(px, py, sx, sy);
        mx = g.viewX + (int)std::lround(sx);
        my = g.viewY + (int)std::lround(sy);
    } else {
        mx = g.viewX + topDownSafeColumns() / 2;
        my = g.viewY + topDownSafeRows() / 2;
    }
    mx = std::max(0, std::min(mx, MAP_W - 1));
    my = std::max(0, std::min(my, MAP_H - 1));
    return true;
}

static void chooseZoomAnchor(int requestedX, int requestedY, int& anchorX, int& anchorY, int& mx, int& my) {
    if (requestedX >= 0 && requestedY >= 0 && screenToMap(requestedX, requestedY, mx, my)) {
        anchorX = requestedX;
        anchorY = requestedY;
        return;
    }
    if (mapTileScreenCenter(g.cursorX, g.cursorY, anchorX, anchorY)) {
        mx = g.cursorX;
        my = g.cursorY;
        return;
    }
    mapTileAtViewportCenter(mx, my, anchorX, anchorY);
}

static void setZoom(int newTile, int anchorX = -1, int anchorY = -1) {
    int oldTile = s.tile;
    newTile = std::max(14, std::min(44, newTile));
    if (newTile == oldTile) return;

    int oldMx = g.cursorX, oldMy = g.cursorY;
    int fixedX = anchorX, fixedY = anchorY;
    chooseZoomAnchor(anchorX, anchorY, fixedX, fixedY, oldMx, oldMy);

    s.tile = newTile;
    g.cursorX = std::max(0, std::min(oldMx, MAP_W-1));
    g.cursorY = std::max(0, std::min(oldMy, MAP_H-1));
    updateViewMetrics(false);

    int newSx = 0, newSy = 0;
    if (screenToMapOffset(fixedX, fixedY, newSx, newSy)) {
        g.viewX = g.cursorX - newSx;
        g.viewY = g.cursorY - newSy;
        clampView();
    } else {
        centerViewOnTile(g.cursorX, g.cursorY);
    }
    // Text textures may be re-used scaled; no need to rebuild font cache.
}

static void startMiddlePan(int px, int py) {
    s.middleDown = true;
    s.leftDown = false;
    g.dragging = false;
    s.panStartMouseX = px;
    s.panStartMouseY = py;
    s.panStartViewX = g.viewX;
    s.panStartViewY = g.viewY;
}

static void updateMiddlePan(int px, int py) {
    int dx = px - s.panStartMouseX;
    int dy = py - s.panStartMouseY;
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        g.viewX = s.panStartViewX - (int)std::lround(dx / (float)std::max(1, mapCellW));
        g.viewY = s.panStartViewY - (int)std::lround(dy / (float)std::max(1, mapCellH));
    } else if (s.isometric) {
        int hw = std::max(1, isoHalfW());
        int hh = std::max(1, isoHalfH());
        float viewDx = -0.5f * (dx / (float)hw + dy / (float)hh);
        float viewDy =  0.5f * (dx / (float)hw - dy / (float)hh);
        g.viewX = s.panStartViewX + (int)std::lround(viewDx);
        g.viewY = s.panStartViewY + (int)std::lround(viewDy);
    } else {
        g.viewX = s.panStartViewX - (int)std::lround(dx / (float)std::max(1, s.tile));
        g.viewY = s.panStartViewY - (int)std::lround(dy / (float)std::max(1, s.tile));
    }
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) clampTerminalView();
    else clampView();
}

static void moveCursorToViewCenter() {
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        g.cursorX = g.viewX + g.viewW / 2;
        g.cursorY = g.viewY + g.viewH / 2;
    } else if (s.isometric) {
        SDL_Rect safe = mapSafeRect();
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(safe.x + safe.w / 2, safe.y + safe.h / 2, sx, sy);
        g.cursorX = g.viewX + (int)std::lround(sx);
        g.cursorY = g.viewY + (int)std::lround(sy);
    } else {
        g.cursorX = g.viewX + topDownSafeColumns() / 2;
        g.cursorY = g.viewY + topDownSafeRows() / 2;
    }
    g.cursorX = std::max(0, std::min(g.cursorX, MAP_W - 1));
    g.cursorY = std::max(0, std::min(g.cursorY, MAP_H - 1));
}

static int keyToInput(SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z) return 'a' + (int)(key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9) return '0' + (int)(key - SDLK_0);
    switch (key) {
        case SDLK_UP: return KEY_UP;
        case SDLK_DOWN: return KEY_DOWN;
        case SDLK_LEFT: return KEY_LEFT;
        case SDLK_RIGHT: return KEY_RIGHT;
        case SDLK_RETURN: return '\n';
        case SDLK_KP_ENTER: return '\n';
        case SDLK_ESCAPE: return 27;
        case SDLK_SPACE: return ' ';
        case SDLK_PAGEUP: return KEY_PPAGE;
        case SDLK_PAGEDOWN: return KEY_NPAGE;
        case SDLK_HOME: return KEY_HOME;
        case SDLK_END: return KEY_END;
        case SDLK_EQUALS: return '=';
        case SDLK_MINUS: return '-';
        default: return 0;
    }
}

static const char* seasonNameSafe() { return getSeasonName(); }
static const char* timeNameSafe() { return getTimeName(); }
static const char* weatherName() {
    switch (g.weather) {
        case W_RAIN: return "Rain";
        case W_STORM: return "Storm";
        case W_SNOW: return "Snow";
        default: return "Clear";
    }
}

static std::string trimPanelLine(const std::string& s, size_t maxLen = 32) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 1) + "~";
}

static std::string cursorTileSummary() {
    if (!inBounds(g.cursorX, g.cursorY)) return "Tile: out of bounds";
    const Tile& t = g.map[g.cursorY][g.cursorX];
    std::ostringstream ss;
    ss << terrainName(t.terrain) << " / " << biomeName(t.biome);
    if (t.resources > 0) ss << " / " << t.resources << " res";
    return trimPanelLine(ss.str());
}

static std::string cursorStackSummary() {
    if (!inBounds(g.cursorX, g.cursorY) || !g.map[g.cursorY][g.cursorX].visible[0]) return "Stack: not visible";
    std::ostringstream ss;
    int count = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.state == S_GARRISONED) continue;
        auto& st = STATS[e.type];
        bool covers = st.isBuilding
            ? (g.cursorX >= e.x && g.cursorX < e.x + st.sizeW && g.cursorY >= e.y && g.cursorY < e.y + st.sizeH)
            : (g.cursorX == e.x && g.cursorY == e.y);
        if (!covers) continue;
        if (count++ > 0) ss << ", ";
        if (e.owner == 0) ss << "You ";
        else if (e.owner == OWNER_NATURE) ss << "Neutral ";
        else ss << "P" << (e.owner + 1) << ' ';
        ss << st.name;
        if (count >= 3) break;
    }
    if (count == 0) return "Stack: empty";
    if (count < (int)g.entities.size()) {
        int more = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.state == S_GARRISONED) continue;
            auto& st = STATS[e.type];
            bool covers = st.isBuilding
                ? (g.cursorX >= e.x && g.cursorX < e.x + st.sizeW && g.cursorY >= e.y && g.cursorY < e.y + st.sizeH)
                : (g.cursorX == e.x && g.cursorY == e.y);
            if (covers) more++;
        }
        if (more > count) ss << " +" << (more - count);
    }
    return trimPanelLine("Stack: " + ss.str());
}

struct TileVisual {
    bool visible = false;
    bool explored = false;
    bool cursor = false;
    bool selected = false;
    Entity* ent = nullptr;
    Color bg = rgb(0,0,0);
    Color fg = rgb(230,230,220);
    std::string glyph;
    bool emoji = false;
    bool tint = false;
};

static TileVisual makeTileVisual(int mx, int my) {
    TileVisual v;
    const Tile& tile = g.map[my][mx];
    v.visible = tile.visible[0];
    v.explored = tile.explored[0];
    if (!v.explored) { v.bg = rgb(8,9,12); return v; }

    v.ent = v.visible ? entityAt(mx,my) : nullptr;
    if (!v.ent && v.visible) v.ent = corpseAt(mx, my);
    v.cursor = (mx == g.cursorX && my == g.cursorY);
    v.bg = terrainBg(tile, mx, my);

    if (v.ent && v.ent->alive && v.ent->owner != OWNER_NATURE && (isUnit(v.ent->type) || isBuilding(v.ent->type)))
        v.bg = timeTint(ownerBg(v.ent->owner));
    v.bg = applyVisionAndLight(v.bg, mx, my);
    if (v.cursor) v.bg = blend(v.bg, rgb(225, 190, 50), 0.78f);

    v.fg = glyphColorForTerrain(tile, mx, my);
    if (displayMode == DM_ASCII) {
        v.emoji = false;
        if (v.visible && v.ent && v.ent->alive) {
            v.glyph.assign(1, STATS[v.ent->type].glyph);
            v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(230,230,210) : rgb(255,255,255);
        } else if (v.visible && v.ent && v.ent->state == S_DEAD) {
            v.glyph.assign(1, v.ent->deathTicks >= DEATH_DECAY_TICKS ? '*' : '%');
            v.fg = rgb(180,180,170);
        } else if (v.visible) {
            v.glyph.assign(1, terrainAscii(tile.terrain));
        } else {
            v.glyph = "."; v.fg = rgb(95,95,105,150);
        }
    } else if (v.visible && v.ent && v.ent->alive) {
        bool usesSymbolFont = false;
        v.glyph = tilesetEntityVisual(*v.ent, usesSymbolFont);
        v.emoji = usesSymbolFont;
        v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(245,245,235) : rgb(255,255,255);
        v.tint = usesSymbolFont;
    } else if (v.visible && v.ent && v.ent->state == S_DEAD) {
        bool usesSymbolFont = false;
        v.glyph = tilesetEntityVisual(*v.ent, usesSymbolFont);
        v.emoji = usesSymbolFont;
        v.fg = rgb(190,190,180);
        v.tint = false;
    } else if (v.visible) {
        logMissingTerrainImageTile(tile.terrain);
        logMissingVisualTileParts(tile);
        v.glyph = terrainGlyph(tile, mx, my);
        v.emoji = isResourceEmojiTerrain(tile.terrain);
        v.tint = v.emoji;
    } else {
        v.glyph = "·"; v.emoji = false; v.fg = rgb(95,95,105,150);
    }
    v.fg = applyVisionToGlyph(v.fg, mx, my);

    v.selected = (v.visible && v.ent && isSelected(v.ent));
    if (v.visible && !v.ent) {
        for (const auto& m : g.actionMarkers) {
            if (m.x == mx && m.y == my && m.ticks > 0 && (g.tick % 6) < 4) {
                v.glyph = (m.glyph == '#') ? u8"■" : (m.glyph == '!') ? "!" : u8"×";
                v.emoji = false;
                v.tint = false;
                v.fg = rgb(255,235,105);
                break;
            }
        }
    }
    return v;
}

static void drawTile(int mx, int my, SDL_Rect r) {
    const Tile& tile = g.map[my][mx];
    TileVisual v = makeTileVisual(mx, my);

    if (!v.explored) {
        setDraw(v.bg); SDL_RenderFillRect(s.ren, &r);
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(24,28,34,120)); SDL_RenderDrawRect(s.ren, &r);
        return;
    }

    setDraw(v.bg); SDL_RenderFillRect(s.ren, &r);
    applyTerrainTexture(r, tile, mx, my);

    if (!v.glyph.empty()) drawCentered(v.glyph, r, v.fg, v.emoji, v.tint);
    drawFeatureOccluderIfNeeded(mx, my, r);

    // HP sliver for damaged visible entities.
    if (v.visible && v.ent && v.ent->alive && v.ent->hp < v.ent->maxHp) {
        int w = std::max(1, r.w * v.ent->hp / std::max(1, v.ent->maxHp));
        SDL_Rect hb{r.x+2, r.y+r.h-4, std::max(1, r.w-4), 2};
        setDraw(rgb(80,20,20,190)); SDL_RenderFillRect(s.ren, &hb);
        hb.w = std::max(1, w-4);
        setDraw(v.ent->hp*3 > v.ent->maxHp*2 ? rgb(65,230,90) : v.ent->hp*3 > v.ent->maxHp ? rgb(230,210,70) : rgb(230,60,55));
        SDL_RenderFillRect(s.ren, &hb);
    }

    if (v.selected) {
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(255,255,255,180));
        SDL_RenderDrawRect(s.ren, &r);
        SDL_Rect r2{r.x+1,r.y+1,r.w-2,r.h-2}; SDL_RenderDrawRect(s.ren, &r2);
    }

    if (v.cursor) {
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(40,20,0,230));
        SDL_RenderDrawRect(s.ren, &r);
    }
}

static void toggleFullscreen() {
#if defined(REALM_WEB)
    EM_ASM({
        if (typeof window !== 'undefined' && typeof window.realmToggleFullscreen === 'function') {
            window.realmToggleFullscreen();
        }
    });
#else
    s.fullscreen = !s.fullscreen;
    if (SDL_SetWindowFullscreen(s.win, s.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        std::cerr << "realm: fullscreen toggle failed: " << SDL_GetError() << "\n";
        s.fullscreen = !s.fullscreen;
        return;
    }
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    updateViewMetrics(true);
    setStatus(s.fullscreen ? "Fullscreen." : "Windowed.");
#endif
}

static void drawMobileBuildPreviewTopDown() {
    if (!isMobileGui() || s.mobileBuildType == E_NONE) return;
    SDL_Rect mr = mapRect();
    EntityType bt = s.mobileBuildType;
    bool ok = canPlace(bt, g.cursorX, g.cursorY, 0);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    Color fill = ok ? rgb(70,210,120,72) : rgb(230,65,65,78);
    Color edge = ok ? rgb(130,255,170,220) : rgb(255,120,110,230);
    for (int dy = 0; dy < STATS[bt].sizeH; ++dy) {
        for (int dx = 0; dx < STATS[bt].sizeW; ++dx) {
            int mx = g.cursorX + dx, my = g.cursorY + dy;
            if (!inBounds(mx, my)) continue;
            int sx = mx - g.viewX, sy = my - g.viewY;
            if (sx < 0 || sy < 0 || sx >= g.viewW || sy >= g.viewH) continue;
            SDL_Rect r{mr.x + sx * s.tile, mr.y + sy * s.tile, s.tile, s.tile};
            setDraw(fill); SDL_RenderFillRect(s.ren, &r);
            setDraw(edge); SDL_RenderDrawRect(s.ren, &r);
        }
    }
}

static void drawMobileBuildPreviewIso() {
    if (!isMobileGui() || s.mobileBuildType == E_NONE) return;
    EntityType bt = s.mobileBuildType;
    bool ok = canPlace(bt, g.cursorX, g.cursorY, 0);
    Color fill = ok ? rgb(70,210,120,72) : rgb(230,65,65,78);
    Color edge = ok ? rgb(130,255,170,220) : rgb(255,120,110,230);
    for (int dy = 0; dy < STATS[bt].sizeH; ++dy) {
        for (int dx = 0; dx < STATS[bt].sizeW; ++dx) {
            int mx = g.cursorX + dx, my = g.cursorY + dy;
            if (!inBounds(mx, my)) continue;
            int cx, cy; isoTileCenterFromScreenOffset(mx - g.viewX, my - g.viewY, cx, cy);
            fillDiamond(cx, cy, isoHalfW(), isoHalfH(), fill);
            drawDiamondOutline(cx, cy, isoHalfW(), isoHalfH(), edge);
        }
    }
}

static void drawIsoTileBase(int mx, int my) {
    int sx = mx - g.viewX, sy = my - g.viewY;
    int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
    int hw = isoHalfW(), hh = isoHalfH();
    const Tile& tile = g.map[my][mx];
    TileVisual v = makeTileVisual(mx, my);
    fillDiamond(cx, cy, hw, hh, v.bg);
    if (v.explored) applyTerrainTextureIso(cx, cy, hw, hh, tile, mx, my);
    if (!v.explored) drawDiamondOutline(cx, cy, hw, hh, rgb(20,22,26,160));
}

static void drawIsoTileForeground(int mx, int my) {
    int sx = mx - g.viewX, sy = my - g.viewY;
    int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
    int hw = isoHalfW(), hh = isoHalfH();
    TileVisual v = makeTileVisual(mx, my);

    if (!v.glyph.empty()) {
        // Upright sprite/glyph over the flat isometric board.  The diamond is
        // isometric; the emoji/text itself is not skewed.
        int glyphSize = v.emoji ? std::max(16, (int)(s.tile * 0.96f)) : std::max(12, (int)(s.tile * 0.78f));
        if (v.visible && v.ent && imageTilesetEnabled()) {
            glyphSize = std::max(glyphSize, (int)(s.tile * 1.55f));
        }
        SDL_Rect gr{cx - glyphSize/2, cy - glyphSize/2, glyphSize, glyphSize};
        bool drewImage = false;
        if (v.visible && v.ent) {
            Color mod = applyVisionToGlyph(rgb(255,255,255), mx, my);
            drewImage = drawEntityImageTile(*v.ent, gr, mod);
        }
        if (!drewImage) {
            drawCentered(v.glyph, gr, v.visible ? v.fg : scale(v.fg, 0.55f), v.emoji, v.tint);
        }
        drawFeatureOccluderIfNeeded(mx, my, gr);
    }

    if (v.visible && v.ent && v.ent->alive && v.ent->hp < v.ent->maxHp) {
        int barW = std::max(8, s.tile);
        SDL_Rect hb{cx - barW/2, cy + hh - 5, barW, 3};
        setDraw(rgb(80,20,20,190)); SDL_RenderFillRect(s.ren, &hb);
        hb.w = std::max(1, barW * v.ent->hp / std::max(1, v.ent->maxHp));
        setDraw(v.ent->hp*3 > v.ent->maxHp*2 ? rgb(65,230,90) : v.ent->hp*3 > v.ent->maxHp ? rgb(230,210,70) : rgb(230,60,55));
        SDL_RenderFillRect(s.ren, &hb);
    }

    if (v.selected) {
        drawDiamondOutline(cx, cy, hw-1, hh-1, rgb(255,255,255,210));
        if (hw > 4 && hh > 3) drawDiamondOutline(cx, cy, hw-4, hh-3, rgb(255,255,255,110));
    }
    if (v.cursor) {
        drawDiamondOutline(cx, cy, hw, hh, rgb(40,20,0,240));
        if (hw > 3 && hh > 2) drawDiamondOutline(cx, cy, hw-3, hh-2, rgb(255,245,150,210));
    }
}

static void drawMapIso() {
    SDL_Rect mr = mapRect();
    setDraw(rgb(4,6,8)); SDL_RenderFillRect(s.ren, &mr);
    updateViewMetrics(!s.middleDown);
    SDL_RenderSetClipRect(s.ren, &mr);

    IsoOffsetBounds b = isoVisibleOffsetBounds();
    int minSum = b.minSx + b.minSy;
    int maxSum = b.maxSx + b.maxSy;
    for (int sum = minSum; sum <= maxSum; ++sum) {
        for (int sy = b.minSy; sy <= b.maxSy; ++sy) {
            int sx = sum - sy;
            if (sx < b.minSx || sx > b.maxSx) continue;
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx, my)) continue;
            drawIsoTileBase(mx, my);
        }
    }
    for (int sum = minSum; sum <= maxSum; ++sum) {
        for (int sy = b.minSy; sy <= b.maxSy; ++sy) {
            int sx = sum - sy;
            if (sx < b.minSx || sx > b.maxSx) continue;
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx, my)) continue;
            drawIsoTileForeground(mx, my);
        }
    }

    if (s.leftDown) {
        int x0 = std::max(0, std::min(s.dragStartX, g.cursorX));
        int x1 = std::min(MAP_W - 1, std::max(s.dragStartX, g.cursorX));
        int y0 = std::max(0, std::min(s.dragStartY, g.cursorY));
        int y1 = std::min(MAP_H - 1, std::max(s.dragStartY, g.cursorY));
        for (int my = y0; my <= y1; ++my) {
            for (int mx = x0; mx <= x1; ++mx) {
                int cx, cy; isoTileCenterFromScreenOffset(mx-g.viewX, my-g.viewY, cx, cy);
                fillDiamond(cx, cy, isoHalfW(), isoHalfH(), rgb(255,255,255,32));
                drawDiamondOutline(cx, cy, isoHalfW(), isoHalfH(), rgb(255,255,255,145));
            }
        }
    }
    drawMobileBuildPreviewIso();
    SDL_RenderSetClipRect(s.ren, nullptr);
}

static void drawMap() {
    if (s.isometric) { drawMapIso(); return; }
    SDL_Rect mr = mapRect();
    setDraw(rgb(4,6,8)); SDL_RenderFillRect(s.ren, &mr);
    updateViewMetrics(!s.middleDown);
    SDL_RenderSetClipRect(s.ren, &mr);

    for (int sy=0; sy<g.viewH; ++sy) {
        for (int sx=0; sx<g.viewW; ++sx) {
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx, my)) continue;
            SDL_Rect r{mr.x + sx*s.tile, mr.y + sy*s.tile, s.tile, s.tile};
            drawTile(mx,my,r);
        }
    }

    // Drag selection rectangle.
    if (s.leftDown) {
        int mx0 = s.dragStartX, my0 = s.dragStartY;
        int mx1 = g.cursorX, my1 = g.cursorY;
        int x0 = std::min(mx0,mx1) - g.viewX;
        int x1 = std::max(mx0,mx1) - g.viewX;
        int y0 = std::min(my0,my1) - g.viewY;
        int y1 = std::max(my0,my1) - g.viewY;
        SDL_Rect sel{mr.x+x0*s.tile, mr.y+y0*s.tile, (x1-x0+1)*s.tile, (y1-y0+1)*s.tile};
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(255,255,255,70)); SDL_RenderFillRect(s.ren, &sel);
        setDraw(rgb(255,255,255,190)); SDL_RenderDrawRect(s.ren, &sel);
    }
    drawMobileBuildPreviewTopDown();
    SDL_RenderSetClipRect(s.ren, nullptr);
}

static void drawMiniMap(int x, int y, int w, int h) {
    setDraw(rgb(10,12,18)); SDL_Rect bg{x,y,w,h}; SDL_RenderFillRect(s.ren,&bg);
    for (int yy=0; yy<h; ++yy) {
        int my = yy * MAP_H / std::max(1,h);
        for (int xx=0; xx<w; ++xx) {
            int mx = xx * MAP_W / std::max(1,w);
            const Tile& t = g.map[my][mx];
            Color c = t.explored[0] ? terrainBg(t,mx,my) : rgb(5,5,8);
            Entity* e = t.visible[0] ? entityAt(mx,my) : nullptr;
            if (e && e->alive && e->owner != OWNER_NATURE) c = ownerBg(e->owner);
            setDraw(c); SDL_RenderDrawPoint(s.ren, x+xx, y+yy);
        }
    }
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    int vx0 = g.viewX, vy0 = g.viewY;
    int vx1 = g.viewX + g.viewW, vy1 = g.viewY + g.viewH;
    if (s.isometric) {
        IsoOffsetBounds b = isoVisibleOffsetBounds();
        vx0 = g.viewX + b.minSx;
        vx1 = g.viewX + b.maxSx + 1;
        vy0 = g.viewY + b.minSy;
        vy1 = g.viewY + b.maxSy + 1;
    }
    if (vx1 > vx0 && vy1 > vy0) {
        auto miniCoord = [](int origin, int value, int size, int mapSize) {
            return origin + (int)std::floor((double)value * (double)size / (double)mapSize);
        };
        int x0 = miniCoord(x, vx0, w, MAP_W);
        int y0 = miniCoord(y, vy0, h, MAP_H);
        int x1 = miniCoord(x, vx1, w, MAP_W);
        int y1 = miniCoord(y, vy1, h, MAP_H);
        if (vx0 < 0) x0 = x - 1;
        if (vy0 < 0) y0 = y - 1;
        if (vx1 > MAP_W) x1 = x + w + 1;
        if (vy1 > MAP_H) y1 = y + h + 1;
        SDL_Rect view{x0, y0, std::max(2, x1 - x0), std::max(2, y1 - y0)};
        SDL_Rect clip{x, y, w, h};
        SDL_RenderSetClipRect(s.ren, &clip);
        setDraw(rgb(255,255,255,210)); SDL_RenderDrawRect(s.ren, &view);
        SDL_RenderSetClipRect(s.ren, nullptr);
    }
}

static void drawTopBar() {
    SDL_Rect top{0,0,s.winW,s.topH};
    setDraw(rgb(12,32,58)); SDL_RenderFillRect(s.ren,&top);
    Player& p = g.players[0];
    std::ostringstream ss;
    ss << "G:" << p.gold << "  W:" << p.wood << "  F:" << p.food
       << "  Pop:" << p.supply << "/" << p.supplyMax
       << "  " << seasonNameSafe() << ' ' << timeNameSafe() << ' ' << weatherName();
    drawTextFit(10, 7, ss.str(), rgb(235,238,230), std::max(1, s.winW - s.panelW - 18));
}

static bool isTrainProducer(EntityType t) {
    return t == E_TOWNHALL || t == E_BARRACKS || t == E_STABLE || t == E_DOCK || t == E_CASTLE;
}

static std::string trainPromptFor(const Entity* sel) {
    if (!sel || sel->owner != 0 || !isTrainProducer(sel->type))
        return "TRAIN: select a production building, Esc cancel";
    switch (sel->type) {
        case E_TOWNHALL: return "TRAIN: P Peasant (50g), repeat to queue, Esc cancel";
        case E_BARRACKS: return "TRAIN: M Militia  A Archer  S Spearman  C Catapult  R Ram, Esc cancel";
        case E_STABLE: return "TRAIN: K Knight, repeat to queue, Esc cancel";
        case E_CASTLE: return "TRAIN: P Peasant  T Trebuchet, repeat to queue, Esc cancel";
        case E_DOCK: return "TRAIN: B Fishing boat  W Warship  T Transport, Esc cancel";
        default: return "TRAIN: no units available, Esc cancel";
    }
}

static std::vector<std::string> trainPanelHintsFor(EntityType t) {
    switch (t) {
        case E_TOWNHALL: return {"P: peasant (50g)"};
        case E_BARRACKS: return {"M: militia  A: archer", "S: spearman  C: catapult", "R: ram"};
        case E_STABLE: return {"K: knight"};
        case E_CASTLE: return {"P: peasant", "T: trebuchet"};
        case E_DOCK: return {"B: fish boat  W: warship", "T: transport"};
        default: return {};
    }
}

static std::vector<std::pair<std::string, int>> trainOptionTokensFor(EntityType t) {
    switch (t) {
        case E_TOWNHALL: return {{"P", 'p'}, {"Esc", 27}};
        case E_BARRACKS: return {{"M", 'm'}, {"A", 'a'}, {"S", 's'}, {"C", 'c'}, {"R", 'r'}, {"Esc", 27}};
        case E_STABLE: return {{"K", 'k'}, {"Esc", 27}};
        case E_CASTLE: return {{"P", 'p'}, {"T", 't'}, {"Esc", 27}};
        case E_DOCK: return {{"B", 'b'}, {"W", 'w'}, {"T", 't'}, {"Esc", 27}};
        default: return {{"Esc", 27}};
    }
}

static std::vector<std::pair<std::string, int>> desktopBuildTokensLine1() {
    return {{"H", 'h'}, {"B", 'b'}, {"S", 's'}, {"T", 't'},
            {"F", 'f'}, {"W", 'w'}, {"K", 'k'}};
}

static std::vector<std::pair<std::string, int>> desktopBuildTokensLine2() {
    return {{"L", 'l'}, {"N", 'n'}, {"I", 'i'}, {"D", 'd'}, {"Esc", 27}};
}

static std::vector<std::pair<std::string, int>> terminalBuildTokens() {
    return {{"[H]", 'h'}, {"[B]", 'b'}, {"[S]", 's'}, {"[T]", 't'}, {"[F]", 'f'},
            {"[W]", 'w'}, {"[G]", 'g'}, {"[A]", 'a'}, {"[C]", 'c'}, {"[M]", 'm'},
            {"[K]", 'k'}, {"[L]", 'l'}, {"[N]", 'n'}, {"[I]", 'i'}, {"[D]", 'd'},
            {"[Esc]", 27}};
}

static std::vector<std::pair<std::string, int>> defaultBottomTokens() {
#if defined(REALM_WEB)
    return {{"B:Build", 'b'}, {"T:Train", 't'}, {"F5-F8:Save", 'v'}, {"F9-F12:Load", 'l'},
            {"D:Diag", 'd'}, {"Q:Resign", 'q'}};
#else
    return {{"B:Build", 'b'}, {"T:Train", 't'}, {"F5-F8:Save", 'v'}, {"F9-F12:Load", 'l'},
            {"D:Diag", 'd'}, {"Q:Resign", 'q'}, {"X:Exit", 'x'}};
#endif
}

static bool devCaptureEnabled() {
#ifndef REALM_DEV_CAPTURE_DEFAULT
#define REALM_DEV_CAPTURE_DEFAULT 1
#endif
    const char* env = std::getenv("REALM_DEV_CAPTURE");
    if (env && *env) {
        std::string value(env);
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return (char)std::tolower(ch); });
        return !(value == "0" || value == "false" || value == "off" || value == "no");
    }
    return REALM_DEV_CAPTURE_DEFAULT != 0;
}

static int mobileButtonH() {
    return std::max(44, (int)std::lround(48.0f * s.mobileUiScale));
}

static void drawButton(const MobileButton& b, bool active = false, bool danger = false) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    bool hovered = rectHovered(b.r);
    Color bg = active ? rgb(42,86,118) : danger ? rgb(86,42,42) : rgb(24,31,39);
    Color bd = active ? rgb(120,195,235) : hovered ? rgb(220,230,210) : rgb(92,105,118);
    setDraw(bg); SDL_RenderFillRect(s.ren, &b.r);
    setDraw(bd); SDL_RenderDrawRect(s.ren, &b.r);
    drawTextFit(b.r.x + 8, b.r.y + std::max(4, (b.r.h - 18) / 2), b.label,
                rgb(235,240,235), std::max(1, b.r.w - 16), s.monoSmall ? s.monoSmall : s.mono);
    drawHoverMark(SDL_Rect{b.r.x + 8, b.r.y + b.r.h - 9, std::max(1, b.r.w - 16), 6}, bd);
}

static void drawConsoleButton(const MobileButton& b, bool active = false, bool danger = false) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    bool hovered = rectHovered(b.r);
    Color bg = active ? rgb(255, 226, 95, 230) : danger ? rgb(58, 18, 18, 230) : rgb(3, 5, 8, 235);
    Color bd = active ? rgb(255, 245, 170) : danger ? rgb(220, 115, 115) : hovered ? rgb(255, 230, 120) : rgb(128, 143, 150);
    Color fg = active ? rgb(20, 16, 0) : danger ? rgb(255, 185, 170) : rgb(218, 224, 218);
    setDraw(bg); SDL_RenderFillRect(s.ren, &b.r);
    setDraw(bd); SDL_RenderDrawRect(s.ren, &b.r);
    if (b.r.w > 18 && b.r.h > 18) {
        SDL_Rect inner{b.r.x + 2, b.r.y + 2, b.r.w - 4, b.r.h - 4};
        setDraw(active ? rgb(20, 16, 0, 150) : rgb(60, 75, 82, 150));
        SDL_RenderDrawRect(s.ren, &inner);
    }
    std::string label = "[" + b.label + "]";
    drawTextFit(b.r.x + 8, b.r.y + std::max(4, (b.r.h - 18) / 2), label,
                fg, std::max(1, b.r.w - 16), s.monoSmall ? s.monoSmall : s.mono);
    drawHoverMark(SDL_Rect{b.r.x + 8, b.r.y + b.r.h - 9, std::max(1, b.r.w - 16), 6}, bd);
}

static void addGridButtons(std::vector<MobileButton>& out, int x, int y, int w,
                           const std::vector<std::pair<std::string, std::string>>& items,
                           int cols = 3) {
    if (items.empty()) return;
    int gap = 8;
    int h = mobileButtonH();
    cols = std::max(1, cols);
    int bw = std::max(44, (w - gap * (cols - 1)) / cols);
    for (size_t i = 0; i < items.size(); ++i) {
        int col = (int)i % cols;
        int row = (int)i / cols;
        out.push_back({SDL_Rect{x + col * (bw + gap), y + row * (h + gap), bw, h},
                       items[i].first, items[i].second});
    }
}

static std::string mobileSelectionSummary() {
    if (!g.selectedIds.empty()) {
        int idle = 0, gathering = 0, military = 0;
        for (int id : g.selectedIds) {
            Entity* e = findEntity(id);
            if (!e || !e->alive) continue;
            if (e->state == S_IDLE) idle++;
            if (e->state == S_GATHERING) gathering++;
            if (isMilitary(e->type)) military++;
        }
        std::ostringstream ss;
        ss << g.selectedIds.size() << " units";
        if (idle || gathering || military) ss << "  " << idle << " idle  " << gathering << " gathering";
        return ss.str();
    }
    Entity* sel = findEntity(g.selectedId);
    if (!sel) return "No selection";
    std::ostringstream ss;
    ss << STATS[sel->type].name << "  HP " << sel->hp << "/" << sel->maxHp;
    if (sel->owner == 0) {
        if (sel->producing != E_NONE) {
            int pct = sel->trainTime > 0 ? (sel->trainProgress * 100 / sel->trainTime) : 0;
            ss << "  Training " << STATS[sel->producing].name << " " << pct << "%";
        } else if (sel->cargo.type != CR_NONE) {
            ss << "  carrying " << sel->cargo.amount << " " << cargoResourceName(sel->cargo.type);
        } else {
            ss << "  " << stateName(sel->state);
        }
    }
    return ss.str();
}

static bool mobileHasSelectedWorker() {
    if (!g.selectedIds.empty()) {
        for (int id : g.selectedIds) {
            Entity* e = findEntity(id);
            if (e && e->alive && e->owner == 0 && canBuild(e->type)) return true;
        }
        return false;
    }
    Entity* e = findEntity(g.selectedId);
    return e && e->alive && e->owner == 0 && canBuild(e->type);
}

static bool mobileHasSelectedMilitary() {
    if (!g.selectedIds.empty()) {
        for (int id : g.selectedIds) {
            Entity* e = findEntity(id);
            if (e && e->alive && e->owner == 0 && isMilitary(e->type)) return true;
        }
        return false;
    }
    Entity* e = findEntity(g.selectedId);
    return e && e->alive && e->owner == 0 && isMilitary(e->type);
}

static EntityType mobileDefaultTrainType(EntityType producer) {
    switch (producer) {
        case E_TOWNHALL: return E_PEASANT;
        case E_BARRACKS: return E_MILITIA;
        case E_STABLE: return E_KNIGHT;
        case E_CASTLE: return E_TREBUCHET;
        case E_DOCK: return E_FISHING_BOAT;
        default: return E_NONE;
    }
}

static std::vector<MobileButton> mobileHudButtons() {
    std::vector<MobileButton> buttons;
    SDL_Rect pr = panelRect();
    SDL_Rect mm = miniMapRect();
    int pad = mobileSafePad();
    int gap = 8;
    int bh = mobileButtonH();

    int cmdX = pr.x + pad;
    int cmdY = 0;
    int cmdW = std::max(1, pr.w - pad * 2);
    if (mobilePortrait()) {
        cmdY = pr.y + pad + 132;
        cmdW = std::max(1, mm.x - cmdX - gap);
        if (cmdW < 170) {
            cmdW = std::max(1, pr.w - pad * 2);
            cmdY = mm.y + mm.h + gap;
        }
    } else {
        cmdY = mm.y + mm.h + gap;
    }

    std::vector<std::pair<std::string, std::string>> cmd;
    if (s.mobileBuildType != E_NONE) {
        cmd.push_back({"cancel", "Cancel"});
    } else if (g.mode == M_BUILD_SELECT) {
        if (s.mobileBuildPage == 0) {
            cmd = {{"build:house", "House"}, {"build:farm", "Farm"}, {"build:barracks", "Barracks"},
                   {"build:tower", "Tower"}, {"cancel", "Cancel"}, {"buildmore", "More"}};
        } else {
            cmd = {{"build:stable", "Stable"}, {"build:lumber", "Lumber"}, {"build:mining", "Mining"},
                   {"build:mill", "Mill"}, {"build:dock", "Dock"}, {"buildback", "Back"}};
        }
    } else {
        Entity* sel = findEntity(g.selectedId);
        if (mobileHasSelectedWorker()) {
            cmd = {{"move", "Move"}, {"gather", "Gather"}, {"build", "Build"}, {"stop", "Stop"}};
        } else if (mobileHasSelectedMilitary()) {
            cmd = {{"move", "Move"}, {"attack", "Attack"}, {"attackmove", "Attack Move"}, {"stop", "Stop"}};
        } else if (sel && sel->owner == 0 && isBuilding(sel->type) && !sel->underConstruction) {
            if (isTrainProducer(sel->type)) {
                EntityType tt = mobileDefaultTrainType(sel->type);
                cmd.push_back({"train", tt == E_NONE ? "Train" : std::string("Train ") + STATS[tt].name});
            }
            if (sel->type == E_TOWNHALL || sel->type == E_CASTLE || sel->type == E_BARRACKS
                || sel->type == E_STABLE || sel->type == E_DOCK) {
                cmd.push_back({"rally", "Rally"});
            }
            if (sel->type == E_MARKET) cmd.push_back({"trade", "Trade"});
            if (sel->type == E_BLACKSMITH) cmd.push_back({"research", "Research"});
            cmd.push_back({"cancelqueue", "Cancel Queue"});
        } else {
            cmd = {{"selectarmy", "Select Army"}, {"help", "Help"}};
        }
    }
    addGridButtons(buttons, cmdX, cmdY, cmdW, cmd, mobilePortrait() ? 3 : 2);

    int utilityY = pr.y + pr.h - bh - pad;
    int utilityW = std::max(1, pr.w - pad * 2);
    addGridButtons(buttons, pr.x + pad, utilityY, utilityW,
                   {{"menu", "Menu"}, {"pause", g.mode == M_PAUSED ? "Resume" : "Pause"},
                    {"fullscreen", "Full"}, {"idle", "Idle"}}, 4);
    return buttons;
}

static void mobileDrawResources(int x, int y, int w) {
    Player& p = g.players[0];
    std::ostringstream ss;
    if (w < 340) {
        ss << "F " << p.food << "  W " << p.wood << "  G " << p.gold
           << "  P " << p.supply << "/" << p.supplyMax;
    } else {
        ss << "Food " << p.food << "   Wood " << p.wood << "   Gold " << p.gold
           << "   Pop " << p.supply << "/" << p.supplyMax;
    }
    drawTextFit(x, y, ss.str(), rgb(236,240,226), w, s.monoSmall ? s.monoSmall : s.mono);
}

static void drawMobileHud() {
    SDL_Rect pr = panelRect();
    int pad = mobileSafePad();
    setDraw(rgb(8,10,14)); SDL_RenderFillRect(s.ren, &pr);
    setDraw(rgb(68,82,94)); SDL_RenderDrawRect(s.ren, &pr);

    mobileDrawResources(pr.x + pad, pr.y + pad, std::max(1, pr.w - pad * 2));
    int summaryY = pr.y + pad + 28;
    SDL_Rect mm = miniMapRect();
    int summaryW = mobilePortrait() ? std::max(1, mm.x - (pr.x + pad) - 10) : std::max(1, pr.w - pad * 2);
    drawTextFit(pr.x + pad, summaryY, mobileSelectionSummary(), rgb(255,230,135), summaryW);
    if (s.mobileBuildType != E_NONE) {
        drawTextFit(pr.x + pad, summaryY + 22,
                    std::string("Placing ") + STATS[s.mobileBuildType].name + " - tap a valid tile",
                    rgb(145,220,245), summaryW);
    } else if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE || g.mode == M_BUILD_SELECT || g.mode == M_MARKET_TRADE) {
        drawTextFit(pr.x + pad, summaryY + 22, modeName(g.mode), rgb(145,220,245), summaryW);
    } else if (g.statusTimer > 0) {
        drawTextFit(pr.x + pad, summaryY + 22, g.statusMsg, rgb(255,230,120), summaryW);
    }

    drawMiniMap(mm.x, mm.y, mm.w, mm.h);
    for (const MobileButton& b : mobileHudButtons()) {
        bool active = (b.id == "build" && g.mode == M_BUILD_SELECT)
                   || (b.id == "attack" && g.mode == M_ATTACK_MOVE)
                   || (b.id == "rally" && g.mode == M_RALLY_SET);
        drawButton(b, active, b.id == "cancel");
    }
    if (g.statusTimer > 0) g.statusTimer--;
}

static void drawPanel() {
    if (isMobileGui()) {
        drawMobileHud();
        return;
    }
    SDL_Rect pr = panelRect();
    setDraw(rgb(8,10,14)); SDL_RenderFillRect(s.ren, &pr);
    // Keep the console look: a pipe divider drawn as text, not a modern UI bar.
    for (int y=0; y<s.winH; y+=16) drawText(pr.x, y, "|", rgb(95,105,115));

    int x = pr.x + 14, y = 12;
    int textW = std::max(1, pr.w - 28);
    SDL_Rect mini = miniMapRect();
    drawMiniMap(mini.x, mini.y, mini.w, mini.h); y += 124;

    drawText(x, y, "Realm", rgb(245,245,230)); y += 22;
    std::ostringstream c; c << "Cursor: (" << g.cursorX << "," << g.cursorY << ")";
    drawTextFit(x, y, c.str(), rgb(185,190,195), textW); y += 20;
    drawTextFit(x, y, cursorTileSummary(), rgb(180,190,185), textW); y += 20;
    drawTextFit(x, y, cursorStackSummary(), rgb(180,190,185), textW); y += 20;
    drawTextFit(x, y, displayMode == DM_ASCII ? "Visual: ASCII" : "Visual: Tileset", rgb(150,170,190), textW); y += 20;
    drawTextFit(x, y, "Wheel zoom / middle pan", rgb(150,160,168), textW); y += 26;

    if (g.diagnostics) {
        std::ostringstream ds;
        ds << "Diag tick " << g.tick << " mode " << modeName(g.mode);
        drawTextFit(x, y, trimPanelLine(ds.str()), rgb(255,210,120), textW); y += 20;
        std::ostringstream ds2;
        ds2 << "Ent " << g.entities.size() << " Proj " << g.projectiles.size()
            << " Seed " << g.seed;
        drawTextFit(x, y, trimPanelLine(ds2.str()), rgb(255,210,120), textW); y += 20;
        if (Entity* selDiag = findEntity(g.selectedId)) {
            std::ostringstream ds3;
            ds3 << "Sel #" << selDiag->id << ' ' << STATS[selDiag->type].name
                << ' ' << stateName(selDiag->state);
            drawTextFit(x, y, trimPanelLine(ds3.str()), rgb(255,210,120), textW); y += 20;
        }
    }

    Entity* sel = findEntity(g.selectedId);
    if (!g.selectedIds.empty()) {
        std::ostringstream gs; gs << "Group: " << g.selectedIds.size() << " units";
        drawTextFit(x, y, gs.str(), rgb(255,230,135), textW); y += 22;
        drawTextFit(x, y, "R-click: command group", rgb(180,185,190), textW); y += 20;
    } else if (sel) {
        Color badge = (sel->owner == OWNER_NATURE) ? rgb(95,95,80) : ownerBg(sel->owner);
        SDL_Rect b{x,y,22,22}; setDraw(badge); SDL_RenderFillRect(s.ren,&b);
        bool usesSymbolFont = false;
        drawCentered(tilesetEntityVisual(*sel, usesSymbolFont), b, rgb(255,255,255), usesSymbolFont);
        drawTextFit(x+30, y+2, STATS[sel->type].name, rgb(255,230,135), std::max(1, textW - 30)); y += 26;
        std::ostringstream hp; hp << "HP: " << sel->hp << "/" << sel->maxHp;
        drawTextFit(x, y, hp.str(), rgb(220,220,210), textW); y += 20;
        drawTextFit(x, y, stateName(sel->state), rgb(180,190,200), textW); y += 22;
        if (sel->owner == 0) {
            if (sel->type == E_PEASANT) {
                drawKeyOptionText(x,y,"B: build",'b',rgb(150,210,230), textW); y+=20;
                drawKeyOptionText(x,y,"Enter/R-click: command",'\n',rgb(150,210,230), textW); y+=20;
            }
            else if (isBuilding(sel->type) && !sel->underConstruction) {
                if (isTrainProducer(sel->type)) {
                    drawKeyOptionText(x,y,"T: train",'t',rgb(150,210,230), textW); y+=20;
                    for (const std::string& hint : trainPanelHintsFor(sel->type)) {
                        drawTextFit(x,y,hint,rgb(180,205,210), textW); y+=20;
                    }
                } else {
                    drawTextFit(x,y,"No train options",rgb(130,145,150), textW); y+=20;
                }
                if (sel->type == E_FARM) {
                    std::ostringstream ripe;
                    ripe << "Ripe: " << sel->storedFood << " / 20";
                    drawTextFit(x,y,ripe.str(),rgb(180,205,210), textW); y+=20;
                } else if (sel->type == E_MILL) {
                    std::ostringstream stored;
                    stored << "Stored: " << sel->storedFood << " food";
                    drawTextFit(x,y,stored.str(),rgb(180,205,210), textW); y+=20;
                    drawTextFit(x,y,"Lost if destroyed",rgb(210,165,135), textW); y+=20;
                }
            }
        }
    } else {
        drawText(x, y, "No selection", rgb(130,135,145)); y += 26;
        drawText(x, y, "Legend", rgb(205,210,215)); y += 22;
        drawTextFit(x, y, "$ gold     T wood", rgb(210,210,200), textW); y += 20;
        drawTextFit(x, y, ": berries  p peasant", rgb(210,210,200), textW); y += 20;
        drawTextFit(x, y, "m militia  k cavalry", rgb(210,210,200), textW); y += 20;
        drawTextFit(x, y, "> deer  < wolf  @ boar", rgb(210,210,200), textW); y += 20;
        drawTextFit(x, y, "Blue you; warm enemies", rgb(170,180,188), textW); y += 20;
        drawTextFit(x, y, "! combat; x/+/# orders", rgb(170,180,188), textW); y += 20;
    }
}

static void drawBottom() {
    SDL_Rect bot{0,s.winH-s.bottomH,s.winW,s.bottomH};
    setDraw(rgb(12,32,58)); SDL_RenderFillRect(s.ren,&bot);
    std::string controls1 = "Arrows:Move  Space/Click:Select  Enter/R-click:Cmd  B:Build  T:Train";
    std::string controls2 =
#if defined(REALM_WEB)
        "F5-F8:Save  F9-F12:Load  D:Diag  Alt+Enter:Full  +/-:Zoom  Q:Resign";
#else
        "F5-F8:Save  F9-F12:Load  D:Diag  Alt+Enter:Full  +/-:Zoom  Q:Resign  X:Exit";
#endif
    ;
    if (g.mode == M_PAUSED) { controls1 = "PAUSED - Press P to resume"; controls2.clear(); }
    else if (g.mode == M_GAME_OVER) {
#if defined(REALM_WEB)
        controls1 = (g.winner==0) ? "VICTORY - Enter/Q for menu" : "DEFEAT - Enter/Q for menu";
#else
        controls1 = (g.winner==0) ? "VICTORY - Enter/Q for menu, X to exit" : "DEFEAT - Enter/Q for menu, X to exit";
#endif
        controls2.clear();
    }
    else if (g.mode == M_BUILD_SELECT) { controls1 = "BUILD: H House, B Barracks, S Stable, T Tower, F Farm, W Wall, K Castle"; controls2 = "G Gate  A Armory  C Church  M Market  L Lumber  N Mine  I Mill  D Dock  Esc"; }
    else if (g.mode == M_TRAIN_SELECT) { controls1 = trainPromptFor(findEntity(g.selectedId)); controls2.clear(); }
    else if (g.mode == M_MARKET_TRADE) { controls1 = "MARKET: G 40g->30w  W 40w->30g  F 50g->30f  V 40f->30g"; controls2 = "Esc cancel"; }
    int hintX = s.winW - 14;
    if (devCaptureEnabled()) {
        const std::string captureHint = "Y:Capture issue";
        int hintW = textWidth(captureHint);
        hintX = std::max(10, s.winW - hintW - 14);
        drawText(hintX, s.winH-s.bottomH+6, captureHint, rgb(255,230,120));
    }

    int maxW = std::max(1, s.winW - 20);
    int topLineW = std::max(1, hintX - 20);
    if (g.mode == M_BUILD_SELECT) {
        drawKeyTokensInText(10, s.winH-s.bottomH+6, controls1, desktopBuildTokensLine1(),
                            rgb(230,235,230), topLineW);
    } else if (g.mode == M_TRAIN_SELECT) {
        Entity* sel = findEntity(g.selectedId);
        drawKeyTokensInText(10, s.winH-s.bottomH+6, controls1,
                            sel ? trainOptionTokensFor(sel->type) : std::vector<std::pair<std::string, int>>{{"Esc", 27}},
                            rgb(230,235,230), topLineW);
    } else if (g.mode == M_PAUSED) {
        drawKeyTokensInText(10, s.winH-s.bottomH+6, controls1, {{"P", 'p'}},
                            rgb(230,235,230), topLineW);
    } else if (g.mode == M_GAME_OVER) {
        drawKeyTokensInText(10, s.winH-s.bottomH+6, controls1,
#if defined(REALM_WEB)
                            {{"Enter", '\n'}, {"Q", 'q'}},
#else
                            {{"Enter", '\n'}, {"Q", 'q'}, {"X", 'x'}},
#endif
                            rgb(230,235,230), topLineW);
    } else {
        drawKeyTokensInText(10, s.winH-s.bottomH+6, controls1, defaultBottomTokens(),
                            rgb(230,235,230), topLineW);
    }
    if (g.statusTimer > 0) {
        drawTextFit(10, s.winH-s.bottomH+26, ">> " + g.statusMsg, rgb(255,230,120), maxW);
        g.statusTimer--;
    } else if (!controls2.empty()) {
        if (g.mode == M_BUILD_SELECT) {
            drawKeyTokensInText(10, s.winH-s.bottomH+26, controls2, desktopBuildTokensLine2(),
                                rgb(200,213,220), maxW);
        } else {
            drawKeyTokensInText(10, s.winH-s.bottomH+26, controls2, defaultBottomTokens(),
                                rgb(200,213,220), maxW);
        }
    }
}

static Color termBg() { return rgb(3, 5, 8); }
static Color termFg() { return rgb(218, 224, 218); }
static Color termDim() { return rgb(128, 143, 150); }
static Color termBar() { return rgb(12, 32, 58); }
static Color termHigh() { return rgb(255, 230, 120); }
static Color termAccent() { return rgb(145, 220, 245); }

static float terminalZoomScale() {
    return std::max(0.58f, std::min(1.85f, s.tile / 24.0f));
}

static void terminalCellMetrics(int& cellW, int& cellH) {
    int w = 0, h = 0;
    if (s.mono) TTF_SizeText(s.mono, "M", &w, &h);
    cellW = std::max(8, w);
    cellH = std::max(16, s.mono ? TTF_FontLineSkip(s.mono) : h);
}

static void terminalMapCellMetrics(int& cellW, int& cellH) {
    terminalCellMetrics(cellW, cellH);
    float zoom = terminalZoomScale();
    cellW = std::max(5, (int)std::lround(cellW * zoom));
    cellH = std::max(9, (int)std::lround(cellH * zoom));
}

static SDL_Rect terminalMapPixelRect(const TerminalFrame& frame) {
    int panelW = 24;
    int panelX = frame.cols - panelW;
    int mapCols = panelX >= 1 ? panelX - 1 : frame.cols;
    int topRows = 2;
    int bottomRows = 2;
    return SDL_Rect{0, topRows * frame.cellH,
                    std::max(1, mapCols * frame.cellW),
                    std::max(1, (frame.rows - topRows - bottomRows) * frame.cellH)};
}

static TerminalFrame makeBlankTerminalFrame() {
    int cellW = 9, cellH = 18;
    terminalCellMetrics(cellW, cellH);
    int cols = std::max(80, s.winW / std::max(1, cellW));
    int rows = std::max(24, s.winH / std::max(1, cellH));
    TerminalCell base{' ', termFg(), termBg()};
    TerminalFrame frame;
    frame.cols = cols;
    frame.rows = rows;
    frame.cellW = cellW;
    frame.cellH = cellH;
    frame.cells.assign((size_t)cols * (size_t)rows, base);
    return frame;
}

static void termPut(TerminalFrame& frame, int x, int y, char ch, Color fg, Color bg) {
    if (x < 0 || y < 0 || x >= frame.cols || y >= frame.rows) return;
    frame.at(x, y) = TerminalCell{ch, fg, bg};
}

static void termPutString(TerminalFrame& frame, int x, int y, const std::string& text,
                          Color fg, Color bg) {
    if (y < 0 || y >= frame.rows) return;
    for (size_t i = 0; i < text.size() && x + (int)i < frame.cols; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 126) ch = '?';
        termPut(frame, x + (int)i, y, (char)ch, fg, bg);
    }
}

static void termFillH(TerminalFrame& frame, int y, int x, int count, char ch, Color fg, Color bg) {
    for (int i = 0; i < count; ++i) termPut(frame, x + i, y, ch, fg, bg);
}

static void termFillV(TerminalFrame& frame, int x, int y, int count, char ch, Color fg, Color bg) {
    for (int i = 0; i < count; ++i) termPut(frame, x, y + i, ch, fg, bg);
}

static std::string termTrunc(const std::string& value, int width) {
    if (width <= 0) return "";
    if ((int)value.size() <= width) return value;
    if (width == 1) return value.substr(0, 1);
    return value.substr(0, (size_t)width - 1) + "~";
}

static Color ownerTermFg(int owner) {
    if (owner == 0) return rgb(160, 210, 255);
    if (owner > 0 && owner < MAX_PLAYERS) return rgb(255, 150, 120);
    return rgb(220, 220, 190);
}

static void clampTerminalView() {
    g.viewX = std::max(0, std::min(g.viewX, MAP_W - g.viewW));
    g.viewY = std::max(0, std::min(g.viewY, MAP_H - g.viewH));
}

static void updateTerminalCamera(int cols, int rows, bool keepCursor) {
    int panelW = 24;
    int uiCellW = 9, uiCellH = 18;
    int mapCellW = 9, mapCellH = 18;
    terminalCellMetrics(uiCellW, uiCellH);
    terminalMapCellMetrics(mapCellW, mapCellH);
    int mapCols = cols - panelW - 1;
    if (mapCols < 30) mapCols = cols;
    int mapRows = rows - 4;
    if (mapRows < 10) mapRows = rows - 2;
    int mapPixelW = std::max(1, mapCols * uiCellW);
    int mapPixelH = std::max(1, mapRows * uiCellH);
    g.viewW = std::max(1, mapPixelW / std::max(1, mapCellW));
    g.viewH = std::max(1, mapPixelH / std::max(1, mapCellH));
    g.viewW = std::min(g.viewW, MAP_W);
    g.viewH = std::min(g.viewH, MAP_H);
    if (keepCursor) {
        if (g.cursorX < g.viewX + 3) g.viewX = g.cursorX - 3;
        if (g.cursorX > g.viewX + g.viewW - 4) g.viewX = g.cursorX - g.viewW + 4;
        if (g.cursorY < g.viewY + 3) g.viewY = g.cursorY - 3;
        if (g.cursorY > g.viewY + g.viewH - 3) g.viewY = g.cursorY - g.viewH + 3;
    }
    clampTerminalView();
}

static TerminalCell terminalMapCell(int mx, int my) {
    const Tile& tile = g.map[my][mx];
    TerminalCell cell{' ', termDim(), termBg()};
    if (!tile.explored[0]) return cell;

    cell.ch = tile.visible[0] ? terrainAscii(tile.terrain) : '.';
    cell.fg = tile.visible[0] ? glyphColorForTerrain(tile, mx, my) : rgb(95, 95, 105);
    cell.bg = tile.visible[0] ? scale(terrainBg(tile, mx, my), 0.35f) : rgb(8, 9, 12);

    Entity* ent = tile.visible[0] ? entityAt(mx, my) : nullptr;
    if (ent && ent->alive) {
        cell.ch = STATS[ent->type].glyph;
        cell.fg = ownerTermFg(ent->owner);
        if (ent->owner != OWNER_NATURE) cell.bg = scale(ownerBg(ent->owner), 0.55f);
    }

    for (const auto& m : g.actionMarkers) {
        if (m.x == mx && m.y == my && m.ticks > 0 && (g.tick % 6) < 4) {
            cell.ch = m.glyph;
            cell.fg = termHigh();
            break;
        }
    }

    if (s.leftDown && g.dragging) {
        int x0 = std::min(s.dragStartX, g.cursorX);
        int x1 = std::max(s.dragStartX, g.cursorX);
        int y0 = std::min(s.dragStartY, g.cursorY);
        int y1 = std::max(s.dragStartY, g.cursorY);
        if (mx >= x0 && mx <= x1 && my >= y0 && my <= y1) {
            cell.bg = blend(cell.bg, rgb(255, 255, 255), 0.24f);
            cell.fg = blend(cell.fg, rgb(255, 255, 255), 0.30f);
        }
    }

    bool selected = ent && (ent->id == g.selectedId ||
        std::find(g.selectedIds.begin(), g.selectedIds.end(), ent->id) != g.selectedIds.end());
    if (selected) {
        cell.fg = rgb(10, 10, 12);
        cell.bg = rgb(240, 240, 230);
    }
    if (mx == g.cursorX && my == g.cursorY) {
        cell.fg = rgb(20, 16, 0);
        cell.bg = rgb(255, 226, 95);
    }
    return cell;
}

static void terminalDrawTop(TerminalFrame& frame) {
    termFillH(frame, 0, 0, frame.cols, ' ', termFg(), termBar());
    Player& p = g.players[0];
    int idleCount = 0, idleBldg = 0, popForecast = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != 0) continue;
        if (e.type == E_PEASANT && e.state == S_IDLE) idleCount++;
        if (isBuilding(e.type) && !e.underConstruction) {
            bool producer = (e.type == E_TOWNHALL || e.type == E_BARRACKS || e.type == E_STABLE || e.type == E_DOCK);
            if (producer && e.producing == E_NONE && e.queue.empty()) idleBldg++;
            if (e.producing != E_NONE) popForecast += STATS[e.producing].supplyUsed;
            for (int qt : e.queue) popForecast += STATS[(EntityType)qt].supplyUsed;
        }
    }
    std::ostringstream ss;
    ss << " REALM  Gold:" << p.gold << " Wood:" << p.wood << " Food:" << p.food
       << " Pop:" << p.supply << "/" << p.supplyMax << "(+" << popForecast << ")"
       << " Idle:" << idleCount << "/" << idleBldg;
    termPutString(frame, 0, 0, termTrunc(ss.str(), frame.cols), termFg(), termBar());

    std::string weather = (g.weather == W_STORM) ? "Storm" : (g.weather == W_RAIN) ? "Rain" :
                          (g.weather == W_SNOW) ? "Snow" : "Clear";
    std::ostringstream right;
    right << (getBrightness() > 0.5f ? "*" : "o") << " " << timeNameSafe()
          << " " << seasonNameSafe() << " " << weather;
    int rx = std::max(0, frame.cols - (int)right.str().size() - 1);
    termPutString(frame, rx, 0, right.str(), termFg(), termBar());
}

static void terminalDrawTerrainBar(TerminalFrame& frame) {
    int w = std::max(0, std::min(g.viewW, frame.cols));
    termFillH(frame, 1, 0, w, '-', termDim(), termBg());
    if (!inBounds(g.cursorX, g.cursorY) || !g.map[g.cursorY][g.cursorX].explored[0]) return;
    const Tile& ct = g.map[g.cursorY][g.cursorX];
    std::ostringstream ss;
    ss << terrainName(ct.terrain) << " [" << biomeName(ct.biome) << "]";
    if (ct.resources > 0) ss << " Res:" << ct.resources;
    termPutString(frame, 1, 1, termTrunc(ss.str(), std::max(0, w - 2)), termFg(), termBg());
}

static void terminalDrawMap(TerminalFrame& frame) {
    for (int sy = 0; sy < g.viewH; ++sy) {
        int my = g.viewY + sy;
        int row = sy + 2;
        if (row < 0 || row >= frame.rows - 2) continue;
        for (int sx = 0; sx < g.viewW; ++sx) {
            int mx = g.viewX + sx;
            if (!inBounds(mx, my) || sx >= frame.cols) continue;
            TerminalCell cell = terminalMapCell(mx, my);
            termPut(frame, sx, row, cell.ch, cell.fg, cell.bg);
        }
    }
}

static void terminalDrawMinimap(TerminalFrame& frame, int panelX, int panelW) {
    termPutString(frame, panelX + 1, 0, "Map", termAccent(), termBar());
    int mmW = panelW - 2;
    int mmH = std::min(g.viewH / 3, 14);
    int mmY = 1;
    for (int my = 0; my < mmH; ++my) {
        for (int mx = 0; mx < mmW; ++mx) {
            int mapX = mx * MAP_W / std::max(1, mmW);
            int mapY = my * MAP_H / std::max(1, mmH);
            char ch = ' ';
            Color fg = termDim();
            if (g.map[mapY][mapX].explored[0]) {
                Terrain t = g.map[mapY][mapX].terrain;
                if (t == T_WATER || t == T_SHALLOWS) { ch = '~'; fg = rgb(90, 150, 220); }
                else if (t == T_MOUNTAIN || t == T_STONE) { ch = '^'; fg = rgb(150, 150, 150); }
                else if (t == T_GOLD) { ch = '$'; fg = rgb(235, 210, 70); }
                else if (t == T_CASTLE_WALL || t == T_CASTLE_GATE) { ch = '#'; fg = rgb(190, 190, 190); }
                else { ch = '.'; fg = rgb(90, 135, 90); }
            }
            if (g.map[mapY][mapX].visible[0]) {
                Entity* ent = entityAt(mapX, mapY);
                if (ent && ent->alive) {
                    ch = isBuilding(ent->type) ? '#' : '*';
                    fg = ownerTermFg(ent->owner);
                }
            }
            termPut(frame, panelX + 1 + mx, mmY + my, ch, fg, termBg());
        }
    }
}

static void terminalDrawSelection(TerminalFrame& frame, int panelX, int panelW, int startY) {
    int y = startY;
    auto line = [&](const std::string& text, Color fg = termFg()) {
        if (y >= frame.rows - 2) return;
        termPutString(frame, panelX + 1, y++, termTrunc(text, panelW - 2), fg, termBg());
    };

    if (inBounds(g.cursorX, g.cursorY)) {
        const Tile& ct = g.map[g.cursorY][g.cursorX];
        line(std::string("Tile: ") + terrainName(ct.terrain));
        line(std::string("Biome: ") + biomeName(ct.biome));
        if (ct.resources > 0) {
            std::ostringstream res; res << "Resource: " << ct.resources;
            line(res.str(), termHigh());
        }
        int stack = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.state == S_GARRISONED) continue;
            auto& st = STATS[e.type];
            bool covers = st.isBuilding
                ? (g.cursorX >= e.x && g.cursorX < e.x + st.sizeW && g.cursorY >= e.y && g.cursorY < e.y + st.sizeH)
                : (g.cursorX == e.x && g.cursorY == e.y);
            if (!covers) continue;
            if (stack == 0) line("Stack:");
            if (stack < 3) line(std::string(" ") + st.name);
            stack++;
        }
        if (stack == 0) line("Stack: empty", termDim());
        else if (stack > 3) {
            std::ostringstream more; more << "+" << (stack - 3) << " more";
            line(more.str());
        }
        if (g.diagnostics) {
            std::ostringstream ds; ds << "Diag T" << g.tick << " M:" << modeName(g.mode);
            line(ds.str(), termHigh());
            std::ostringstream ds2; ds2 << "Ent:" << g.entities.size() << " Proj:" << g.projectiles.size();
            line(ds2.str(), termHigh());
            std::ostringstream ds3; ds3 << "Seed:" << g.seed << " AI:" << g.startupAIs;
            line(ds3.str(), termHigh());
        }
        y++;
    }

    if (g.selectedIds.size() > 1) {
        std::ostringstream gs; gs << "Group: " << g.selectedIds.size() << " units";
        line(gs.str(), termHigh());
        line("[Enter] Move/Attack", termAccent());
        line("[G] Assign to group", termAccent());
        line("[A] Select all mil.", termAccent());
        line("[1-9] Groups", termAccent());
        return;
    }

    Entity* sel = findEntity(g.selectedId);
    if (!sel) {
        line("No selection", termDim());
        y++;
        line("-- Legend (ASCII) --", termDim());
        line("$ Gold   T Oak");
        line("^ Mtn    Y Pine");
        line("~ Water  n Hills");
        line(": Berry  % Wheat");
        line("# Castle & Ruins");
        y++;
        line("p Peasant  m Militia", ownerTermFg(0));
        line("a Archer   k Knight", ownerTermFg(0));
        line("c Catapult", ownerTermFg(0));
        y++;
        line("d Deer  s Sheep", ownerTermFg(OWNER_NATURE));
        line("w Wolf  o Boar", ownerTermFg(OWNER_NATURE));
        return;
    }

    auto& st = STATS[sel->type];
    line(st.name, ownerTermFg(sel->owner));
    int barW = panelW - 4;
    int filled = sel->hp * barW / std::max(1, sel->maxHp);
    std::string hp = "HP";
    hp.append((size_t)std::max(0, filled), '|');
    hp.append((size_t)std::max(0, barW - filled), '-');
    line(hp);
    std::ostringstream hpNum; hpNum << sel->hp << " / " << sel->maxHp;
    line(hpNum.str());
    if (isUnit(sel->type)) {
        std::ostringstream stats; stats << "ATK " << st.atk << "  RNG " << st.range;
        line(stats.str());
        line(stateName(sel->state), termAccent());
        if (sel->cargo.amount > 0) {
            std::ostringstream cargo; cargo << "Carrying: " << sel->cargo.amount << " " << cargoResourceName(sel->cargo.type);
            line(cargo.str(), termHigh());
        }
    }
    if (sel->producing != E_NONE) {
        int pct = sel->trainProgress * 100 / std::max(1, sel->trainTime);
        line(std::string("Training: ") + STATS[sel->producing].name, termHigh());
        std::ostringstream pctLine; pctLine << pct << "%";
        line(pctLine.str(), termHigh());
    }
    if (!sel->queue.empty()) {
        std::ostringstream q; q << "Queue: " << sel->queue.size();
        line(q.str(), termDim());
    }
    if (sel->underConstruction) {
        std::ostringstream b; b << "Building: " << (sel->hp * 100 / std::max(1, sel->maxHp)) << "%";
        line(b.str(), termHigh());
    }
    y++;
    if (sel->owner == 0) {
        if (sel->type == E_PEASANT) {
            line("[B] Build", termAccent());
            line("[Enter] Move/Gather", termAccent());
        } else if (isUnit(sel->type)) {
            line("[Enter] Move/Attack", termAccent());
        } else if (isBuilding(sel->type) && !sel->underConstruction) {
            if (isTrainProducer(sel->type)) line("[T] Train", termAccent());
            if (sel->type == E_MARKET) line("[R] Trade", termAccent());
            if (sel->type == E_BLACKSMITH) line("[R] Research", termAccent());
            if (sel->type == E_FARM) {
                line("Generates food", termAccent());
                std::ostringstream ripe; ripe << "Ripe: " << sel->storedFood << " / 20";
                line(ripe.str(), termAccent());
            }
        }
    }
}

static void terminalDrawPanel(TerminalFrame& frame) {
    int panelW = 24;
    int panelX = frame.cols - panelW;
    if (panelX < 1) return;
    termFillV(frame, panelX - 1, 0, frame.rows, '|', termDim(), termBg());
    terminalDrawMinimap(frame, panelX, panelW);
    int mmH = std::min(g.viewH / 3, 14);
    int y = 1 + mmH + 1;
    termFillH(frame, y - 1, panelX, panelW, '-', termDim(), termBg());
    terminalDrawSelection(frame, panelX, panelW, y);
}

static void terminalDrawBottom(TerminalFrame& frame) {
    int botY2 = frame.rows - 2;
    int botY1 = frame.rows - 1;
    termFillH(frame, botY2, 0, frame.cols, ' ', termFg(), termBar());
    termFillH(frame, botY1, 0, frame.cols, ' ', termFg(), termBar());
    std::string line;
    if (g.mode == M_BUILD_SELECT)
        line = " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [Esc] ";
    else if (g.mode == M_TRAIN_SELECT)
        line = trainPromptFor(findEntity(g.selectedId));
    else if (g.mode == M_MARKET_TRADE)
        line = " MARKET: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g  [Esc] ";
    else if (g.mode == M_PAUSED)
        line = " PAUSED - Press [P] to resume ";
    else if (g.mode == M_GAME_OVER)
        line = (g.winner == 0) ? " VICTORY! The realm is yours. [Enter/Q] Main menu  [X] Exit "
                               : " DEFEAT! Your kingdom has fallen. [Enter/Q] Main menu  [X] Exit ";
    else if (g.groupAssignPending)
        line = " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel ";
    else
        line = " Arrows:Move  Spc:Select  Enter:Cmd  B:Build T:Train ?:Help D:Diag V:Save L:Load Q:Resign X:Exit ";
    termPutString(frame, 1, botY2, termTrunc(line, frame.cols - 2), termFg(), termBar());
    if (g.statusTimer > 0) {
        termPutString(frame, 1, botY1, termTrunc(">> " + g.statusMsg, frame.cols - 14), termHigh(), termBar());
    }
    std::ostringstream pos; pos << "(" << g.cursorX << "," << g.cursorY << ")";
    termPutString(frame, std::max(0, frame.cols - (int)pos.str().size() - 1), botY1, pos.str(), termDim(), termBar());
}

static void registerTerminalKeyTokens(const TerminalFrame& frame) {
    int y = frame.rows - 2;
    std::string line;
    std::vector<std::pair<std::string, int>> tokens;
    if (g.mode == M_BUILD_SELECT) {
        line = " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [Esc] ";
        tokens = terminalBuildTokens();
    } else if (g.mode == M_TRAIN_SELECT) {
        Entity* sel = findEntity(g.selectedId);
        line = trainPromptFor(sel);
        tokens = sel ? trainOptionTokensFor(sel->type) : std::vector<std::pair<std::string, int>>{{"Esc", 27}};
    } else if (g.mode == M_MARKET_TRADE) {
        line = " MARKET: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g  [Esc] ";
        tokens = {{"[G]", 'g'}, {"[W]", 'w'}, {"[F]", 'f'}, {"[V]", 'v'}, {"[Esc]", 27}};
    } else if (g.mode == M_PAUSED) {
        line = " PAUSED - Press [P] to resume ";
        tokens = {{"[P]", 'p'}};
    } else if (g.mode == M_GAME_OVER) {
        line = (g.winner == 0) ? " VICTORY! The realm is yours. [Enter/Q] Main menu  [X] Exit "
                               : " DEFEAT! Your kingdom has fallen. [Enter/Q] Main menu  [X] Exit ";
        tokens = {{"Enter", '\n'}, {"Q", 'q'}, {"[X]", 'x'}};
    } else {
        line = " Arrows:Move  Spc:Select  Enter:Cmd  B:Build T:Train ?:Help D:Diag V:Save L:Load Q:Resign X:Exit ";
        tokens = {{"B:Build", 'b'}, {"T:Train", 't'}, {"?:Help", '?'}, {"D:Diag", 'd'},
                  {"V:Save", 'v'}, {"L:Load", 'l'}, {"Q:Resign", 'q'}, {"X:Exit", 'x'}};
    }
    size_t searchFrom = 0;
    for (const auto& token : tokens) {
        size_t pos = line.find(token.first, searchFrom);
        if (pos == std::string::npos) pos = line.find(token.first);
        if (pos == std::string::npos || (int)pos >= frame.cols - 1) continue;
        int cells = std::min((int)token.first.size(), std::max(1, frame.cols - 1 - (int)pos));
        SDL_Rect r{(int)pos * frame.cellW, y * frame.cellH, cells * frame.cellW, frame.cellH};
        registerKeyHit(r, token.second);
        drawHoverMark(r, termHigh());
        searchFrom = pos + token.first.size();
    }
}

static void terminalDrawHelpOverlay(TerminalFrame& frame) {
    if (!g.helpOverlay) return;
    int w = std::min(frame.cols - 4, 78);
    int h = std::min(frame.rows - 4, 24);
    if (w < 20 || h < 8) return;
    int x = std::max(1, (frame.cols - w) / 2);
    int y = std::max(1, (frame.rows - h) / 2);
    Color fg = termFg();
    Color bg = rgb(6, 8, 11);
    for (int yy = 0; yy < h; ++yy) {
        termFillH(frame, y + yy, x, w, ' ', fg, bg);
    }
    termFillH(frame, y, x, w, '-', termDim(), bg);
    termFillH(frame, y + h - 1, x, w, '-', termDim(), bg);
    termFillV(frame, x, y, h, '|', termDim(), bg);
    termFillV(frame, x + w - 1, y, h, '|', termDim(), bg);
    termPut(frame, x, y, '+', termDim(), bg);
    termPut(frame, x + w - 1, y, '+', termDim(), bg);
    termPut(frame, x, y + h - 1, '+', termDim(), bg);
    termPut(frame, x + w - 1, y + h - 1, '+', termDim(), bg);

    int row = y + 1;
    termPutString(frame, x + 2, row++, "Help", termHigh(), bg);
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    for (int i = 0; i < n && row < y + h - 5; ++i) {
        std::ostringstream line;
        line << commands[i].keys << "  " << commands[i].label << " - " << commands[i].help;
        termPutString(frame, x + 2, row++, termTrunc(line.str(), w - 4), fg, bg);
    }
    if (row < y + h - 4) {
        termPutString(frame, x + 2, row++, "Food: berries, hunting, farms, wheat, and fishing feed your stockpile.", termDim(), bg);
    }
    if (row < y + h - 3) {
        termPutString(frame, x + 2, row++, "Winter drains food from living units; starvation damages units.", termDim(), bg);
    }
    termPutString(frame, x + 2, y + h - 2, "Press ? to close", termHigh(), bg);
}

static TerminalFrame buildAsciiTerminalFrame() {
    TerminalFrame frame = makeBlankTerminalFrame();
    updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
    terminalDrawTop(frame);
    terminalDrawTerrainBar(frame);
    terminalDrawPanel(frame);
    terminalDrawBottom(frame);
    terminalDrawHelpOverlay(frame);
    return frame;
}

static void drawTerminalCellAt(const SDL_Rect& r, const TerminalCell& cell, TTF_Font* font) {
    setDraw(cell.bg);
    SDL_RenderFillRect(s.ren, &r);
    if (cell.ch == ' ') return;
    std::string text(1, cell.ch);
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, cell.fg);
    if (!tex) return;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    if (w <= 0 || h <= 0) return;
    float scale = std::min(r.w / (float)w, r.h / (float)h);
    int dw = std::max(1, (int)std::lround(w * scale));
    int dh = std::max(1, (int)std::lround(h * scale));
    SDL_Rect dst{r.x + (r.w - dw) / 2, r.y + (r.h - dh) / 2, dw, dh};
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, cell.fg.a);
    SDL_RenderCopy(s.ren, tex, nullptr, &dst);
}

static void drawAsciiTerminalMap(const TerminalFrame& frame) {
    int mapCellW = 9, mapCellH = 18;
    terminalMapCellMetrics(mapCellW, mapCellH);
    SDL_Rect mr = terminalMapPixelRect(frame);
    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &mr);
    SDL_RenderSetClipRect(s.ren, &mr);
    for (int sy = 0; sy < g.viewH; ++sy) {
        int my = g.viewY + sy;
        for (int sx = 0; sx < g.viewW; ++sx) {
            int mx = g.viewX + sx;
            if (!inBounds(mx, my)) continue;
            SDL_Rect r{mr.x + sx * mapCellW, mr.y + sy * mapCellH, mapCellW, mapCellH};
            drawTerminalCellAt(r, terminalMapCell(mx, my), s.mono);
        }
    }
    SDL_RenderSetClipRect(s.ren, nullptr);
}

static void drawAsciiTerminalFrame(bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    TerminalFrame frame = buildAsciiTerminalFrame();
    setDraw(termBg());
    SDL_RenderClear(s.ren);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            const TerminalCell& cell = frame.at(x, y);
            SDL_Rect r{x * frame.cellW, y * frame.cellH, frame.cellW, frame.cellH};
            drawTerminalCellAt(r, cell, s.mono);
        }
    }
    drawAsciiTerminalMap(frame);
    registerTerminalKeyTokens(frame);
    if (present) SDL_RenderPresent(s.ren);
}

static void updateAsciiMobileCamera(int cols, int rows) {
    g.viewW = std::max(1, std::min(cols, MAP_W));
    g.viewH = std::max(1, std::min(rows, MAP_H));
    g.viewX = g.cursorX - g.viewW / 2;
    g.viewY = g.cursorY - g.viewH / 2;
    g.viewX = std::max(0, std::min(g.viewX, MAP_W - g.viewW));
    g.viewY = std::max(0, std::min(g.viewY, MAP_H - g.viewH));
}

static void drawAsciiMobileMap() {
    SDL_Rect mr = mapRect();
    int cellW = 8, cellH = 15;
    asciiMobileCellMetrics(cellW, cellH);
    int cols = std::max(1, std::min(MAP_W, mr.w / std::max(1, cellW)));
    int rows = std::max(1, std::min(MAP_H, mr.h / std::max(1, cellH)));
    updateAsciiMobileCamera(cols, rows);

    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &mr);
    SDL_RenderSetClipRect(s.ren, &mr);
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    for (int sy = 0; sy < g.viewH; ++sy) {
        int my = g.viewY + sy;
        for (int sx = 0; sx < g.viewW; ++sx) {
            int mx = g.viewX + sx;
            if (!inBounds(mx, my)) continue;
            SDL_Rect r{mr.x + sx * cellW, mr.y + sy * cellH, cellW, cellH};
            drawTerminalCellAt(r, terminalMapCell(mx, my), font);
        }
    }
    SDL_RenderSetClipRect(s.ren, nullptr);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &mr);
}

static void drawAsciiMobileMiniMapText(SDL_Rect r) {
    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &r);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &r);

    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    int cellW = 8, cellH = 15;
    if (font) {
        int w = 0, h = 0;
        TTF_SizeText(font, "M", &w, &h);
        cellW = std::max(7, w);
        cellH = std::max(13, TTF_FontLineSkip(font));
    }
    int cols = std::max(1, (r.w - 6) / cellW);
    int rows = std::max(1, (r.h - 6) / cellH);
    int x0 = r.x + 3;
    int y0 = r.y + 3;
    for (int yy = 0; yy < rows; ++yy) {
        for (int xx = 0; xx < cols; ++xx) {
            int mx = xx * MAP_W / std::max(1, cols);
            int my = yy * MAP_H / std::max(1, rows);
            char ch = ' ';
            Color fg = termDim();
            if (g.map[my][mx].explored[0]) {
                Terrain t = g.map[my][mx].terrain;
                if (t == T_WATER || t == T_SHALLOWS) { ch = '~'; fg = rgb(90, 150, 220); }
                else if (t == T_MOUNTAIN || t == T_STONE) { ch = '^'; fg = rgb(150, 150, 150); }
                else if (t == T_GOLD) { ch = '$'; fg = rgb(235, 210, 70); }
                else if (t == T_CASTLE_WALL || t == T_CASTLE_GATE) { ch = '#'; fg = rgb(190, 190, 190); }
                else { ch = '.'; fg = rgb(90, 135, 90); }
            }
            if (g.map[my][mx].visible[0]) {
                Entity* ent = entityAt(mx, my);
                if (ent && ent->alive) {
                    ch = isBuilding(ent->type) ? '#' : '*';
                    fg = ownerTermFg(ent->owner);
                }
            }
            drawText(x0 + xx * cellW, y0 + yy * cellH, std::string(1, ch), fg, font);
        }
    }

    SDL_Rect view{
        x0 + g.viewX * std::max(1, cols * cellW) / MAP_W,
        y0 + g.viewY * std::max(1, rows * cellH) / MAP_H,
        std::max(3, g.viewW * std::max(1, cols * cellW) / MAP_W),
        std::max(3, g.viewH * std::max(1, rows * cellH) / MAP_H)
    };
    setDraw(termHigh());
    SDL_RenderDrawRect(s.ren, &view);
}

static void drawAsciiMobileHelpOverlay() {
    if (!g.helpOverlay) return;
    int pad = mobileSafePad();
    SDL_Rect r{pad, pad, std::max(1, s.winW - pad * 2), std::max(1, s.winH - pad * 2)};
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(rgb(3, 5, 8, 245));
    SDL_RenderFillRect(s.ren, &r);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &r);
    int x = r.x + 14;
    int y = r.y + 12;
    int w = std::max(1, r.w - 28);
    drawTextFit(x, y, "REALM HELP", termHigh(), w, s.mono); y += 28;
    drawTextFit(x, y, "Tap map: select or command. Drag map: pan.", termFg(), w, s.monoSmall ? s.monoSmall : s.mono); y += 22;
    drawTextFit(x, y, "Use terminal buttons for build, attack, rally, pause, and idle.", termFg(), w, s.monoSmall ? s.monoSmall : s.mono); y += 26;
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    for (int i = 0; i < n && y < r.y + r.h - 42; ++i) {
        std::ostringstream line;
        line << commands[i].label << " - " << commands[i].help;
        drawTextFit(x, y, trimPanelLine(line.str(), 62), termDim(), w, s.monoSmall ? s.monoSmall : s.mono);
        y += 19;
    }
    drawTextFit(x, r.y + r.h - 28, "Tap [Help] to close", termHigh(), w, s.monoSmall ? s.monoSmall : s.mono);
}

static void drawAsciiMobileHud() {
    SDL_Rect pr = panelRect();
    int pad = mobileSafePad();
    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &pr);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &pr);

    Player& p = g.players[0];
    std::ostringstream res;
    res << "REALM  G:" << p.gold << " W:" << p.wood << " F:" << p.food
        << " Pop:" << p.supply << "/" << p.supplyMax;
    int y = pr.y + pad;
    int textW = std::max(1, pr.w - pad * 2);
    drawTextFit(pr.x + pad, y, res.str(), termFg(), textW, s.monoSmall ? s.monoSmall : s.mono);
    y += 22;

    std::ostringstream tile;
    if (inBounds(g.cursorX, g.cursorY)) {
        const Tile& ct = g.map[g.cursorY][g.cursorX];
        tile << "Tile: " << terrainName(ct.terrain);
        if (ct.resources > 0) tile << " Res:" << ct.resources;
    } else {
        tile << "Tile: unknown";
    }
    SDL_Rect mm = miniMapRect();
    int summaryW = mobilePortrait() ? std::max(1, mm.x - (pr.x + pad) - 10) : textW;
    drawTextFit(pr.x + pad, y, termTrunc(tile.str(), 54), termDim(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    y += 20;
    drawTextFit(pr.x + pad, y, mobileSelectionSummary(), termHigh(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    y += 20;
    if (s.mobileBuildType != E_NONE) {
        drawTextFit(pr.x + pad, y, std::string("Placing ") + STATS[s.mobileBuildType].name,
                    termAccent(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    } else if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE || g.mode == M_BUILD_SELECT) {
        drawTextFit(pr.x + pad, y, modeName(g.mode), termAccent(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    } else if (g.statusTimer > 0) {
        drawTextFit(pr.x + pad, y, ">> " + g.statusMsg, termHigh(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    }

    drawAsciiMobileMiniMapText(mm);
    for (const MobileButton& b : mobileHudButtons()) {
        bool active = (b.id == "build" && g.mode == M_BUILD_SELECT)
                   || (b.id == "attack" && g.mode == M_ATTACK_MOVE)
                   || (b.id == "rally" && g.mode == M_RALLY_SET);
        drawConsoleButton(b, active, b.id == "cancel");
    }
    if (g.statusTimer > 0) g.statusTimer--;
}

static void drawAsciiMobileFrame(bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    s.isometric = false;
    setDraw(termBg());
    SDL_RenderClear(s.ren);
    drawAsciiMobileMap();
    drawAsciiMobileHud();
    drawAsciiMobileHelpOverlay();
    if (present) SDL_RenderPresent(s.ren);
}

static bool saveRendererPixels(const std::string& path) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, s.winW, s.winH, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        std::cerr << "realm: screenshot surface failed: " << SDL_GetError() << "\n";
        return false;
    }
    bool ok = SDL_RenderReadPixels(s.ren, nullptr, SDL_PIXELFORMAT_ARGB8888,
        surface->pixels, surface->pitch) == 0;
    if (!ok) {
        std::cerr << "realm: screenshot read failed: " << SDL_GetError() << "\n";
    } else if (SDL_SaveBMP(surface, path.c_str()) != 0) {
        std::cerr << "realm: screenshot save failed: " << SDL_GetError() << "\n";
        ok = false;
    }
    SDL_FreeSurface(surface);
    SDL_RenderPresent(s.ren);
    return ok;
}

static void drawHelpOverlay() {
    if (!g.helpOverlay) return;
    SDL_Rect r{std::max(20, s.winW / 2 - 360), std::max(20, s.winH / 2 - 260), 720, 520};
    if (r.x + r.w > s.winW - 20) r.w = std::max(320, s.winW - 40), r.x = 20;
    if (r.y + r.h > s.winH - 20) r.h = std::max(320, s.winH - 40), r.y = 20;
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(rgb(6,8,11,235)); SDL_RenderFillRect(s.ren, &r);
    setDraw(rgb(160,170,185,220)); SDL_RenderDrawRect(s.ren, &r);

    int x = r.x + 18, y = r.y + 16;
    drawText(x, y, "Help", rgb(255,235,145)); y += 28;
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    for (int i = 0; i < n && y < r.y + r.h - 96; i++) {
        std::ostringstream line;
        line << commands[i].keys << "  " << commands[i].label << " - " << commands[i].help;
        drawText(x, y, trimPanelLine(line.str(), 78), rgb(220,225,220));
        y += 20;
    }
    y += 10;
    drawText(x, y, "Food: berries, hunting, farms/mills, wheat work, and fishing all feed your stockpile.", rgb(185,195,200)); y += 20;
    drawText(x, y, "Winter drains food from living units; starvation damages units when stores run out.", rgb(185,195,200)); y += 20;
    drawText(x, y, "Legend: owner colours mark player/enemies; animals are neutral; ! marks recent combat.", rgb(185,195,200)); y += 20;
    drawText(x, r.y + r.h - 28, "Press ? to close", rgb(255,230,120));
}

static void clearTextCache() {
    for (auto& kv : s.textCache) SDL_DestroyTexture(kv.second);
    s.textCache.clear();
}

static bool pointInRect(int x, int y, SDL_Rect r) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static std::vector<MobileButton> mobileSplashButtons() {
    std::vector<MobileButton> buttons;
    int pad = mobileSafePad();
    int bw = std::min(360, std::max(220, s.winW - pad * 2));
    int x = (s.winW - bw) / 2;
    int y = mobilePortrait() ? std::max(120, s.winH / 4) : std::max(70, s.winH / 5);
    if (s.mobileSplashSettings) {
        addGridButtons(buttons, x, y, bw,
            {{"orient", s.mobileOrientation == 0 ? "Orientation Auto" : s.mobileOrientation == 1 ? "Portrait Lock" : "Landscape Lock"},
             {"uiscale", "UI Scale"},
             {"zoom", "Zoom Level"},
             {"minimap", s.mobileMinimapTap ? "Minimap On" : "Minimap Off"},
             {"confirm", s.mobileConfirmCommands ? "Confirm On" : "Confirm Off"},
             {"edge", s.mobileEdgeScroll ? "Edge Scroll On" : "Edge Scroll Off"},
             {"settings", "Back"}}, 1);
        return buttons;
    }
    if (s.mobileSplashHelp) {
        addGridButtons(buttons, x, y + 230, bw, {{"helpback", "Back"}}, 1);
        return buttons;
    }
    std::vector<std::pair<std::string, std::string>> mainItems{
        {"new", "New Game"}, {"load", "Load Game"}
    };
    if (!s.asciiOnly) {
        mainItems.push_back({"visual", displayMode == DM_ASCII ? "Visual ASCII" : "Visual Tileset"});
    }
#if defined(REALM_WEB)
    mainItems.push_back({"fullscreen", "Full Screen"});
    mainItems.push_back({"settings", "Settings"});
    mainItems.push_back({"help", "Help"});
#else
    mainItems.push_back({"settings", "Settings"});
    mainItems.push_back({"help", "Help"});
    mainItems.push_back({"quit", "Quit"});
#endif
    addGridButtons(buttons, x, y, bw, mainItems, 1);
    return buttons;
}

static void drawAsciiMobileSplash(int numAIs, int biomeIdx) {
    setDraw(termBg());
    SDL_RenderClear(s.ren);
    int pad = mobileSafePad();
    int x = pad;
    int y = pad + 8;
    int w = std::max(1, s.winW - pad * 2);
    setDraw(termDim());
    SDL_Rect border{pad, pad, w, std::max(1, s.winH - pad * 2)};
    SDL_RenderDrawRect(s.ren, &border);
    drawTextFit(x + 10, y, "R E A L M", termHigh(), std::max(1, w - 20), s.mono); y += 28;
    drawTextFit(x + 10, y, "-- Medieval Warlord --", termAccent(), std::max(1, w - 20),
                s.monoSmall ? s.monoSmall : s.mono);
    y += 32;

    if (s.mobileSplashHelp) {
        drawTextFit(x + 10, y, "TOUCH CONTROLS", termHigh(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono); y += 26;
        drawTextFit(x + 10, y, "Tap selects or commands. Drag the map to pan.", termFg(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono); y += 22;
        drawTextFit(x + 10, y, "Tap terminal buttons for build, attack, rally, pause, and idle.", termFg(),
                    std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono); y += 22;
        drawTextFit(x + 10, y, "Long press inspects. Double tap selects nearby units of the same type.", termDim(),
                    std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono);
    } else if (s.mobileSplashSettings) {
        drawTextFit(x + 10, y, "SETTINGS", termHigh(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono); y += 26;
        drawTextFit(x + 10, y, "Tap an option to cycle it.", termDim(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono);
    } else {
        static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
        std::ostringstream ss;
        ss << "Opponents:" << numAIs << "  Biome:" << biomeNames[biomeIdx] << "  Display:ASCII";
        drawTextFit(x + 10, y, ss.str(), termFg(), std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono); y += 24;
        drawTextFit(x + 10, y, "Tap [New Game] to start. Tap [Visual ASCII] for tileset.", termDim(),
                    std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono);
    }

    for (const MobileButton& b : mobileSplashButtons()) {
        drawConsoleButton(b, b.id == "visual", b.id == "quit");
    }
    SDL_RenderPresent(s.ren);
}

static void drawMobileSplash(int numAIs, int biomeIdx) {
    if (displayMode == DM_ASCII) {
        drawAsciiMobileSplash(numAIs, biomeIdx);
        return;
    }
    setDraw(rgb(7,10,15)); SDL_RenderClear(s.ren);
    int pad = mobileSafePad();
    drawTextFit(pad, pad + 8, "R E A L M", rgb(255,235,145), s.winW - pad * 2);
    drawTextFit(pad, pad + 34, "Medieval Warlord", rgb(180,205,230), s.winW - pad * 2);
    if (s.mobileSplashHelp) {
        int y = pad + 82;
        drawTextFit(pad, y, "Touch Controls", rgb(255,230,135), s.winW - pad * 2); y += 28;
        drawTextFit(pad, y, "Tap selects or commands. Drag the map to pan.", rgb(220,225,220), s.winW - pad * 2); y += 24;
        drawTextFit(pad, y, "Use command buttons for build, attack, rally, pause, and idle peasants.", rgb(220,225,220), s.winW - pad * 2); y += 24;
        drawTextFit(pad, y, "Long press inspects. Double tap selects nearby units of the same type.", rgb(220,225,220), s.winW - pad * 2);
    } else if (!s.mobileSplashSettings) {
        static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
        std::ostringstream ss;
        ss << "Opponents " << numAIs << "   Biome " << biomeNames[biomeIdx];
        drawTextFit(pad, pad + 64, ss.str(), rgb(220,225,220), s.winW - pad * 2);
    }
    for (const MobileButton& b : mobileSplashButtons()) drawButton(b);
    SDL_RenderPresent(s.ren);
}

static bool handleMobileSplashTap(int px, int py, int& numAIs, int& biomeIdx, bool& done) {
    for (const MobileButton& b : mobileSplashButtons()) {
        if (!pointInRect(px, py, b.r)) continue;
        if (b.id == "new") { done = true; return true; }
        if (b.id == "load") { s.loadGameRequested = true; done = true; return true; }
        if (b.id == "fullscreen") { toggleFullscreen(); return true; }
        if (b.id == "visual" && !s.asciiOnly) {
            if (displayMode == DM_ASCII) {
                displayMode = DM_EMOJI;
                s.isometric = true;
            } else {
                displayMode = DM_ASCII;
                s.isometric = false;
            }
            updateViewMetrics(true);
            return true;
        }
        if (b.id == "quit") { gfxShutdown(); std::exit(0); }
        if (b.id == "settings") { s.mobileSplashSettings = !s.mobileSplashSettings; s.mobileSplashHelp = false; return true; }
        if (b.id == "help") { s.mobileSplashHelp = true; s.mobileSplashSettings = false; return true; }
        if (b.id == "helpback") { s.mobileSplashHelp = false; return true; }
        if (b.id == "orient") { s.mobileOrientation = (s.mobileOrientation + 1) % 3; return true; }
        if (b.id == "uiscale") { s.mobileUiScale = s.mobileUiScale >= 1.25f ? 0.9f : s.mobileUiScale + 0.1f; return true; }
        if (b.id == "zoom") { setZoom(s.tile >= 36 ? 20 : s.tile + 4); return true; }
        if (b.id == "minimap") { s.mobileMinimapTap = !s.mobileMinimapTap; return true; }
        if (b.id == "confirm") { s.mobileConfirmCommands = !s.mobileConfirmCommands; return true; }
        if (b.id == "edge") { s.mobileEdgeScroll = !s.mobileEdgeScroll; return true; }
    }
    (void)numAIs; (void)biomeIdx;
    return false;
}

static int applySplashChoice(int ch, int& numAIs, int& biomeIdx) {
    if (ch == '\n' || ch == '\r') {
        g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
        return 1;
    }
    if (ch == 'q' || ch == 'Q' || ch == 'x' || ch == 'X') {
#if defined(REALM_WEB)
        setStatus("Close the browser tab to exit.");
        return 0;
#else
        return -1;
#endif
    }
    if (ch == '1') numAIs = 1;
    else if (ch == '2') numAIs = 2;
    else if (ch == '3') numAIs = 3;
    else if (ch == '0') biomeIdx = 7;
    else if (ch == 't' || ch == 'T') biomeIdx = 0;
    else if (ch == 'd' || ch == 'D') biomeIdx = 1;
    else if (ch == 's' || ch == 'S') biomeIdx = 2;
    else if (ch == 'w' || ch == 'W') biomeIdx = 3;
    else if (ch == 'f' || ch == 'F') biomeIdx = 4;
    else if (ch == 'c' || ch == 'C') biomeIdx = 6;
    else if (ch == '4') displayMode = DM_ASCII;
    else if (ch == '5' && !s.asciiOnly) { displayMode = DM_EMOJI; s.isometric = true; }
    return 0;
}

static bool handleSplashKeyHit(int px, int py, int& numAIs, int& biomeIdx, int& result) {
    for (const KeyHit& hit : s.keyHits) {
        if (!pointInRect(px, py, hit.r)) continue;
        result = applySplashChoice(hit.ch, numAIs, biomeIdx);
        return true;
    }
    return false;
}

static void drawSplash(int numAIs, int biomeIdx) {
    s.keyHits.clear();
    SDL_GetMouseState(&s.mouseX, &s.mouseY);
    if (isMobileGui()) {
        drawMobileSplash(numAIs, biomeIdx);
        return;
    }
    setDraw(rgb(7,10,15)); SDL_RenderClear(s.ren);
    int col = s.winW/2 - 360;
    int y = std::max(40, s.winH/2 - 255);
    auto line = [&](const std::string& t, Color c = rgb(220,225,220), int dy = 22) {
        drawText(col, y, t, c); y += dy;
    };
    drawText(col+210, y, "R  E  A  L  M", rgb(255,235,145)); y += 28;
    drawText(col+175, y, "-- Medieval Warlord --", rgb(180,205,230)); y += 40;
    line("You are lord of a small settlement in a hostile realm.");
    line("Gather resources, build an army, and outlast every rival.");
    y += 10;
    line("CONTROLS", rgb(255,230,135));
    line("  Space/click    Select unit or building");
    line("  Enter/R-click  Command (move/attack/gather)");
    line("  Mouse wheel    Zoom in/out");
    line("  Middle-drag    Pan the map");
    line("  B/T/A/H/P      Build / Train / Military / Town hall / Pause");
    y += 10;
    line("OPPONENTS", rgb(255,230,135));
    drawKeyTokensInText(col, y, "  [1] Duel       [2] Three-way     [3] Four-way",
                        {{"[1]", '1'}, {"[2]", '2'}, {"[3]", '3'}},
                        rgb(220,225,220), 720); y += 22;
    y += 4;
    line("BIOME", rgb(255,230,135));
    drawKeyTokensInText(col, y, "  [0] Random    [T] Temperate  [D] Desert",
                        {{"[0]", '0'}, {"[T]", 't'}, {"[D]", 'd'}},
                        rgb(220,225,220), 720); y += 22;
    drawKeyTokensInText(col, y, "  [S] Snow      [W] Swamp      [F] Forest",
                        {{"[S]", 's'}, {"[W]", 'w'}, {"[F]", 'f'}},
                        rgb(220,225,220), 720); y += 22;
    drawKeyTokensInText(col, y, "  [C] Coastal",
                        {{"[C]", 'c'}},
                        rgb(220,225,220), 720); y += 22;
    if (!s.asciiOnly) {
        y += 4;
        line("DISPLAY", rgb(255,230,135));
        std::string displayLine = std::string("  [4] ASCII     [5] Tileset     > ") + (displayMode==DM_EMOJI ? "Tileset" : "ASCII");
        drawKeyTokensInText(col, y, displayLine, {{"[4]", '4'}, {"[5]", '5'}},
                            rgb(220,225,220), 720); y += 22;
    }
    y += 10;
    static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
    std::ostringstream ss; ss << "  > Opponents: " << numAIs << "    Biome: " << biomeNames[biomeIdx];
    line(ss.str(), rgb(255,245,180));
    y += 4;
    drawKeyTokensInText(col, y, "  [Enter] Start game            [Q/X] Quit",
                        {{"[Enter]", '\n'}, {"Q", 'q'}, {"X", 'x'}},
                        rgb(210,230,245), 720);
    SDL_RenderPresent(s.ren);
}

} // namespace

bool gfxInit() {
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n"; return false;
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n"; return false;
    }

    s.win = SDL_CreateWindow("Realm - graphical terminal renderer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, s.winW, s.winH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!s.win) { std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n"; return false; }
    s.ren = SDL_CreateRenderer(s.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s.ren) s.ren = SDL_CreateRenderer(s.win, -1, SDL_RENDERER_SOFTWARE);
    if (!s.ren) { std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n"; return false; }
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);

    std::vector<std::string> monoPaths = {
        // Native Windows / MSYS2.
        "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/Consola.ttf",
        "C:/Windows/Fonts/cour.ttf", "C:/Windows/Fonts/Cour.ttf",
        // WSL / WSLg accessing the Windows font directory.
        "/mnt/c/Windows/Fonts/consola.ttf", "/mnt/c/Windows/Fonts/Consola.ttf",
        "/mnt/c/Windows/Fonts/cour.ttf", "/mnt/c/Windows/Fonts/Cour.ttf",
        "/c/Windows/Fonts/consola.ttf", "/c/Windows/Fonts/cour.ttf",
        // Linux/macOS fallbacks.
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc", "/Library/Fonts/Menlo.ttc"
    };
    std::vector<std::string> emojiPaths = {
#if defined(REALM_WEB)
        "/assets/fonts/RealmSymbols.ttf",
#endif
        // Native Windows / MSYS2.
        "C:/Windows/Fonts/seguiemj.ttf", "C:/Windows/Fonts/Segoe UI Emoji.ttf",
        "C:/Windows/Fonts/seguisym.ttf",
        // WSL / WSLg accessing the Windows font directory.
        "/mnt/c/Windows/Fonts/seguiemj.ttf", "/mnt/c/Windows/Fonts/seguisym.ttf",
        "/c/Windows/Fonts/seguiemj.ttf", "/c/Windows/Fonts/seguisym.ttf",
        // Linux fallbacks if installed.
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/opentype/noto/NotoColorEmoji.ttf",
        "/usr/local/share/fonts/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/truetype/ancient-scripts/Symbola_hint.ttf",
        // macOS fallbacks.
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/System/Library/Fonts/Apple Symbols.ttf"
    };
    s.mono = openFont(monoPaths, 16, &s.monoPath);
    s.monoSmall = openFont(monoPaths, 13);
    s.emoji = openFont(emojiPaths, 28, &s.emojiPath);
    if (!s.mono) { std::cerr << "Could not find a monospace font.\n"; return false; }
    if (!s.emoji) {
        s.emoji = s.mono;
        s.emojiFontLoaded = false;
        std::cerr << "No tileset symbol font found; using ASCII glyph fallbacks.\n";
    } else {
        s.emojiFontLoaded = true;
    }
    std::cerr << "Text font: " << (s.monoPath.empty() ? "<unknown>" : s.monoPath) << "\n";
    std::cerr << "Tileset symbol font: " << (s.emojiFontLoaded ? s.emojiPath : std::string("<fallback>")) << "\n";
    SDL_StartTextInput();
    return true;
}

void gfxShutdown() {
#if !defined(REALM_WEB)
    tilesetAssetsClear();
#endif
    clearTextCache();
    if (s.mono) TTF_CloseFont(s.mono);
    if (s.monoSmall) TTF_CloseFont(s.monoSmall);
    if (s.emoji && s.emoji != s.mono) TTF_CloseFont(s.emoji);
    if (s.ren) SDL_DestroyRenderer(s.ren);
    if (s.win) SDL_DestroyWindow(s.win);
    TTF_Quit();
    SDL_Quit();
}

int gfxSplashFrame(int& numAIs, int& biomeIdx) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return -1;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            s.winW = e.window.data1; s.winH = e.window.data2;
        }
        if (!isMobileGui() && e.type == SDL_MOUSEMOTION) {
            s.mouseX = e.motion.x;
            s.mouseY = e.motion.y;
        }
        if (!isMobileGui() && e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            s.mouseX = e.button.x;
            s.mouseY = e.button.y;
            int result = 0;
            if (handleSplashKeyHit(e.button.x, e.button.y, numAIs, biomeIdx, result)) return result;
        }
        if (isMobileGui() && (e.type == SDL_FINGERDOWN || e.type == SDL_MOUSEBUTTONDOWN)) {
            int px = 0, py = 0;
            if (e.type == SDL_FINGERDOWN) {
                px = (int)std::lround(e.tfinger.x * s.winW);
                py = (int)std::lround(e.tfinger.y * s.winH);
                s.suppressNextMouse = true;
            } else {
                if (s.suppressNextMouse) { s.suppressNextMouse = false; continue; }
                if (e.button.button != SDL_BUTTON_LEFT) continue;
                px = e.button.x; py = e.button.y;
            }
            bool done = false;
            if (handleMobileSplashTap(px, py, numAIs, biomeIdx, done) && done) {
                g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
                return 1;
            }
            continue;
        }
        if (e.type != SDL_KEYDOWN) continue;
        SDL_Keycode k = e.key.keysym.sym;
        int ch = 0;
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) ch = '\n';
        else if (k == SDLK_q) ch = 'q';
        else if (k == SDLK_x) ch = 'x';
        else if (k == SDLK_1) ch = '1';
        else if (k == SDLK_2) ch = '2';
        else if (k == SDLK_3) ch = '3';
        else if (k == SDLK_0) ch = '0';
        else if (k == SDLK_t) ch = 't';
        else if (k == SDLK_d) ch = 'd';
        else if (k == SDLK_s) ch = 's';
        else if (k == SDLK_w) ch = 'w';
        else if (k == SDLK_f) ch = 'f';
        else if (k == SDLK_v) ch = 'v';
        else if (k == SDLK_c) ch = 'c';
        else if (k == SDLK_4) ch = '4';
        else if (k == SDLK_5) ch = '5';
        if (ch) {
            int result = applySplashChoice(ch, numAIs, biomeIdx);
            if (result) return result;
        }
    }
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    drawSplash(numAIs, biomeIdx);
    return 0;
}

void gfxSetAsciiOnly(bool asciiOnly) {
    s.asciiOnly = asciiOnly;
    if (s.asciiOnly) {
        displayMode = DM_ASCII;
        s.isometric = false;
    }
}

int gfxShowSplash() {
    int numAIs = 1;
    int biomeIdx = 7;
    bool loggedReady = false;
    while (true) {
        int result = gfxSplashFrame(numAIs, biomeIdx);
        if (!loggedReady) {
            std::cerr << "realm: main screen ready\n";
            loggedReady = true;
            const char* smoke = std::getenv("REALM_SMOKE_TEST");
            if (smoke) return std::string(smoke) == "match" ? numAIs : -1;
        }
        if (result < 0) { gfxShutdown(); std::exit(0); }
        if (result > 0) return numAIs;
        SDL_Delay(16);
    }
}

bool gfxConsumeLoadGameRequest() {
    bool requested = s.loadGameRequested;
    s.loadGameRequested = false;
    return requested;
}

static std::string captureTimestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d-%H%M%S")
       << '-' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

static void captureIssueBundle() {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::absolute(fs::path("captures") / ("realm-issue-" + captureTimestamp()), ec);
    if (ec) dir = fs::path("captures") / ("realm-issue-" + captureTimestamp());
    fs::create_directories(dir, ec);
    if (ec) {
        setStatus("Issue capture failed.");
        std::cerr << "realm: issue capture mkdir failed " << dir.string()
                  << " error=" << ec.message() << "\n";
        return;
    }

    fs::path savePath = dir / "realm-save.txt";
    fs::path shotPath = dir / "screenshot.bmp";
    fs::path infoPath = dir / "capture-info.txt";

    bool saved = saveGame(savePath.string());
    bool shot = gfxSaveScreenshot(shotPath.string());

    std::ofstream info(infoPath);
    if (info) {
        info << "Realm issue capture\n"
             << "directory: " << dir.string() << "\n"
             << "save: " << savePath.filename().string() << "\n"
             << "screenshot: " << shotPath.filename().string() << "\n"
             << "tick: " << g.tick << "\n"
             << "seed: " << g.seed << "\n"
             << "cursor: " << g.cursorX << "," << g.cursorY << "\n"
             << "view: " << g.viewX << "," << g.viewY << " "
             << g.viewW << "x" << g.viewH << "\n"
             << "projection: " << (displayMode == DM_EMOJI ? "isometric" : "grid") << "\n"
             << "visuals: " << (displayMode == DM_EMOJI ? "tileset" : "ascii") << "\n"
             << "window: " << s.winW << "x" << s.winH << "\n";
    }

    int clip = SDL_SetClipboardText(dir.string().c_str());
    if (saved && shot && clip == 0) {
        setStatus("Issue capture saved; path copied.");
    } else if (saved || shot) {
        setStatus("Issue capture partial; see log.");
    } else {
        setStatus("Issue capture failed.");
    }

    std::cerr << "realm: issue capture dir=" << dir.string()
              << " save=" << (saved ? "ok" : "failed")
              << " screenshot=" << (shot ? "ok" : "failed")
              << " clipboard=" << (clip == 0 ? "ok" : SDL_GetError()) << "\n";
}

void gfxOnNewGame() {
    centerViewOnTile(g.cursorX, g.cursorY);
}

static Entity* primaryOwnedSelection() {
    if (!g.selectedIds.empty()) {
        for (int id : g.selectedIds) {
            Entity* e = findEntity(id);
            if (e && e->alive && e->owner == 0) return e;
        }
        return nullptr;
    }
    Entity* e = findEntity(g.selectedId);
    return (e && e->alive && e->owner == 0) ? e : nullptr;
}

static void mobileSelectIdlePeasant() {
    Entity* pick = nullptr;
    for (auto& e : g.entities) {
        if (e.alive && e.owner == 0 && e.type == E_PEASANT && e.state == S_IDLE) {
            pick = &e;
            break;
        }
    }
    if (!pick) {
        setStatus("No idle peasants");
        return;
    }
    g.selectedId = pick->id;
    g.selectedIds.clear();
    g.cursorX = pick->x;
    g.cursorY = pick->y;
    centerViewOnTile(g.cursorX, g.cursorY);
    setStatus("Idle peasant selected");
}

static void mobileStopSelection() {
    int n = 0;
    auto stopOne = [&](Entity* e) {
        if (!e || !e->alive || e->owner != 0 || !isUnit(e->type)) return;
        e->state = S_IDLE;
        e->targetId = -1;
        e->path.clear();
        e->pathIdx = 0;
        e->attackMove = 0;
        n++;
    };
    if (!g.selectedIds.empty()) for (int id : g.selectedIds) stopOne(findEntity(id));
    else stopOne(findEntity(g.selectedId));
    if (n) setStatus("Stopped.");
}

static EntityType mobileBuildTypeForId(const std::string& id) {
    if (id == "build:house") return E_HOUSE;
    if (id == "build:farm") return E_FARM;
    if (id == "build:barracks") return E_BARRACKS;
    if (id == "build:stable") return E_STABLE;
    if (id == "build:tower") return E_TOWER;
    if (id == "build:lumber") return E_LUMBER_CAMP;
    if (id == "build:mining") return E_MINING_CAMP;
    if (id == "build:mill") return E_MILL;
    if (id == "build:dock") return E_DOCK;
    if (id == "build:castle") return E_CASTLE;
    return E_NONE;
}

static void mobileCancelCommand() {
    s.mobileBuildType = E_NONE;
    s.mobileBuildPage = 0;
    g.mode = M_NORMAL;
    g.dragging = false;
    s.leftDown = false;
    setStatus("Cancelled.");
}

static void handleMobileHudButton(const std::string& id) {
    Entity* sel = primaryOwnedSelection();
    if (id == "cancel") { mobileCancelCommand(); return; }
    if (id == "buildmore") { s.mobileBuildPage = 1; return; }
    if (id == "buildback") { s.mobileBuildPage = 0; return; }
    if (id == "menu") { g.returnToMenu = true; return; }
    if (id == "pause") { handleInput('p'); return; }
    if (id == "fullscreen") { toggleFullscreen(); return; }
    if (id == "idle") { mobileSelectIdlePeasant(); return; }
    if (id == "help") { g.helpOverlay = !g.helpOverlay; setStatus(g.helpOverlay ? "Help open." : "Help closed."); return; }
    if (id == "selectarmy") { handleInput('A'); return; }
    if (id == "stop") { mobileStopSelection(); return; }
    if (id == "move") { g.mode = M_NORMAL; setStatus("Tap a destination."); return; }
    if (id == "gather") { g.mode = M_NORMAL; setStatus("Tap a resource."); return; }
    if (id == "build") {
        if (sel && sel->type == E_PEASANT) {
            g.mode = M_BUILD_SELECT;
            s.mobileBuildPage = 0;
            setStatus("Choose a building.");
        } else {
            setStatus("Select a peasant first.");
        }
        return;
    }
    EntityType bt = mobileBuildTypeForId(id);
    if (bt != E_NONE) {
        if (!sel || sel->type != E_PEASANT) {
            setStatus("Select a peasant first.");
            g.mode = M_NORMAL;
            return;
        }
        s.mobileBuildType = bt;
        g.mode = M_NORMAL;
        setStatus(std::string("Placing ") + STATS[bt].name + ". Tap a valid tile.");
        return;
    }
    if (id == "attack" || id == "attackmove") {
        if (mobileHasSelectedMilitary()) {
            g.mode = M_ATTACK_MOVE;
            setStatus("Tap an attack target or destination.");
        } else {
            setStatus("Select military units first.");
        }
        return;
    }
    if (id == "rally") {
        if (sel && isBuilding(sel->type)) {
            g.mode = M_RALLY_SET;
            setStatus("Tap a rally point.");
        }
        return;
    }
    if (id == "train") {
        if (sel && isBuilding(sel->type)) {
            EntityType tt = mobileDefaultTrainType(sel->type);
            if (tt != E_NONE) orderTrain(*sel, tt);
        }
        return;
    }
    if (id == "trade") {
        if (sel && sel->type == E_MARKET) {
            g.mode = M_MARKET_TRADE;
            setStatus("Choose a market trade.");
        }
        return;
    }
    if (id == "research") {
        if (sel && sel->type == E_BLACKSMITH) {
            g.mode = M_RESEARCH_SELECT;
            setStatus("Choose research.");
        }
        return;
    }
    if (id == "cancelqueue") {
        if (sel && isBuilding(sel->type)) {
            sel->queue.clear();
            if (sel->producing != E_NONE) {
                sel->producing = E_NONE;
                sel->trainProgress = 0;
                sel->trainTime = 0;
                sel->state = S_IDLE;
            }
            setStatus("Queue cancelled.");
        }
    }
}

static bool handleMobileHudHit(int px, int py) {
    if (!isMobileGui()) return false;
    if (!pointInRect(px, py, panelRect())) return false;
    if (s.mobileMinimapTap && pointInRect(px, py, miniMapRect())) {
        moveViewFromMiniMap(px, py, true);
        s.miniMapDown = true;
        return true;
    }
    for (const MobileButton& b : mobileHudButtons()) {
        if (pointInRect(px, py, b.r)) {
            handleMobileHudButton(b.id);
            return true;
        }
    }
    return true;
}

static bool handleKeyHitAt(int px, int py) {
    if (isMobileGui()) return false;
    for (const KeyHit& hit : s.keyHits) {
        if (!pointInRect(px, py, hit.r)) continue;
        bool globalShortcut = g.mode == M_NORMAL || g.mode == M_GAME_OVER || g.mode == M_PAUSED;
        if (globalShortcut && (hit.ch == 'v' || hit.ch == 'V')) {
            saveGame("realm-save.txt");
        } else if (globalShortcut && (hit.ch == 'd' || hit.ch == 'D')) {
            g.diagnostics = !g.diagnostics;
        } else if (globalShortcut && (hit.ch == 'l' || hit.ch == 'L')) {
            loadGame("realm-save.txt");
            updateViewMetrics(true);
        } else {
            handleInput(hit.ch);
        }
        return true;
    }
    return false;
}

static bool screenToMapWithTolerance(int px, int py, int& mx, int& my) {
    if (screenToMap(px, py, mx, my)) return true;
    int radius = std::max(10, std::min(28, s.tile));
    for (int r = 8; r <= radius; r += 8) {
        const int pts[8][2] = {{r,0},{-r,0},{0,r},{0,-r},{r,r},{r,-r},{-r,r},{-r,-r}};
        for (auto& p : pts) {
            if (screenToMap(px + p[0], py + p[1], mx, my)) return true;
        }
    }
    return false;
}

static void mobileInspectAt(int mx, int my) {
    g.cursorX = mx;
    g.cursorY = my;
    Entity* e = entityAt(mx, my);
    if (e && e->alive && g.map[my][mx].visible[0]) {
        std::ostringstream ss;
        ss << STATS[e->type].name << " HP " << e->hp << "/" << e->maxHp << " " << stateName(e->state);
        setStatus(ss.str());
    } else {
        setStatus(cursorTileSummary());
    }
}

static void mobileTapMap(int px, int py) {
    int mx = 0, my = 0;
    if (!screenToMapWithTolerance(px, py, mx, my)) return;
    g.cursorX = mx;
    g.cursorY = my;
    if (g.mode == M_PAUSED || g.mode == M_GAME_OVER) return;
    if (s.mobileBuildType != E_NONE) {
        Entity* builder = primaryOwnedSelection();
        if (!builder || builder->type != E_PEASANT) {
            setStatus("Select a peasant first.");
            s.mobileBuildType = E_NONE;
            return;
        }
        if (!canPlace(s.mobileBuildType, mx, my, 0)) {
            setStatus("Cannot build there.");
            return;
        }
        orderBuild(*builder, s.mobileBuildType, mx, my);
        s.mobileBuildType = E_NONE;
        g.mode = M_NORMAL;
        setStatus("Building placed.");
        return;
    }
    if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE) {
        handleInput('\n');
        return;
    }
    if (primaryOwnedSelection() && (isUnit(primaryOwnedSelection()->type) || !g.selectedIds.empty())) {
        rendererCommandAtTile(mx, my);
    } else {
        rendererSelectAtTile(mx, my);
    }
}

static void mobilePointerDown(int px, int py) {
    s.touchDown = true;
    s.touchPanning = false;
    s.touchOnMap = false;
    s.touchDownTicks = SDL_GetTicks();
    s.touchStartX = s.touchLastX = px;
    s.touchStartY = s.touchLastY = py;
    if (handleMobileHudHit(px, py)) return;
    if (pointInRect(px, py, mapRect())) {
        s.touchOnMap = true;
        startMiddlePan(px, py);
        s.touchPanning = false;
    }
}

static void mobilePointerMotion(int px, int py) {
    if (s.miniMapDown) {
        moveViewFromMiniMap(px, py, true);
        return;
    }
    if (!s.touchDown || !s.touchOnMap) return;
    int distPx = std::abs(px - s.touchStartX) + std::abs(py - s.touchStartY);
    if (distPx > 12) s.touchPanning = true;
    if (s.touchPanning) updateMiddlePan(px, py);
    s.touchLastX = px;
    s.touchLastY = py;
}

static void mobilePointerUp(int px, int py) {
    if (s.miniMapDown) {
        moveViewFromMiniMap(px, py, true);
        s.miniMapDown = false;
        s.touchDown = false;
        s.middleDown = false;
        moveCursorToViewCenter();
        return;
    }
    bool wasMap = s.touchOnMap;
    bool wasPan = s.touchPanning;
    Uint32 held = SDL_GetTicks() - s.touchDownTicks;
    s.touchDown = false;
    s.touchOnMap = false;
    s.touchPanning = false;
    if (s.middleDown) {
        if (wasPan) updateMiddlePan(px, py);
        s.middleDown = false;
        if (wasPan) moveCursorToViewCenter();
    }
    if (!wasMap || wasPan) return;
    int mx = 0, my = 0;
    if (!screenToMapWithTolerance(px, py, mx, my)) return;
    if (held >= 550) mobileInspectAt(mx, my);
    else mobileTapMap(px, py);
}

void gfxPollInput(bool& quitRequested) {
    quitRequested = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { quitRequested = true; return; }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            s.winW = e.window.data1; s.winH = e.window.data2; updateViewMetrics(true);
        }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && isMobileGui()) {
#if defined(REALM_WEB)
            continue;
#else
            if (g.mode == M_NORMAL) {
                g.mode = M_PAUSED;
                setStatus("Paused while in background.");
            }
#endif
        }
        if (isMobileGui() && (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP)) {
            int px = (int)std::lround(e.tfinger.x * s.winW);
            int py = (int)std::lround(e.tfinger.y * s.winH);
            s.suppressNextMouse = true;
            if (e.type == SDL_FINGERDOWN) mobilePointerDown(px, py);
            else if (e.type == SDL_FINGERMOTION) mobilePointerMotion(px, py);
            else mobilePointerUp(px, py);
            continue;
        }
        if (isMobileGui() && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONUP)) {
            if (s.suppressNextMouse) {
                if (e.type == SDL_MOUSEBUTTONUP) s.suppressNextMouse = false;
                continue;
            }
            if (e.type == SDL_MOUSEMOTION) { s.mouseX = e.motion.x; s.mouseY = e.motion.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) { s.mouseX = e.button.x; s.mouseY = e.button.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { mobilePointerDown(e.button.x, e.button.y); continue; }
            if (e.type == SDL_MOUSEMOTION && (s.touchDown || s.miniMapDown)) { mobilePointerMotion(e.motion.x, e.motion.y); continue; }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) { mobilePointerUp(e.button.x, e.button.y); continue; }
        }
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my; SDL_GetMouseState(&mx, &my);
            if (e.wheel.y > 0) setZoom(s.tile + 3, mx, my);
            if (e.wheel.y < 0) setZoom(s.tile - 3, mx, my);
        }
        if (e.type == SDL_MOUSEMOTION) {
            s.mouseX = e.motion.x;
            s.mouseY = e.motion.y;
            if (s.miniMapDown) {
                moveViewFromMiniMap(e.motion.x, e.motion.y, true);
                continue;
            }
            if (s.middleDown) {
                updateMiddlePan(e.motion.x, e.motion.y);
                continue;
            }
            int mx,my;
            if (screenToMap(e.motion.x, e.motion.y, mx, my)) {
                g.cursorX = mx; g.cursorY = my;
                if (s.leftDown) { s.lastMouseMapX = mx; s.lastMouseMapY = my; }
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            int mx,my;
            s.mouseX = e.button.x;
            s.mouseY = e.button.y;
            if (e.button.button == SDL_BUTTON_MIDDLE) {
                startMiddlePan(e.button.x, e.button.y);
            } else if (e.button.button == SDL_BUTTON_LEFT && handleKeyHitAt(e.button.x, e.button.y)) {
                s.leftDown = false;
                s.middleDown = false;
                g.dragging = false;
                continue;
            } else if (e.button.button == SDL_BUTTON_LEFT &&
                       moveViewFromMiniMap(e.button.x, e.button.y)) {
                s.miniMapDown = true;
                s.leftDown = false;
                s.middleDown = false;
                g.dragging = false;
            } else if (screenToMap(e.button.x, e.button.y, mx, my)) {
                g.cursorX = mx; g.cursorY = my;
                if (g.mode == M_TRAIN_SELECT) g.mode = M_NORMAL;
                if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE) {
                    handleInput('\n');
                } else if (e.button.button == SDL_BUTTON_LEFT) {
                    if (e.button.clicks >= 2) rendererSelectAllOfTypeInView(mx,my);
                    else {
                        s.leftDown = true;
                        s.dragStartX = mx;
                        s.dragStartY = my;
                        s.lastMouseMapX = mx;
                        s.lastMouseMapY = my;
                        g.dragging = true;
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    rendererCommandAtTile(mx,my);
                }
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP) {
            int mx,my;
            s.mouseX = e.button.x;
            s.mouseY = e.button.y;
            if (e.button.button == SDL_BUTTON_LEFT && s.miniMapDown) {
                moveViewFromMiniMap(e.button.x, e.button.y, true);
                s.miniMapDown = false;
                s.leftDown = false;
                g.dragging = false;
                continue;
            }
            if (e.button.button == SDL_BUTTON_MIDDLE) {
                if (s.middleDown) updateMiddlePan(e.button.x, e.button.y);
                moveCursorToViewCenter();
                s.middleDown = false;
                continue;
            }
            if (e.button.button == SDL_BUTTON_LEFT) {
                bool hasMap = screenToMap(e.button.x, e.button.y, mx, my);
                if (!hasMap && s.leftDown && inBounds(s.lastMouseMapX, s.lastMouseMapY)) {
                    mx = s.lastMouseMapX;
                    my = s.lastMouseMapY;
                    hasMap = true;
                }
                if (!hasMap) {
                    s.leftDown = false;
                    g.dragging = false;
                    continue;
                }
                g.cursorX = mx; g.cursorY = my;
                if (g.mode == M_TRAIN_SELECT) g.mode = M_NORMAL;
                if (s.leftDown) {
                    bool moved = (std::abs(mx - s.dragStartX) + std::abs(my - s.dragStartY)) > 1;
                    if (moved) rendererBoxSelect(s.dragStartX, s.dragStartY, mx, my);
                    else       rendererSelectAtTile(mx,my);
                }
                s.leftDown = false; g.dragging = false;
            }
        }
        if (e.type == SDL_KEYDOWN) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                toggleFullscreen();
                continue;
            }
            if (k == SDLK_x) {
#if defined(REALM_WEB)
                setStatus("Close the browser tab to exit.");
                continue;
#else
                quitRequested = true;
                return;
#endif
            }
            if (k >= SDLK_F5 && k <= SDLK_F8) {
                int slot = (int)(k - SDLK_F5) + 1;
                std::string path = "realm-slot" + std::to_string(slot) + ".sav";
                if (saveGame(path)) setStatus("Saved to slot " + std::to_string(slot) + ".");
                else setStatus("Save failed.");
                continue;
            }
            if (k >= SDLK_F9 && k <= SDLK_F12) {
                int slot = (int)(k - SDLK_F9) + 1;
                std::string path = "realm-slot" + std::to_string(slot) + ".sav";
                if (loadGame(path)) { setStatus("Loaded slot " + std::to_string(slot) + "."); updateViewMetrics(true); }
                else setStatus("Load failed.");
                continue;
            }
            if (devCaptureEnabled() && k == SDLK_y) { captureIssueBundle(); continue; }
            if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { setZoom(s.tile+3); continue; }
            if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { setZoom(s.tile-3); continue; }
            int ch = keyToInput(k);
            if (ch) handleInput(ch);
        }
    }
}

static void drawFrame(bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    s.keyHits.clear();
    SDL_GetMouseState(&s.mouseX, &s.mouseY);
    if (displayMode == DM_ASCII && isMobileGui()) {
        drawAsciiMobileFrame(present);
        return;
    }
    if (displayMode == DM_ASCII && !isMobileGui()) {
        drawAsciiTerminalFrame(present);
        return;
    }
    if (displayMode == DM_EMOJI) s.isometric = true;
    setDraw(rgb(3,5,8)); SDL_RenderClear(s.ren);
    if (!isMobileGui()) drawTopBar();
    drawMap();
    drawPanel();
    if (!isMobileGui()) drawBottom();
    drawHelpOverlay();
    if (present) SDL_RenderPresent(s.ren);
}

void gfxRender() {
    drawFrame(true);
}

void gfxDelay(int ms) {
    SDL_Delay((Uint32)std::max(0, ms));
}

void gfxSetProjection(bool isometric) {
    s.isometric = (displayMode == DM_EMOJI) ? true : isometric;
    updateViewMetrics(true);
}

void gfxSetZoomForTest(int tilePx) {
    setZoom(tilePx);
}

void gfxSetZoomAnchoredForTest(int tilePx, int anchorX, int anchorY) {
    setZoom(tilePx, anchorX, anchorY);
}

bool gfxMapTileAtScreenForTest(int px, int py, int& mx, int& my) {
    return screenToMap(px, py, mx, my);
}

bool gfxScreenCenterForMapTileForTest(int mx, int my, int& px, int& py) {
    return mapTileScreenCenter(mx, my, px, py);
}

void gfxSetWindowSizeForTest(int width, int height) {
    width = std::max(640, width);
    height = std::max(480, height);
    SDL_SetWindowSize(s.win, width, height);
    SDL_SetWindowPosition(s.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    updateViewMetrics(true);
}

bool gfxSaveScreenshot(const std::string& path) {
    drawFrame(false);
    return saveRendererPixels(path);
}

bool gfxSaveAsciiTerminalReference(const std::string& path) {
    drawAsciiTerminalFrame(false);
    return saveRendererPixels(path);
}

bool gfxSaveAsciiTerminalText(const std::string& path) {
    TerminalFrame frame = buildAsciiTerminalFrame();
    std::ofstream out(path);
    if (!out) return false;
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) out << frame.at(x, y).ch;
        out << '\n';
    }
    return true;
}

bool gfxSaveSplashScreenshot(const std::string& path, int numAIs, int biomeIdx) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    drawSplash(numAIs, biomeIdx);
    return saveRendererPixels(path);
}

namespace {

constexpr int LAB_X = MAP_W / 2;
constexpr int LAB_Y = MAP_H / 2;

struct LabState {
    int previewMode = 2; // 0 tile, 1 entity, 2 combined
    int terrain = T_GRASS;
    int biome = B_TEMPERATE;
    int resources = 0;
    int fog = 0; // 0 visible, 1 explored, 2 unexplored
    int season = SPRING;
    int seasonPercent = 0;
    int timeStep = 3;
    int weather = W_CLEAR;
    int lightMode = 0; // 0 none, 1 town hall, 2 tower, 3 custom candle
    int entityType = E_NONE;
    int owner = 0;
    int actionIndex = 0;
    int direction = 0; // 0 front, 1 back
    int frame = 0;
    int hue = 197;
    int speedPercent = 100;
    bool playing = true;
    bool damaged = false;
    int activeDropdown = 0;
    int dropdownScroll = 0;
    bool hueDragging = false;
};

static void clampLabState(LabState& lab);
static bool saveLabShot(const LabState& lab, const std::filesystem::path& path);

enum LabDropdownKind {
    LAB_DD_NONE = 0,
    LAB_DD_PREVIEW,
    LAB_DD_TERRAIN,
    LAB_DD_BIOME,
    LAB_DD_SEASON,
    LAB_DD_SEASON_PERCENT,
    LAB_DD_TIME,
    LAB_DD_WEATHER,
    LAB_DD_FOG,
    LAB_DD_LIGHT,
    LAB_DD_RESOURCE,
    LAB_DD_ENTITY,
    LAB_DD_ACTION,
    LAB_DD_DIRECTION,
    LAB_DD_FRAME,
    LAB_DD_OWNER,
    LAB_DD_SPEED
};

enum LabButtonKind {
    LAB_BTN_PLAY = 1,
    LAB_BTN_DAMAGED,
    LAB_BTN_SCREENSHOT
};

struct LabDropdownOption {
    std::string label;
    int value = 0;
};

struct LabDropdownControl {
    SDL_Rect r{};
    int kind = LAB_DD_NONE;
    std::string label;
    std::string value;
    bool disabled = false;
};

struct LabButtonControl {
    SDL_Rect r{};
    int kind = 0;
    std::string label;
    bool active = false;
};

struct LabControlLayout {
    std::vector<LabDropdownControl> dropdowns;
    std::vector<LabButtonControl> buttons;
    SDL_Rect hueWheel{0,0,0,0};
};

static std::vector<EntityType> labEntityTypes() {
    std::vector<EntityType> out;
    out.push_back(E_NONE);
    for (int t = E_PEASANT; t <= E_BOAR; ++t) out.push_back((EntityType)t);
    return out;
}

static SDL_Color hueColor(int hue) {
    float h = std::fmod((float)((hue % 360) + 360), 360.0f) / 60.0f;
    float c = 1.0f;
    float x = c * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 1.0f) { r = c; g = x; }
    else if (h < 2.0f) { r = x; g = c; }
    else if (h < 3.0f) { g = c; b = x; }
    else if (h < 4.0f) { g = x; b = c; }
    else if (h < 5.0f) { r = x; b = c; }
    else { r = c; b = x; }
    return SDL_Color{(Uint8)std::lround(r * 235.0f), (Uint8)std::lround(g * 235.0f),
                     (Uint8)std::lround(b * 235.0f), 255};
}

static Color colorFromSdl(SDL_Color c, int alpha = 255) {
    return rgb(c.r, c.g, c.b, alpha);
}

static const char* directionId(int direction) {
    return direction == 1 ? "back" : "front";
}

static const char* seasonNameValue(int season) {
    static const char* names[] = {"Spring", "Summer", "Autumn", "Winter"};
    return names[((season % 4) + 4) % 4];
}

static const char* timeStepName(int step) {
    static const char* names[] = {"Night", "Dawn", "Morning", "Noon", "Dusk", "Late dusk"};
    return names[std::max(0, std::min(step, 5))];
}

static std::string labEntityName(EntityType type) {
    if (type == E_NONE) return "None";
    return STATS[type].name;
}

static const EntityActionAnimationSpec* labActionSpec(const LabState& lab) {
    EntityType type = (EntityType)lab.entityType;
    int count = entityActionAnimationSpecCount(type);
    if (count <= 0) return nullptr;
    int index = std::max(0, std::min(lab.actionIndex, count - 1));
    return entityActionAnimationSpecAt(type, index);
}

static const char* labActionId(const LabState& lab) {
    if (const EntityActionAnimationSpec* spec = labActionSpec(lab)) return spec->action;
    return "idle";
}

static int labFrameCount(const LabState& lab) {
    if (const EntityActionAnimationSpec* spec = labActionSpec(lab)) return std::max(1, spec->frameCount);
    return 1;
}

static std::filesystem::path labManualShotPath(const LabState& lab) {
    namespace fs = std::filesystem;
    fs::path outDir = fs::path("build") / "lab-screenshots";
    fs::create_directories(outDir);
    std::string entity = lowerSlug(labEntityName((EntityType)lab.entityType));
    if (entity.empty()) entity = "none";
    std::ostringstream name;
    name << "manual-" << entity << "-" << labActionId(lab) << "-" << directionId(lab.direction)
         << "-frame_" << std::setw(2) << std::setfill('0') << lab.frame << ".bmp";
    return outDir / name.str();
}

static const char* terrainNameSafe(int terrain) {
    return terrainName((Terrain)std::max(0, std::min(terrain, (int)T_CASTLE_GATE)));
}

static const char* biomeNameSafe(int biome) {
    return biomeName((Biome)std::max(0, std::min(biome, (int)B_OCEAN)));
}

static const char* weatherNameSafe(int weather) {
    switch ((Weather)weather) {
        case W_RAIN: return "Rain";
        case W_STORM: return "Storm";
        case W_SNOW: return "Snow";
        default: return "Clear";
    }
}

static const char* previewModeName(int mode) {
    switch (mode) {
        case 0: return "Tile only";
        case 1: return "Entity only";
        default: return "Tile + Entity";
    }
}

static const char* fogName(int fog) {
    switch (fog) {
        case 1: return "Explored";
        case 2: return "Unexplored";
        default: return "Visible";
    }
}

static const char* lightName(int mode) {
    switch (mode) {
        case 1: return "Town Hall torch";
        case 2: return "Tower torch";
        case 3: return "Custom candle";
        default: return "None";
    }
}

static std::vector<LabDropdownOption> labDropdownOptions(int kind, const LabState& lab) {
    std::vector<LabDropdownOption> out;
    switch (kind) {
        case LAB_DD_PREVIEW:
            out.push_back({"Tile only", 0});
            out.push_back({"Entity only", 1});
            out.push_back({"Tile + Entity", 2});
            break;
        case LAB_DD_TERRAIN:
            for (int t = 0; t <= (int)T_CASTLE_GATE; ++t) out.push_back({terrainNameSafe(t), t});
            break;
        case LAB_DD_BIOME:
            for (int b = 0; b <= (int)B_OCEAN; ++b) out.push_back({biomeNameSafe(b), b});
            break;
        case LAB_DD_SEASON:
            for (int sidx = 0; sidx < 4; ++sidx) out.push_back({seasonNameValue(sidx), sidx});
            break;
        case LAB_DD_SEASON_PERCENT:
            for (int p = 0; p <= 90; p += 10) out.push_back({std::to_string(p) + "%", p});
            out.push_back({"99%", 99});
            break;
        case LAB_DD_TIME:
            for (int t = 0; t < 6; ++t) out.push_back({timeStepName(t), t});
            break;
        case LAB_DD_WEATHER:
            for (int w = 0; w < 4; ++w) out.push_back({weatherNameSafe(w), w});
            break;
        case LAB_DD_FOG:
            for (int f = 0; f < 3; ++f) out.push_back({fogName(f), f});
            break;
        case LAB_DD_LIGHT:
            for (int l = 0; l < 4; ++l) out.push_back({lightName(l), l});
            break;
        case LAB_DD_RESOURCE:
            for (int r : {0, 25, 50, 100, 150, 200, 500, 999}) out.push_back({std::to_string(r), r});
            break;
        case LAB_DD_ENTITY:
            for (EntityType type : labEntityTypes()) out.push_back({labEntityName(type), (int)type});
            break;
        case LAB_DD_ACTION: {
            int count = entityActionAnimationSpecCount((EntityType)lab.entityType);
            for (int i = 0; i < count; ++i) {
                if (const EntityActionAnimationSpec* spec = entityActionAnimationSpecAt((EntityType)lab.entityType, i)) {
                    out.push_back({spec->action, i});
                }
            }
            break;
        }
        case LAB_DD_DIRECTION:
            out.push_back({"front", 0});
            out.push_back({"back", 1});
            break;
        case LAB_DD_FRAME:
            for (int f = 0; f < labFrameCount(lab); ++f) out.push_back({"frame " + std::to_string(f), f});
            break;
        case LAB_DD_OWNER:
            for (int o = 0; o < MAX_PLAYERS; ++o) out.push_back({"player " + std::to_string(o), o});
            break;
        case LAB_DD_SPEED:
            for (int sPct : {10, 25, 50, 75, 100, 150, 200, 300, 400}) {
                out.push_back({std::to_string(sPct) + "%", sPct});
            }
            break;
        default:
            break;
    }
    return out;
}

static float labTimePhase(int step) {
    static const float phases[] = {0.02f, 0.16f, 0.28f, 0.50f, 0.72f, 0.84f};
    return phases[std::max(0, std::min(step, 5))];
}

static void labConfigureEntityForAction(Entity& e, const char* action) {
    e.state = S_IDLE;
    e.targetX = LAB_X;
    e.targetY = LAB_Y + 1;
    e.resourceX = LAB_X;
    e.resourceY = LAB_Y + 1;
    e.path.clear();
    e.pathIdx = 0;
    e.cargo = {CR_NONE, 0, -1, -1};
    if (std::strcmp(action, "walk") == 0) {
        e.state = S_MOVING;
        e.path.push_back({LAB_X, LAB_Y + 1});
    } else if (std::strncmp(action, "carry_", 6) == 0) {
        e.state = S_RETURNING;
        e.path.push_back({LAB_X, LAB_Y + 1});
        e.cargo.amount = 10;
        e.cargo.type = std::strstr(action, "gold") ? CR_GOLD
            : std::strstr(action, "wood") ? CR_WOOD : CR_FOOD;
        e.cargo.sourceX = LAB_X;
        e.cargo.sourceY = LAB_Y + 1;
    } else if (std::strcmp(action, "build") == 0 || std::strcmp(action, "hoe_soil") == 0) {
        e.state = S_BUILDING;
        e.targetY = LAB_Y - 1;
    } else if (std::strncmp(action, "gather_", 7) == 0 || std::strcmp(action, "chop_wood") == 0
               || std::strcmp(action, "mine_gold") == 0) {
        e.state = S_GATHERING;
        e.targetY = LAB_Y - 1;
        e.resourceY = LAB_Y - 1;
        if (std::strcmp(action, "mine_gold") == 0) g.map[LAB_Y - 1][LAB_X].terrain = T_GOLD;
        else if (std::strcmp(action, "chop_wood") == 0) g.map[LAB_Y - 1][LAB_X].terrain = T_FOREST;
        else if (std::strcmp(action, "gather_wheat") == 0) g.map[LAB_Y - 1][LAB_X].terrain = T_WHEAT;
        else if (std::strcmp(action, "gather_berries") == 0) g.map[LAB_Y - 1][LAB_X].terrain = T_BERRY;
    } else if (std::strcmp(action, "club_attack") == 0) {
        e.state = S_ATTACKING;
        e.targetY = LAB_Y - 1;
    } else if (std::strcmp(action, "death") == 0) {
        e.state = S_DEAD;
    }
}

static void labApplyWorld(const LabState& lab) {
    labForcesImageTileset = true;
    displayMode = DM_EMOJI;
    s.isometric = true;
    g.tick = lab.playing ? g.tick : g.tick;
    g.dayPhase = labTimePhase(lab.timeStep);
    g.seasonPhase = (float)lab.season + lab.seasonPercent / 100.0f;
    g.weather = lab.weather;
    g.weatherTimer = 999;

    for (int y = LAB_Y - 4; y <= LAB_Y + 4; ++y) {
        for (int x = LAB_X - 4; x <= LAB_X + 4; ++x) {
            if (!inBounds(x, y)) continue;
            Tile& t = g.map[y][x];
            t.terrain = (Terrain)lab.terrain;
            t.biome = (Biome)lab.biome;
            t.resources = (x == LAB_X && y == LAB_Y) ? lab.resources : 0;
            t.preWinterTerrain = t.terrain;
            t.wear = 0;
            bool explored = lab.fog != 2;
            bool visible = lab.fog == 0;
            for (int p = 0; p < MAX_PLAYERS; ++p) {
                t.explored[p] = explored;
                t.visible[p] = visible;
            }
        }
    }

    g.entities.clear();
    g.nextId = 1;
    labLightOverride.enabled = false;
    if (lab.lightMode == 1 || lab.lightMode == 2) {
        Entity light{};
        light.id = g.nextId++;
        light.type = lab.lightMode == 1 ? E_TOWNHALL : E_TOWER;
        light.owner = 0;
        light.x = LAB_X - 3;
        light.y = LAB_Y;
        light.hp = light.maxHp = STATS[light.type].maxHp;
        light.alive = true;
        light.state = S_IDLE;
        light.targetId = -1;
        g.entities.push_back(light);
    } else if (lab.lightMode == 3) {
        labLightOverride.enabled = true;
        labLightOverride.x = LAB_X - 1;
        labLightOverride.y = LAB_Y;
        labLightOverride.strength = 0.52f;
        labLightOverride.radius = 4.0f;
    }

    if (lab.previewMode != 0 && lab.entityType != E_NONE) {
        Entity e{};
        e.id = g.nextId++;
        e.type = (EntityType)lab.entityType;
        e.owner = isWildAnimal(e.type) ? OWNER_NATURE : lab.owner;
        e.x = LAB_X;
        e.y = LAB_Y;
        e.hp = e.maxHp = STATS[e.type].maxHp;
        if (lab.damaged) e.hp = std::max(1, e.maxHp / 2);
        e.alive = true;
        e.targetId = -1;
        e.producing = E_NONE;
        e.rallyX = e.rallyY = -1;
        labConfigureEntityForAction(e, labActionId(lab));
        g.entities.push_back(e);
    }
}

static void drawHueWheel(int cx, int cy, int radius, int hue) {
    for (int y = -radius; y <= radius; ++y) {
        for (int x = -radius; x <= radius; ++x) {
            float d = std::sqrt((float)(x * x + y * y));
            if (d > radius || d < radius * 0.58f) continue;
            float angle = std::atan2((float)y, (float)x) * 180.0f / 3.14159265f;
            SDL_Color c = hueColor((int)std::lround(angle + 360.0f));
            setDraw(rgb(c.r, c.g, c.b, 255));
            SDL_RenderDrawPoint(s.ren, cx + x, cy + y);
        }
    }
    float a = hue * 3.14159265f / 180.0f;
    int mx = cx + (int)std::lround(std::cos(a) * radius * 0.8f);
    int my = cy + (int)std::lround(std::sin(a) * radius * 0.8f);
    setDraw(rgb(255,255,255));
    SDL_Rect mark{mx - 3, my - 3, 6, 6};
    SDL_RenderDrawRect(s.ren, &mark);
}

static void drawLabLine(int x, int& y, const std::string& text, Color c = rgb(220,225,220)) {
    drawTextFit(x, y, text, c, 330, s.monoSmall ? s.monoSmall : s.mono);
    y += 19;
}

static LabControlLayout labBuildControls(const LabState& lab) {
    LabControlLayout layout;
    const int x = 18;
    const int w = 324;
    const int h = 30;
    int y = 54;

    auto addDropdown = [&](int kind, const std::string& label, const std::string& value, bool disabled = false) {
        layout.dropdowns.push_back({SDL_Rect{x, y, w, h}, kind, label, value, disabled});
        y += 36;
    };
    auto addButton = [&](int kind, const std::string& label, bool active = false) {
        layout.buttons.push_back({SDL_Rect{x, y, w, h}, kind, label, active});
        y += 36;
    };

    addDropdown(LAB_DD_PREVIEW, "Preview", previewModeName(lab.previewMode));
    y += 10;
    addDropdown(LAB_DD_TERRAIN, "Terrain/decor", terrainNameSafe(lab.terrain));
    addDropdown(LAB_DD_BIOME, "Biome", biomeNameSafe(lab.biome));
    addDropdown(LAB_DD_SEASON, "Season", seasonNameValue(lab.season));
    addDropdown(LAB_DD_SEASON_PERCENT, "Season progress", std::to_string(lab.seasonPercent) + "%");
    addDropdown(LAB_DD_TIME, "Time", timeStepName(lab.timeStep));
    addDropdown(LAB_DD_WEATHER, "Weather", weatherNameSafe(lab.weather));
    addDropdown(LAB_DD_FOG, "Fog", fogName(lab.fog));
    addDropdown(LAB_DD_LIGHT, "Light", lightName(lab.lightMode));
    addDropdown(LAB_DD_RESOURCE, "Resource amount", std::to_string(lab.resources));

    y += 10;
    addDropdown(LAB_DD_ENTITY, "Entity", labEntityName((EntityType)lab.entityType));
    addDropdown(LAB_DD_ACTION, "Action", labActionSpec(lab) ? labActionId(lab) : "No authored action",
                entityActionAnimationSpecCount((EntityType)lab.entityType) <= 0);
    addDropdown(LAB_DD_DIRECTION, "Direction", directionId(lab.direction), lab.entityType == E_NONE);
    addDropdown(LAB_DD_FRAME, "Frame", std::to_string(lab.frame), lab.entityType == E_NONE);
    addDropdown(LAB_DD_OWNER, "Owner", "player " + std::to_string(lab.owner), lab.entityType == E_NONE);
    addDropdown(LAB_DD_SPEED, "Animation speed", std::to_string(lab.speedPercent) + "%", lab.entityType == E_NONE);
    addButton(LAB_BTN_PLAY, lab.playing ? "Pause animation" : "Play animation", lab.playing);
    addButton(LAB_BTN_DAMAGED, lab.damaged ? "Damaged: yes" : "Damaged: no", lab.damaged);
    addButton(LAB_BTN_SCREENSHOT, "Save screenshot");

    layout.hueWheel = SDL_Rect{x + 226, y + 8, 84, 84};
    return layout;
}

static const LabDropdownControl* labFindControl(const LabControlLayout& layout, int kind) {
    for (const LabDropdownControl& control : layout.dropdowns) {
        if (control.kind == kind) return &control;
    }
    return nullptr;
}

static int labDropdownSelectedValue(int kind, const LabState& lab) {
    switch (kind) {
        case LAB_DD_PREVIEW: return lab.previewMode;
        case LAB_DD_TERRAIN: return lab.terrain;
        case LAB_DD_BIOME: return lab.biome;
        case LAB_DD_SEASON: return lab.season;
        case LAB_DD_SEASON_PERCENT: return lab.seasonPercent;
        case LAB_DD_TIME: return lab.timeStep;
        case LAB_DD_WEATHER: return lab.weather;
        case LAB_DD_FOG: return lab.fog;
        case LAB_DD_LIGHT: return lab.lightMode;
        case LAB_DD_RESOURCE: return lab.resources;
        case LAB_DD_ENTITY: return lab.entityType;
        case LAB_DD_ACTION: return lab.actionIndex;
        case LAB_DD_DIRECTION: return lab.direction;
        case LAB_DD_FRAME: return lab.frame;
        case LAB_DD_OWNER: return lab.owner;
        case LAB_DD_SPEED: return lab.speedPercent;
        default: return 0;
    }
}

static void labSetDropdownValue(LabState& lab, int kind, int value) {
    switch (kind) {
        case LAB_DD_PREVIEW: lab.previewMode = value; break;
        case LAB_DD_TERRAIN: lab.terrain = value; break;
        case LAB_DD_BIOME: lab.biome = value; break;
        case LAB_DD_SEASON: lab.season = value; break;
        case LAB_DD_SEASON_PERCENT: lab.seasonPercent = value; break;
        case LAB_DD_TIME: lab.timeStep = value; break;
        case LAB_DD_WEATHER: lab.weather = value; break;
        case LAB_DD_FOG: lab.fog = value; break;
        case LAB_DD_LIGHT: lab.lightMode = value; break;
        case LAB_DD_RESOURCE: lab.resources = value; break;
        case LAB_DD_ENTITY:
            lab.entityType = value;
            lab.actionIndex = 0;
            lab.frame = 0;
            break;
        case LAB_DD_ACTION:
            lab.actionIndex = value;
            lab.frame = 0;
            break;
        case LAB_DD_DIRECTION: lab.direction = value; break;
        case LAB_DD_FRAME: lab.frame = value; break;
        case LAB_DD_OWNER: lab.owner = value; break;
        case LAB_DD_SPEED: lab.speedPercent = value; break;
        default: break;
    }
}

static void labOpenDropdown(LabState& lab, int kind) {
    lab.activeDropdown = kind;
    std::vector<LabDropdownOption> options = labDropdownOptions(kind, lab);
    int selected = labDropdownSelectedValue(kind, lab);
    int selectedIndex = 0;
    for (int i = 0; i < (int)options.size(); ++i) {
        if (options[i].value == selected) {
            selectedIndex = i;
            break;
        }
    }
    lab.dropdownScroll = std::max(0, selectedIndex - 4);
}

static void labSetHueFromPoint(LabState& lab, int mx, int my) {
    SDL_Rect r = labBuildControls(lab).hueWheel;
    int cx = r.x + r.w / 2;
    int cy = r.y + r.h / 2;
    float angle = std::atan2((float)(my - cy), (float)(mx - cx)) * 180.0f / 3.14159265f;
    lab.hue = ((int)std::lround(angle) + 360) % 360;
}

static void clampLabState(LabState& lab);

static SDL_Rect labDropdownPopupRect(const LabState& lab, const LabControlLayout& layout, int& optionH, int& visibleCount) {
    optionH = 24;
    visibleCount = 0;
    const LabDropdownControl* control = labFindControl(layout, lab.activeDropdown);
    if (!control) return SDL_Rect{0,0,0,0};
    int count = (int)labDropdownOptions(lab.activeDropdown, lab).size();
    visibleCount = std::min(count, std::max(3, std::min(12, (s.winH - control->r.y - control->r.h - 18) / optionH)));
    int h = std::max(optionH, visibleCount * optionH + 2);
    return SDL_Rect{control->r.x, control->r.y + control->r.h + 2, control->r.w, h};
}

static void drawLabDropdownControl(const LabDropdownControl& control, bool open) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    Color bg = control.disabled ? rgb(18,20,24,210) : open ? rgb(42,72,94,240)
        : rectHovered(control.r) ? rgb(28,38,48,240) : rgb(16,22,30,235);
    Color bd = open ? rgb(155,220,245) : control.disabled ? rgb(70,76,84) : rgb(86,102,116);
    setDraw(bg);
    SDL_RenderFillRect(s.ren, &control.r);
    setDraw(bd);
    SDL_RenderDrawRect(s.ren, &control.r);
    drawTextFit(control.r.x + 10, control.r.y + 6, control.label, control.disabled ? rgb(105,112,120) : rgb(166,178,186),
                128, s.monoSmall ? s.monoSmall : s.mono);
    drawTextFit(control.r.x + 145, control.r.y + 6, control.value, control.disabled ? rgb(105,112,120) : rgb(232,238,230),
                control.r.w - 176, s.monoSmall ? s.monoSmall : s.mono);
    drawTextFit(control.r.x + control.r.w - 22, control.r.y + 6, "v", control.disabled ? rgb(80,86,94) : rgb(255,230,135),
                18, s.monoSmall ? s.monoSmall : s.mono);
}

static void drawLabButtonControl(const LabButtonControl& button) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    Color bg = button.active ? rgb(60,83,56,235) : rectHovered(button.r) ? rgb(34,42,48,235) : rgb(18,24,30,235);
    Color bd = button.active ? rgb(150,222,135) : rgb(86,102,116);
    setDraw(bg);
    SDL_RenderFillRect(s.ren, &button.r);
    setDraw(bd);
    SDL_RenderDrawRect(s.ren, &button.r);
    drawTextFit(button.r.x + 10, button.r.y + 6, button.label, rgb(232,238,230), button.r.w - 20,
                s.monoSmall ? s.monoSmall : s.mono);
}

static void drawLabDropdownPopup(const LabState& lab) {
    if (lab.activeDropdown == LAB_DD_NONE) return;
    LabControlLayout layout = labBuildControls(lab);
    const LabDropdownControl* control = labFindControl(layout, lab.activeDropdown);
    if (!control) return;
    std::vector<LabDropdownOption> options = labDropdownOptions(lab.activeDropdown, lab);
    if (options.empty()) return;
    int optionH = 24;
    int visibleCount = 0;
    SDL_Rect popup = labDropdownPopupRect(lab, layout, optionH, visibleCount);
    int maxScroll = std::max(0, (int)options.size() - visibleCount);
    int scroll = std::max(0, std::min(lab.dropdownScroll, maxScroll));

    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(rgb(4,7,10,255));
    SDL_RenderFillRect(s.ren, &popup);
    setDraw(rgb(155,220,245));
    SDL_RenderDrawRect(s.ren, &popup);

    int selected = labDropdownSelectedValue(lab.activeDropdown, lab);
    for (int i = 0; i < visibleCount; ++i) {
        int optionIndex = scroll + i;
        if (optionIndex >= (int)options.size()) break;
        SDL_Rect row{popup.x + 1, popup.y + 1 + i * optionH, popup.w - 2, optionH};
        bool isSelected = options[optionIndex].value == selected;
        bool hovered = rectHovered(row);
        if (isSelected || hovered) {
            setDraw(isSelected ? rgb(60,89,70,230) : rgb(26,38,48,230));
            SDL_RenderFillRect(s.ren, &row);
        }
        drawTextFit(row.x + 8, row.y + 4, options[optionIndex].label,
                    isSelected ? rgb(255,235,145) : rgb(226,232,226), row.w - 16,
                    s.monoSmall ? s.monoSmall : s.mono);
    }
    if ((int)options.size() > visibleCount) {
        std::string page = std::to_string(scroll + 1) + "-" + std::to_string(std::min((int)options.size(), scroll + visibleCount))
            + "/" + std::to_string(options.size());
        drawTextFit(popup.x + popup.w - 72, popup.y + popup.h - 18, page, rgb(150,165,174), 68,
                    s.monoSmall ? s.monoSmall : s.mono);
    }
}

static bool labHandleMouseDown(LabState& lab, int mx, int my) {
    LabControlLayout layout = labBuildControls(lab);

    if (lab.activeDropdown != LAB_DD_NONE) {
        int optionH = 24;
        int visibleCount = 0;
        SDL_Rect popup = labDropdownPopupRect(lab, layout, optionH, visibleCount);
        std::vector<LabDropdownOption> options = labDropdownOptions(lab.activeDropdown, lab);
        int maxScroll = std::max(0, (int)options.size() - visibleCount);
        lab.dropdownScroll = std::max(0, std::min(lab.dropdownScroll, maxScroll));
        if (pointInRect(mx, my, popup)) {
            int row = (my - popup.y - 1) / optionH;
            int optionIndex = lab.dropdownScroll + row;
            if (row >= 0 && row < visibleCount && optionIndex >= 0 && optionIndex < (int)options.size()) {
                labSetDropdownValue(lab, lab.activeDropdown, options[optionIndex].value);
                lab.activeDropdown = LAB_DD_NONE;
                clampLabState(lab);
            }
            return true;
        }
        lab.activeDropdown = LAB_DD_NONE;
    }

    for (const LabDropdownControl& control : layout.dropdowns) {
        if (!control.disabled && pointInRect(mx, my, control.r)) {
            labOpenDropdown(lab, control.kind);
            return true;
        }
    }
    for (const LabButtonControl& button : layout.buttons) {
        if (pointInRect(mx, my, button.r)) {
            if (button.kind == LAB_BTN_PLAY) lab.playing = !lab.playing;
            else if (button.kind == LAB_BTN_DAMAGED) lab.damaged = !lab.damaged;
            else if (button.kind == LAB_BTN_SCREENSHOT) {
                std::filesystem::path path = labManualShotPath(lab);
                bool ok = saveLabShot(lab, path);
                std::cerr << "realm: lab manual screenshot " << (ok ? "ok " : "failed ")
                          << path.string() << "\n";
            }
            return true;
        }
    }
    int cx = layout.hueWheel.x + layout.hueWheel.w / 2;
    int cy = layout.hueWheel.y + layout.hueWheel.h / 2;
    float dx = (float)(mx - cx);
    float dy = (float)(my - cy);
    float d = std::sqrt(dx * dx + dy * dy);
    if (d <= layout.hueWheel.w / 2.0f && d >= layout.hueWheel.w * 0.20f) {
        lab.hueDragging = true;
        labSetHueFromPoint(lab, mx, my);
        return true;
    }
    return false;
}

static void drawLabPreview(const LabState& lab, SDL_Rect area, TilesetAssetFrame& assetFrame) {
    setDraw(rgb(7,9,12));
    SDL_RenderFillRect(s.ren, &area);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(rgb(70,82,94));
    SDL_RenderDrawRect(s.ren, &area);

    int cx = area.x + area.w / 2;
    int cy = area.y + area.h / 2 + 18;
    int hw = std::max(58, std::min(112, area.w / 6));
    int hh = hw / 2;
    const Tile& tile = g.map[LAB_Y][LAB_X];

    if (lab.previewMode != 1) {
        Color bg = applyVisionAndLight(terrainBg(tile, LAB_X, LAB_Y), LAB_X, LAB_Y);
        fillDiamond(cx, cy, hw, hh, bg);
        applyTerrainTextureIso(cx, cy, hw, hh, tile, LAB_X, LAB_Y);
        drawDiamondOutline(cx, cy, hw, hh, rgb(245,235,150,210));
    } else {
        SDL_Rect empty{cx - 72, cy - 72, 144, 144};
        setDraw(rgb(20,24,30,210));
        SDL_RenderFillRect(s.ren, &empty);
        setDraw(rgb(110,120,132,180));
        SDL_RenderDrawRect(s.ren, &empty);
    }

    Entity* ent = entityAt(LAB_X, LAB_Y);
    if (lab.previewMode != 0 && ent) {
        int spriteSize = 128;
        SDL_Rect dst{cx - spriteSize / 2, cy - spriteSize / 2 - 18, spriteSize, spriteSize};
        SDL_Color team = hueColor(lab.hue);
        Color mod = applyVisionToGlyph(rgb(255,255,255), LAB_X, LAB_Y);
        if (!drawEntityImageTile(*ent, dst, mod, labActionId(lab), directionId(lab.direction),
                                 lab.frame, team, &assetFrame)) {
            bool usesSymbolFont = false;
            drawCentered(tilesetEntityVisual(*ent, usesSymbolFont), dst, rgb(255,255,255),
                         usesSymbolFont, usesSymbolFont);
        }
    } else if (lab.previewMode == 1) {
        drawTextFit(cx - 82, cy - 8, "No entity selected", rgb(150,160,168), 164);
    }
}

static void drawLabFrame(const LabState& lab, bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    setDraw(rgb(3,5,8));
    SDL_RenderClear(s.ren);
    labApplyWorld(lab);

    int leftW = 360;
    int rightW = 420;
    SDL_Rect preview{leftW + 12, 56, std::max(260, s.winW - leftW - rightW - 24), std::max(300, s.winH - 112)};
    TilesetAssetFrame assetFrame;
    drawLabPreview(lab, preview, assetFrame);

    drawText(18, 14, "Realm Tileset Lab", rgb(255,235,145));
    drawTextFit(leftW + 18, 18, previewModeName(lab.previewMode), rgb(180,205,230), preview.w);
    drawTextFit(preview.x, preview.y + preview.h + 14,
                "Click dropdowns to edit; drag the hue wheel. Keys still work: Esc quit, Space play/pause, Q/E terrain, U/I entity.",
                rgb(180,188,196), preview.w);

    LabControlLayout layout = labBuildControls(lab);
    drawText(18, 36, "Tile", rgb(255,230,135));
    drawText(18, 418, "Entity", rgb(255,230,135));
    for (const LabDropdownControl& control : layout.dropdowns) {
        drawLabDropdownControl(control, lab.activeDropdown == control.kind);
    }
    for (const LabButtonControl& button : layout.buttons) {
        drawLabButtonControl(button);
    }

    SDL_Color team = hueColor(lab.hue);
    SDL_Rect swatch{layout.hueWheel.x - 118, layout.hueWheel.y + 20, 92, 28};
    setDraw(colorFromSdl(team));
    SDL_RenderFillRect(s.ren, &swatch);
    setDraw(rgb(255,255,255,180));
    SDL_RenderDrawRect(s.ren, &swatch);
    drawTextFit(swatch.x, swatch.y - 20, "Team colour", rgb(166,178,186), 120, s.monoSmall ? s.monoSmall : s.mono);
    drawTextFit(swatch.x, swatch.y + 34, "hue " + std::to_string(lab.hue), rgb(232,238,230), 120,
                s.monoSmall ? s.monoSmall : s.mono);
    drawHueWheel(layout.hueWheel.x + layout.hueWheel.w / 2, layout.hueWheel.y + layout.hueWheel.h / 2,
                 layout.hueWheel.w / 2, lab.hue);

    int rx = s.winW - rightW + 18;
    int ry = 54;
    drawLabLine(rx, ry, "Animation Info", rgb(255,230,135));
    if (const EntityActionAnimationSpec* spec = labActionSpec(lab)) {
        drawLabLine(rx, ry, "id: " + std::string(spec->action));
        drawLabLine(rx, ry, "family: " + std::string(spec->family));
        drawLabLine(rx, ry, "relation: " + std::string(actionTargetRelationId(spec->targetRelation)));
        drawLabLine(rx, ry, "range: " + std::to_string(spec->rangeTiles)
            + " loop: " + (spec->loop ? "true" : "false")
            + " hold: " + (spec->holdLast ? "true" : "false"));
        drawLabLine(rx, ry, "transition: " + std::to_string(spec->transitionAfterMs) + "ms");
        drawLabLine(rx, ry, "fit: " + std::string(spec->fitProfile));
        if (spec->tool && *spec->tool) drawLabLine(rx, ry, "tool: " + std::string(spec->tool));
        if (spec->carriedObject && *spec->carriedObject) drawLabLine(rx, ry, "carry: " + std::string(spec->carriedObject));
        drawTextFit(rx, ry, spec->description, rgb(205,214,220), rightW - 36, s.monoSmall ? s.monoSmall : s.mono);
        ry += 44;
        for (int i = 0; i < spec->frameCount; ++i) {
            const AnimationFrameSpec& f = spec->frames[i];
            drawLabLine(rx, ry, std::string(i == lab.frame ? "> " : "  ") + f.id
                + "  " + std::to_string(f.durationMs) + "ms", i == lab.frame ? rgb(255,230,135) : rgb(210,218,220));
            drawTextFit(rx + 18, ry, f.description, rgb(150,165,174), rightW - 54, s.monoSmall ? s.monoSmall : s.mono);
            ry += 34;
        }
    } else {
        drawLabLine(rx, ry, "No authored animation spec; showing idle placeholder.", rgb(210,165,135));
    }

    ry += 6;
    drawLabLine(rx, ry, "Asset", rgb(255,230,135));
    drawLabLine(rx, ry, std::string("status: ") + (assetFrame.status.empty() ? "not requested" : assetFrame.status));
    drawLabLine(rx, ry, std::string("base: ") + (assetFrame.baseLoaded ? "loaded" : "missing"));
    drawTextFit(rx, ry, assetFrame.basePath, rgb(150,165,174), rightW - 36, s.monoSmall ? s.monoSmall : s.mono);
    ry += 38;
    drawLabLine(rx, ry, std::string("mask: ") + (assetFrame.maskLoaded ? "loaded" : "missing"));
    drawTextFit(rx, ry, assetFrame.maskPath, rgb(150,165,174), rightW - 36, s.monoSmall ? s.monoSmall : s.mono);
    ry += 38;

    drawLabLine(rx, ry, "ASCII Cell", rgb(255,230,135));
    TerminalCell cell = terminalMapCell(LAB_X, LAB_Y);
    std::string glyph(1, cell.ch);
    drawLabLine(rx, ry, "glyph: " + glyph);
    drawLabLine(rx, ry, "fg rgb: " + std::to_string(cell.fg.r) + "," + std::to_string(cell.fg.g) + "," + std::to_string(cell.fg.b));
    drawLabLine(rx, ry, "bg rgb: " + std::to_string(cell.bg.r) + "," + std::to_string(cell.bg.g) + "," + std::to_string(cell.bg.b));

    SDL_Rect asciiBox{rx, ry + 4, 92, 72};
    setDraw(cell.bg);
    SDL_RenderFillRect(s.ren, &asciiBox);
    setDraw(rgb(255,255,255,120));
    SDL_RenderDrawRect(s.ren, &asciiBox);
    drawCentered(glyph, asciiBox, cell.fg, false);

    drawLabDropdownPopup(lab);

    if (present) SDL_RenderPresent(s.ren);
}

static void clampLabState(LabState& lab) {
    lab.previewMode = (lab.previewMode + 3) % 3;
    lab.terrain = (lab.terrain + (int)T_CASTLE_GATE + 1) % ((int)T_CASTLE_GATE + 1);
    lab.biome = (lab.biome + (int)B_OCEAN + 1) % ((int)B_OCEAN + 1);
    lab.season = (lab.season + 4) % 4;
    lab.seasonPercent = std::max(0, std::min(99, lab.seasonPercent));
    lab.timeStep = (lab.timeStep + 6) % 6;
    lab.weather = (lab.weather + 4) % 4;
    lab.fog = (lab.fog + 3) % 3;
    lab.lightMode = (lab.lightMode + 4) % 4;
    lab.resources = std::max(0, std::min(999, lab.resources));
    std::vector<EntityType> types = labEntityTypes();
    int idx = 0;
    for (int i = 0; i < (int)types.size(); ++i) if (types[i] == lab.entityType) idx = i;
    lab.entityType = types[std::max(0, std::min(idx, (int)types.size() - 1))];
    int actionCount = entityActionAnimationSpecCount((EntityType)lab.entityType);
    if (actionCount <= 0) lab.actionIndex = 0;
    else lab.actionIndex = (lab.actionIndex + actionCount) % actionCount;
    lab.direction = (lab.direction + 2) % 2;
    lab.frame = (lab.frame + labFrameCount(lab)) % labFrameCount(lab);
    lab.owner = (lab.owner + MAX_PLAYERS) % MAX_PLAYERS;
    lab.hue = (lab.hue + 360) % 360;
    lab.speedPercent = std::max(10, std::min(400, lab.speedPercent));
    if (lab.activeDropdown != LAB_DD_NONE) {
        std::vector<LabDropdownOption> options = labDropdownOptions(lab.activeDropdown, lab);
        if (options.empty()) lab.activeDropdown = LAB_DD_NONE;
        else lab.dropdownScroll = std::max(0, std::min(lab.dropdownScroll, std::max(0, (int)options.size() - 1)));
    }
}

static void stepLabEntity(LabState& lab, int delta) {
    std::vector<EntityType> types = labEntityTypes();
    int idx = 0;
    for (int i = 0; i < (int)types.size(); ++i) if (types[i] == lab.entityType) idx = i;
    idx = (idx + delta + (int)types.size()) % (int)types.size();
    lab.entityType = types[idx];
    lab.actionIndex = 0;
    lab.frame = 0;
}

static bool saveLabShot(const LabState& lab, const std::filesystem::path& path) {
    drawLabFrame(lab, false);
    return saveRendererPixels(path.string());
}

static int runLabSmoke() {
    namespace fs = std::filesystem;
    fs::path outDir = fs::path("build") / "lab-screenshots";
    fs::create_directories(outDir);
    gfxSetWindowSizeForTest(1280, 820);

    bool ok = true;
    LabState lab;
    ok = saveLabShot(lab, outDir / "00-default-no-entity.bmp") && ok;

    lab.activeDropdown = LAB_DD_ENTITY;
    ok = saveLabShot(lab, outDir / "00b-entity-dropdown.bmp") && ok;
    lab.activeDropdown = LAB_DD_NONE;

    lab.entityType = E_PEASANT;
    ok = saveLabShot(lab, outDir / "01-combined-peasant.bmp") && ok;

    lab.previewMode = 1;
    lab.actionIndex = 0;
    lab.direction = 0;
    lab.frame = 0;
    ok = saveLabShot(lab, outDir / "01a-peasant-idle-front-frame0.bmp") && ok;
    lab.frame = 1;
    ok = saveLabShot(lab, outDir / "01b-peasant-idle-front-frame1-arms-crossed.bmp") && ok;
    lab.direction = 1;
    lab.frame = 0;
    ok = saveLabShot(lab, outDir / "01c-peasant-idle-back-frame0.bmp") && ok;
    lab.frame = 1;
    ok = saveLabShot(lab, outDir / "01d-peasant-idle-back-frame1-arms-crossed.bmp") && ok;

    lab.previewMode = 0;
    lab.entityType = E_NONE;
    lab.terrain = T_WATER;
    lab.weather = W_RAIN;
    ok = saveLabShot(lab, outDir / "02-tile-only-rain-water.bmp") && ok;

    lab.previewMode = 1;
    lab.entityType = E_PEASANT;
    lab.terrain = T_GRASS;
    lab.weather = W_CLEAR;
    lab.hue = 0;
    ok = saveLabShot(lab, outDir / "03-peasant-red-team.bmp") && ok;

    lab.hue = 125;
    lab.actionIndex = 1;
    lab.frame = 1;
    ok = saveLabShot(lab, outDir / "04-peasant-walk-green-team.bmp") && ok;

    lab.entityType = E_MILITIA;
    lab.actionIndex = 0;
    lab.frame = 0;
    ok = saveLabShot(lab, outDir / "05-missing-militia-placeholder.bmp") && ok;

    lab.previewMode = 2;
    lab.entityType = E_PEASANT;
    lab.timeStep = 0;
    lab.lightMode = 3;
    ok = saveLabShot(lab, outDir / "06-night-candle.bmp") && ok;

    std::cerr << "realm: lab smoke " << (ok ? "complete" : "failed")
              << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
}

} // namespace

int gfxRunTilesetLab() {
    std::cerr << "realm: lab started\n";
    labForcesImageTileset = true;
    displayMode = DM_EMOJI;
    gfxSetProjection(true);
    initGameWithSeed(0, 2468, 0);
    g.viewX = LAB_X - 8;
    g.viewY = LAB_Y - 8;
    gfxSetZoomForTest(34);

    if (std::getenv("REALM_LAB_SMOKE")) return runLabSmoke();

    LabState lab;
    bool quit = false;
    Uint32 lastTick = SDL_GetTicks();
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                s.winW = e.window.data1;
                s.winH = e.window.data2;
            }
            if (e.type == SDL_MOUSEMOTION) {
                s.mouseX = e.motion.x;
                s.mouseY = e.motion.y;
                if (lab.hueDragging) labSetHueFromPoint(lab, s.mouseX, s.mouseY);
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                s.mouseX = e.button.x;
                s.mouseY = e.button.y;
                labHandleMouseDown(lab, s.mouseX, s.mouseY);
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                lab.hueDragging = false;
            }
            if (e.type == SDL_MOUSEWHEEL && lab.activeDropdown != LAB_DD_NONE) {
                lab.dropdownScroll -= e.wheel.y;
            }
            if (e.type != SDL_KEYDOWN) continue;
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_ESCAPE) quit = true;
            else if (k == SDLK_1) lab.previewMode++;
            else if (k == SDLK_q) lab.terrain--;
            else if (k == SDLK_e) lab.terrain++;
            else if (k == SDLK_a) lab.biome--;
            else if (k == SDLK_d && (e.key.keysym.mod & KMOD_SHIFT)) lab.damaged = !lab.damaged;
            else if (k == SDLK_d) lab.biome++;
            else if (k == SDLK_z) lab.season--;
            else if (k == SDLK_x) lab.season++;
            else if (k == SDLK_c) lab.seasonPercent -= 10;
            else if (k == SDLK_v) lab.seasonPercent += 10;
            else if (k == SDLK_n) lab.timeStep--;
            else if (k == SDLK_m) lab.timeStep++;
            else if (k == SDLK_r) lab.weather++;
            else if (k == SDLK_b) lab.fog++;
            else if (k == SDLK_l) lab.lightMode++;
            else if (k == SDLK_t) lab.resources = lab.resources >= 200 ? 0 : lab.resources + 50;
            else if (k == SDLK_u) stepLabEntity(lab, -1);
            else if (k == SDLK_i) stepLabEntity(lab, 1);
            else if (k == SDLK_j) { lab.actionIndex--; lab.frame = 0; }
            else if (k == SDLK_k) { lab.actionIndex++; lab.frame = 0; }
            else if (k == SDLK_h) lab.direction--;
            else if (k == SDLK_y) lab.direction++;
            else if (k == SDLK_f) lab.frame--;
            else if (k == SDLK_g) lab.frame++;
            else if (k == SDLK_o) lab.owner--;
            else if (k == SDLK_p) lab.owner++;
            else if (k == SDLK_LEFTBRACKET) lab.hue -= 8;
            else if (k == SDLK_RIGHTBRACKET) lab.hue += 8;
            else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) lab.speedPercent -= 10;
            else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) lab.speedPercent += 10;
            else if (k == SDLK_SPACE) lab.playing = !lab.playing;
            else if (k == SDLK_s) {
                std::filesystem::path path = labManualShotPath(lab);
                bool ok = saveLabShot(lab, path);
                std::cerr << "realm: lab manual screenshot " << (ok ? "ok " : "failed ")
                          << path.string() << "\n";
            }
            clampLabState(lab);
        }

        Uint32 now = SDL_GetTicks();
        if (lab.playing && now - lastTick >= (Uint32)std::max(12, (TICK_MS * 100) / std::max(10, lab.speedPercent))) {
            g.tick++;
            if (labFrameCount(lab) > 1) lab.frame = (lab.frame + 1) % labFrameCount(lab);
            lastTick = now;
        }
        clampLabState(lab);
        drawLabFrame(lab, true);
        SDL_Delay(16);
    }
    labLightOverride.enabled = false;
    labForcesImageTileset = false;
    return 0;
}
