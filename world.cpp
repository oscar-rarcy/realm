#include "realm.h"

// ============================================================
// PASSIVE BUILDING TICKS
// ============================================================
void tickTowers() {
    // Towers always defend. Town Hall/Castle/House defend only when garrisoned.
    // Garrisoned archers add ranged punch; militia/knights add a smaller bonus.
    for (auto& e : g.entities) {
        if (!e.alive || e.underConstruction) continue;
        if (!isBuilding(e.type)) continue;
        bool isTower    = (e.type == E_TOWER);
        bool isCastle   = (e.type == E_CASTLE);
        bool canGarrAtk = canGarrisonIn(e.type) && !e.garrison.empty();
        if (!isTower && !isCastle && !canGarrAtk) continue;

        int archers = 0, fighters = 0;
        for (int uid : e.garrison) {
            Entity* u = findEntity(uid);
            if (!u || !u->alive) continue;
            if (u->type == E_ARCHER) archers++;
            else if (u->type == E_MILITIA || u->type == E_KNIGHT) fighters++;
        }
        int atk = STATS[e.type].atk + archers*5 + fighters*2;
        int rng = STATS[e.type].range;
        if (e.type == E_TOWNHALL && canGarrAtk) rng = std::max(rng, 6);
        if (isCastle) { rng = std::max(rng, 9); atk = std::max(atk, 12); }
        if (e.type == E_HOUSE    && canGarrAtk) rng = std::max(rng, 4);
        if (rng <= 0 || atk <= 0) { if (e.atkCd > 0) e.atkCd--; continue; }

        int sx = e.x + STATS[e.type].sizeW/2;
        int sy = e.y + STATS[e.type].sizeH/2;
        // Castles loose a volley: up to 4 arrows at the 4 nearest enemies per cycle.
        // Towers/halls/houses still fire a single bolt.
        int volley = isCastle ? 4 : 1;
        int aShots = 0;
        if (e.atkCd <= 0) {
            // Pick the N nearest enemies in range and shoot each one.
            std::vector<Entity*> targets;
            bool concealing = isConcealing();
            for (auto& o : g.entities) {
                if (!o.alive || o.owner == e.owner) continue;
                if (o.state == S_GARRISONED) continue;
                if (e.owner != OWNER_NATURE && o.owner == OWNER_NATURE) continue;
                if (dist(sx, sy, o.x, o.y) > rng) continue;
                // Towers/castles can't shoot what they can't see — wheat hides
                // enemies just like night/storm does, unless close-detected.
                bool inCrop = !isBuilding(o.type) && g.map[o.y][o.x].terrain == T_WHEAT;
                if ((concealing || inCrop) && o.owner != OWNER_NATURE
                        && e.owner < MAX_PLAYERS
                        && !isDetectedBy(o.x, o.y, e.owner)) continue;
                targets.push_back(&o);
            }
            std::sort(targets.begin(), targets.end(), [sx,sy](Entity* a, Entity* b){
                return dist(sx,sy,a->x,a->y) < dist(sx,sy,b->x,b->y);
            });
            for (Entity* en : targets) {
                if (aShots >= volley) break;
                int dmg = damageVs(E_ARCHER, en->type, atk, en->owner);
                en->hp -= dmg; en->alertTicks = 12;
                spawnProjectile(sx, sy, en->x, en->y, '-', CP_PROJ_TOWER);
                if (en->hp <= 0) killEntity(*en);
                aShots++;
            }
            if (aShots > 0) {
                e.atkCd = isTower ? STATS[E_TOWER].atkSpeed : (isCastle ? 6 : 9);
            }
        } else e.atkCd--;
    }
}

void tickGates() {
    for (auto& gate : g.entities) {
        if (!gate.alive || gate.type != E_GATE || gate.underConstruction) continue;
        if (gate.gateLocked) continue; // manually locked — don't auto-toggle
        bool allyNear = false;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != gate.owner || isBuilding(u.type)) continue;
            if (dist(u.x, u.y, gate.x, gate.y) <= 2) { allyNear = true; break; }
        }
        gate.gateOpen = allyNear;
    }
}

void tickFarms() {
    g.farmTimer++;
    if (g.farmTimer < 40) return;
    g.farmTimer = 0;

    // Wheat dies at the onset of winter
    if (getSeason() == WINTER) {
        for (auto& e : g.entities)
            if (e.alive && e.type == E_FARM) { e.alive = false; e.state = S_DEAD; }
        return;
    }

    const int FARM_CAP = 20;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        bool hasMill = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==p && e.type==E_MILL && !e.underConstruction) { hasMill=true; break; }

        for (auto& farm : g.entities) {
            if (!farm.alive || farm.type!=E_FARM || farm.owner!=p || farm.underConstruction) continue;
            // Any adjacent peasant keeps the wheat growing.
            bool tended = false;
            for (auto& u : g.entities) {
                if (!u.alive || u.owner!=p || u.type!=E_PEASANT) continue;
                if (dist(u.x, u.y, farm.x, farm.y) <= 1) { tended=true; break; }
            }
            // Mill doubles output; summer adds +1. No Mill still works — just slower.
            if (tended && farm.carrying < FARM_CAP) {
                int rate = hasMill ? 6 : 3;
                if (getSeason() == SUMMER) rate++;
                farm.carrying = std::min(FARM_CAP, farm.carrying + rate);
            }

            // AI helper: if ripe food is sitting on an AI farm with no courier
            // assigned, grab the nearest idle owner-peasant and send them to tend.
            // Player keeps explicit control — never auto-yanks the player's peasants.
            if (p != 0 && farm.carrying >= 3) {
                bool assigned = false;
                for (auto& u : g.entities) {
                    if (!u.alive || u.owner!=p || u.type!=E_PEASANT) continue;
                    if (u.state == S_BUILDING && u.targetId == farm.id) { assigned = true; break; }
                }
                if (!assigned) {
                    Entity* best = nullptr; int bestD = 99999;
                    for (auto& u : g.entities) {
                        if (!u.alive || u.owner!=p || u.type!=E_PEASANT) continue;
                        if (u.state != S_IDLE) continue;
                        if (u.carrying > 0) continue;
                        int d = mdist(u.x, u.y, farm.x, farm.y);
                        if (d <= 12 && d < bestD) { bestD = d; best = &u; }
                    }
                    if (best) orderHelp(*best, farm.id);
                }
            }
        }
    }
}

void tickMarkets() {
    if (g.tick % 50 != 0) return;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        int m = 0;
        for (auto& e : g.entities) if (e.alive && e.owner==p && e.type==E_MARKET && !e.underConstruction) m++;
        g.players[p].gold += m * 5;
    }
}

void tickChurches() {
    const int CHURCH_RANGE = 6;
    for (auto& ch : g.entities) {
        if (!ch.alive || ch.type != E_CHURCH || ch.underConstruction) continue;

        // Heal friendly units in range every 25 ticks.
        if (g.tick % 25 == 0) {
            for (auto& u : g.entities) {
                if (!u.alive || u.owner != ch.owner || !isUnit(u.type)) continue;
                if (u.state == S_GARRISONED) continue;
                if (dist(u.x, u.y, ch.x, ch.y) <= CHURCH_RANGE && u.hp < u.maxHp)
                    u.hp = std::min(u.maxHp, u.hp + 1);
            }
        }

        // Conversion: enemy units in range slowly accumulate faith pressure.
        // Threshold scales with unit toughness so knights take much longer than peasants.
        for (auto& u : g.entities) {
            if (!u.alive || u.owner == ch.owner || u.owner == OWNER_NATURE) continue;
            if (!isUnit(u.type) || isBuilding(u.type)) continue;
            if (u.state == S_GARRISONED) continue;
            if (dist(u.x, u.y, ch.x, ch.y) <= CHURCH_RANGE) {
                u.convertTicks++;
                int threshold = 200 + u.maxHp * 3; // ~300 for peasants, ~530 for knights
                if (u.convertTicks >= threshold) {
                    int oldOwner = u.owner;
                    u.owner = ch.owner;
                    u.convertTicks = 0;
                    u.state = S_IDLE; u.path.clear(); u.pathIdx = 0;
                    updateSupply(oldOwner);
                    updateSupply(ch.owner);
                    if (ch.owner == 0)
                        setStatus(std::string(STATS[u.type].name) + " converted to your cause!");
                    else if (oldOwner == 0)
                        setStatus("A unit has been turned against you!");
                }
            } else {
                if (u.convertTicks > 0) u.convertTicks--;  // slow decay when out of range
            }
        }
    }
}

// ============================================================
// SEASONS — winter transformation, spring thaw, hunger
// ============================================================
static void applyWinter() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.preWinterTerrain = t.terrain;
        switch (t.terrain) {
            case T_GRASS: case T_TALL_GRASS: case T_FLOWERS: case T_MEADOW:
            case T_DIRT:  case T_ROAD:       case T_GRAVEL:  case T_RUINS:
            case T_SAND:  case T_DUNES:      case T_WHEAT:   case T_BERRY:
            case T_MUD:   case T_CASTLE_FLOOR:
                t.terrain = T_SNOW; break;
            case T_WATER: case T_SHALLOWS: case T_MARSH: case T_REEDS: {
                // Partial freeze: deeper water freezes less readily than shallows/marsh.
                unsigned h = ((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u) ^ 0xCAFEBABEu;
                int pct = (t.terrain == T_WATER) ? 60 : (t.terrain == T_MARSH) ? 80 : 75;
                if ((h % 100) < (unsigned)pct) t.terrain = T_ICE;
                break;
            }
            default: break; // forests, hills, mountains, gold, walls, stone keep their look
        }
    }
    // Cull a chunk of wildlife — the herd is thinned by the cold.
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;
        if (e.type != E_DEER && e.type != E_SHEEP && e.type != E_BOAR) continue;
        if (simRand() % 100 < 35) killEntity(e);
    }
    if (g.players[0].alive) setStatus("Winter falls. The land freezes over.");
}

void tickSeasons() {
    // Decrement the under-attack notification cooldown once per tick.
    if (g.attackNotifyCd > 0) g.attackNotifyCd--;

    // Season transitions.
    int s = (int)getSeason();
    if (s != g.prevSeason) {
        if (s == WINTER) applyWinter();
        if (g.players[0].alive) {
            if (s == SPRING && g.prevSeason == WINTER)
                setStatus("The frost retreats. New life stirs across the land.");
            else if (s == SUMMER)
                setStatus("Summer is upon the realm. Long days and warm soil.");
            else if (s == AUTUMN)
                setStatus("Autumn descends. The harvest calls — winter is not far.");
        }
        g.prevSeason = s;
    }

    // Time-of-day transitions (fire once per phase crossing, not every tick).
    if (g.players[0].alive) {
        int phase;
        if (isDawn())       phase = 3;
        else if (isDusk())  phase = 1;
        else if (isNight()) phase = 2;
        else                phase = 0; // day
        if (phase != g.prevTimePhase) {
            if      (phase == 3) setStatus("Dawn breaks. The realm stirs to life.");
            else if (phase == 1) setStatus("Evening falls. The shadows lengthen.");
            else if (phase == 2) setStatus("Night. Stars keep watch over the sleeping land.");
            // No message for plain day — players see it clearly.
            g.prevTimePhase = phase;
        }
    }
}

void tickThaw() {
    if (g.tick % 5 != 0) return;
    if (getSeason() != SPRING) return;
    float progress = getSeasonProgress();
    // Patchy melt completes by ~40% of spring — earlier sessions felt snowy way too
    // deep into the season. Tiles thaw faster, world greens up quickly.
    int threshold = std::max(0, (int)(progress * 2600.0f));
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        if (t.terrain != T_SNOW && t.terrain != T_ICE) continue;
        if (t.preWinterTerrain == t.terrain) continue;
        unsigned h = ((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u);
        if ((int)(h & 0x3ff) < threshold) t.terrain = t.preWinterTerrain;
    }
}

void tickWinter() {
    if (getSeason() != WINTER) return;
    if (g.tick % 100 != 0) return;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!g.players[p].alive) continue;
        int unitCount = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
            unitCount++;
        }
        if (unitCount == 0) continue;
        Player& pl = g.players[p];
        int drain = unitCount; // 1 food per unit per 100 ticks
        if (pl.food >= drain) {
            spendPlayerFood(p, drain);
        } else {
            int starve = drain - pl.food;
            spendPlayerFood(p, pl.food);
            // Damage `starve` random units. If any die, they're gone.
            int hits = 0;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
                e.hp -= 3;
                if (e.hp <= 0) killEntity(e);
                if (++hits >= starve) break;
            }
            if (p == 0) setStatus("Starvation! Units are losing health.");
        }
    }
}

// ============================================================
// PAVING — building creep + path wear + decay
// ============================================================
void tickPaving() {
    // Buildings emit creep into adjacent natural ground.
    if (g.tick % 100 == 0) {
        for (auto& e : g.entities) {
            if (!e.alive || !isBuilding(e.type) || e.underConstruction) continue;
            auto& s = STATS[e.type];
            for (int dy = -3; dy <= s.sizeH+2; dy++) for (int dx = -3; dx <= s.sizeW+2; dx++) {
                if (dx >= 0 && dx < s.sizeW && dy >= 0 && dy < s.sizeH) continue;
                int nx = e.x+dx, ny = e.y+dy;
                if (!inBounds(nx,ny)) continue;
                int ringDist = std::max(std::max(0, -dx), std::max(0, dx-s.sizeW+1))
                             + std::max(std::max(0, -dy), std::max(0, dy-s.sizeH+1));
                if (ringDist > 3) continue;
                Tile& t = g.map[ny][nx];
                Terrain ter = t.terrain;
                if (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW
                 || ter==T_SAND ||ter==T_DUNES) {
                    int gain = (ringDist <= 1) ? 5 : (ringDist == 2) ? 3 : 1;
                    if (t.wear < 80) t.wear += gain;
                    // Lower threshold so visible haloes appear within ~50 seconds.
                    if (t.wear >= 30 && (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW)) {
                        t.terrain = T_DIRT; t.preWinterTerrain = T_DIRT;
                    }
                }
            }
        }
    }
    // Decay: unused paving gradually returns to nature.
    if (g.tick % 250 == 0) {
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Tile& t = g.map[y][x];
            if (t.wear > 0) t.wear--;
            if (t.wear == 0 && t.terrain == T_ROAD) {
                t.terrain = T_DIRT; t.preWinterTerrain = T_DIRT;
            }
            // Dirt slowly regrows — patches of grass return after long disuse
            if (t.wear == 0 && t.terrain == T_DIRT && (simRand() % 500) == 0) {
                t.terrain = T_GRASS; t.preWinterTerrain = T_GRASS;
            }
        }
    }
}

// ============================================================
// WEATHER
// ============================================================
void tickWeather() {
    Season s = getSeason();
    float sp = getSeasonProgress();

    // Snow doesn't create mud; rain/storm do.
    if (g.tick % 50 == 0) {
        if (g.weather == W_RAIN || g.weather == W_STORM) {
            int hits = (g.weather == W_STORM) ? 60 : 30;
            for (int i = 0; i < hits; i++) {
                int x = simRand() % MAP_W, y = simRand() % MAP_H;
                Tile& t = g.map[y][x];
                if (t.terrain == T_GRASS || t.terrain == T_MEADOW
                 || t.terrain == T_DIRT  || t.terrain == T_TALL_GRASS) {
                    t.terrain = T_MUD;
                }
            }
        } else {
            // Drying — mud reverts to dirt once skies clear (or freeze over).
            for (int i = 0; i < 40; i++) {
                int x = simRand() % MAP_W, y = simRand() % MAP_H;
                Tile& t = g.map[y][x];
                if (t.terrain == T_MUD) t.terrain = T_DIRT;
            }
        }
    }

    // Season-appropriate weather: rain/storm can't persist into winter; snow can't persist into spring/summer.
    if (s == WINTER && (g.weather == W_RAIN || g.weather == W_STORM)) {
        g.weather = W_SNOW;
        g.weatherTimer = 300;
        if (g.players[0].alive) setStatus("The rain turns to snow.");
        return;
    }
    bool lateAutumn = (s == AUTUMN && sp > 0.5f);
    if (!lateAutumn && s != WINTER && g.weather == W_SNOW) {
        g.weather = W_CLEAR;
        g.weatherTimer = 100;
        if (g.players[0].alive) setStatus("The skies clear.");
        return;
    }

    if (g.weatherTimer > 0) { g.weatherTimer--; return; }

    int roll = simRand() % 100;

    if (s == WINTER) {
        // Winter: only clear or snow.
        if (g.weather == W_CLEAR) {
            if (roll < 40) { g.weather = W_SNOW; g.weatherTimer = 500 + simRand() % 900; }
            else             g.weatherTimer = 300 + simRand() % 500;
        } else { // W_SNOW
            if (roll < 50) g.weather = W_CLEAR;
            g.weatherTimer = 300 + simRand() % 600;
        }
    } else if (lateAutumn) {
        // Late autumn: rain fades, first snows begin. Progress 0.5→1 maps to 0→1 of this range.
        float late = (sp - 0.5f) * 2.0f;
        int snowBias  = (int)(late * 30);           // up to 30% snow chance by end of autumn
        int rainBias  = (int)(50 * (1.0f - late * 0.6f)); // rain fades 50→20
        int stormBias = (int)(15 * (1.0f - late));  // storms fade out entirely
        if (g.weather == W_CLEAR) {
            if      (roll < stormBias)              g.weather = W_STORM;
            else if (roll < rainBias)               g.weather = W_RAIN;
            else if (roll < rainBias + snowBias)    g.weather = W_SNOW;
            g.weatherTimer = 400 + simRand() % 800;
        } else {
            if (roll < 60) g.weather = W_CLEAR;
            else if (g.weather == W_RAIN  && roll < 75) g.weather = W_STORM;
            else if (g.weather == W_STORM && roll < 80) g.weather = W_RAIN;
            // snow just clears, doesn't escalate
            g.weatherTimer = 300 + simRand() % 600;
        }
    } else {
        // Spring / summer / early autumn: rain and storms only.
        int rainBias  = (s == AUTUMN) ? 50 : (s == SPRING) ? 35 : 25;
        int stormBias = (s == AUTUMN) ? 15 : 8;
        if (g.weather == W_CLEAR) {
            if (roll < stormBias)     g.weather = W_STORM;
            else if (roll < rainBias) g.weather = W_RAIN;
            g.weatherTimer = 400 + simRand() % 800;
        } else {
            if (roll < 60) g.weather = W_CLEAR;
            else if (g.weather == W_RAIN  && roll < 75) g.weather = W_STORM;
            else if (g.weather == W_STORM && roll < 80) g.weather = W_RAIN;
            g.weatherTimer = 300 + simRand() % 600;
        }
    }

    if (g.players[0].alive) {
        if      (g.weather == W_RAIN)  setStatus("Rain begins.");
        else if (g.weather == W_STORM) setStatus("A storm rolls in!");
        else if (g.weather == W_SNOW)  setStatus("Snow begins to fall.");
        else                           setStatus("The skies clear.");
    }
}

// ============================================================
// ANIMALS
// ============================================================
void tickAnimals() {
    static int atick = 0; atick++;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;

        // Deer flee in herds: one spooked deer panics nearby deer in the same direction.
        if (e.type == E_DEER) {
            if (e.state != S_MOVING || e.path.empty()) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 5) {
                        int fx = std::max(1, std::min(e.x + (e.x-o.x)*4, MAP_W-2));
                        int fy = std::max(1, std::min(e.y + (e.y-o.y)*4, MAP_H-2));
                        if (isPassable(fx, fy)) {
                            orderMove(e, fx, fy);
                            // Spook nearby herd members to bolt the same way.
                            for (auto& nb : g.entities) {
                                if (!nb.alive || nb.type != E_DEER || nb.id == e.id) continue;
                                if (nb.state == S_MOVING && !nb.path.empty()) continue;
                                if (dist(e.x, e.y, nb.x, nb.y) > 6) continue;
                                int nbfx = std::max(1, std::min(nb.x+(nb.x-o.x)*4, MAP_W-2));
                                int nbfy = std::max(1, std::min(nb.y+(nb.y-o.y)*4, MAP_H-2));
                                if (isPassable(nbfx, nbfy)) orderMove(nb, nbfx, nbfy);
                            }
                        }
                        break;
                    }
                }
            }
        }

        // Sheep still flee individually.
        if (e.type == E_SHEEP) {
            if (e.state != S_MOVING || e.path.empty()) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 4) {
                        int fx = std::max(1, std::min(e.x + (e.x-o.x)*4, MAP_W-2));
                        int fy = std::max(1, std::min(e.y + (e.y-o.y)*4, MAP_H-2));
                        if (isPassable(fx, fy)) orderMove(e, fx, fy);
                        break;
                    }
                }
            }
        }

        // Boars only charge when provoked (alertTicks > 0). Calm boars are passive.
        if (e.type == E_BOAR && e.alertTicks > 0) {
            if (e.state == S_IDLE || (e.state == S_MOVING && e.path.empty())) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner == OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 4) { orderAttack(e, o.id); break; }
                }
            }
        }

        // Wolves: give buildings a modest berth in summer/spring; bolder in autumn/winter.
        if (e.type == E_WOLF) {
            bool winter = (getSeason() == WINTER);
            int huntRange = winter ? 8 : 5;
            int settleAvoid = winter ? 0 : 8; // smaller avoid radius — wolves press closer
            bool nearSettlement = false;
            int fleeX = -1, fleeY = -1;
            if (settleAvoid > 0) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner == OWNER_NATURE || !isBuilding(o.type)) continue;
                    int d = dist(e.x, e.y, o.x, o.y);
                    if (d <= settleAvoid) {
                        nearSettlement = true;
                        fleeX = std::max(1, std::min(e.x + (e.x - o.x)*3, MAP_W-2));
                        fleeY = std::max(1, std::min(e.y + (e.y - o.y)*3, MAP_H-2));
                        break;
                    }
                }
            }
            if (nearSettlement) {
                if (e.state == S_ATTACKING) { e.state = S_IDLE; e.path.clear(); }
                if (e.state == S_IDLE && fleeX >= 0 && isPassable(fleeX, fleeY))
                    orderMove(e, fleeX, fleeY);
            } else if (e.state==S_IDLE || (e.state==S_MOVING && e.path.empty())) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= huntRange) { orderAttack(e, o.id); break; }
                }
            }
        }

        // Random wander when idle
        if (e.state == S_IDLE && atick % (35 + (e.id%25)) == 0) {
            int wx = e.x + (simRand()%9)-4, wy = e.y + (simRand()%9)-4;
            wx = std::max(1, std::min(wx, MAP_W-2));
            wy = std::max(1, std::min(wy, MAP_H-2));
            if (isPassable(wx, wy)) orderMove(e, wx, wy);
        }
    }
}

// ============================================================
// WIN CONDITION
// ============================================================
void checkWin() {
    int aliveCount = 0; int lastAlive = -1;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!g.players[p].alive) continue;
        bool hasBase = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==p && (e.type==E_TOWNHALL||e.type==E_CASTLE)) { hasBase=true; break; }
        if (!hasBase) g.players[p].alive = false;
        else { aliveCount++; lastAlive = p; }
    }
    // Human defeat ends the match immediately — no point watching the AIs fight
    // each other after the player's been eliminated.
    if (!g.players[0].alive) { g.winner = -1; g.mode = M_GAME_OVER; return; }
    if (aliveCount <= 1) { g.winner = lastAlive; g.mode = M_GAME_OVER; }
}
