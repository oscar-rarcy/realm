#include "realm.h"
#include <cstdio>

// Save/load: simple binary dump of the game struct. Skips transient state
// (projectiles, status text, cursor) and rebuilds derived caches on load.

static constexpr char MAGIC[4] = {'R','L','M','1'};
static constexpr int  SAVE_VERSION = 1;

// Trivial reader/writer wrappers.
template<typename T> static void wr(FILE* f, const T& v) { fwrite(&v, sizeof(T), 1, f); }
template<typename T> static bool rd(FILE* f, T& v)        { return fread(&v, sizeof(T), 1, f) == 1; }

// Write/read a fixed POD block.
static void wrBlock(FILE* f, const void* p, size_t n) { fwrite(p, 1, n, f); }
static bool rdBlock(FILE* f, void* p, size_t n)        { return fread(p, 1, n, f) == n; }

bool saveGame(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;

    fwrite(MAGIC, 1, 4, f);
    wr(f, SAVE_VERSION);

    // Top-level scalars.
    wr(f, g.nextId);   wr(f, g.tick);
    wr(f, g.dayPhase); wr(f, g.seasonPhase);
    wr(f, g.prevSeason); wr(f, g.prevTimePhase);
    wr(f, g.attackNotifyCd);
    wr(f, g.weather); wr(f, g.weatherTimer);
    wr(f, g.biomeChoice);
    wr(f, g.winner); wr(f, g.aiTimer); wr(f, g.farmTimer);

    // Players (fixed-size array, POD).
    wrBlock(f, g.players, sizeof(g.players));

    // Map (fixed-size 2D array, POD).
    wrBlock(f, g.map, sizeof(g.map));

    // Entities — variable length. Each entity has vectors we serialise by hand.
    int entCount = (int)g.entities.size();
    wr(f, entCount);
    for (auto& e : g.entities) {
        // Write scalar fields one by one (can't memcpy a struct with std::vector).
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
        // Vectors: length + contents.
        int n = (int)e.path.size(); wr(f, n);
        for (auto& p : e.path) { wr(f, p.first); wr(f, p.second); }
        n = (int)e.queue.size(); wr(f, n);
        for (int q : e.queue)    wr(f, q);
        n = (int)e.garrison.size(); wr(f, n);
        for (int gid : e.garrison) wr(f, gid);
    }

    fclose(f);
    return true;
}

bool loadGame(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, MAGIC, 4) != 0) { fclose(f); return false; }
    int ver;
    if (!rd(f, ver) || ver != SAVE_VERSION) { fclose(f); return false; }

    // Read top-level scalars.
    rd(f, g.nextId);   rd(f, g.tick);
    rd(f, g.dayPhase); rd(f, g.seasonPhase);
    rd(f, g.prevSeason); rd(f, g.prevTimePhase);
    rd(f, g.attackNotifyCd);
    rd(f, g.weather); rd(f, g.weatherTimer);
    rd(f, g.biomeChoice);
    rd(f, g.winner); rd(f, g.aiTimer); rd(f, g.farmTimer);

    // Players + map: fixed-size blocks.
    rdBlock(f, g.players, sizeof(g.players));
    rdBlock(f, g.map, sizeof(g.map));

    // Entities.
    int entCount = 0;
    rd(f, entCount);
    g.entities.clear();
    g.entities.reserve(std::max(8192, entCount + 1024));
    for (int i = 0; i < entCount; i++) {
        Entity e{};
        rd(f, e.id); rd(f, e.type); rd(f, e.owner);
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
        int n = 0;
        rd(f, n); e.path.reserve(n);
        for (int j = 0; j < n; j++) { int a, b; rd(f, a); rd(f, b); e.path.push_back({a,b}); }
        rd(f, n); e.queue.reserve(n);
        for (int j = 0; j < n; j++) { int q; rd(f, q); e.queue.push_back(q); }
        rd(f, n); e.garrison.reserve(n);
        for (int j = 0; j < n; j++) { int gid; rd(f, gid); e.garrison.push_back(gid); }
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
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);
    updateFog();
    return true;
}
