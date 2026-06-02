#include "save_reader.h"

#include "realm.h"
#include "sim/save_migration.h"
#include "sim/save_schema.h"

#include <fstream>
#include <sstream>

namespace {

bool readIntVec(std::istream& is, std::vector<int>& v) {
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

bool parseSaveStream(std::istream& is, Game& ng, int& version) {
    std::string tag;
    version = 0;
    if (!(is >> tag >> version) || tag != "REALM_SAVE" || !isSupportedSaveVersion(version)) return false;
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
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int i = 0; i < 9; i++)
            ng.controlGroupsByOwner[p][i].clear();
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
    return true;
}

} // namespace

SaveHeaderInfo inspectSaveHeader(const std::string& path) {
    std::ifstream in(path);
    if (!in) return { false, 0, "Load file not found." };

    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "REALM_SAVE") {
        return { false, 0, "File is not a Realm save." };
    }
    if (version < REALM_MIN_SUPPORTED_SAVE_VERSION || version > REALM_SAVE_VERSION) {
        std::ostringstream out;
        out << "Unsupported save version " << version << " (supported "
            << REALM_MIN_SUPPORTED_SAVE_VERSION << "-" << REALM_SAVE_VERSION << ").";
        return { false, version, out.str() };
    }
    return { true, version, {} };
}

bool readSaveFile(const std::string& path, Game& game, int& version) {
    std::ifstream is(path);
    if (!is) return false;
    return parseSaveStream(is, game, version);
}
