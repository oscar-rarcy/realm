#include "realm.h"
#include <cstdio>
#include <sys/stat.h>

// ============================================================
// COMMAND FUNNEL
// Input queues Commands (pushCommand); the tick drains the queue
// (applyPendingCommands). The AI calls applyCommand directly — it runs
// inside the sim, so its commands need no queue and no replay logging:
// playback re-derives them tick-perfectly from the same seed.
// Everything here validates against c.player, never against UI state:
// a command must apply identically on a machine that has no idea what
// the issuer had selected.
// ============================================================

static Entity* cmdEnt(const Command& c, int id) {
    Entity* e = findEntity(id);
    if (!e || !e->alive || e->owner != c.player) return nullptr;
    // Garrisoned units can't act on the map. Without this, recalling a
    // control group whose members fled into a building let orders reach
    // them — they'd path out as ghosts while still in the garrison list.
    if (isUnit(e->type) && e->state == S_GARRISONED) return nullptr;
    return e;
}

// A direct order overrides queued waypoints/patrol.
static void clearQueued(Entity& u) { u.waypoints.clear(); u.patrolMode = false; }

// Valid, owned, alive units from the command's id list.
static std::vector<int> cmdUnits(const Command& c, bool landOnly = false) {
    std::vector<int> ids;
    for (int id : c.units) {
        Entity* u = cmdEnt(c, id);
        if (!u || !isUnit(u->type)) continue;
        if (landOnly && isNaval(u->type)) continue;
        ids.push_back(id);
    }
    return ids;
}

void applyCommand(const Command& c) {
    if (c.player < 0 || c.player >= MAX_PLAYERS) return;
    bool human = (c.player == 0);

    switch (c.type) {

    case CMD_MOVE: {
        std::vector<int> ids = cmdUnits(c);
        for (int id : ids) clearQueued(*findEntity(id));
        if (ids.size() > 1)      orderGroupMove(ids, c.x, c.y);
        else if (ids.size() == 1) orderMove(*findEntity(ids[0]), c.x, c.y);
        break;
    }

    case CMD_ATTACK: {
        Entity* t = findEntity(c.target);
        if (!t || !t->alive || t->owner == c.player) return;
        std::vector<int> ids = cmdUnits(c);
        for (int id : ids) clearQueued(*findEntity(id));
        if (ids.size() > 1 && !c.arg) orderGroupAttack(ids, c.target);
        else for (int id : ids) {
            Entity& u = *findEntity(id);
            orderAttack(u, c.target);
            if (c.arg) u.attackMove = 1;  // wave dispatch: keep engaging en route
        }
        break;
    }

    case CMD_ATTACK_MOVE: {
        std::vector<int> ids = cmdUnits(c);
        for (int id : ids) clearQueued(*findEntity(id));
        if (ids.size() > 1) orderGroupAttackMove(ids, c.x, c.y);
        else if (ids.size() == 1) {
            Entity& u = *findEntity(ids[0]);
            orderMove(u, c.x, c.y);
            u.attackMove = 1;
        }
        break;
    }

    case CMD_GATHER: {
        if (!inBounds(c.x, c.y)) return;
        for (int id : cmdUnits(c)) {
            Entity& u = *findEntity(id);
            clearQueued(u);
            orderGather(u, c.x, c.y);
        }
        break;
    }

    case CMD_BUILD: {
        if (c.units.empty() || !inBounds(c.x, c.y)) return;
        if (!isBuilding((EntityType)c.arg)) return;
        Entity* u = cmdEnt(c, c.units[0]);
        if (!u || u->type != E_PEASANT) return;
        clearQueued(*u);
        orderBuild(*u, (EntityType)c.arg, c.x, c.y);
        break;
    }

    case CMD_BUILD_WALL: {
        if (c.units.empty()) return;
        Entity* u = cmdEnt(c, c.units[0]);
        if (!u || u->type != E_PEASANT) return;
        if (!inBounds(c.x, c.y) || !inBounds(c.x2, c.y2)) return;
        Player& p = g.players[c.player];
        // Bresenham line of wall segments; each costs wood, skipping blocked tiles.
        int x0 = c.x, y0 = c.y, x1 = c.x2, y1 = c.y2;
        int dx = std::abs(x1-x0), sx = x0<x1 ? 1 : -1;
        int dy = -std::abs(y1-y0), sy = y0<y1 ? 1 : -1;
        int err = dx+dy, firstId = -1;
        while (true) {
            if (canPlace(E_WALL, x0, y0, c.player) && p.wood >= STATS[E_WALL].costWood) {
                drainStores(c.player, 0, STATS[E_WALL].costWood, x0, y0);
                int wid = spawnEntity(E_WALL, c.player, x0, y0, false);
                if (firstId < 0) firstId = wid;
            }
            if (x0==x1 && y0==y1) break;
            int e2 = 2*err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        if (firstId >= 0) {
            clearQueued(*u);
            orderHelp(*u, firstId);
            if (human) setStatus("Building walls...");
        }
        break;
    }

    case CMD_SOW_FARM: {
        if (c.units.empty() || !inBounds(c.x, c.y)) return;
        Entity* u = cmdEnt(c, c.units[0]);
        if (!u || u->type != E_PEASANT) return;
        if (g.map[c.y][c.x].terrain != T_WHEAT) return;
        if (entityAt(c.x, c.y)) return;
        if (!canPlace(E_FARM, c.x, c.y, c.player)) return;
        int fid = spawnEntity(E_FARM, c.player, c.x, c.y, true);
        clearQueued(*u);
        orderHelp(*u, fid);
        break;
    }

    case CMD_TRAIN: {
        Entity* b = cmdEnt(c, c.target);
        if (!b || !isBuilding(b->type)) return;
        if (c.arg <= E_NONE || c.arg > E_SAPPER) return; // trainables are the unit range
        orderTrain(*b, (EntityType)c.arg);
        break;
    }

    case CMD_HELP: {
        if (c.units.empty()) return;
        Entity* u = cmdEnt(c, c.units[0]);
        Entity* b = cmdEnt(c, c.target);
        // Abandoned village houses: a derelict neutral shell is claimed the
        // moment a peasant starts repairing it — half the work is done, and
        // the timber costs nothing but the labour.
        if (!b && u) {
            Entity* r = findEntity(c.target);
            if (r && r->alive && r->owner == OWNER_NATURE && r->type == E_HOUSE
                && r->underConstruction) {
                r->owner = c.player;
                if (human) setStatus("Restoring the old house — it's yours now.");
                b = r;
            }
        }
        if (!u || u->type != E_PEASANT || !b || !isBuilding(b->type)) return;
        clearQueued(*u);
        orderHelp(*u, c.target);
        break;
    }

    case CMD_GARRISON: {
        // Own buildings/transports — or a neutral claimable (capturable).
        Entity* b = cmdEnt(c, c.target);
        if (!b) {
            Entity* r = findEntity(c.target);
            if (r && r->alive && isClaimable(r->type) && r->owner == OWNER_NATURE) b = r;
        }
        if (!b || b->underConstruction || !canGarrisonIn(b->type)) return;
        for (int id : cmdUnits(c)) {
            Entity& u = *findEntity(id);
            if (u.type == E_CATAPULT || u.type == E_TREBUCHET) continue;
            clearQueued(u);
            orderGarrison(u, c.target);
        }
        break;
    }

    case CMD_UNGARRISON: {
        Entity* b = cmdEnt(c, c.target);
        if (!b || !canGarrisonIn(b->type)) return;
        if (!b->garrison.empty()) ejectGarrison(*b);
        break;
    }

    case CMD_STOP: {
        for (int id : cmdUnits(c)) {
            Entity& u = *findEntity(id);
            u.state = S_IDLE; u.path.clear(); u.pathIdx = 0;
            u.targetId = -1; u.attackMove = 0;
            clearQueued(u);
        }
        break;
    }

    case CMD_HOLD: {
        for (int id : cmdUnits(c)) {
            Entity& u = *findEntity(id);
            u.state = S_IDLE; u.path.clear(); u.pathIdx = 0;
            u.attackMove = 0; u.holdPosition = 1; u.targetId = -1;
            clearQueued(u);
        }
        break;
    }

    case CMD_PATROL: {
        if (!inBounds(c.x, c.y)) return;
        for (int id : cmdUnits(c, /*landOnly=*/true)) {
            Entity& u = *findEntity(id);
            if (u.x == c.x && u.y == c.y) continue; // no-op patrol
            int homeX = u.x, homeY = u.y;
            u.waypoints.clear();
            u.holdPosition = 0; u.retreating = 0;
            orderMove(u, c.x, c.y);                 // outbound leg starts immediately
            u.waypoints.push_back({homeX, homeY});  // then back home...
            u.waypoints.push_back({c.x, c.y});      // ...then out again; patrolMode re-queues forever
            u.patrolMode = true;
        }
        break;
    }

    case CMD_WAYPOINT: {
        if (!inBounds(c.x, c.y)) return;
        for (int id : cmdUnits(c))
            findEntity(id)->waypoints.push_back({c.x, c.y});
        break;
    }

    case CMD_RALLY: {
        Entity* b = cmdEnt(c, c.target);
        if (!b || !isBuilding(b->type) || !inBounds(c.x, c.y)) return;
        b->rallyX = c.x; b->rallyY = c.y; b->rallySet = 1;
        if (human) setStatus("Rally point set.");
        break;
    }

    case CMD_GATE: {
        Entity* b = cmdEnt(c, c.target);
        if (!b || b->type != E_GATE || b->underConstruction) return;
        if (!b->gateLocked) {
            b->gateLocked = true; b->gateOpen = true;
            if (human) setStatus("Gate locked open");
        } else if (b->gateOpen) {
            b->gateOpen = false;
            if (human) setStatus("Gate locked closed");
        } else {
            b->gateLocked = false;
            if (human) setStatus("Gate auto");
        }
        break;
    }

    case CMD_PACK: {
        if (c.units.empty()) return;
        Entity* t = cmdEnt(c, c.units[0]);
        if (!t || t->type != E_TREBUCHET) return;
        if (t->packTicks > 0) { if (human) setStatus("Already transitioning."); return; }
        int pT = (g.players[c.player].research & R_COUNTERWEIGHT) ? 25 : 40;
        if (t->packed == 1) {
            t->packed = 0; t->packTicks = pT;
            t->state = S_IDLE; t->path.clear(); t->pathIdx = 0;
            if (human) setStatus("Deploying trebuchet...");
        } else {
            t->packed = 1; t->packTicks = pT;
            t->state = S_IDLE; t->targetId = -1;
            if (human) setStatus("Packing trebuchet...");
        }
        break;
    }

    case CMD_RESEARCH: {
        Entity* b = cmdEnt(c, c.target);
        if (!b || b->type != E_BLACKSMITH || b->underConstruction) return;
        struct ResEntry { int bit, gold, wood, ticks; const char* msg; };
        // ~75 sec at 80 ms tick = 940 ticks.
        static const ResEntry table[] = {
            { R_IRON_WEAPONS,  100, 100,  940, "Researching Iron Weapons..." },
            { R_CROSSBOWS,      80,  80,  820, "Researching Crossbows..." },
            { R_PIKES,         100, 100,  900, "Researching Pikes..." },
            { R_COUNTERWEIGHT, 120, 150, 1000, "Researching Counterweight..." },
            { R_PLATE_HELM,    120, 100, 1000, "Researching Plate Helm..." },
        };
        const ResEntry* r = nullptr;
        for (auto& e : table) if (e.bit == c.arg) { r = &e; break; }
        if (!r) return;
        Player& p = g.players[c.player];
        if (p.research & r->bit)   { if (human) setStatus("Already researched.");  return; }
        if (b->researching != 0)   { if (human) setStatus("Already researching."); return; }
        if (p.gold < r->gold || p.wood < r->wood) {
            if (human) setStatus("Not enough resources!"); return;
        }
        drainStores(c.player, r->gold, r->wood, b->x, b->y);
        b->researching = r->bit; b->prodProgress = 0; b->prodTime = r->ticks;
        if (human) setStatus(r->msg);
        break;
    }

    case CMD_TRADE: {
        Entity* b = cmdEnt(c, c.target);
        if (!b || b->type != E_MARKET || b->underConstruction) return;
        struct TradeEntry { int cg, cw, cf, gg, gw, gf; const char* msg; };
        static const TradeEntry table[] = {
            { 40, 0, 0,  0, 30,  0, "Traded gold for wood." },
            {  0,40, 0, 30,  0,  0, "Traded wood for gold." },
            { 50, 0, 0,  0,  0, 30, "Traded gold for food." },
            {  0, 0,40, 30,  0,  0, "Traded food for gold." },
        };
        if (c.arg < 0 || c.arg >= 4) return;
        const TradeEntry& t = table[c.arg];
        Player& p = g.players[c.player];
        if (p.gold < t.cg || p.wood < t.cw || p.food < t.cf) {
            if (human) setStatus("Not enough resources!"); return;
        }
        // Pay from the piles near the market; credit the gains the same way.
        drainStores(c.player, t.cg, t.cw, b->x, b->y);
        if (t.cf) spendPlayerFood(c.player, t.cf);
        depositToNearest(c.player, t.gg, t.gw, F_GRAIN, t.gf, b->x, b->y);
        if (human) setStatus(t.msg);
        break;
    }

    case CMD_FEAST: {
        // Tavern throws a feast: 10 ale, every friendly unit nearby heals 20%.
        Entity* b = cmdEnt(c, c.target);
        if (!b || b->type != E_TAVERN || b->underConstruction) return;
        if (b->atkCd > 0) { if (human) setStatus("The tavern is still cleaning up the last feast."); return; }
        if (b->storeFood[F_ALE] < 10) { if (human) setStatus("Need 10 ale for a feast!"); return; }
        b->storeFood[F_ALE] -= 10;
        int fed = 0;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != c.player || !isUnit(u.type)) continue;
            if (u.state == S_GARRISONED) continue;
            if (dist(u.x, u.y, b->x, b->y) > 8) continue;
            if (u.hp < u.maxHp) { u.hp = std::min(u.maxHp, u.hp + u.maxHp/5); fed++; }
        }
        b->atkCd = 1500;
        if (human) setStatus("A feast! " + std::to_string(fed) + " soldiers eat, drink and mend.");
        break;
    }

    case CMD_HAUL: {
        // Wagon ↔ depot: the wagon resolves load-vs-unload on arrival.
        if (c.units.empty()) return;
        Entity* w = cmdEnt(c, c.units[0]);
        Entity* b = cmdEnt(c, c.target);
        if (!w || w->type != E_WAGON || !b || !isBuilding(b->type) || b->underConstruction) return;
        clearQueued(*w);
        w->state = S_RETURNING; w->targetId = b->id;
        w->targetX = b->x; w->targetY = b->y;
        w->path = findPathFor(*w, b->x, b->y); w->pathIdx = 0;
        break;
    }

    case CMD_REVEAL: {
        // Debug. Goes through the funnel because explored[] feeds sim decisions
        // (findNearbyResource) — a local-only reveal would desync a replay.
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            g.map[y][x].visible[c.player]  = true;
            g.map[y][x].explored[c.player] = true;
        }
        if (human) setStatus("Debug: map revealed");
        break;
    }

    default: break;
    }
}

// ============================================================
// REPLAYS — header (seed + match setup) + the human command stream.
// AI commands are NOT recorded: the AI is deterministic sim code, so
// playback re-derives them exactly. Binary little-endian, host arch
// (same caveat as save files).
// ============================================================
static constexpr char REP_MAGIC[4] = {'R','L','R','P'};
static constexpr int  REP_VERSION  = 4;   // v4: stockpile economy + structures (v3: elevation)

static FILE* recF  = nullptr;   // recording
static FILE* playF = nullptr;   // playback
static bool  playbackMode = false;
static Command pendingRec;      // next playback record, primed
static int     pendingTick = -1;
static bool    havePendingRec = false;

bool replayPlaying() { return playbackMode; }

static void wrI(FILE* f, int v)                  { fwrite(&v, sizeof v, 1, f); }
static void wrU64(FILE* f, unsigned long long v) { fwrite(&v, sizeof v, 1, f); }
static bool rdI(FILE* f, int& v)                  { return fread(&v, sizeof v, 1, f) == 1; }
static bool rdU64(FILE* f, unsigned long long& v) { return fread(&v, sizeof v, 1, f) == 1; }

bool replayStartRecording(int numAIs) {
    replayStopRecording();
    if (playbackMode) return false;   // never record a playback of itself
    mkdir("replays", 0755);
    char path[128];
    time_t now = time(nullptr);
    struct tm* tmv = localtime(&now);
    strftime(path, sizeof path, "replays/realm-%Y%m%d-%H%M%S.rep", tmv);
    recF = fopen(path, "wb");
    if (!recF) return false;
    fwrite(REP_MAGIC, 4, 1, recF);
    wrI(recF, REP_VERSION);
    wrU64(recF, g.simSeed);
    wrI(recF, numAIs);
    wrI(recF, g.biomeChoice);
    wrI(recF, g.difficulty);
    fflush(recF);
    return true;
}

void replayStopRecording() {
    if (recF) { fclose(recF); recF = nullptr; }
}

static void replayRecord(const Command& c) {
    if (!recF) return;
    wrI(recF, g.tick);
    wrI(recF, c.type); wrI(recF, c.player);
    wrI(recF, c.x); wrI(recF, c.y); wrI(recF, c.x2); wrI(recF, c.y2);
    wrI(recF, c.target); wrI(recF, c.arg);
    wrI(recF, (int)c.units.size());
    for (int id : c.units) wrI(recF, id);
    fflush(recF);   // survive the mid-game exit(0) quit path
}

static bool replayReadNext() {
    havePendingRec = false;
    if (!playF) return false;
    Command c; int tick, n;
    if (!rdI(playF, tick)) { fclose(playF); playF = nullptr; return false; }
    if (!rdI(playF, c.type) || !rdI(playF, c.player)) return false;
    if (!rdI(playF, c.x) || !rdI(playF, c.y) || !rdI(playF, c.x2) || !rdI(playF, c.y2)) return false;
    if (!rdI(playF, c.target) || !rdI(playF, c.arg) || !rdI(playF, n)) return false;
    if (n < 0 || n > 10000) return false;
    c.units.resize(n);
    for (int i = 0; i < n; i++) if (!rdI(playF, c.units[i])) return false;
    pendingRec = std::move(c);
    pendingTick = tick;
    havePendingRec = true;
    return true;
}

bool replayLoadFile(const char* path, unsigned long long& seed, int& numAIs,
                    int& biomeChoice, int& difficulty) {
    playF = fopen(path, "rb");
    if (!playF) return false;
    char magic[4]; int ver;
    if (fread(magic, 4, 1, playF) != 1 || memcmp(magic, REP_MAGIC, 4) != 0
        || !rdI(playF, ver) || ver != REP_VERSION
        || !rdU64(playF, seed) || !rdI(playF, numAIs) || !rdI(playF, biomeChoice)
        || !rdI(playF, difficulty)) {
        fclose(playF); playF = nullptr; return false;
    }
    playbackMode = true;
    replayReadNext();
    return true;
}

void replayInjectCommands() {
    if (!playbackMode) return;
    while (havePendingRec && pendingTick <= g.tick) {
        g.pendingCmds.push_back(pendingRec);
        replayReadNext();
    }
}

// ============================================================
// QUEUE
// ============================================================
void pushCommand(const Command& c) {
    if (playbackMode) return;   // watching a replay: local orders are inert
    g.pendingCmds.push_back(c);
}

void applyPendingCommands() {
    // Drain into a local copy first: applying a command can never enqueue
    // another (and recording must see the exact applied order).
    std::vector<Command> cmds;
    cmds.swap(g.pendingCmds);
    for (const Command& c : cmds) {
        replayRecord(c);
        applyCommand(c);
    }
}

// ============================================================
// STATE HASH — FNV-1a over everything that defines the sim. Two runs
// from the same seed + command stream must produce identical hashes
// tick for tick; the first differing tick localises a desync source.
// ============================================================
static inline void fnv(unsigned long long& h, long long v) {
    for (int i = 0; i < 8; i++) {
        h ^= (unsigned long long)((v >> (i*8)) & 0xFF);
        h *= 1099511628211ull;
    }
}

unsigned long long simStateHash() {
    unsigned long long h = 1469598103934665603ull;
    fnv(h, g.tick);
    fnv(h, (long long)g.rngState);
    for (int p = 0; p <= MAX_PLAYERS; p++) {
        const Player& pl = g.players[p];
        fnv(h, pl.gold); fnv(h, pl.wood); fnv(h, pl.food);
        fnv(h, pl.supply); fnv(h, pl.supplyMax);
        fnv(h, pl.alive ? 1 : 0); fnv(h, pl.research);
    }
    for (const auto& e : g.entities) {
        if (!e.alive) continue;
        fnv(h, e.id); fnv(h, e.type); fnv(h, e.owner);
        fnv(h, e.x); fnv(h, e.y); fnv(h, e.hp);
        fnv(h, e.state); fnv(h, e.carrying);
        fnv(h, e.targetId); fnv(h, (int)e.garrison.size());
        fnv(h, e.storeGold); fnv(h, e.storeWood);
        for (int k = 0; k < F_COUNT; k++) fnv(h, e.storeFood[k]);
        fnv(h, e.aleTicks);
    }
    return h;
}

void simHashTick() {
    static int enabled = -1;
    if (enabled < 0) {
        const char* env = getenv("REALM_HASH");
        enabled = (env && *env && *env != '0') ? 1 : 0;
    }
    if (!enabled || g.tick % 100 != 0) return;
    FILE* f = fopen("realm-hash.log", "a");
    if (!f) return;
    fprintf(f, "%d %016llx\n", g.tick, simStateHash());
    fclose(f);
}
