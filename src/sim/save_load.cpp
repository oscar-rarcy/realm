#include "realm.h"
#include "entity_animation.h"
#include "sim/save_migration.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <system_error>

static void writeIntVec(std::ostream& os, const std::vector<int>& v) {
    os << v.size();
    for (int x : v) os << ' ' << x;
}

static bool readIntVec(std::istream& is, std::vector<int>& v) {
    size_t n = 0;
    if (!(is >> n)) return false;
    v.clear();
    v.reserve(n);
    for (size_t i = 0; i < n; i++) {
        int x = 0;
        if (!(is >> x)) return false;
        v.push_back(x);
    }
    return true;
}

static std::string lowerAssetSlug(const std::string& text) {
    std::string out;
    bool underscore = false;
    for (unsigned char raw : text) {
        char ch = (char)std::tolower(raw);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            out.push_back(ch);
            underscore = false;
        } else if (!underscore && !out.empty()) {
            out.push_back('_');
            underscore = true;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "unknown" : out;
}

int dumpMissingTilesetAssets() {
    namespace fs = std::filesystem;
    int missing = 0;
    auto report = [&](const std::string& kind, const std::string& key, const fs::path& path) {
        if (fs::exists(path)) return;
        std::cout << kind << " | " << key << " | missing=" << path.generic_string() << "\n";
        missing++;
    };

    bool sawGround[32] = {};
    bool sawFeature[32] = {};
    bool sawDecal[32] = {};
    for (int t = T_GRASS; t <= T_CASTLE_GATE; ++t) {
        Tile tile{};
        tile.terrain = (Terrain)t;
        tile.biome = (t == T_SNOW || t == T_PINE) ? B_SNOW : B_TEMPERATE;
        tile.resources = 100;
        VisualTileParts parts = visualPartsForTile(tile);
        if (!sawGround[(int)parts.ground]) {
            sawGround[(int)parts.ground] = true;
            std::string name = groundTypeName(parts.ground);
            report("ground", name, fs::path("assets") / "tiles" / "grounds" / (name + ".png"));
        }
        if (parts.feature != F_NONE && !sawFeature[(int)parts.feature]) {
            sawFeature[(int)parts.feature] = true;
            std::string name = featureTypeName(parts.feature);
            report("feature", name, fs::path("assets") / "tiles" / "features" / name / "manifest.json");
        }
        for (VisualDecalType decal : parts.decals) {
            if (sawDecal[(int)decal]) continue;
            sawDecal[(int)decal] = true;
            std::string name = visualDecalName(decal);
            report("decal", name, fs::path("assets") / "tiles" / "decals" / (name + ".png"));
        }
    }

    const char* effects[] = {
        "arrow_projectile", "tower_bolt_projectile", "warship_shot_projectile",
        "catapult_boulder_projectile", "melee_hit_spark", "arrow_hit",
        "boulder_impact", "boulder_water_splash", "building_hit_dust",
        "rain_frame_1", "rain_frame_2", "storm_rain_frame_1", "storm_rain_frame_2",
        "snowfall_frame_1", "snowfall_frame_2", "move_marker", "attack_marker",
        "gather_marker", "build_marker", "rally_marker", "attack_move_marker",
        "hold_position_marker", "selection_ring", "group_selection_ring",
        "range_ring_dot", "build_preview_valid", "build_preview_invalid",
        "wall_preview", "garrison_indicator", "queued_unit_marker",
        "research_active_marker", "completed_research_icon_treatment"
    };
    for (const char* effect : effects)
        report("effect-ui", effect, fs::path("assets") / "tiles" / "effects-ui" / (std::string(effect) + ".png"));

    for (int type = E_PEASANT; type <= E_BOAR; ++type) {
        EntityType entityType = (EntityType)type;
        std::string slug = lowerAssetSlug(STATS[entityType].name ? STATS[entityType].name : "unknown");
        int count = entityActionAnimationSpecCount(entityType);
        for (int i = 0; i < count; ++i) {
            const EntityActionAnimationSpec* spec = entityActionAnimationSpecAt(entityType, i);
            if (!spec || !spec->action) continue;
            const char* dirs[] = {"front", "back"};
            for (const char* dir : dirs) {
                for (int f = 0; f < std::max(1, spec->frameCount); ++f) {
                    std::ostringstream frame;
                    frame << "frame_" << (f < 10 ? "0" : "") << f << "_base.png";
                    fs::path path = fs::path("assets") / "tiles" / "entities" / slug
                        / spec->action / dir / frame.str();
                    report("entity", slug + "/" + spec->action + "/" + dir, path);
                }
            }
        }
    }

    std::cout << "missing_tileset_assets=" << missing << "\n";
    return 0;
}

bool saveGame(const std::string& path) {
    std::filesystem::path finalPath(path);
    std::filesystem::path tmpPath = finalPath;
    tmpPath.replace_extension(tmpPath.extension().string() + ".tmp");
    std::ofstream os(tmpPath);
    if (!os) return false;
    os << std::setprecision(std::numeric_limits<float>::max_digits10);
    os << "REALM_SAVE " << REALM_SAVE_VERSION << "\n";
    os << "META " << g.seed << ' ' << g.startupAIs << ' ' << g.humanCorner << ' '
       << g.matchNumber << ' ' << g.biomeChoice << ' ' << g.tick << ' '
       << (int)g.mode << ' ' << g.selectedId << ' ' << g.winner << ' ' << g.aiTimer << ' ' << g.farmTimer << ' '
       << g.animalTimer << ' '
       << g.dayPhase << ' ' << g.seasonPhase << ' ' << g.prevSeason << ' '
       << g.weather << ' ' << g.weatherTimer << ' ' << g.prevTimePhase << ' ' << g.attackNotifyCd << ' '
       << g.nextId << ' ' << g.rngState << ' '
       << (g.returnToMenu ? 1 : 0) << ' ' << (g.diagnostics ? 1 : 0) << ' '
       << (g.helpOverlay ? 1 : 0) << "\n";
    for (int p = 0; p <= MAX_PLAYERS; p++) {
        const Player& pl = g.players[p];
        os << "PLAYER " << p << ' ' << pl.gold << ' ' << pl.wood << ' ' << pl.food << ' '
           << pl.supply << ' ' << pl.supplyMax << ' ' << (pl.alive ? 1 : 0) << ' '
           << pl.research << ' ' << pl.aiWaveCd << "\n";
    }
    os << "SELECTED ";
    writeIntVec(os, g.selectedIds);
    os << "\n";
    for (int i = 0; i < 9; i++) {
        os << "GROUP " << i << ' ';
        writeIntVec(os, g.controlGroups[i]);
        os << "\n";
    }
    os << "MAP " << MAP_W << ' ' << MAP_H << "\n";
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        const Tile& t = g.map[y][x];
        os << "TILE " << x << ' ' << y << ' ' << (int)t.terrain << ' ' << t.resources << ' '
           << (int)t.biome << ' ' << (int)t.preWinterTerrain << ' ' << t.wear;
        for (int p = 0; p < MAX_PLAYERS; p++) os << ' ' << (t.visible[p] ? 1 : 0);
        for (int p = 0; p < MAX_PLAYERS; p++) os << ' ' << (t.explored[p] ? 1 : 0);
        os << "\n";
    }
    os << "ENTITIES " << g.entities.size() << "\n";
    for (const Entity& e : g.entities) {
        os << "ENTITY " << e.id << ' ' << (int)e.type << ' ' << e.owner << ' ' << e.x << ' ' << e.y
           << ' ' << e.hp << ' ' << e.maxHp << ' ' << (int)e.state << ' ' << e.targetId
           << ' ' << e.targetX << ' ' << e.targetY << ' ' << e.pathIdx << ' ' << e.moveCd
           << ' ' << e.atkCd << ' ' << e.gatherCd << ' ' << (int)e.cargo.type << ' '
           << e.cargo.amount << ' ' << e.cargo.sourceX << ' ' << e.cargo.sourceY << ' '
           << (int)e.producing << ' ' << e.trainProgress << ' ' << e.trainTime << ' '
           << e.researchProgress << ' ' << e.researchTime << ' '
           << (e.underConstruction ? 1 : 0) << ' ' << (e.alive ? 1 : 0) << ' '
           << e.rallyX << ' ' << e.rallyY << ' ' << e.resourceX << ' ' << e.resourceY << ' '
           << e.storedFood << ' ' << e.stuckTicks << ' '
           << e.alertTicks << ' ' << e.deathTicks << ' '
           << e.carcassFoodRemaining << ' ' << e.carcassFoodMax << ' '
           << e.rallySet << ' ' << e.researching << ' '
           << e.attackMove << ' ' << e.holdPosition << ' ' << e.facingDx << ' ' << e.facingDy << ' '
           << (e.gateOpen ? 1 : 0) << ' '
           << (e.gateLocked ? 1 : 0) << ' '
           << e.convertTicks << ' ' << e.retreating << ' ' << e.packed << ' ' << e.packTicks;
        os << " PATH " << e.path.size();
        for (auto pt : e.path) os << ' ' << pt.first << ' ' << pt.second;
        os << " QUEUE ";
        writeIntVec(os, e.queue);
        os << " GARRISON ";
        writeIntVec(os, e.garrison);
        os << "\n";
    }
    os << "PROJECTILES " << g.projectiles.size() << "\n";
    for (const Projectile& p : g.projectiles) {
        os << "PROJECTILE " << p.x << ' ' << p.y << ' ' << p.tx << ' ' << p.ty << ' '
           << (int)p.glyph << ' ' << p.color << ' ' << p.life << ' ' << (p.alive ? 1 : 0) << "\n";
    }
    os << "MARKERS " << g.actionMarkers.size() << "\n";
    for (const ActionMarker& m : g.actionMarkers)
        os << "MARKER " << m.x << ' ' << m.y << ' ' << m.ticks << ' ' << (int)m.glyph << "\n";
    os.flush();
    if (!os) return false;
    os.close();
    if (!os) return false;
    std::error_code ec;
    if (std::filesystem::exists(finalPath, ec)) std::filesystem::remove(finalPath, ec);
    ec = {};
    std::filesystem::rename(tmpPath, finalPath, ec);
    return !ec;
}

bool loadGame(const std::string& path) {
    std::ifstream is(path);
    if (!is) return false;
    std::string tag;
    int version = 0;
    if (!(is >> tag >> version) || tag != "REALM_SAVE" || !isSupportedSaveVersion(version)) return false;
    Game ng{};
    if (!(is >> tag) || tag != "META") return false;
    int mode = 0, ret = 0, diag = 0, help = 0;
    if (!(is >> ng.seed >> ng.startupAIs >> ng.humanCorner >> ng.matchNumber >> ng.biomeChoice
          >> ng.tick >> mode >> ng.selectedId >> ng.winner >> ng.aiTimer >> ng.farmTimer
          >> ng.animalTimer
          >> ng.dayPhase >> ng.seasonPhase >> ng.prevSeason >> ng.weather >> ng.weatherTimer)) return false;
    if (!(is >> ng.prevTimePhase >> ng.attackNotifyCd)) return false;
    if (!(is >> ng.nextId >> ng.rngState >> ret >> diag >> help)) return false;
    ng.mode = (GameMode)mode;
    ng.returnToMenu = ret != 0;
    ng.diagnostics = diag != 0;
    ng.helpOverlay = help != 0;
    for (int i = 0; i < 9; i++) ng.controlGroups[i].clear();
    for (int p = 0; p <= MAX_PLAYERS; p++) {
        int idx = -1, alive = 0;
        if (!(is >> tag) || tag != "PLAYER") return false;
        if (!(is >> idx)) return false;
        if (idx < 0 || idx > MAX_PLAYERS) return false;
        Player& pl = ng.players[idx];
        if (!(is >> pl.gold >> pl.wood >> pl.food >> pl.supply >> pl.supplyMax
              >> alive >> pl.research >> pl.aiWaveCd)) return false;
        pl.alive = alive != 0;
    }
    if (!(is >> tag) || tag != "SELECTED") return false;
    if (!readIntVec(is, ng.selectedIds)) return false;
    for (int i = 0; i < 9; i++) {
        int idx = -1;
        if (!(is >> tag) || tag != "GROUP") return false;
        if (!(is >> idx) || idx < 0 || idx >= 9) return false;
        if (!readIntVec(is, ng.controlGroups[idx])) return false;
    }
    int mw = 0, mh = 0;
    if (!(is >> tag >> mw >> mh) || tag != "MAP" || mw != MAP_W || mh != MAP_H) return false;
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        int x = 0, y = 0, ter = 0, biome = 0, pre = 0;
        if (!(is >> tag) || tag != "TILE") return false;
        Tile t{};
        if (!(is >> x >> y >> ter >> t.resources >> biome >> pre >> t.wear)) return false;
        if (!inBounds(x, y)) return false;
        t.terrain = (Terrain)ter;
        t.biome = (Biome)biome;
        t.preWinterTerrain = (Terrain)pre;
        for (int p = 0; p < MAX_PLAYERS; p++) { int v = 0; if (!(is >> v)) return false; t.visible[p] = v != 0; }
        for (int p = 0; p < MAX_PLAYERS; p++) { int v = 0; if (!(is >> v)) return false; t.explored[p] = v != 0; }
        ng.map[y][x] = t;
    }
    size_t n = 0;
    if (!(is >> tag >> n) || tag != "ENTITIES") return false;
    ng.entities.clear();
    for (size_t i = 0; i < n; i++) {
        if (!(is >> tag) || tag != "ENTITY") return false;
        Entity e{};
        int type = 0, state = 0, producing = 0, under = 0, alive = 0, gateOpen = 0, gateLocked = 0;
        int cargoType = 0;
        if (!(is >> e.id >> type >> e.owner >> e.x >> e.y >> e.hp >> e.maxHp >> state
              >> e.targetId >> e.targetX >> e.targetY >> e.pathIdx >> e.moveCd >> e.atkCd
              >> e.gatherCd >> cargoType >> e.cargo.amount >> e.cargo.sourceX >> e.cargo.sourceY
              >> producing >> e.trainProgress >> e.trainTime >> e.researchProgress >> e.researchTime
              >> under >> alive >> e.rallyX >> e.rallyY >> e.resourceX >> e.resourceY >> e.storedFood >> e.stuckTicks
              >> e.alertTicks)) return false;
        if (!(is >> e.deathTicks)) return false;
        if (!(is >> e.carcassFoodRemaining >> e.carcassFoodMax)) return false;
        if (!(is >> e.rallySet >> e.researching >> e.attackMove >> e.holdPosition)) return false;
        if (!(is >> e.facingDx >> e.facingDy)) return false;
        if (!(is >> gateOpen >> gateLocked)) return false;
        if (!(is >> e.convertTicks >> e.retreating >> e.packed >> e.packTicks)) return false;
        e.type = (EntityType)type;
        e.state = (EntityState)state;
        e.cargo.type = (CargoResource)cargoType;
        e.producing = (EntityType)producing;
        e.underConstruction = under != 0;
        e.alive = alive != 0;
        e.gateOpen = gateOpen != 0;
        e.gateLocked = gateLocked != 0;
        size_t pathN = 0;
        if (!(is >> tag >> pathN) || tag != "PATH") return false;
        e.path.reserve(pathN);
        for (size_t j = 0; j < pathN; j++) {
            int x = 0, y = 0;
            if (!(is >> x >> y)) return false;
            e.path.push_back({x, y});
        }
        if (!(is >> tag) || tag != "QUEUE") return false;
        if (!readIntVec(is, e.queue)) return false;
        if (!(is >> tag) || tag != "GARRISON") return false;
        if (!readIntVec(is, e.garrison)) return false;
        ng.entities.push_back(e);
    }
    if (!(is >> tag >> n) || tag != "PROJECTILES") return false;
    ng.projectiles.clear();
    ng.projectiles.reserve(std::max<size_t>(256, n));
    for (size_t i = 0; i < n; i++) {
        if (!(is >> tag) || tag != "PROJECTILE") return false;
        Projectile p{};
        int glyph = 0, alive = 0;
        if (!(is >> p.x >> p.y >> p.tx >> p.ty >> glyph >> p.color >> p.life >> alive)) return false;
        p.glyph = (char)glyph;
        p.alive = alive != 0;
        ng.projectiles.push_back(p);
    }
    if (!(is >> tag >> n) || tag != "MARKERS") return false;
    ng.actionMarkers.clear();
    ng.actionMarkers.reserve(n);
    for (size_t i = 0; i < n; i++) {
        if (!(is >> tag) || tag != "MARKER") return false;
        ActionMarker m{};
        int glyph = 0;
        if (!(is >> m.x >> m.y >> m.ticks >> glyph)) return false;
        m.glyph = (char)glyph;
        ng.actionMarkers.push_back(m);
    }
    if (!migrateLoadedGame(ng, version)) return false;
    g = std::move(ng);
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);
    resetDetectMapCache();
    return true;
}
