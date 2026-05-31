#include "realm.h"
#include "gfx_renderer.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Color { Uint8 r, g, b, a; };

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

    // GUI-only projection mode. Terminal/ncurses build does not see this.
    // false = classic top-down grid, true = isometric diamond tiles.
    bool isometric = true;

    bool leftDown = false;
    bool middleDown = false;
    bool miniMapDown = false;
    int dragStartX = 0, dragStartY = 0;
    int panStartMouseX = 0, panStartMouseY = 0;
    int panStartViewX = 0, panStartViewY = 0;
    int lastMouseMapX = -9999, lastMouseMapY = -9999;

    std::unordered_map<std::string, SDL_Texture*> textCache;
} s;

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

static float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

static void setDraw(Color c) {
    SDL_SetRenderDrawColor(s.ren, c.r, c.g, c.b, c.a);
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

static int textWidth(const std::string& text, TTF_Font* font = nullptr) {
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, rgb(255,255,255));
    if (!tex) return 0;
    int w=0,h=0; SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    return w;
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

static const char* entityGlyph(const Entity& e) {
    switch (e.type) {
        case E_PEASANT: return peasantGlyph(e);
        case E_MILITIA: return u8"🤺";
        case E_ARCHER: return u8"🏹";
        case E_KNIGHT: return u8"🐎";
        case E_CATAPULT: return u8"🛞";
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
        default: return "?";
    }
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

static SDL_Rect mapRect() {
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
    return SDL_Rect{s.winW - s.panelW, 0, s.panelW, s.winH};
}

static SDL_Rect miniMapRect() {
    SDL_Rect pr = panelRect();
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
    SDL_Rect mr = mapRect();
    if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;

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
    SDL_Rect mr = mapRect();
    if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;

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
    if (s.isometric) {
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
    clampView();
}

static void moveCursorToViewCenter() {
    if (s.isometric) {
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
        } else if (v.visible) {
            v.glyph.assign(1, terrainAscii(tile.terrain));
        } else {
            v.glyph = "."; v.fg = rgb(95,95,105,150);
        }
    } else if (v.visible && v.ent && v.ent->alive) {
        v.glyph = entityGlyph(*v.ent); v.emoji = true;
        v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(245,245,235) : rgb(255,255,255);
        v.tint = true;
    } else if (v.visible) {
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
        int glyphSize = v.emoji ? std::max(16, (int)(s.tile * 0.96f)) : std::max(10, (int)(s.tile * 0.62f));
        SDL_Rect gr{cx - glyphSize/2, cy - glyphSize/2, glyphSize, glyphSize};
        drawCentered(v.glyph, gr, v.visible ? v.fg : scale(v.fg, 0.55f), v.emoji, v.tint);
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
    vx0 = std::max(0, std::min(vx0, MAP_W));
    vy0 = std::max(0, std::min(vy0, MAP_H));
    vx1 = std::max(0, std::min(vx1, MAP_W));
    vy1 = std::max(0, std::min(vy1, MAP_H));
    if (vx1 > vx0 && vy1 > vy0) {
        SDL_Rect view{x + vx0*w/MAP_W, y + vy0*h/MAP_H,
                      std::max(2,(vx1-vx0)*w/MAP_W), std::max(2,(vy1-vy0)*h/MAP_H)};
        setDraw(rgb(255,255,255,210)); SDL_RenderDrawRect(s.ren, &view);
    }
}

static void drawTopBar() {
    SDL_Rect top{0,0,s.winW,s.topH};
    setDraw(rgb(12,32,58)); SDL_RenderFillRect(s.ren,&top);
    Player& p = g.players[0];
    std::ostringstream ss;
    ss << "G:" << p.gold << "  W:" << p.wood << "  F:" << p.food
       << "  Pop:" << p.supply << "/" << p.supplyMax
       << "  " << seasonNameSafe() << ' ' << timeNameSafe() << ' ' << weatherName()
       << "  " << (s.isometric ? "Iso" : "Top")
       << "  " << s.tile << "px";
    drawTextFit(10, 7, ss.str(), rgb(235,238,230), std::max(1, s.winW - s.panelW - 18));
}

static bool isTrainProducer(EntityType t) {
    return t == E_TOWNHALL || t == E_BARRACKS || t == E_STABLE || t == E_DOCK;
}

static std::string trainPromptFor(const Entity* sel) {
    if (!sel || sel->owner != 0 || !isTrainProducer(sel->type))
        return "TRAIN: select a production building, Esc cancel";
    switch (sel->type) {
        case E_TOWNHALL: return "TRAIN: P Peasant (50g), repeat to queue, Esc cancel";
        case E_BARRACKS: return "TRAIN: M Militia  A Archer  C Catapult  R Ram, Esc cancel";
        case E_STABLE: return "TRAIN: K Knight, repeat to queue, Esc cancel";
        case E_DOCK: return "TRAIN: B Fishing boat  W Warship  T Transport, Esc cancel";
        default: return "TRAIN: no units available, Esc cancel";
    }
}

static std::vector<std::string> trainPanelHintsFor(EntityType t) {
    switch (t) {
        case E_TOWNHALL: return {"P: peasant (50g)"};
        case E_BARRACKS: return {"M: militia  A: archer", "C: catapult  R: ram"};
        case E_STABLE: return {"K: knight"};
        case E_DOCK: return {"B: fish boat  W: warship", "T: transport"};
        default: return {};
    }
}

static void drawPanel() {
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
    drawTextFit(x, y, s.isometric ? "Projection: Isometric" : "Projection: Top-down", rgb(150,170,190), textW); y += 20;
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
        drawCentered(entityGlyph(*sel), b, rgb(255,255,255), true);
        drawTextFit(x+30, y+2, STATS[sel->type].name, rgb(255,230,135), std::max(1, textW - 30)); y += 26;
        std::ostringstream hp; hp << "HP: " << sel->hp << "/" << sel->maxHp;
        drawTextFit(x, y, hp.str(), rgb(220,220,210), textW); y += 20;
        drawTextFit(x, y, stateName(sel->state), rgb(180,190,200), textW); y += 22;
        if (sel->owner == 0) {
            if (sel->type == E_PEASANT) {
                drawTextFit(x,y,"B: build",rgb(150,210,230), textW); y+=20;
                drawTextFit(x,y,"Enter/R-click: command",rgb(150,210,230), textW); y+=20;
            }
            else if (isBuilding(sel->type) && !sel->underConstruction) {
                if (isTrainProducer(sel->type)) {
                    drawTextFit(x,y,"T: train",rgb(150,210,230), textW); y+=20;
                    for (const std::string& hint : trainPanelHintsFor(sel->type)) {
                        drawTextFit(x,y,hint,rgb(180,205,210), textW); y+=20;
                    }
                } else {
                    drawTextFit(x,y,"No train options",rgb(130,145,150), textW); y+=20;
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
    std::string controls2 = "F5:Save  F8:Diag  F9:Load  F6/F7:View  +/-:Zoom  Q:Resign  X:Exit";
    if (g.mode == M_PAUSED) { controls1 = "PAUSED - Press P to resume"; controls2.clear(); }
    else if (g.mode == M_GAME_OVER) { controls1 = (g.winner==0) ? "VICTORY - Enter/Q for menu, X to exit" : "DEFEAT - Enter/Q for menu, X to exit"; controls2.clear(); }
    else if (g.mode == M_BUILD_SELECT) { controls1 = "BUILD: H House, B Barracks, S Stable, T Tower, F Farm, W Wall, K Castle"; controls2 = "L Lumber camp  N Mining camp  I Mill  D Dock  Esc cancel"; }
    else if (g.mode == M_TRAIN_SELECT) { controls1 = trainPromptFor(findEntity(g.selectedId)); controls2.clear(); }
    const std::string captureHint = "F12:Capture issue";
    int hintW = textWidth(captureHint);
    int hintX = std::max(10, s.winW - hintW - 14);
    drawText(hintX, s.winH-s.bottomH+6, captureHint, rgb(255,230,120));

    int maxW = std::max(1, s.winW - 20);
    int topLineW = std::max(1, hintX - 20);
    drawTextFit(10, s.winH-s.bottomH+6, controls1, rgb(230,235,230), topLineW);
    if (g.statusTimer > 0) {
        drawTextFit(10, s.winH-s.bottomH+26, ">> " + g.statusMsg, rgb(255,230,120), maxW);
        g.statusTimer--;
    } else if (!controls2.empty()) {
        drawTextFit(10, s.winH-s.bottomH+26, controls2, rgb(200,213,220), maxW);
    }
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

static void drawSplash(int numAIs, int biomeIdx) {
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
    line("  [6]/[7] menu   Top-down / isometric projection");
    line("  B/T/A/H/P      Build / Train / Military / Town hall / Pause");
    y += 10;
    line("OPPONENTS", rgb(255,230,135));
    line("  [1] Duel       [2] Three-way     [3] Four-way");
    y += 4;
    line("BIOME", rgb(255,230,135));
    line("  [0] Random    [T] Temperate  [D] Desert");
    line("  [S] Snow      [W] Swamp      [F] Forest");
    line("  [V] Volcanic  [C] Coastal");
    y += 4;
    line("DISPLAY", rgb(255,230,135));
    line(std::string("  [4] ASCII     [5] Emoji       > ") + (displayMode==DM_EMOJI ? "Emoji" : "ASCII"));
    y += 4;
    line("PROJECTION", rgb(255,230,135));
    line(std::string("  [6] Top-down  [7] Isometric   > ") + (s.isometric ? "Isometric" : "Top-down"));
    y += 10;
    static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
    std::ostringstream ss; ss << "  > Opponents: " << numAIs << "    Biome: " << biomeNames[biomeIdx];
    line(ss.str(), rgb(255,245,180));
    y += 4;
    line("  [Enter] Start game            [Q/X] Quit", rgb(210,230,245));
    SDL_RenderPresent(s.ren);
}

} // namespace

bool gfxInit() {
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
        std::cerr << "No emoji font found; using ASCII-style emoji fallbacks.\n";
    } else {
        s.emojiFontLoaded = true;
    }
    std::cerr << "Text font: " << (s.monoPath.empty() ? "<unknown>" : s.monoPath) << "\n";
    std::cerr << "Emoji font: " << (s.emojiFontLoaded ? s.emojiPath : std::string("<fallback>")) << "\n";
    SDL_StartTextInput();
    return true;
}

void gfxShutdown() {
    clearTextCache();
    if (s.mono) TTF_CloseFont(s.mono);
    if (s.monoSmall) TTF_CloseFont(s.monoSmall);
    if (s.emoji && s.emoji != s.mono) TTF_CloseFont(s.emoji);
    if (s.ren) SDL_DestroyRenderer(s.ren);
    if (s.win) SDL_DestroyWindow(s.win);
    TTF_Quit();
    SDL_Quit();
}

int gfxShowSplash() {
    int numAIs = 1;
    int biomeIdx = 7;
    bool loggedReady = false;
    while (true) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) { gfxShutdown(); std::exit(0); }
            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                s.winW = e.window.data1; s.winH = e.window.data2;
            }
            if (e.type != SDL_KEYDOWN) continue;
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_q || k == SDLK_x) { gfxShutdown(); std::exit(0); }
            if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
                g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
                return numAIs;
            }
            if (k == SDLK_1) numAIs = 1;
            else if (k == SDLK_2) numAIs = 2;
            else if (k == SDLK_3) numAIs = 3;
            else if (k == SDLK_0) biomeIdx = 7;
            else if (k == SDLK_t) biomeIdx = 0;
            else if (k == SDLK_d) biomeIdx = 1;
            else if (k == SDLK_s) biomeIdx = 2;
            else if (k == SDLK_w) biomeIdx = 3;
            else if (k == SDLK_f) biomeIdx = 4;
            else if (k == SDLK_v) biomeIdx = 5;
            else if (k == SDLK_c) biomeIdx = 6;
            else if (k == SDLK_4) displayMode = DM_ASCII;
            else if (k == SDLK_5) displayMode = DM_EMOJI;
            else if (k == SDLK_6) s.isometric = false;
            else if (k == SDLK_7) s.isometric = true;
        }
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        drawSplash(numAIs, biomeIdx);
        if (!loggedReady) {
            std::cerr << "realm: main screen ready\n";
            loggedReady = true;
            const char* smoke = std::getenv("REALM_SMOKE_TEST");
            if (smoke) return std::string(smoke) == "match" ? numAIs : -1;
        }
        SDL_Delay(16);
    }
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
             << "projection: " << (s.isometric ? "isometric" : "top-down") << "\n"
             << "visuals: " << (displayMode == DM_EMOJI ? "emoji" : "ascii") << "\n"
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

void gfxPollInput(bool& quitRequested) {
    quitRequested = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { quitRequested = true; return; }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            s.winW = e.window.data1; s.winH = e.window.data2; updateViewMetrics(true);
        }
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my; SDL_GetMouseState(&mx, &my);
            if (e.wheel.y > 0) setZoom(s.tile + 3, mx, my);
            if (e.wheel.y < 0) setZoom(s.tile - 3, mx, my);
        }
        if (e.type == SDL_MOUSEMOTION) {
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
            if (e.button.button == SDL_BUTTON_MIDDLE) {
                startMiddlePan(e.button.x, e.button.y);
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
                    else { s.leftDown = true; s.dragStartX = mx; s.dragStartY = my; g.dragging = true; }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    rendererCommandAtTile(mx,my);
                }
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP) {
            int mx,my;
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
            if (e.button.button == SDL_BUTTON_LEFT && screenToMap(e.button.x, e.button.y, mx, my)) {
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
            if (k == SDLK_x) { quitRequested = true; return; }
            if (k == SDLK_F6) { s.isometric = false; updateViewMetrics(true); continue; }
            if (k == SDLK_F7) { s.isometric = true;  updateViewMetrics(true); continue; }
            if (k == SDLK_F5) { saveGame("realm-save.txt"); continue; }
            if (k == SDLK_F8) { g.diagnostics = !g.diagnostics; continue; }
            if (k == SDLK_F9) { loadGame("realm-save.txt"); updateViewMetrics(true); continue; }
            if (k == SDLK_F12) { captureIssueBundle(); continue; }
            if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { setZoom(s.tile+3); continue; }
            if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { setZoom(s.tile-3); continue; }
            int ch = keyToInput(k);
            if (ch) handleInput(ch);
        }
    }
}

static void drawFrame(bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    setDraw(rgb(3,5,8)); SDL_RenderClear(s.ren);
    drawTopBar();
    drawMap();
    drawPanel();
    drawBottom();
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
    s.isometric = isometric;
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
