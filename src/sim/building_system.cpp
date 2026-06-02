#include "realm.h"
#include "core/game_events.h"
#include "core/world_index.h"

// ============================================================
// PASSIVE BUILDING TICKS
// ============================================================
void tickTowers() {
    tickTowers(g);
}

void tickTowers(Game& game) {
    // Towers always defend. Town Hall/Castle/House defend only when garrisoned.
    // Garrisoned archers add ranged punch; militia/knights add a smaller bonus.
    for (auto& e : game.entities) {
        if (!e.alive || e.underConstruction) continue;
        if (!isBuilding(e.type)) continue;
        if (e.type == E_CASTLE) {
            if (e.atkCd > 0) { e.atkCd--; continue; }
            std::vector<Entity*> targets;
            for (auto& o : game.entities) {
                if (!o.alive || o.owner == e.owner || o.owner == OWNER_NATURE || o.state == S_GARRISONED) continue;
                if (!isDetectedBy(game, o.x, o.y, e.owner)) continue;
                if (dist(e.x, e.y, o.x, o.y) <= 9) targets.push_back(&o);
            }
            std::sort(targets.begin(), targets.end(), [&](Entity* a, Entity* b) {
                return dist(e.x, e.y, a->x, a->y) < dist(e.x, e.y, b->x, b->y);
            });
            int sx = e.x + STATS[e.type].sizeW/2;
            int sy = e.y + STATS[e.type].sizeH/2;
            int fired = 0;
            for (Entity* en : targets) {
                if (fired >= 4) break;
                int dmg = damageVs(game, E_ARCHER, en->type, 12, en->owner);
                en->hp -= dmg;
                en->alertTicks = 12;
                spawnProjectile(game, sx, sy, en->x, en->y, '-', CP_PROJ_TOWER);
                if (en->hp <= 0) killEntity(game, *en);
                fired++;
            }
            if (fired > 0) e.atkCd = 6;
            continue;
        }
        bool isTower    = (e.type == E_TOWER);
        bool canGarrAtk = canGarrisonIn(e.type) && !e.garrison.empty();
        if (!isTower && !canGarrAtk) continue;

        int archers = 0, fighters = 0;
        for (int uid : e.garrison) {
            WorldIndex world = buildWorldIndex(game);
            Entity* u = findEntity(game, world, uid);
            if (!u || !u->alive) continue;
            if (u->type == E_ARCHER) archers++;
            else if (u->type == E_MILITIA || u->type == E_KNIGHT) fighters++;
        }
        int atk = STATS[e.type].atk + archers*5 + fighters*2;
        int rng = STATS[e.type].range;
        if (e.type == E_TOWNHALL && canGarrAtk) rng = std::max(rng, 6);
        if (e.type == E_CASTLE   && canGarrAtk) rng = std::max(rng, 8);
        if (e.type == E_HOUSE    && canGarrAtk) rng = std::max(rng, 4);
        if (rng <= 0 || atk <= 0) { if (e.atkCd > 0) e.atkCd--; continue; }

        int sx = e.x + STATS[e.type].sizeW/2;
        int sy = e.y + STATS[e.type].sizeH/2;
        // Build a temporary anchor entity for range checks (use e directly — its x,y is top-left, close enough)
        Entity* en = findNearestEnemy(game, e, rng);
        if (en) {
            if (e.atkCd <= 0) {
                // Towers/garrison-fire respect the same building-damage rule —
                // garrisoned archers can wear down walls but not blow them open.
                int dmg = damageVs(game, E_ARCHER, en->type, atk, en->owner);
                en->hp -= dmg; e.atkCd = isTower ? STATS[E_TOWER].atkSpeed : 9;
                en->alertTicks = 12;
                // Bolt-style projectile so tower fire reads as arrows instead of stars.
                spawnProjectile(game, sx, sy, en->x, en->y, '-', CP_PROJ_TOWER);
                if (en->hp <= 0) killEntity(game, *en);
            } else e.atkCd--;
        } else if (e.atkCd > 0) e.atkCd--;
    }
}

void tickGates() {
    tickGates(g);
}

void tickGates(Game& game) {
    for (auto& gate : game.entities) {
        if (!gate.alive || gate.type != E_GATE || gate.underConstruction) continue;
        if (gate.gateLocked) continue; // manually locked — don't auto-toggle
        bool allyNear = false;
        for (auto& u : game.entities) {
            if (!u.alive || u.owner != gate.owner || isBuilding(u.type)) continue;
            if (dist(u.x, u.y, gate.x, gate.y) <= 2) { allyNear = true; break; }
        }
        gate.gateOpen = allyNear;
    }
}

void tickFarms() {
    tickFarms(g);
}

void tickFarms(Game& game) {
    game.farmTimer++;
    if (game.farmTimer < 40) return;
    game.farmTimer = 0;

    // Wheat dies at the onset of winter
    if (getSeason(game) == WINTER) {
        for (auto& e : game.entities)
            if (e.alive && e.type == E_FARM) killEntity(game, e);
        return;
    }

    const int FARM_CAP = 20;
    int bonus = (getSeason(game) == SUMMER) ? 1 : 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        bool hasMill = false;
        for (auto& e : game.entities)
            if (e.alive && e.owner==p && e.type==E_MILL && !e.underConstruction) { hasMill=true; break; }

        for (auto& farm : game.entities) {
            if (!farm.alive || farm.type!=E_FARM || farm.owner!=p || farm.underConstruction) continue;
            // Any adjacent peasant (explicitly tending or just idle nearby) keeps the wheat growing
            bool tended = false;
            for (auto& u : game.entities) {
                if (!u.alive || u.owner!=p || !isWorker(u.type)) continue;
                if (dist(u.x, u.y, farm.x, farm.y) <= 1) { tended=true; break; }
            }
            // Food ripens on the farm itself (capped); a courier peasant carries it
            // to a Mill or Town Hall — see S_BUILDING farm-tending branch.
            if (tended && farm.storedFood < FARM_CAP) {
                int yield = (hasMill ? 6 : 3) + bonus;
                farm.storedFood = std::min(FARM_CAP, farm.storedFood + yield);
            }

            // AI helper: if ripe food is sitting on an AI farm with no courier
            // assigned, grab the nearest idle owner-peasant and send them to tend.
            // Player keeps explicit control — never auto-yanks the player's peasants.
            if (p != 0 && farm.storedFood >= 3) {
                bool assigned = false;
                for (auto& u : game.entities) {
                    if (!u.alive || u.owner!=p || !isWorker(u.type)) continue;
                    if (u.state == S_BUILDING && u.targetId == farm.id) { assigned = true; break; }
                }
                if (!assigned) {
                    Entity* best = nullptr; int bestD = 99999;
                    for (auto& u : game.entities) {
                        if (!u.alive || u.owner!=p || !isWorker(u.type)) continue;
                        if (u.state != S_IDLE) continue;
                        if (u.cargo.amount > 0) continue;
                        int d = mdist(u.x, u.y, farm.x, farm.y);
                        if (d <= 12 && d < bestD) { bestD = d; best = &u; }
                    }
                    if (best) {
                        WorldIndex world = buildWorldIndex(game);
                        orderHelp(game, world, *best, farm.id);
                    }
                }
            }
        }
    }
}

void tickMarkets() {
    tickMarkets(g);
}

void tickMarkets(Game& game) {
    if (game.tick % 50 != 0) return;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        int m = 0;
        for (auto& e : game.entities) if (e.alive && e.owner==p && e.type==E_MARKET && !e.underConstruction) m++;
        game.players[p].gold += m * 5;
    }
}

void tickChurches() {
    tickChurches(g);
}

void tickChurches(Game& game) {
    for (auto& e : game.entities) {
        if (!e.alive || e.type!=E_CHURCH || e.underConstruction) continue;
        for (auto& u : game.entities) {
            if (!u.alive || !isUnit(u.type) || u.owner == OWNER_NATURE || u.state == S_GARRISONED) continue;
            if (u.owner == e.owner) {
                if (game.tick % 25 == 0 && dist(u.x,u.y,e.x,e.y) <= 6 && u.hp < u.maxHp)
                    u.hp = std::min(u.maxHp, u.hp + 1);
                continue;
            }
            if (dist(u.x,u.y,e.x,e.y) <= 6) {
                u.convertTicks++;
                int threshold = 200 + u.maxHp * 3;
                if (u.convertTicks >= threshold) {
                    int oldOwner = u.owner;
                    u.owner = e.owner;
                    u.convertTicks = 0;
                    u.state = S_IDLE;
                    u.targetId = -1;
                    u.path.clear();
                    u.pathIdx = 0;
                    updateSupply(game, oldOwner);
                    updateSupply(game, u.owner);
                    if (oldOwner == 0) emitStatusEvent(oldOwner, std::string(STATS[u.type].name) + " was converted.");
                    else if (u.owner == 0) emitStatusEvent(u.owner, std::string(STATS[u.type].name) + " converted.");
                }
            } else if (u.convertTicks > 0 && game.tick % 8 == 0) {
                u.convertTicks--;
            }
        }
    }
}
