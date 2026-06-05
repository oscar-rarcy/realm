#include "render/sdl/sdl_text.h"
#include "realm.h"
#include "render/ground_shader.h"

Gfx s;
bool labForcesImageTileset = false;
LabLightOverride labLightOverride;
Color rgb(int r, int g, int b, int a) {
    return Color{(Uint8)std::max(0,std::min(255,r)),
                 (Uint8)std::max(0,std::min(255,g)),
                 (Uint8)std::max(0,std::min(255,b)),
                 (Uint8)std::max(0,std::min(255,a))};
}

Color scale(Color c, float f) {
    return rgb((int)(c.r*f), (int)(c.g*f), (int)(c.b*f), c.a);
}

Color blend(Color a, Color b, float t) {
    return rgb((int)(a.r + (b.r-a.r)*t), (int)(a.g + (b.g-a.g)*t), (int)(a.b + (b.b-a.b)*t),
               (int)(a.a + (b.a-a.a)*t));
}

Color groundShaderColor(GroundShaderColor c) {
    return rgb(c.r, c.g, c.b, c.a);
}

GroundShaderContext currentGroundShaderContext() {
    GroundShaderContext context;
    context.season = getSeason(g);
    context.seasonProgress = getSeasonProgress(g);
    context.weather = (Weather)g.weather;
    context.tick = g.tick;
    return context;
}

GroundShaderResult shadeGroundTileForCurrentGame(const Tile& tile, const VisualTileParts& parts, int x, int y) {
    return shadeGroundTile(tile, parts, x, y, currentGroundShaderContext());
}

std::string lowerSlug(const std::string& text) {
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

std::string quoteLogValue(const std::string& text) {
    std::string out = "\"";
    for (char ch : text) {
        if (ch == '\\' || ch == '"') out.push_back('\\');
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

bool envFlagEnabled(const char* name, bool fallback) {
    const char* raw = std::getenv(name);
    if (!raw || !*raw) return fallback;
    std::string value(raw);
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return (char)std::tolower(ch); });
    return !(value == "0" || value == "false" || value == "off" || value == "no");
}

bool localTilesetAuditEnabled() {
    if (!envFlagEnabled("REALM_TILESET_AUDIT", true)) return false;
#if defined(REALM_WEB)
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.location) return 0;
        var host = String(window.location.hostname || "").toLowerCase();
        return (host === 'localhost' || host === '127.0.0.1' || host === '::1' || host === "") ? 1 : 0;
    }) != 0;
#else
    return true;
#endif
}

void appendMissingTileLocalhostLog(const std::string& line) {
#if defined(REALM_WEB)
    EM_ASM({
        if (typeof window === 'undefined' || !window.location) return;
        var host = String(window.location.hostname || "").toLowerCase();
        if (!(host === 'localhost' || host === '127.0.0.1' || host === '::1' || host === "")) return;
        var line = UTF8ToString($0);
        var storageKey = 'realm.missingTilesLog';
        var header = '# Realm missing tiles\n'
            + '# Local browser run only. Add real assets for these keys; current renderer used placeholders.\n';
        var existing = "";
        try { existing = window.localStorage.getItem(storageKey) || ""; } catch (error) {}
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

void logMissingTile(const std::string& kind, const std::string& key,
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

float clamp01(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

void setDraw(Color c) {
    SDL_SetRenderDrawColor(s.ren, c.r, c.g, c.b, c.a);
}

void applyRendererOutputScale() {
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

unsigned hash2(int x, int y, unsigned salt) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + salt * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float noisePatch(int x, int y, unsigned salt) {
    // Coarse noise: gives soft painted patches, not salt-and-pepper.
    unsigned a = hash2(x/5,  y/4,  salt);
    unsigned b = hash2(x/12, y/9,  salt+11u);
    unsigned c = hash2(x/23, y/17, salt+29u);
    return ((a & 255) + ((b & 255) * 0.65f) + ((c & 255) * 0.35f)) / (255.0f * 2.0f);
}

int terrainFamily(Terrain t) {
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

int boundaryStrength(int x, int y) {
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

Color seasonTint(Color base) {
    switch (getSeason(g)) {
        case SPRING: return blend(base, rgb(120,190,105), 0.12f);
        case SUMMER: return blend(base, rgb(205,170,80), 0.07f);
        case AUTUMN: return blend(base, rgb(190,115,55), 0.18f);
        case WINTER: return blend(base, rgb(205,215,225), 0.24f);
    }
    return base;
}

Color timeTint(Color base) {
    float b = clamp01(getBrightness(g));
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

Color biomeBase(Biome b) {
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

Color terrainBg(const Tile& t, int x, int y) {
    VisualTileParts parts = visualPartsForTile(t);
    Color c = groundShaderColor(shadeGroundTileForCurrentGame(t, parts, x, y).baseFill);
    float shade = 1.0f;
    int edge = boundaryStrength(x,y);
    if (edge) shade += 0.04f * edge;
    c = scale(c, shade);
    return timeTint(c);
}

Color colorFromHue(int hue) {
    float h = normalizePlayerColorHue(hue) / 60.0f;
    float chroma = 0.82f;
    float x = chroma * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));
    float r = 0.0f, g = 0.0f, b = 0.0f;
    if (h < 1.0f) { r = chroma; g = x; }
    else if (h < 2.0f) { r = x; g = chroma; }
    else if (h < 3.0f) { g = chroma; b = x; }
    else if (h < 4.0f) { g = x; b = chroma; }
    else if (h < 5.0f) { r = x; b = chroma; }
    else { r = chroma; b = x; }
    const float m = 0.18f;
    return rgb((int)std::lround((r + m) * 210.0f),
               (int)std::lround((g + m) * 210.0f),
               (int)std::lround((b + m) * 210.0f));
}

Color ownerBg(int owner) {
    int idx = owner % MAX_PLAYERS;
    if (idx < 0) idx += MAX_PLAYERS;
    return colorFromHue(g.playerColorHue[idx]);
}

std::string firstExisting(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        FILE* f = std::fopen(p.c_str(), "rb");
        if (f) { std::fclose(f); return p; }
    }
    return "";
}

TTF_Font* openFont(const std::vector<std::string>& paths, int size, std::string* usedPath) {
    std::string p = firstExisting(paths);
    if (usedPath) *usedPath = p;
    if (p.empty()) return nullptr;
    TTF_Font* f = TTF_OpenFont(p.c_str(), size);
    if (f) TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    return f;
}

TTF_Font* openFontAtPath(const std::string& path, int size) {
    if (path.empty()) return nullptr;
    TTF_Font* f = TTF_OpenFont(path.c_str(), size);
    if (f) TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    return f;
}

TTF_Font* fittedMonoFontForCell(int maxW, int maxH, TTF_Font* fallback) {
    if (s.monoPath.empty() || maxW <= 0 || maxH <= 0) return fallback ? fallback : s.mono;
    int fitW = std::max(1, maxW);
    int fitH = std::max(1, maxH);
    std::ostringstream key;
    key << fitW << 'x' << fitH;
    std::string cacheKey = key.str();
    auto it = s.sizedMonoFonts.find(cacheKey);
    if (it != s.sizedMonoFonts.end()) return it->second ? it->second : (fallback ? fallback : s.mono);

    int lo = 4;
    int hi = std::max(4, std::min(96, maxH * 2));
    int best = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        TTF_Font* candidate = openFontAtPath(s.monoPath, mid);
        int w = 0;
        int h = 0;
        if (candidate) {
            TTF_SizeText(candidate, "M", &w, &h);
            h = std::max(h, TTF_FontLineSkip(candidate));
        }
        bool fits = candidate && w <= fitW && h <= fitH;
        if (candidate) TTF_CloseFont(candidate);
        if (fits) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    TTF_Font* fitted = best > 0 ? openFontAtPath(s.monoPath, best) : nullptr;
    s.sizedMonoFonts[cacheKey] = fitted;
    return fitted ? fitted : (fallback ? fallback : s.mono);
}

std::string emojiFallbackGlyph(const std::string& text) {
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

SDL_Texture* cachedText(TTF_Font* font, const std::string& text, Color col, bool blended) {
    if (!font) return nullptr;
    std::ostringstream k;
    k << (void*)font << '|' << (blended ? 'b' : 's') << '|'
      << (int)col.r << ',' << (int)col.g << ',' << (int)col.b << ',' << (int)col.a << '|' << text;
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

void drawText(int x, int y, const std::string& text, Color col, TTF_Font* font) {
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, col);
    if (!tex) return;
    int w=0,h=0; SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{x,y,w,h};
    SDL_SetTextureColorMod(tex, 255,255,255);
    SDL_SetTextureAlphaMod(tex, col.a);
    SDL_RenderCopy(s.ren, tex, nullptr, &dst);
}

void drawTextFit(int x, int y, const std::string& text, Color col, int maxW, TTF_Font* font);

int textWidth(const std::string& text, TTF_Font* font) {
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, rgb(255,255,255));
    if (!tex) return 0;
    int w=0,h=0; SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    return w;
}

int textLineHeight(TTF_Font* font) {
    TTF_Font* f = font ? font : s.mono;
    return std::max(16, f ? TTF_FontLineSkip(f) : 18);
}

bool rectHovered(SDL_Rect r) {
    return s.mouseX >= r.x && s.mouseY >= r.y && s.mouseX < r.x + r.w && s.mouseY < r.y + r.h;
}

void registerKeyHit(SDL_Rect r, int ch) {
    if (ch == 0 || r.w <= 0 || r.h <= 0) return;
    s.keyHits.push_back({r, ch});
}

void drawHoverMark(SDL_Rect r, Color color) {
    if (!rectHovered(r)) return;
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(color);
    SDL_RenderDrawLine(s.ren, r.x, r.y + r.h - 2, r.x + r.w, r.y + r.h - 2);
    SDL_RenderDrawLine(s.ren, r.x, r.y + r.h - 1, r.x + r.w, r.y + r.h - 1);
}

void drawKeyOptionText(int x, int y, const std::string& text, int ch,
                              Color color, int maxW, TTF_Font* font) {
    TTF_Font* f = font ? font : s.mono;
    drawTextFit(x, y, text, color, maxW, f);
    SDL_Rect r{x, y, std::min(std::max(1, maxW), std::max(1, textWidth(text, f))), textLineHeight(f)};
    registerKeyHit(r, ch);
    drawHoverMark(r, color);
}

void drawKeyTokensInText(int x, int y, const std::string& text,
                                const std::vector<std::pair<std::string, int>>& tokens,
                                Color color, int maxW, TTF_Font* font) {
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

void drawTextFit(int x, int y, const std::string& text, Color col, int maxW, TTF_Font* font) {
    if (maxW <= 0) return;
    std::string out = text;
    TTF_Font* f = font ? font : s.mono;
    if (textWidth(out, f) > maxW) {
        while (out.size() > 1 && textWidth(out + "~", f) > maxW) out.pop_back();
        if (!out.empty()) out += "~";
    }
    drawText(x, y, out, col, f);
}

void drawCentered(const std::string& text, SDL_Rect rect, Color col, bool emoji, bool tint) {
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

void drawTerminalCellGlyph(SDL_Rect rect, char ch, Color col, TTF_Font* fallback) {
    if (ch == ' ') return;
    TTF_Font* font = fittedMonoFontForCell(rect.w, rect.h, fallback ? fallback : s.mono);
    std::string text(1, ch);
    SDL_Texture* tex = cachedText(font, text, col);
    if (!tex) return;
    int tw = 0;
    int th = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) return;
    SDL_Rect dst{rect.x + (rect.w - tw) / 2, rect.y + (rect.h - th) / 2, tw, th};
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, col.a);
    SDL_RenderCopy(s.ren, tex, nullptr, &dst);
}
