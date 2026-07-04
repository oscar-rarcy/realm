#include "realm.h"
#include <cstdio>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir(p, 0755)
#endif

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

// ============================================================
// RESEARCH TABLE — every tech: where it's bought, which era unlocks it,
// what it costs. commands.cpp charges from it, ui.cpp prints from it,
// input.cpp maps keys from it, ai.cpp shops from it.
// ============================================================
static const ResearchDef RESEARCH[] = {
    { R_IRON_WEAPONS,   E_BLACKSMITH, ERA_TOWNSHIP,   100, 100,  940, 'I', "Iron Weapons",   "militia/knights +2 atk" },
    { R_FLETCHING,      E_BLACKSMITH, ERA_TOWNSHIP,    80,  60,  800, 'F', "Fletching",      "archers/crossbows +1 atk" },
    { R_CROSSBOWS,      E_BLACKSMITH, ERA_TOWNSHIP,    80,  80,  820, 'C', "Crossbows",      "archers +2 range" },
    { R_PIKES,          E_BLACKSMITH, ERA_TOWNSHIP,   100, 100,  900, 'P', "Pikes",          "spearmen +1 range" },
    { R_PLATE_HELM,     E_BLACKSMITH, ERA_STRONGHOLD, 120, 100, 1000, 'H', "Plate Helm",     "knights shrug cuts/arrows" },
    { R_COUNTERWEIGHT,  E_BLACKSMITH, ERA_STRONGHOLD, 120, 150, 1000, 'W', "Counterweight",  "trebuchets deploy faster" },
    { R_HEAVY_PLOUGH,   E_MILL,       ERA_TOWNSHIP,    60, 100,  850, 'P', "Heavy Plough",   "farms yield +1" },
    { R_HORSE_BREEDING, E_STABLE,     ERA_STRONGHOLD, 120,  80,  950, 'H', "Horse Breeding", "new cavalry +15 HP" },
    { R_MASONRY,        E_STONEMASON, ERA_STRONGHOLD, 100, 150, 1000, 'M', "Masonry",        "buildings take -20% damage" },
};
const ResearchDef* researchTable(int& n) {
    n = (int)(sizeof(RESEARCH) / sizeof(RESEARCH[0]));
    return RESEARCH;
}

// A field is sown as a 2x2 square. The clicked wheat tile may sit in any
// corner of it — pick the anchor whose footprint folds in the most wild
// wheat (ties resolve in fixed scan order, so both lockstep sides agree).
bool farmAnchorFor(int x, int y, int player, int ignoreId, int& ax, int& ay) {
    int fw = STATS[E_FARM].sizeW, fh = STATS[E_FARM].sizeH;
    int best = -1;
    for (int oy = 1 - fh; oy <= 0; oy++) for (int ox = 1 - fw; ox <= 0; ox++) {
        int tx = x + ox, ty = y + oy;
        if (!canPlace(E_FARM, tx, ty, player, ignoreId)) continue;
        int score = 0;
        for (int dy = 0; dy < fh; dy++) for (int dx = 0; dx < fw; dx++)
            if (g.map[ty+dy][tx+dx].terrain == T_WHEAT) score++;
        if (score > best) { best = score; ax = tx; ay = ty; }
    }
    return best >= 0;
}

void applyCommand(const Command& c) {
    if (c.player < 0 || c.player >= MAX_PLAYERS) return;
    bool human = (c.player == g.localPlayer);

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
        std::vector<int> ids = cmdUnits(c);
        // Same-kind classifier so a mass-assign can fan the group out to
        // neighbouring resource tiles (AoE2-style) instead of stacking on one.
        auto resClass = [](Terrain t) -> int {
            if (t == T_GOLD)                                                return 0;
            if (t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE)          return 1;
            if (t == T_FISH)                                               return 2;
            if (t == T_BERRY)                                              return 3;
            return -1;
        };
        int cls = resClass(g.map[c.y][c.x].terrain);
        auto gatherAllAtTarget = [&]() {
            for (int id : ids) { Entity& u = *findEntity(id); clearQueued(u); orderGather(u, c.x, c.y); }
        };
        if (ids.size() <= 1 || cls < 0) { gatherAllAtTarget(); break; }

        // Collect same-class resource tiles around the click, nearest-first.
        int owner = findEntity(ids[0])->owner;
        struct Cand { int x, y, d; };
        std::vector<Cand> cand;
        const int R = 7;
        for (int dy = -R; dy <= R; dy++) for (int dx = -R; dx <= R; dx++) {
            int nx = c.x+dx, ny = c.y+dy;
            if (!inBounds(nx,ny)) continue;
            if (owner >= 0 && owner < OWNER_NATURE && !g.map[ny][nx].explored[owner]) continue;
            if (g.map[ny][nx].resources <= 0) continue;
            if (resClass(g.map[ny][nx].terrain) != cls) continue;
            cand.push_back({nx, ny, std::abs(dx) + std::abs(dy)});
        }
        if (cand.empty()) { gatherAllAtTarget(); break; }
        std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) {
            return a.d != b.d ? a.d < b.d : (a.y != b.y ? a.y < b.y : a.x < b.x);
        });
        // Fill nearest tiles first up to a soft cap before spilling to farther
        // ones — keeps the group clustered around the click but not stacked.
        std::vector<int> load(cand.size(), 0);
        int cap = 1;
        for (int id : ids) {
            Entity& u = *findEntity(id);
            int pick = -1;
            while (pick < 0) {
                for (size_t k = 0; k < cand.size(); k++) if (load[k] < cap) { pick = (int)k; break; }
                if (pick < 0) cap++;
            }
            load[pick]++;
            clearQueued(u);
            orderGather(u, cand[pick].x, cand[pick].y);
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
        int ax, ay;
        if (!farmAnchorFor(c.x, c.y, c.player, u->id, ax, ay)) return;
        int fid = spawnEntity(E_FARM, c.player, ax, ay, true);
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
        if (!b || b->underConstruction) return;
        int n = 0;
        const ResearchDef* table = researchTable(n);
        const ResearchDef* r = nullptr;
        for (int i = 0; i < n; i++) if (table[i].bit == c.arg) { r = &table[i]; break; }
        if (!r || b->type != r->building) return;
        Player& p = g.players[c.player];
        if (p.era < r->era) {
            if (human) setStatus(std::string(r->name) + " needs the " + eraName(r->era) + " era.");
            return;
        }
        if (p.research & r->bit)   { if (human) setStatus("Already researched.");  return; }
        if (b->researching != 0)   { if (human) setStatus("Already researching."); return; }
        if (p.gold < r->gold || p.wood < r->wood) {
            if (human) setStatus("Not enough resources!"); return;
        }
        drainStores(c.player, r->gold, r->wood, b->x, b->y);
        b->researching = r->bit; b->prodProgress = 0; b->prodTime = r->ticks;
        if (human) setStatus(std::string("Researching ") + r->name + "...");
        break;
    }

    case CMD_ERA_UP: {
        // The match arc: a long, expensive upgrade at the TC/Castle.
        Entity* b = cmdEnt(c, c.target);
        if (!b || (b->type != E_TOWNHALL && b->type != E_CASTLE) || b->underConstruction) return;
        Player& p = g.players[c.player];
        int food, gold, wood, ticks;
        if (!eraUpCost(p.era, food, gold, wood, ticks)) {
            if (human) setStatus("Your civilisation already stands at its height.");
            return;
        }
        if (b->researching != 0) { if (human) setStatus("That hall is already busy."); return; }
        // One advance at a time, realm-wide.
        for (auto& e : g.entities)
            if (e.alive && e.owner == c.player && e.researching == R_ERA_ADVANCE)
                { if (human) setStatus("The era is already advancing."); return; }
        if (p.food < food || p.gold < gold || p.wood < wood) {
            if (human) setStatus("Advancing needs " + std::to_string(food) + "f "
                                 + std::to_string(gold) + "g "
                                 + (wood ? std::to_string(wood) + "w" : ""));
            return;
        }
        spendPlayerFood(c.player, food);
        drainStores(c.player, gold, wood, b->x, b->y);
        b->researching = R_ERA_ADVANCE; b->prodProgress = 0; b->prodTime = ticks;
        if (human) setStatus(std::string("Advancing to the ") + eraName(p.era + 1) + " era...");
        break;
    }

    case CMD_RAID: {
        // March on an enemy stockyard and carry off its piles. Any land unit
        // can steal; the goods ride home through the normal courier flow.
        Entity* yard = findEntity(c.target);
        if (!yard || !yard->alive || yard->type != E_STOCKYARD
            || yard->owner == c.player || yard->owner >= MAX_PLAYERS
            || yard->underConstruction) return;
        for (int id : cmdUnits(c, /*landOnly=*/true)) {
            Entity& u = *findEntity(id);
            if (u.type == E_TREBUCHET || u.type == E_CATAPULT || u.type == E_RAM) continue;
            clearQueued(u);
            u.state = S_RAIDING; u.targetId = yard->id;
            u.targetX = yard->x; u.targetY = yard->y;
            u.holdPosition = 0; u.retreating = 0;
            u.path = findPathFor(u, yard->x, yard->y); u.pathIdx = 0;
        }
        if (human) setStatus("Raiders away — steal from their piles and run!");
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
static constexpr int  REP_VERSION  = 9;   // v9: 2x2 farm fields changed sim rules — old replays would desync

// ---- Command codec ----
// One binary layout — a flat int32 field sequence — shared by the replay
// file and the network wire. Playback of a multiplayer game re-derives the
// AI seats and reads BOTH humans' streams from the same records.
void encodeCommand(const Command& c, std::vector<int>& out) {
    out.push_back(c.type); out.push_back(c.player);
    out.push_back(c.x);  out.push_back(c.y);
    out.push_back(c.x2); out.push_back(c.y2);
    out.push_back(c.target); out.push_back(c.arg);
    out.push_back((int)c.units.size());
    for (int id : c.units) out.push_back(id);
}

int decodeCommand(const int* f, int avail, Command& c) {
    if (avail < 9) return 0;
    int n = f[8];
    if (n < 0 || n > 10000) return -1;
    if (avail < 9 + n) return 0;
    c.type = f[0]; c.player = f[1];
    c.x = f[2]; c.y = f[3]; c.x2 = f[4]; c.y2 = f[5];
    c.target = f[6]; c.arg = f[7];
    c.units.assign(f + 9, f + 9 + n);
    return 9 + n;
}

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
    MKDIR("replays");
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
    wrI(recF, g.layoutChoice);
    wrI(recF, g.difficulty);
    wrI(recF, g.humanMask);
    for (int i = 0; i < MAX_PLAYERS; i++) wrI(recF, g.civChoice[i]);
    fflush(recF);
    return true;
}

void replayStopRecording() {
    if (recF) { fclose(recF); recF = nullptr; platformPersistFiles(); }
}

// Leave playback mode (the splash replay browser returns to live play —
// without this, pushCommand would stay inert for the next real match).
void replayStopPlayback() {
    if (playF) { fclose(playF); playF = nullptr; }
    playbackMode = false;
    havePendingRec = false;
}

static void replayRecord(const Command& c) {
    if (!recF) return;
    std::vector<int> f;
    f.push_back(g.tick);
    encodeCommand(c, f);
    fwrite(f.data(), sizeof(int), f.size(), recF);
    fflush(recF);   // survive the mid-game exit(0) quit path
}

static bool replayReadNext() {
    havePendingRec = false;
    if (!playF) return false;
    Command c; int tick;
    if (!rdI(playF, tick)) { fclose(playF); playF = nullptr; return false; }
    // Fixed fields + unit count first, then the unit ids — the same int32
    // sequence decodeCommand expects, buffered so the codec stays shared.
    std::vector<int> f(9);
    if (fread(f.data(), sizeof(int), 9, playF) != 9) return false;
    int n = f[8];
    if (n < 0 || n > 10000) return false;
    f.resize(9 + n);
    if (n > 0 && fread(f.data() + 9, sizeof(int), n, playF) != (size_t)n) return false;
    if (decodeCommand(f.data(), (int)f.size(), c) <= 0) return false;
    pendingRec = std::move(c);
    pendingTick = tick;
    havePendingRec = true;
    return true;
}

bool replayLoadFile(const char* path, unsigned long long& seed, int& numAIs,
                    int& biomeChoice, int& layoutChoice, int& difficulty, int& humanMask) {
    playF = fopen(path, "rb");
    if (!playF) return false;
    char magic[4]; int ver;
    if (fread(magic, 4, 1, playF) != 1 || memcmp(magic, REP_MAGIC, 4) != 0
        || !rdI(playF, ver) || ver != REP_VERSION
        || !rdU64(playF, seed) || !rdI(playF, numAIs) || !rdI(playF, biomeChoice)
        || !rdI(playF, layoutChoice) || !rdI(playF, difficulty) || !rdI(playF, humanMask)) {
        fclose(playF); playF = nullptr; return false;
    }
    // Seat civ choices (-1 = rolled from the seed; initGame re-derives those).
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (!rdI(playF, g.civChoice[i])) { fclose(playF); playF = nullptr; return false; }
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
    // Network match: local commands travel to BOTH sims via the lockstep
    // scheduler (they run D ticks later, here and on the peer).
    if (netActive()) { netQueueLocal(c); return; }
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
    fnv(h, g.siteHoldOwner); fnv(h, g.siteHoldTicks);
    for (int p = 0; p <= MAX_PLAYERS; p++) {
        const Player& pl = g.players[p];
        fnv(h, pl.gold); fnv(h, pl.wood); fnv(h, pl.food);
        fnv(h, pl.supply); fnv(h, pl.supplyMax);
        fnv(h, pl.alive ? 1 : 0); fnv(h, pl.research);
        fnv(h, pl.era); fnv(h, pl.civ);
        fnv(h, pl.aiPersona); fnv(h, pl.aiWaveCd); fnv(h, pl.aiRaidCd);
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
        fnv(h, e.morale); fnv(h, e.routTicks); fnv(h, e.chargeSteps);
        fnv(h, e.stamina); fnv(h, e.kills);
        fnv(h, e.prisoner); fnv(h, e.origOwner); fnv(h, e.captureTicks);
        fnv(h, e.entrenchTicks);
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
