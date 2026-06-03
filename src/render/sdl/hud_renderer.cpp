#include "render/sdl/sdl_hud.h"
#include "realm.h"
#include "commands/input_intent.h"
#include "core/build_service.h"
#include "core/production_service.h"
#include "core/world_index.h"
#include "view_state.h"

void drawMiniMap(const WorldIndex& world, int x, int y, int w, int h) {
    setDraw(rgb(10,12,18)); SDL_Rect bg{x,y,w,h}; SDL_RenderFillRect(s.ren,&bg);
    for (int yy=0; yy<h; ++yy) {
        int my = yy * MAP_H / std::max(1,h);
        for (int xx=0; xx<w; ++xx) {
            int mx = xx * MAP_W / std::max(1,w);
            const Tile& t = g.map[my][mx];
            Color c = t.explored[0] ? terrainBg(t,mx,my) : rgb(5,5,8);
            Entity* e = t.visible[0] ? renderEntityAt(g, world, mx, my) : nullptr;
            if (e && e->alive && e->owner != OWNER_NATURE) c = ownerBg(e->owner);
            setDraw(c); SDL_RenderDrawPoint(s.ren, x+xx, y+yy);
        }
    }
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    int vx0 = view.viewX, vy0 = view.viewY;
    int vx1 = view.viewX + view.viewW, vy1 = view.viewY + view.viewH;
    if (s.isometric) {
        IsoOffsetBounds b = isoVisibleOffsetBounds();
        vx0 = view.viewX + b.minSx;
        vx1 = view.viewX + b.maxSx + 1;
        vy0 = view.viewY + b.minSy;
        vy1 = view.viewY + b.maxSy + 1;
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

void drawTopBar() {
    SDL_Rect top{0,0,s.winW,s.topH};
    setDraw(rgb(12,32,58)); SDL_RenderFillRect(s.ren,&top);
    Player& p = g.players[0];
    std::ostringstream ss;
    ss << "G:" << p.gold << "  W:" << p.wood << "  F:" << p.food
       << "  Pop:" << p.supply << "/" << p.supplyMax
       << "  " << seasonNameSafe() << ' ' << timeNameSafe() << ' ' << weatherName();
    drawTextFit(10, 7, ss.str(), rgb(235,238,230), std::max(1, s.winW - s.panelW - 18));
}

static std::string menuKeyLabel(char key) {
    char upper = (char)std::toupper((unsigned char)key);
    return std::string(1, upper);
}

static std::vector<std::pair<std::string, int>> productionTokensFor(EntityType producer, bool bracketed) {
    std::vector<std::pair<std::string, int>> tokens;
    const ProductionRule* rule = productionRule(producer);
    if (!rule || !rule->menuHotkeys) {
        tokens.push_back({ bracketed ? "[Esc]" : "Esc", 27 });
        return tokens;
    }
    for (int i = 0; i < rule->allowedCount && rule->menuHotkeys[i] != '\0'; i++) {
        std::string label = menuKeyLabel(rule->menuHotkeys[i]);
        if (bracketed) label = "[" + label + "]";
        tokens.push_back({ label, rule->menuHotkeys[i] });
    }
    tokens.push_back({ bracketed ? "[Esc]" : "Esc", 27 });
    return tokens;
}

static std::vector<std::pair<std::string, int>> buildTokens(bool bracketed) {
    std::vector<std::pair<std::string, int>> tokens;
    int count = 0;
    const BuildRule* rules = buildRules(count);
    for (int i = 0; i < count; i++) {
        if (rules[i].menuHotkey == '\0') continue;
        std::string label = menuKeyLabel(rules[i].menuHotkey);
        if (bracketed) label = "[" + label + "]";
        tokens.push_back({ label, rules[i].menuHotkey });
    }
    tokens.push_back({ bracketed ? "[Esc]" : "Esc", 27 });
    return tokens;
}

static std::string unitMenuName(EntityType type) {
    const char* name = STATS[type].name ? STATS[type].name : "Unknown";
    std::string out(name);
    if (!out.empty()) out[0] = (char)std::tolower((unsigned char)out[0]);
    return out;
}

bool isTrainProducer(EntityType t) {
    return productionRule(t) != nullptr;
}

std::string trainPromptFor(const Entity* sel) {
    if (!sel || sel->owner != 0 || !isTrainProducer(sel->type))
        return "TRAIN: select a production building, Esc cancel";
    const ProductionRule* rule = productionRule(sel->type);
    if (!rule || !rule->menuHotkeys) return "TRAIN: no units available, Esc cancel";
    std::ostringstream prompt;
    prompt << "TRAIN:";
    for (int i = 0; i < rule->allowedCount && rule->menuHotkeys[i] != '\0'; i++) {
        EntityType unit = rule->allowedUnits[i];
        prompt << ' ' << menuKeyLabel(rule->menuHotkeys[i]) << ' ' << STATS[unit].name;
    }
    prompt << ", repeat to queue, Esc cancel";
    return prompt.str();
}

std::vector<std::string> trainPanelHintsFor(EntityType t) {
    std::vector<std::string> lines;
    const ProductionRule* rule = productionRule(t);
    if (!rule || !rule->menuHotkeys) return lines;
    std::ostringstream line;
    int onLine = 0;
    for (int i = 0; i < rule->allowedCount && rule->menuHotkeys[i] != '\0'; i++) {
        if (onLine == 2) {
            lines.push_back(line.str());
            line.str("");
            line.clear();
            onLine = 0;
        }
        if (onLine > 0) line << "  ";
        line << menuKeyLabel(rule->menuHotkeys[i]) << ": " << unitMenuName(rule->allowedUnits[i]);
        onLine++;
    }
    if (onLine > 0) lines.push_back(line.str());
    return lines;
}

std::vector<std::pair<std::string, int>> trainOptionTokensFor(EntityType t) {
    return productionTokensFor(t, false);
}

std::vector<std::pair<std::string, int>> desktopBuildTokensLine1() {
    std::vector<std::pair<std::string, int>> tokens = buildTokens(false);
    if (tokens.size() <= 8) return tokens;
    return std::vector<std::pair<std::string, int>>(tokens.begin(), tokens.begin() + 8);
}

std::vector<std::pair<std::string, int>> desktopBuildTokensLine2() {
    std::vector<std::pair<std::string, int>> tokens = buildTokens(false);
    if (tokens.size() <= 8) return {};
    return std::vector<std::pair<std::string, int>>(tokens.begin() + 8, tokens.end());
}

std::vector<std::pair<std::string, int>> terminalBuildTokens() {
    return buildTokens(true);
}

std::vector<std::pair<std::string, int>> defaultBottomTokens() {
    auto tokenFor = [](const char* id) -> std::pair<std::string, int> {
        int count = 0;
        const CommandHelpBinding* bindings = gameplayHelpBindings(count);
        for (int i = 0; i < count; i++) {
            if (std::string(bindings[i].id) != id || bindings[i].keyCount <= 0) continue;
            return { std::string(bindings[i].keys) + ":" + bindings[i].label, bindings[i].keyCodes[0] };
        }
        return { "", 0 };
    };
    std::vector<std::pair<std::string, int>> tokens = {
        tokenFor("build"),
        tokenFor("train"),
        tokenFor("save"),
        tokenFor("load"),
        tokenFor("diagnostics"),
        tokenFor("resign"),
    };
#if defined(REALM_WEB)
    return tokens;
#else
    tokens.push_back(tokenFor("hold"));
    return tokens;
#endif
}

bool devCaptureEnabled() {
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



















void mobileDrawResources(int x, int y, int w) {
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



void drawPanel(const WorldIndex& world) {
    if (isMobileGui()) {
        drawMobileHud(world);
        return;
    }
    SDL_Rect pr = panelRect();
    setDraw(rgb(8,10,14)); SDL_RenderFillRect(s.ren, &pr);
    // Keep the console look: a pipe divider drawn as text, not a modern UI bar.
    for (int y=0; y<s.winH; y+=16) drawText(pr.x, y, "|", rgb(95,105,115));

    int x = pr.x + 14, y = 12;
    int textW = std::max(1, pr.w - 28);
    SDL_Rect mini = miniMapRect();
    drawMiniMap(world, mini.x, mini.y, mini.w, mini.h); y += 124;

    drawText(x, y, "Realm", rgb(245,245,230)); y += 22;
    std::ostringstream c; c << "Cursor: (" << view.cursorX << "," << view.cursorY << ")";
    drawTextFit(x, y, c.str(), rgb(185,190,195), textW); y += 20;
    drawTextFit(x, y, cursorTileSummary(), rgb(180,190,185), textW); y += 20;
    drawTextFit(x, y, cursorStackSummary(), rgb(180,190,185), textW); y += 20;
    drawTextFit(x, y, displayMode == DM_ASCII ? "Visual: ASCII" : "Visual: Tileset", rgb(150,170,190), textW); y += 20;
    drawTextFit(x, y, "Wheel zoom / middle pan", rgb(150,160,168), textW); y += 26;

    if (g.local.diagnostics) {
        std::ostringstream ds;
        ds << "Diag tick " << g.tick << " mode " << modeName(g.mode);
        drawTextFit(x, y, trimPanelLine(ds.str()), rgb(255,210,120), textW); y += 20;
        std::ostringstream ds2;
        ds2 << "Ent " << g.entities.size() << " Proj " << g.projectiles.size()
            << " Seed " << g.seed;
        drawTextFit(x, y, trimPanelLine(ds2.str()), rgb(255,210,120), textW); y += 20;
        if (Entity* selDiag = renderFindEntity(g, world, g.local.selectedId)) {
            std::ostringstream ds3;
            ds3 << "Sel #" << selDiag->id << ' ' << STATS[selDiag->type].name
                << ' ' << stateName(selDiag->state);
            drawTextFit(x, y, trimPanelLine(ds3.str()), rgb(255,210,120), textW); y += 20;
        }
    }

    Entity* sel = renderFindEntity(g, world, g.local.selectedId);
    if (!g.local.selectedIds.empty()) {
        std::ostringstream gs; gs << "Group: " << g.local.selectedIds.size() << " units";
        drawTextFit(x, y, gs.str(), rgb(255,230,135), textW); y += 22;
        drawTextFit(x, y, "R-click: command group", rgb(180,185,190), textW); y += 20;
    } else if (sel) {
        Color badge = (sel->owner == OWNER_NATURE) ? rgb(95,95,80) : ownerBg(sel->owner);
        SDL_Rect b{x,y,22,22}; setDraw(badge); SDL_RenderFillRect(s.ren,&b);
        bool usesSymbolFont = false;
        drawCentered(tilesetEntityVisual(g, world, *sel, usesSymbolFont), b, rgb(255,255,255), usesSymbolFont);
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

void drawBottom(const WorldIndex& world) {
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
    else if (g.mode == M_BUILD_PLACE) { controls1 = std::string("PLACE ") + (g.local.buildPending != E_NONE ? STATS[g.local.buildPending].name : "building"); controls2 = "Arrows/mouse to position  Enter/click build  Esc/right-click cancel"; }
    else if (g.mode == M_PATROL_SET) { controls1 = "PATROL"; controls2 = "Click target or move cursor + Enter  Esc cancels"; }
    else if (g.mode == M_TRAIN_SELECT) { controls1 = trainPromptFor(renderFindEntity(g, world, g.local.selectedId)); controls2.clear(); }
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
        Entity* sel = renderFindEntity(g, world, g.local.selectedId);
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
    if (ui.statusTimer > 0) {
        drawTextFit(10, s.winH-s.bottomH+26, ">> " + ui.statusMsg, rgb(255,230,120), maxW);
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


