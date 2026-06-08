#include "render/sdl/sdl_splash.h"
#include "realm.h"
#include "core/world_index.h"
#include "view_state.h"

#include <cstdlib>

namespace {

constexpr int LAB_X = MAP_W / 2;
constexpr int LAB_Y = MAP_H / 2;
constexpr int LAB_TERRAIN_BULLSEYE = (int)T_CASTLE_GATE + 1;

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
    std::string forcedAction;
    int forcedFrameCount = 0;
    int hue = 197;
    int speedPercent = 100;
    bool playing = true;
    bool damaged = false;
    int activeDropdown = 0;
    int dropdownScroll = 0;
    bool hueDragging = false;
};

void clampLabState(LabState& lab);
bool saveLabShot(const LabState& lab, const std::filesystem::path& path);

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

std::vector<EntityType> labEntityTypes() {
    std::vector<EntityType> out;
    out.push_back(E_NONE);
    for (int t = E_PEASANT; t < E_TYPE_COUNT; ++t) out.push_back((EntityType)t);
    return out;
}

SDL_Color hueColor(int hue) {
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

Color colorFromSdl(SDL_Color c, int alpha = 255) {
    return rgb(c.r, c.g, c.b, alpha);
}

const char* directionId(int direction) {
    return direction == 1 ? "back" : "front";
}

const char* seasonNameValue(int season) {
    static const char* names[] = {"Spring", "Summer", "Autumn", "Winter"};
    return names[((season % 4) + 4) % 4];
}

const char* timeStepName(int step) {
    static const char* names[] = {"Night", "Dawn", "Morning", "Noon", "Dusk", "Late dusk"};
    return names[std::max(0, std::min(step, 5))];
}

std::string labEntityName(EntityType type) {
    if (type == E_NONE) return "None";
    return STATS[type].name;
}

const EntityActionAnimationSpec* labActionSpec(const LabState& lab) {
    EntityType type = (EntityType)lab.entityType;
    int count = entityActionAnimationSpecCount(type);
    if (count <= 0) return nullptr;
    int index = std::max(0, std::min(lab.actionIndex, count - 1));
    return entityActionAnimationSpecAt(type, index);
}

const char* labActionId(const LabState& lab) {
    if (!lab.forcedAction.empty()) return lab.forcedAction.c_str();
    if (const EntityActionAnimationSpec* spec = labActionSpec(lab)) return spec->action;
    return "idle";
}

int labFrameCount(const LabState& lab) {
    if (lab.forcedFrameCount > 0) return lab.forcedFrameCount;
    if (const EntityActionAnimationSpec* spec = labActionSpec(lab)) return std::max(1, spec->frameCount);
    return 1;
}

std::filesystem::path labManualShotPath(const LabState& lab) {
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

const char* terrainNameSafe(int terrain) {
    if (terrain == LAB_TERRAIN_BULLSEYE) return "Lab bullseye";
    return terrainName((Terrain)std::max(0, std::min(terrain, (int)T_CASTLE_GATE)));
}

const char* biomeNameSafe(int biome) {
    return biomeName((Biome)std::max(0, std::min(biome, (int)B_OCEAN)));
}

const char* weatherNameSafe(int weather) {
    switch ((Weather)weather) {
        case W_RAIN: return "Rain";
        case W_STORM: return "Storm";
        case W_SNOW: return "Snow";
        default: return "Clear";
    }
}

const char* previewModeName(int mode) {
    switch (mode) {
        case 0: return "Tile only";
        case 1: return "Entity only";
        default: return "Tile + Entity";
    }
}

int labEntitySpriteSize() {
    int size = 128;
    if (const char* raw = std::getenv("REALM_LAB_ENTITY_SPRITE_SIZE")) {
        char* end = nullptr;
        long parsed = std::strtol(raw, &end, 10);
        if (end && *end == '\0') size = (int)parsed;
    }
    return std::max(32, std::min(512, size));
}

const char* fogName(int fog) {
    switch (fog) {
        case 1: return "Explored";
        case 2: return "Unexplored";
        default: return "Visible";
    }
}

const char* lightName(int mode) {
    switch (mode) {
        case 1: return "Town Hall torch";
        case 2: return "Tower torch";
        case 3: return "Custom candle";
        default: return "None";
    }
}

std::vector<LabDropdownOption> labDropdownOptions(int kind, const LabState& lab) {
    std::vector<LabDropdownOption> out;
    switch (kind) {
        case LAB_DD_PREVIEW:
            out.push_back({"Tile only", 0});
            out.push_back({"Entity only", 1});
            out.push_back({"Tile + Entity", 2});
            break;
        case LAB_DD_TERRAIN:
            for (int t = 0; t <= (int)T_CASTLE_GATE; ++t) out.push_back({terrainNameSafe(t), t});
            out.push_back({terrainNameSafe(LAB_TERRAIN_BULLSEYE), LAB_TERRAIN_BULLSEYE});
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

float labTimePhase(int step) {
    static const float phases[] = {0.02f, 0.16f, 0.28f, 0.50f, 0.72f, 0.84f};
    return phases[std::max(0, std::min(step, 5))];
}

void labConfigureEntityForAction(Entity& e, const char* action) {
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

void labApplyWorld(const LabState& lab) {
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
            t.terrain = lab.terrain == LAB_TERRAIN_BULLSEYE ? T_GRASS : (Terrain)lab.terrain;
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
        labLightOverride.strength = 0.65f;
        labLightOverride.radius = 2.20f;
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

bool labBullseyeTerrain(const LabState& lab) {
    return lab.terrain == LAB_TERRAIN_BULLSEYE;
}

void drawHueWheel(int cx, int cy, int radius, int hue) {
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

void drawLabLine(int x, int& y, const std::string& text, Color c = rgb(220,225,220)) {
    drawTextFit(x, y, text, c, 330, s.monoSmall ? s.monoSmall : s.mono);
    y += 19;
}

LabControlLayout labBuildControls(const LabState& lab) {
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

const LabDropdownControl* labFindControl(const LabControlLayout& layout, int kind) {
    for (const LabDropdownControl& control : layout.dropdowns) {
        if (control.kind == kind) return &control;
    }
    return nullptr;
}

int labDropdownSelectedValue(int kind, const LabState& lab) {
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

void labSetDropdownValue(LabState& lab, int kind, int value) {
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

void labOpenDropdown(LabState& lab, int kind) {
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

void labSetHueFromPoint(LabState& lab, int mx, int my) {
    SDL_Rect r = labBuildControls(lab).hueWheel;
    int cx = r.x + r.w / 2;
    int cy = r.y + r.h / 2;
    float angle = std::atan2((float)(my - cy), (float)(mx - cx)) * 180.0f / 3.14159265f;
    lab.hue = ((int)std::lround(angle) + 360) % 360;
}

void clampLabState(LabState& lab);

SDL_Rect labDropdownPopupRect(const LabState& lab, const LabControlLayout& layout, int& optionH, int& visibleCount) {
    optionH = 24;
    visibleCount = 0;
    const LabDropdownControl* control = labFindControl(layout, lab.activeDropdown);
    if (!control) return SDL_Rect{0,0,0,0};
    int count = (int)labDropdownOptions(lab.activeDropdown, lab).size();
    visibleCount = std::min(count, std::max(3, std::min(12, (s.winH - control->r.y - control->r.h - 18) / optionH)));
    int h = std::max(optionH, visibleCount * optionH + 2);
    return SDL_Rect{control->r.x, control->r.y + control->r.h + 2, control->r.w, h};
}

void drawLabDropdownControl(const LabDropdownControl& control, bool open) {
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

void drawLabButtonControl(const LabButtonControl& button) {
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

void drawLabDropdownPopup(const LabState& lab) {
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

bool labHandleMouseDown(LabState& lab, int mx, int my) {
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

void drawLabPreview(const LabState& lab, SDL_Rect area, TilesetAssetFrame& assetFrame) {
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
        if (labBullseyeTerrain(lab)) {
            fillDiamond(cx, cy, hw, hh, rgb(236, 230, 198, 255));
            fillDiamond(cx, cy, std::max(1, (int)std::lround(hw * 0.78f)),
                        std::max(1, (int)std::lround(hh * 0.78f)), rgb(176, 36, 35, 255));
            fillDiamond(cx, cy, std::max(1, (int)std::lround(hw * 0.55f)),
                        std::max(1, (int)std::lround(hh * 0.55f)), rgb(236, 230, 198, 255));
            fillDiamond(cx, cy, std::max(1, (int)std::lround(hw * 0.32f)),
                        std::max(1, (int)std::lround(hh * 0.32f)), rgb(176, 36, 35, 255));
            fillDiamond(cx, cy, std::max(3, (int)std::lround(hw * 0.08f)),
                        std::max(2, (int)std::lround(hh * 0.08f)), rgb(32, 36, 42, 255));
            SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
            setDraw(rgb(32, 36, 42, 210));
            SDL_RenderDrawLine(s.ren, cx - hw, cy, cx + hw, cy);
            SDL_RenderDrawLine(s.ren, cx, cy - hh, cx, cy + hh);
        } else {
            applyTerrainTextureIso(cx, cy, hw, hh, tile, LAB_X, LAB_Y);
        }
        drawDiamondOutline(cx, cy, hw, hh, labBullseyeTerrain(lab) ? rgb(32,36,42,230) : rgb(245,235,150,210));
    } else {
        SDL_Rect empty{cx - 72, cy - 72, 144, 144};
        setDraw(rgb(20,24,30,210));
        SDL_RenderFillRect(s.ren, &empty);
        setDraw(rgb(110,120,132,180));
        SDL_RenderDrawRect(s.ren, &empty);
    }

    WorldIndex world = buildWorldIndex(g);
    Entity* ent = renderEntityAt(g, world, LAB_X, LAB_Y);
    if (lab.previewMode != 0 && ent) {
        int spriteSize = labEntitySpriteSize();
        SDL_Rect dst{cx - spriteSize / 2, cy - spriteSize / 2, spriteSize, spriteSize};
        SDL_Color team = hueColor(lab.hue);
        Color mod = applyVisionToGlyph(rgb(255,255,255), LAB_X, LAB_Y);
        if (!drawEntityImageAtAnchor(g, world, *ent, cx, cy + hh, spriteSize, spriteSize, mod,
                                     labActionId(lab), directionId(lab.direction),
                                     lab.frame, team, &assetFrame, nullptr)) {
            bool usesSymbolFont = false;
            drawCentered(tilesetEntityVisual(g, world, *ent, usesSymbolFont), dst, rgb(255,255,255),
                         usesSymbolFont, usesSymbolFont);
        }
    } else if (lab.previewMode == 1) {
        drawTextFit(cx - 82, cy - 8, "No entity selected", rgb(150,160,168), 164);
    }
}

void drawLabFrame(const LabState& lab, bool present) {
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
    if (!lab.forcedAction.empty()) {
        drawLabLine(rx, ry, "id: " + lab.forcedAction);
        drawLabLine(rx, ry, "forced asset action for tileset QA");
        drawLabLine(rx, ry, "frames: " + std::to_string(labFrameCount(lab)));
    } else if (const EntityActionAnimationSpec* spec = labActionSpec(lab)) {
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
    WorldIndex world = buildWorldIndex(g);
    TerminalCell cell = terminalMapCell(world, LAB_X, LAB_Y);
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

void clampLabState(LabState& lab) {
    lab.previewMode = (lab.previewMode + 3) % 3;
    lab.terrain = (lab.terrain + LAB_TERRAIN_BULLSEYE + 1) % (LAB_TERRAIN_BULLSEYE + 1);
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

void stepLabEntity(LabState& lab, int delta) {
    std::vector<EntityType> types = labEntityTypes();
    int idx = 0;
    for (int i = 0; i < (int)types.size(); ++i) if (types[i] == lab.entityType) idx = i;
    idx = (idx + delta + (int)types.size()) % (int)types.size();
    lab.entityType = types[idx];
    lab.actionIndex = 0;
    lab.frame = 0;
}

bool saveLabShot(const LabState& lab, const std::filesystem::path& path) {
    drawLabFrame(lab, false);
    return saveRendererPixels(path.string());
}

int runLabSmoke() {
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

    lab.previewMode = 2;
    lab.terrain = LAB_TERRAIN_BULLSEYE;
    lab.entityType = E_PEASANT;
    lab.actionIndex = 0;
    lab.direction = 0;
    lab.frame = 0;
    lab.timeStep = 3;
    lab.lightMode = 0;
    lab.weather = W_CLEAR;
    ok = saveLabShot(lab, outDir / "07-bullseye-peasant-anchor.bmp") && ok;

    lab.previewMode = 2;
    lab.terrain = LAB_TERRAIN_BULLSEYE;
    lab.entityType = E_TOWNHALL;
    lab.actionIndex = 0;
    lab.direction = 0;
    lab.frame = 0;
    lab.timeStep = 3;
    lab.lightMode = 0;
    lab.weather = W_CLEAR;
    ok = saveLabShot(lab, outDir / "08-bullseye-town-hall-footprint.bmp") && ok;

    auto saveForcedActionShot = [&](EntityType type, const char* action, const char* name, int direction, int frame, int spriteSize) {
        LabState shot;
        shot.previewMode = 2;
        shot.terrain = LAB_TERRAIN_BULLSEYE;
        shot.entityType = type;
        shot.forcedAction = action ? action : "";
        shot.forcedFrameCount = 2;
        shot.direction = direction;
        shot.frame = frame;
        shot.timeStep = 3;
#ifdef _WIN32
        _putenv_s("REALM_LAB_ENTITY_SPRITE_SIZE", std::to_string(spriteSize).c_str());
#else
        setenv("REALM_LAB_ENTITY_SPRITE_SIZE", std::to_string(spriteSize).c_str(), 1);
#endif
        return saveLabShot(shot, outDir / name);
    };

    ok = saveForcedActionShot(E_BOAR, "charge", "09-boar-charge-front.bmp", 0, 0, 180) && ok;
    ok = saveForcedActionShot(E_BOAR, "death", "10-boar-death-skeleton-front.bmp", 0, 3, 180) && ok;
    ok = saveForcedActionShot(E_SPEARMAN, "pike__idle", "11-spearman-pike-idle-front.bmp", 0, 0, 220) && ok;
    ok = saveForcedActionShot(E_SPEARMAN, "pike__spear_thrust", "12-spearman-pike-thrust-front.bmp", 0, 0, 220) && ok;
    ok = saveForcedActionShot(E_KNIGHT, "iron_weapons__open_helmet__idle", "13-knight-iron-lance-idle-front.bmp", 0, 0, 220) && ok;
    ok = saveForcedActionShot(E_KNIGHT, "iron_weapons__open_helmet__charge_strike", "14-knight-iron-lance-charge-front.bmp", 0, 0, 220) && ok;
    ok = saveForcedActionShot(E_KNIGHT, "iron_weapons__plate_helm__idle", "15-knight-plate-lance-idle-front.bmp", 0, 0, 220) && ok;
    ok = saveForcedActionShot(E_KNIGHT, "iron_weapons__plate_helm__charge_strike", "16-knight-plate-lance-charge-front.bmp", 0, 0, 220) && ok;

    std::cerr << "realm: lab smoke " << (ok ? "complete" : "failed")
              << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
}

void drawGrassMapLabFrame(float cameraX, float cameraY, int tilePx, bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    setDraw(rgb(3,5,8));
    SDL_RenderClear(s.ren);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);

    int hw = std::max(8, tilePx);
    int hh = std::max(5, tilePx / 2);
    int centerX = s.winW / 2;
    int centerY = s.winH / 2;
    int radius = std::max(12, (s.winW + s.winH) / std::max(1, tilePx) + 6);
    int baseX = (int)std::floor(cameraX);
    int baseY = (int)std::floor(cameraY);
    TilesetAssetFrame frame = tilesetLoadGroundTileIso(s.ren, G_GRASS, hw * 2 + 1, hh * 2 + 1);

    for (int sum = -radius * 2; sum <= radius * 2; ++sum) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int dy = sum - dx;
            if (dy < -radius || dy > radius) continue;
            int tx = baseX + dx;
            int ty = baseY + dy;
            int sx = centerX + (int)std::lround(((float)tx - cameraX - ((float)ty - cameraY)) * hw);
            int sy = centerY + (int)std::lround(((float)tx - cameraX + ((float)ty - cameraY)) * hh);
            if (sx + hw < -4 || sx - hw > s.winW + 4 || sy + hh < -4 || sy - hh > s.winH + 4) continue;
            if (frame.texture && !frame.placeholder) {
                SDL_Rect dst{sx - hw, sy - hh, hw * 2 + 1, hh * 2 + 1};
                SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
                SDL_SetTextureAlphaMod(frame.texture, 255);
                SDL_RenderCopy(s.ren, frame.texture, nullptr, &dst);
            } else {
                fillDiamond(sx, sy, hw, hh, rgb(92, 126, 45));
                drawDiamondOutline(sx, sy, hw, hh, rgb(30, 38, 24, 180));
            }
        }
    }

    SDL_Rect label{14, 12, 420, 54};
    setDraw(rgb(3, 5, 8, 205));
    SDL_RenderFillRect(s.ren, &label);
    setDraw(rgb(130, 150, 165, 170));
    SDL_RenderDrawRect(s.ren, &label);
    drawText(26, 22, "Grass map preview", rgb(255,235,145));
    drawText(26, 44, "Wheel zoom / drag pan / S screenshot / Esc quit", rgb(190,205,214));

    if (present) SDL_RenderPresent(s.ren);
}

bool saveGrassMapLabShot(float cameraX, float cameraY, int tilePx) {
    namespace fs = std::filesystem;
    fs::path outDir = fs::path("build") / "lab-screenshots";
    fs::create_directories(outDir);
    drawGrassMapLabFrame(cameraX, cameraY, tilePx, false);
    return saveRendererPixels((outDir / "grass-map-preview.bmp").string());
}

int runGrassMapLab() {
    std::cerr << "realm: grass map lab started\n";
    gfxSetWindowSizeForTest(1280, 820);
    float cameraX = 0.0f;
    float cameraY = 0.0f;
    int tilePx = 34;
    if (const char* raw = std::getenv("REALM_TILESET_MAP_ZOOM")) {
        char* end = nullptr;
        long parsed = std::strtol(raw, &end, 10);
        if (end && *end == '\0') tilePx = std::max(12, std::min(96, (int)parsed));
    }
    if (std::getenv("REALM_LAB_GRASS_MAP_SMOKE")) {
        bool ok = saveGrassMapLabShot(cameraX, cameraY, tilePx);
        std::cerr << "realm: grass map lab smoke " << (ok ? "complete" : "failed") << "\n";
        return ok ? 0 : 1;
    }
    bool dragging = false;
    int lastX = 0;
    int lastY = 0;
    bool quit = false;
    while (!quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) quit = true;
            else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                s.winW = e.window.data1;
                s.winH = e.window.data2;
            } else if (e.type == SDL_MOUSEWHEEL) {
                int steps = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
                tilePx = std::max(12, std::min(96, tilePx + steps * 3));
            } else if (e.type == SDL_MOUSEBUTTONDOWN && (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_MIDDLE)) {
                dragging = true;
                lastX = e.button.x;
                lastY = e.button.y;
            } else if (e.type == SDL_MOUSEBUTTONUP && (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_MIDDLE)) {
                dragging = false;
            } else if (e.type == SDL_MOUSEMOTION && dragging) {
                int dx = e.motion.x - lastX;
                int dy = e.motion.y - lastY;
                int hw = std::max(8, tilePx);
                int hh = std::max(5, tilePx / 2);
                cameraX -= dx / (2.0f * hw) + dy / (2.0f * hh);
                cameraY += dx / (2.0f * hw) - dy / (2.0f * hh);
                lastX = e.motion.x;
                lastY = e.motion.y;
            } else if (e.type == SDL_KEYDOWN) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) quit = true;
                else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) tilePx = std::min(96, tilePx + 3);
                else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) tilePx = std::max(12, tilePx - 3);
                else if (k == SDLK_LEFT) cameraX -= 1.0f;
                else if (k == SDLK_RIGHT) cameraX += 1.0f;
                else if (k == SDLK_UP) cameraY -= 1.0f;
                else if (k == SDLK_DOWN) cameraY += 1.0f;
                else if (k == SDLK_s) {
                    bool ok = saveGrassMapLabShot(cameraX, cameraY, tilePx);
                    std::cerr << "realm: grass map lab screenshot " << (ok ? "ok" : "failed") << "\n";
                }
            }
        }
        drawGrassMapLabFrame(cameraX, cameraY, tilePx, true);
        SDL_Delay(16);
    }
    return 0;
}

} // namespace

int gfxRunTilesetLab() {
    std::cerr << "realm: lab started\n";
    labForcesImageTileset = true;
    displayMode = DM_EMOJI;
    gfxSetProjection(true);
    initGameWithSeed(0, 2468, 0);
    view.viewX = LAB_X - 8;
    view.viewY = LAB_Y - 8;
    gfxSetZoomForTest(34);

    if (std::getenv("REALM_LAB_GRASS_MAP")) return runGrassMapLab();
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
