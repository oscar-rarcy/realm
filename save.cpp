#include "realm.h"
#include <cstdio>
#include <cstdint>

// Save/load: simple binary dump of the game struct. Skips transient state
// (projectiles, status text, cursor) and rebuilds derived caches on load.
//
// Stability checks:
//   - Magic bytes "RLM2" identify the file
//   - Version number — saves from older versions are rejected
//   - Map dimensions stored in header — refuses load if MAP_W/MAP_H changed
//   - Atomic write: writes to .tmp first, then renames over the target
//   - Sanity caps: rejects absurd entity counts or vector lengths
//   - Skips garbage corrupt files via fread return-value checks

static constexpr char MAGIC[4] = {'R','L','M','2'};
static constexpr int  SAVE_VERSION = 15; // v15: farms are 2x2 fields (footprint/sim rules changed)
static constexpr int  MAX_ENTITIES = 100000;
static constexpr int  MAX_VEC_LEN  = 50000;

// Tiny IO wrappers. wr returns nothing (writes assumed to succeed for fast
// fail at fclose / disk-full); rd returns false on short read so we can bail.
template<typename T> static void wr(FILE* f, const T& v) { fwrite(&v, sizeof(T), 1, f); }
template<typename T> static bool rd(FILE* f, T& v)        { return fread(&v, sizeof(T), 1, f) == 1; }
static void wrBlock(FILE* f, const void* p, size_t n) { fwrite(p, 1, n, f); }
static bool rdBlock(FILE* f, void* p, size_t n)        { return fread(p, 1, n, f) == n; }

// Canonical slot file path (1-based). One source of truth for the F-keys,
// the visual menu, and peekSave.
void saveSlotPath(int slot, char* buf, int n) {
    snprintf(buf, n, "realm-slot%d.sav", slot);
}

// Read just enough of a save to summarise it for the Save/Load menu: the
// header (magic/version/dims/timestamp) plus the first four game scalars,
// which carry the in-game clock. Never touches the big map/entity blocks.
// Returns false for a missing, wrong-version, or corrupt file (shown "empty").
bool peekSave(const char* path, SaveSlotInfo& out) {
    out = SaveSlotInfo{};
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, MAGIC, 4) != 0) { fclose(f); return false; }
    int ver;
    if (!rd(f, ver) || ver != SAVE_VERSION) { fclose(f); return false; }
    int32_t mapW, mapH, maxPlayers;
    if (!rd(f, mapW) || !rd(f, mapH) || !rd(f, maxPlayers)) { fclose(f); return false; }
    if (mapW != MAP_W || mapH != MAP_H || maxPlayers != MAX_PLAYERS) { fclose(f); return false; }
    int64_t saveTime;
    if (!rd(f, saveTime)) { fclose(f); return false; }
    int nextId, tick, year; float dayPhase, seasonPhase;   // same order as saveGame
    if (!rd(f, nextId) || !rd(f, tick) || !rd(f, dayPhase) || !rd(f, seasonPhase) || !rd(f, year)) { fclose(f); return false; }
    fclose(f);
    out.used     = true;
    out.saveTime = (long long)saveTime;
    out.season   = ((int)seasonPhase % 4 + 4) % 4;
    out.year     = std::max(1, year);
    return true;
}

bool saveGame(const char* path) {
    // Atomic save: write to .tmp, then rename. If anything fails partway,
    // the existing save file is untouched.
    char tmpPath[512];
    snprintf(tmpPath, sizeof(tmpPath), "%s.tmp", path);

    FILE* f = fopen(tmpPath, "wb");
    if (!f) return false;

    // ----- HEADER -----
    fwrite(MAGIC, 1, 4, f);
    wr(f, SAVE_VERSION);
    // Map dimensions: loader refuses to read into the wrong-sized array.
    int32_t mapW = MAP_W, mapH = MAP_H, maxPlayers = MAX_PLAYERS;
    wr(f, mapW); wr(f, mapH); wr(f, maxPlayers);
    // Save timestamp for any future load-menu use.
    int64_t now = (int64_t)time(nullptr);
    wr(f, now);

    // ----- GAME SCALARS -----
    wr(f, g.nextId);   wr(f, g.tick);
    wr(f, g.dayPhase); wr(f, g.seasonPhase);
    wr(f, g.year);
    wr(f, g.prevSeason); wr(f, g.prevTimePhase);
    wr(f, g.attackNotifyCd);
    wr(f, g.weather); wr(f, g.weatherTimer);
    wr(f, g.biomeChoice); wr(f, g.layoutChoice);
    wr(f, g.simSeed); wr(f, g.playerColor);
    wr(f, g.winner); wr(f, g.aiTimer); wr(f, g.farmTimer);
    wr(f, g.rngState);
    wr(f, g.difficulty); wr(f, g.winterSeverity);
    wr(f, g.siteHoldOwner); wr(f, g.siteHoldTicks);

    // ----- PLAYERS, MAP -----
    wrBlock(f, g.players, sizeof(g.players));
    wrBlock(f, g.map, sizeof(g.map));

    // ----- ENTITIES -----
    int32_t entCount = (int32_t)g.entities.size();
    wr(f, entCount);
    for (auto& e : g.entities) {
        wr(f, e.id); wr(f, e.type); wr(f, e.owner);
        wr(f, e.x); wr(f, e.y); wr(f, e.hp); wr(f, e.maxHp);
        wr(f, e.state); wr(f, e.targetId); wr(f, e.targetX); wr(f, e.targetY);
        wr(f, e.pathIdx);
        wr(f, e.moveCd); wr(f, e.atkCd); wr(f, e.gatherCd); wr(f, e.gatherType);
        wr(f, e.producing); wr(f, e.prodProgress); wr(f, e.prodTime);
        wr(f, e.underConstruction); wr(f, e.alive);
        wr(f, e.rallyX); wr(f, e.rallyY);
        wr(f, e.carrying); wr(f, e.stuckTicks); wr(f, e.alertTicks);
        wr(f, e.rallySet); wr(f, e.researching);
        wr(f, e.attackMove); wr(f, e.holdPosition);
        wr(f, e.gateOpen); wr(f, e.gateLocked);
        wr(f, e.convertTicks); wr(f, e.retreating);
        wr(f, e.packed); wr(f, e.packTicks);
        int32_t n = (int32_t)e.path.size(); wr(f, n);
        for (auto& p : e.path) { wr(f, p.first); wr(f, p.second); }
        n = (int32_t)e.queue.size(); wr(f, n);
        for (int q : e.queue)    wr(f, q);
        n = (int32_t)e.garrison.size(); wr(f, n);
        for (int gid : e.garrison) wr(f, gid);
        n = (int32_t)e.waypoints.size(); wr(f, n);
        for (auto& w : e.waypoints) { wr(f, w.first); wr(f, w.second); }
        wr(f, e.patrolMode);
        wr(f, e.storeGold); wr(f, e.storeWood);
        for (int k = 0; k < F_COUNT; k++) wr(f, e.storeFood[k]);
        wr(f, e.foodKind); wr(f, e.aleTicks);
        wr(f, e.morale); wr(f, e.routTicks); wr(f, e.chargeSteps);
        wr(f, e.stamina); wr(f, e.kills);
        wr(f, e.prisoner); wr(f, e.origOwner); wr(f, e.captureTicks);
        wr(f, e.entrenchTicks);
    }

    // Check the stream is healthy before committing.
    if (ferror(f)) { fclose(f); remove(tmpPath); return false; }
    if (fclose(f) != 0) { remove(tmpPath); return false; }

    // Atomic rename. On the failure path the user keeps their last good save.
    if (rename(tmpPath, path) != 0) { remove(tmpPath); return false; }
    platformPersistFiles();   // browser build: flush MEMFS down to IndexedDB
    return true;
}

bool loadGame(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    // ----- HEADER VALIDATION -----
    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, MAGIC, 4) != 0) { fclose(f); return false; }
    int ver;
    if (!rd(f, ver) || ver != SAVE_VERSION) { fclose(f); return false; }
    int32_t mapW, mapH, maxPlayers;
    if (!rd(f, mapW) || !rd(f, mapH) || !rd(f, maxPlayers)) { fclose(f); return false; }
    if (mapW != MAP_W || mapH != MAP_H || maxPlayers != MAX_PLAYERS) { fclose(f); return false; }
    int64_t saveTime;
    if (!rd(f, saveTime)) { fclose(f); return false; }

    // ----- GAME SCALARS -----
    if (!rd(f, g.nextId))   { fclose(f); return false; }
    if (!rd(f, g.tick))     { fclose(f); return false; }
    rd(f, g.dayPhase); rd(f, g.seasonPhase);
    rd(f, g.year);
    rd(f, g.prevSeason); rd(f, g.prevTimePhase);
    rd(f, g.attackNotifyCd);
    rd(f, g.weather); rd(f, g.weatherTimer);
    rd(f, g.biomeChoice); rd(f, g.layoutChoice);
    rd(f, g.simSeed); rd(f, g.playerColor);
    rd(f, g.winner); rd(f, g.aiTimer); rd(f, g.farmTimer);
    rd(f, g.rngState);
    rd(f, g.difficulty); rd(f, g.winterSeverity);
    rd(f, g.siteHoldOwner); rd(f, g.siteHoldTicks);
    // Re-derive the battlefield name from the persisted seed/layout/climate.
    g.mapName = makeMapName(g.simSeed, g.layoutChoice, g.biomeChoice);
    // Commands queued against the pre-load world would mis-target ids in
    // the loaded one. The queue is transient, never saved — just drop it.
    g.pendingCmds.clear();

    // ----- PLAYERS, MAP -----
    if (!rdBlock(f, g.players, sizeof(g.players))) { fclose(f); return false; }
    if (!rdBlock(f, g.map, sizeof(g.map)))         { fclose(f); return false; }

    // ----- ENTITIES -----
    int32_t entCount = 0;
    if (!rd(f, entCount)) { fclose(f); return false; }
    if (entCount < 0 || entCount > MAX_ENTITIES) { fclose(f); return false; }
    g.entities.clear();
    g.entities.reserve(std::max(8192, entCount + 1024));
    for (int i = 0; i < entCount; i++) {
        Entity e{};
        if (!rd(f, e.id) || !rd(f, e.type) || !rd(f, e.owner)) { fclose(f); return false; }
        rd(f, e.x); rd(f, e.y); rd(f, e.hp); rd(f, e.maxHp);
        rd(f, e.state); rd(f, e.targetId); rd(f, e.targetX); rd(f, e.targetY);
        rd(f, e.pathIdx);
        rd(f, e.moveCd); rd(f, e.atkCd); rd(f, e.gatherCd); rd(f, e.gatherType);
        rd(f, e.producing); rd(f, e.prodProgress); rd(f, e.prodTime);
        rd(f, e.underConstruction); rd(f, e.alive);
        rd(f, e.rallyX); rd(f, e.rallyY);
        rd(f, e.carrying); rd(f, e.stuckTicks); rd(f, e.alertTicks);
        rd(f, e.rallySet); rd(f, e.researching);
        rd(f, e.attackMove); rd(f, e.holdPosition);
        rd(f, e.gateOpen); rd(f, e.gateLocked);
        rd(f, e.convertTicks); rd(f, e.retreating);
        rd(f, e.packed); rd(f, e.packTicks);
        int32_t n = 0;
        if (!rd(f, n) || n < 0 || n > MAX_VEC_LEN) { fclose(f); return false; }
        e.path.reserve(n);
        for (int j = 0; j < n; j++) { int a, b; rd(f, a); rd(f, b); e.path.push_back({a,b}); }
        if (!rd(f, n) || n < 0 || n > MAX_VEC_LEN) { fclose(f); return false; }
        e.queue.reserve(n);
        for (int j = 0; j < n; j++) { int q; rd(f, q); e.queue.push_back(q); }
        if (!rd(f, n) || n < 0 || n > MAX_VEC_LEN) { fclose(f); return false; }
        e.garrison.reserve(n);
        for (int j = 0; j < n; j++) { int gid; rd(f, gid); e.garrison.push_back(gid); }
        if (!rd(f, n) || n < 0 || n > MAX_VEC_LEN) { fclose(f); return false; }
        e.waypoints.reserve(n);
        for (int j = 0; j < n; j++) { int a, b; rd(f, a); rd(f, b); e.waypoints.push_back({a,b}); }
        rd(f, e.patrolMode);
        rd(f, e.storeGold); rd(f, e.storeWood);
        for (int k = 0; k < F_COUNT; k++) rd(f, e.storeFood[k]);
        rd(f, e.foodKind); rd(f, e.aleTicks);
        rd(f, e.morale); rd(f, e.routTicks); rd(f, e.chargeSteps);
        rd(f, e.stamina); rd(f, e.kills);
        rd(f, e.prisoner); rd(f, e.origOwner); rd(f, e.captureTicks);
        rd(f, e.entrenchTicks);
        g.entities.push_back(e);
    }

    fclose(f);

    // Wipe transient state and rebuild derived caches.
    g.projectiles.clear();
    g.selectedId = -1; g.selectedIds.clear();
    for (int i = 0; i < 9; i++) g.controlGroups[i].clear();
    g.mode = M_NORMAL;
    g.statusMsg.clear(); g.statusTimer = 0;
    g.buildPending = E_NONE;
    g.dragging = false;
    g.groupAssignPending = false;
    g.returnToMenu = false;
    resetDetectMapCache();
    applyTeamColors();   // re-skin owner pairs from the loaded g.playerColor
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);
    updateFog();
    return true;
}
