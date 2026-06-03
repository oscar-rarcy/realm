#include "render/sdl/sdl_hud.h"
#include "realm.h"
#include "core/build_service.h"
#include "core/production_service.h"
#include "core/world_index.h"
#include "view_state.h"

int mobileButtonH() {
    return std::max(44, (int)std::lround(48.0f * s.mobileUiScale));
}

void drawButton(const MobileButton& b, bool active, bool danger) {
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

void drawConsoleButton(const MobileButton& b, bool active, bool danger) {
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

void addGridButtons(std::vector<MobileButton>& out, int x, int y, int w,
                           const std::vector<std::pair<std::string, std::string>>& items,
                           int cols) {
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

namespace {

constexpr int kMobileBuildButtonsPerPage = 5;

std::string mobileEntityButtonId(const char* prefix, EntityType type) {
    return std::string(prefix) + std::to_string((int)type);
}

std::vector<std::pair<std::string, std::string>> mobileBuildButtonsForPage(int page) {
    std::vector<std::pair<std::string, std::string>> options;
    int count = 0;
    const BuildRule* rules = buildRules(count);
    for (int i = 0; i < count; i++) {
        if (rules[i].menuHotkey == '\0') continue;
        options.push_back({ mobileEntityButtonId("build:", rules[i].buildingType), STATS[rules[i].buildingType].name });
    }

    int totalPages = std::max(1, (int)((options.size() + kMobileBuildButtonsPerPage - 1) / kMobileBuildButtonsPerPage));
    page = std::max(0, std::min(page, totalPages - 1));
    int start = page * kMobileBuildButtonsPerPage;
    int end = std::min((int)options.size(), start + kMobileBuildButtonsPerPage);

    std::vector<std::pair<std::string, std::string>> pageButtons(options.begin() + start, options.begin() + end);
    pageButtons.push_back({ "cancel", "Cancel" });
    if (page > 0) pageButtons.push_back({ "buildback", "Back" });
    if (end < (int)options.size()) pageButtons.push_back({ "buildmore", "More" });
    return pageButtons;
}

std::vector<std::pair<std::string, std::string>> mobileTrainButtonsFor(EntityType producer) {
    std::vector<std::pair<std::string, std::string>> options;
    const ProductionRule* rule = productionRule(producer);
    if (!rule) return options;
    for (int i = 0; i < rule->allowedCount; i++) {
        EntityType unit = rule->allowedUnits[i];
        options.push_back({ mobileEntityButtonId("train:", unit), std::string("Train ") + STATS[unit].name });
    }
    return options;
}

} // namespace

std::string mobileSelectionSummary(const WorldIndex& world) {
    if (!g.local.selectedIds.empty()) {
        int idle = 0, gathering = 0, military = 0;
        for (int id : g.local.selectedIds) {
            Entity* e = renderFindEntity(g, world, id);
            if (!e || !e->alive) continue;
            if (e->state == S_IDLE) idle++;
            if (e->state == S_GATHERING) gathering++;
            if (isMilitary(e->type)) military++;
        }
        std::ostringstream ss;
        ss << g.local.selectedIds.size() << " units";
        if (idle || gathering || military) ss << "  " << idle << " idle  " << gathering << " gathering";
        return ss.str();
    }
    Entity* sel = renderFindEntity(g, world, g.local.selectedId);
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

bool mobileHasSelectedWorker(const WorldIndex& world) {
    if (!g.local.selectedIds.empty()) {
        for (int id : g.local.selectedIds) {
            Entity* e = renderFindEntity(g, world, id);
            if (e && e->alive && e->owner == 0 && canBuild(e->type)) return true;
        }
        return false;
    }
    Entity* e = renderFindEntity(g, world, g.local.selectedId);
    return e && e->alive && e->owner == 0 && canBuild(e->type);
}

bool mobileHasSelectedMilitary(const WorldIndex& world) {
    if (!g.local.selectedIds.empty()) {
        for (int id : g.local.selectedIds) {
            Entity* e = renderFindEntity(g, world, id);
            if (e && e->alive && e->owner == 0 && isMilitary(e->type)) return true;
        }
        return false;
    }
    Entity* e = renderFindEntity(g, world, g.local.selectedId);
    return e && e->alive && e->owner == 0 && isMilitary(e->type);
}

std::vector<MobileButton> mobileHudButtons(const WorldIndex& world) {
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
        cmd = mobileBuildButtonsForPage(s.mobileBuildPage);
    } else {
        Entity* sel = renderFindEntity(g, world, g.local.selectedId);
        if (mobileHasSelectedWorker(world)) {
            cmd = {{"move", "Move"}, {"gather", "Gather"}, {"build", "Build"}, {"stop", "Stop"}};
        } else if (mobileHasSelectedMilitary(world)) {
            cmd = {{"move", "Move"}, {"attack", "Attack"}, {"attackmove", "Attack Move"}, {"stop", "Stop"}};
        } else if (sel && sel->owner == 0 && isBuilding(sel->type) && !sel->underConstruction) {
            std::vector<std::pair<std::string, std::string>> trainButtons = mobileTrainButtonsFor(sel->type);
            cmd.insert(cmd.end(), trainButtons.begin(), trainButtons.end());
            if (productionRule(sel->type)) cmd.push_back({"rally", "Rally"});
            if (sel->type == E_MARKET) cmd.push_back({"trade", "Trade"});
            if (sel->type == E_BLACKSMITH) cmd.push_back({"research", "Research"});
            if (productionRule(sel->type)) cmd.push_back({"cancelqueue", "Cancel Queue"});
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

void drawMobileHud(const WorldIndex& world) {
    SDL_Rect pr = panelRect();
    int pad = mobileSafePad();
    setDraw(rgb(8,10,14)); SDL_RenderFillRect(s.ren, &pr);
    setDraw(rgb(68,82,94)); SDL_RenderDrawRect(s.ren, &pr);

    mobileDrawResources(pr.x + pad, pr.y + pad, std::max(1, pr.w - pad * 2));
    int summaryY = pr.y + pad + 28;
    SDL_Rect mm = miniMapRect();
    int summaryW = mobilePortrait() ? std::max(1, mm.x - (pr.x + pad) - 10) : std::max(1, pr.w - pad * 2);
    drawTextFit(pr.x + pad, summaryY, mobileSelectionSummary(world), rgb(255,230,135), summaryW);
    if (s.mobileBuildType != E_NONE) {
        drawTextFit(pr.x + pad, summaryY + 22,
                    std::string("Placing ") + STATS[s.mobileBuildType].name + " - tap a valid tile",
                    rgb(145,220,245), summaryW);
    } else if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE || g.mode == M_BUILD_SELECT || g.mode == M_BUILD_PLACE || g.mode == M_PATROL_SET || g.mode == M_MARKET_TRADE) {
        drawTextFit(pr.x + pad, summaryY + 22, modeName(g.mode), rgb(145,220,245), summaryW);
    } else if (ui.statusTimer > 0) {
        drawTextFit(pr.x + pad, summaryY + 22, ui.statusMsg, rgb(255,230,120), summaryW);
    }

    drawMiniMap(world, mm.x, mm.y, mm.w, mm.h);
    for (const MobileButton& b : mobileHudButtons(world)) {
        bool active = (b.id == "build" && (g.mode == M_BUILD_SELECT || g.mode == M_BUILD_PLACE))
                   || (b.id == "attack" && g.mode == M_ATTACK_MOVE)
                   || (b.id == "rally" && g.mode == M_RALLY_SET);
        drawButton(b, active, b.id == "cancel");
    }
}

