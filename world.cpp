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
        if (e.type == E_WALL     && canGarrAtk) rng = std::max(rng, 5);  // parapet archer
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
                int dmg = shieldBuilding(*en, damageVs(E_ARCHER, en->type, atk, en->owner));
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

    for (int p = 0; p < MAX_PLAYERS; p++) {
        bool hasMill = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==p && e.type==E_MILL && !e.underConstruction) { hasMill=true; break; }

        for (auto& farm : g.entities) {
            if (!farm.alive || farm.type!=E_FARM || farm.owner!=p || farm.underConstruction) continue;
            // Any peasant beside the field keeps the wheat growing.
            bool tended = false;
            for (auto& u : g.entities) {
                if (!u.alive || u.owner!=p || u.type!=E_PEASANT) continue;
                if (distToBuilding(u.x, u.y, farm) <= 1) { tended=true; break; }
            }
            // A claimed riverside watermill is a half-measure mill.
            bool hasWatermill = false;
            if (!hasMill)
                for (auto& e : g.entities)
                    if (e.alive && e.owner==p && e.type==E_WATERMILL) { hasWatermill = true; break; }
            // Mill doubles output. The farming year has a shape now:
            // spring = planting (slow), summer = steady, early autumn =
            // the great harvest (double), late autumn = fields spent.
            // Stockpile in autumn or starve in winter — and everyone's
            // bursting granaries make autumn the season to raid.
            // Rates are for the full 2x2 field — four plants, one tender.
            if (tended && farm.carrying < FARM_CAP) {
                int rate = hasMill ? 12 : hasWatermill ? 8 : 6;
                Season ss = getSeason();
                float sp = getSeasonProgress();
                if      (ss == SPRING)             rate = std::max(2, rate / 2);
                else if (ss == SUMMER)             rate += 2;
                else if (ss == AUTUMN && sp < 0.6f) rate *= 2;
                else if (ss == AUTUMN)             rate = 0;   // fields spent
                if (rate > 0) {
                    // Better iron and better land: bonuses only help fields
                    // that are actually producing — spent is spent.
                    if (g.players[p].research & R_HEAVY_PLOUGH) rate += 2;
                    if (g.players[p].civ == CIV_FENLANDERS)     rate += 2;
                }
                farm.carrying = std::min(FARM_CAP, farm.carrying + rate);
            }
            // A worked field seeds its verges: wheat creeps into the open
            // grass around the square, so old farmland grows wild and
            // ragged at the edges — and yields new ground worth sowing.
            if (tended && !(getSeason() == AUTUMN && getSeasonProgress() >= 0.6f)
                && simRand() % 6 == 0) {
                auto& fs = STATS[E_FARM];
                int vx[16], vy[16], vn = 0;
                for (int dy = -1; dy <= fs.sizeH; dy++) for (int dx = -1; dx <= fs.sizeW; dx++) {
                    if (dx >= 0 && dx < fs.sizeW && dy >= 0 && dy < fs.sizeH) continue;
                    int nx = farm.x+dx, ny = farm.y+dy;
                    if (!inBounds(nx,ny) || vn >= 16) continue;
                    Terrain vt = g.map[ny][nx].terrain;
                    if (vt==T_GRASS||vt==T_MEADOW||vt==T_TALL_GRASS||vt==T_FLOWERS)
                        { vx[vn]=nx; vy[vn]=ny; vn++; }
                }
                if (vn > 0) {
                    int k = simRand() % vn;
                    g.map[vy[k]][vx[k]].terrain = T_WHEAT;
                }
            }

            // AI helper: if ripe food is sitting on an AI farm with no courier
            // assigned, grab the nearest idle owner-peasant and send them to tend.
            // Player keeps explicit control — never auto-yanks the player's peasants.
            if (!((g.humanMask >> p) & 1) && farm.carrying >= 3) {
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
    for (auto& e : g.entities) {
        if (!e.alive || e.owner >= MAX_PLAYERS || e.underConstruction) continue;
        // Market income lands in the nearest vault, not thin air.
        if (e.type == E_MARKET)
            depositToNearest(e.owner, 5, 0, 0, 0, e.x, e.y);
        // A claimed trading post tolls the road into its own strongbox.
        if (e.type == E_TRADING_POST) {
            g.players[e.owner].gold += 3;
            e.storeGold = std::min(e.storeGold + 3, depotCapGold(E_TRADING_POST));
        }
        // Manor tax: +1 gold per worked farm within 6 tiles.
        if (e.type == E_MANOR) {
            int tax = 0;
            for (auto& f : g.entities) {
                if (!f.alive || f.owner != e.owner || f.type != E_FARM || f.underConstruction) continue;
                if (dist(f.x, f.y, e.x, e.y) > 6) continue;
                for (auto& u : g.entities) {
                    if (!u.alive || u.owner != e.owner || u.type != E_PEASANT) continue;
                    if (distToBuilding(u.x, u.y, f) <= 1) { tax++; break; }
                }
            }
            if (tax > 0) depositToNearest(e.owner, tax, 0, 0, 0, e.x, e.y);
        }
    }
}

// Larder decay: meat and fish keep about a season, berries half that.
// Winter is the great preserver — the freeze halts meat/fish spoilage,
// which is exactly why the autumn hunt fills the granaries.
void tickSpoilage() {
    if (g.tick % 100 != 0) return;
    bool frozen = (getSeason() == WINTER);
    for (auto& e : g.entities) {
        if (!e.alive || e.owner >= MAX_PLAYERS || !isBuilding(e.type)) continue;
        int rot = 0;
        if (!frozen) {
            if (e.storeFood[F_MEAT] > 0) { int r = std::max(1, e.storeFood[F_MEAT]/30); e.storeFood[F_MEAT] -= r; rot += r; }
            if (e.storeFood[F_FISH] > 0) { int r = std::max(1, e.storeFood[F_FISH]/30); e.storeFood[F_FISH] -= r; rot += r; }
        }
        if (e.storeFood[F_BERRY] > 0) { int r = std::max(1, e.storeFood[F_BERRY]/15); e.storeFood[F_BERRY] -= r; rot += r; }
        if (rot > 0) g.players[e.owner].food -= std::min(rot, g.players[e.owner].food);
    }
}

// Taverns brew grain into ale and warm passing soldiers with it.
void tickTaverns() {
    // Brewing: 2 grain -> 1 ale, one batch per tavern per 50 ticks.
    if (g.tick % 50 == 0) {
        for (auto& e : g.entities) {
            if (!e.alive || e.owner >= MAX_PLAYERS || e.type != E_TAVERN || e.underConstruction) continue;
            if (depotFoodSum(e) >= depotCapFood(E_TAVERN)) continue;
            if (spendFoodKind(e.owner, F_GRAIN, 2)) e.storeFood[F_ALE] += 1;
        }
    }
    // Ale-warmed: military passing within 4 of a stocked tavern drinks one.
    // +1 atk and frostbite immunity for ~600 ticks; ranged aim suffers -1.
    if (g.tick % 25 == 0) {
        for (auto& e : g.entities) {
            if (!e.alive || e.owner >= MAX_PLAYERS || e.type != E_TAVERN || e.underConstruction) continue;
            if (e.storeFood[F_ALE] <= 0) continue;
            for (auto& u : g.entities) {
                if (e.storeFood[F_ALE] <= 0) break;
                if (!u.alive || u.owner != e.owner || !isUnit(u.type) || isNaval(u.type)) continue;
                if (STATS[u.type].atk <= 0 || u.state == S_GARRISONED || u.aleTicks > 0) continue;
                if (dist(u.x, u.y, e.x, e.y) > 4) continue;
                e.storeFood[F_ALE]--;
                u.aleTicks = 600;
            }
        }
    }
}

// Prisoners (docs/combat-feel-proposals.md 3.3): a captive is marched to the
// captor's nearest hold and ransomed for coin — or freed if a soldier of his
// old allegiance reaches him first.
void tickPrisoners() {
    if (g.tick % 20 != 0) return;
    for (auto& p : g.entities) {
        if (!p.alive || !p.prisoner) continue;
        // Rescue: an unbroken soldier of the old colours reaches the captive.
        bool rescued = false;
        for (auto& o : g.entities) {
            if (!o.alive || o.owner != p.origOwner || o.prisoner) continue;
            if (!isUnit(o.type) || o.state == S_GARRISONED) continue;
            if (dist(p.x, p.y, o.x, o.y) <= 1) { rescued = true; break; }
        }
        if (rescued) {
            int captor = p.owner;
            p.prisoner = 0; p.owner = p.origOwner; p.origOwner = -1;
            p.morale = 40; p.state = S_IDLE; p.path.clear(); p.pathIdx = 0;
            updateSupply(captor); updateSupply(p.owner);
            if (p.owner == g.localPlayer) setStatus("A captured soldier breaks free!");
            continue;
        }
        // March to the captor's nearest hold; ransom on arrival.
        Entity* hold = nullptr; int bestD = 99999;
        for (auto& o : g.entities) {
            if (!o.alive || o.owner != p.owner || o.underConstruction) continue;
            if (o.type != E_TOWNHALL && o.type != E_CASTLE) continue;
            int d = dist(p.x, p.y, o.x, o.y);
            if (d < bestD) { bestD = d; hold = &o; }
        }
        if (!hold) continue;   // no hold to march to — just held where he stands
        if (bestD <= 2) {
            const int ransom = 25;
            g.players[p.owner].gold += ransom;
            if (p.owner == g.localPlayer) setStatus("Prisoner ransomed: +25 gold.");
            int captor = p.owner, orig = p.origOwner;
            p.alive = false; p.state = S_DEAD; p.prisoner = 0;
            updateSupply(captor); if (orig >= 0) updateSupply(orig);
        } else if (p.state != S_MOVING || p.path.empty()) {
            p.path = findPathFor(p, hold->x, hold->y); p.pathIdx = 0;
            p.state = S_MOVING; p.targetX = hold->x; p.targetY = hold->y;
        }
    }
}

void tickChurches() {
    // Shrines: old stones mend whoever rests beside them; a garrisoned monk
    // projects the blessing to radius 4 for the claimant's units. Wells give
    // the owner's peasants a slow mend. Stonemasons patch nearby buildings,
    // chewing through the map's stone deposits to do it.
    if (g.tick % 15 == 0) {
        for (auto& s : g.entities) {
            if (!s.alive || s.type != E_SHRINE) continue;
            bool monkIn = false;
            for (int gid : s.garrison) {
                Entity* m = findEntity(gid);
                if (m && m->alive && m->type == E_MONK) { monkIn = true; break; }
            }
            int radius = monkIn ? 4 : 1;
            for (auto& u : g.entities) {
                if (!u.alive || !isUnit(u.type) || u.owner >= MAX_PLAYERS) continue;
                if (u.state == S_GARRISONED || u.hp >= u.maxHp) continue;
                if (s.owner != OWNER_NATURE && u.owner != s.owner) continue; // claimed: owner only
                if (dist(u.x, u.y, s.x, s.y) > radius) continue;
                u.hp = std::min(u.maxHp, u.hp + 1);
            }
        }
    }
    if (g.tick % 25 == 0) {
        for (auto& w : g.entities) {
            if (!w.alive || w.type != E_WELL || w.underConstruction || w.owner >= MAX_PLAYERS) continue;
            for (auto& u : g.entities) {
                if (!u.alive || u.owner != w.owner || u.type != E_PEASANT) continue;
                if (u.state == S_GARRISONED || u.hp >= u.maxHp) continue;
                if (dist(u.x, u.y, w.x, w.y) <= 6) u.hp = std::min(u.maxHp, u.hp + 1);
            }
        }
    }
    if (g.tick % 15 == 0) {
        for (auto& m : g.entities) {
            if (!m.alive || m.type != E_STONEMASON || m.underConstruction || m.owner >= MAX_PLAYERS) continue;
            // Restock repair points by quarrying a nearby stone deposit.
            if (m.carrying <= 0) {
                bool found = false;
                for (int dy = -8; dy <= 8 && !found; dy++) for (int dx = -8; dx <= 8 && !found; dx++) {
                    int nx = m.x+dx, ny = m.y+dy;
                    if (!inBounds(nx,ny) || g.map[ny][nx].terrain != T_STONE) continue;
                    g.map[ny][nx].terrain = T_GRAVEL;
                    g.map[ny][nx].preWinterTerrain = T_GRAVEL;
                    m.carrying = 200;
                    found = true;
                }
                if (!found) continue;
            }
            // Patch the most damaged own building in reach, 1 hp per pass.
            Entity* worst = nullptr;
            for (auto& b : g.entities) {
                if (!b.alive || b.owner != m.owner || !isBuilding(b.type) || b.underConstruction) continue;
                if (b.hp >= b.maxHp || dist(b.x, b.y, m.x, m.y) > 8) continue;
                if (!worst || b.hp * worst->maxHp < worst->hp * b.maxHp) worst = &b;
            }
            if (worst) { worst->hp++; m.carrying--; }
        }
    }
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
                    if (ch.owner == g.localPlayer)
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
            case T_MUD:   case T_CASTLE_FLOOR: case T_HEATH:
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
    // Roll this winter's severity — every year is different. Mild winters
    // are a breather; brutal ones are the event you spent autumn preparing
    // for. (Part of sim state: saved, and identical in replays.)
    int roll = simRand() % 100;
    g.winterSeverity = (roll < 25) ? 0 : (roll < 75) ? 1 : 2;

    // Cull wildlife — the herd is thinned by the cold; brutal winters bite deeper.
    int cullPct = (g.winterSeverity == 2) ? 50 : (g.winterSeverity == 1) ? 35 : 25;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;
        if (e.type != E_DEER && e.type != E_SHEEP && e.type != E_BOAR) continue;
        if (simRand() % 100 < cullPct) killEntity(e);
    }
    if (g.players[g.localPlayer].alive) {
        if      (g.winterSeverity == 2) setStatus("A BRUTAL winter descends. The land turns to iron — pray your granaries hold.");
        else if (g.winterSeverity == 0) setStatus("Winter falls — a mild one, mercifully.");
        else                            setStatus("Winter falls. The land freezes over.");
        // Ice-locked fleets are worth a separate warning if anyone owns boats.
        for (auto& e : g.entities)
            if (e.alive && e.owner == g.localPlayer && isNaval(e.type))
                { setStatus("The waters freeze — your fleet is ice-locked until spring."); break; }
    }
}

void tickSeasons() {
    // Decrement the under-attack notification cooldown once per tick.
    if (g.attackNotifyCd > 0) g.attackNotifyCd--;

    // Season transitions.
    int s = (int)getSeason();
    if (s != g.prevSeason) {
        if (s == WINTER) applyWinter();
        if (g.players[g.localPlayer].alive) {
            if (s == SPRING && g.prevSeason == WINTER)
                setStatus("The frost retreats. Mud season — heavy wheels will struggle till summer.");
            else if (s == SUMMER)
                setStatus("Summer is upon the realm. Long days, dry roads — campaign season.");
            else if (s == AUTUMN)
                setStatus("Autumn descends. The great harvest begins — granaries fill, raiders watch.");
        }
        g.prevSeason = s;
    }

    // Mid-winter hard freeze: rivers become marching routes at 25% progress.
    // (Static announce-once flag is presentation only — a save/load mid-winter
    // just repeats the horn, it can't desync anything.)
    if (g.players[g.localPlayer].alive) {
        static bool frozeAnnounced = false;
        if (s == WINTER && getSeasonProgress() > 0.25f && !frozeAnnounced) {
            setStatus("The rivers freeze solid. New paths open across the ice.");
            frozeAnnounced = true;
        }
        if (s != WINTER) frozeAnnounced = false;
    }

    // Time-of-day transitions (fire once per phase crossing, not every tick).
    if (g.players[g.localPlayer].alive) {
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
    // Severity scales the hunger: mild winters drain every other cycle,
    // brutal ones drain double.
    bool mildSkip = (g.winterSeverity == 0 && (g.tick / 100) % 2 == 0);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!g.players[p].alive) continue;
        // Each unit costs 1 food per cycle (2 in a brutal winter), HALVED for
        // units within 10 tiles of a stocked granary — the larder is close.
        // Accumulate in half-units so the halving rounds fairly.
        int unitCount = 0, drainHalves = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
            unitCount++;
            int h = (g.winterSeverity == 2 ? 2 : 1) * 2;
            for (auto& b : g.entities) {
                if (!b.alive || b.owner != p || b.type != E_GRANARY || b.underConstruction) continue;
                if (depotFoodSum(b) > 0 && dist(e.x, e.y, b.x, b.y) <= 10) { h /= 2; break; }
            }
            drainHalves += h;
        }
        if (unitCount == 0) continue;
        Player& pl = g.players[p];
        int drain = drainHalves / 2;
        if (mildSkip) drain = 0;
        if (drain > 0) {
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
        // Frostbite: units campaigning far from any friendly roof bleed hp
        // through the cold — winter offensives need nearby shelter. Mild
        // winters spare them; brutal winters cut twice as deep. Garrisoned
        // units are indoors by definition.
        if (g.winterSeverity >= 1) {
            int frost = g.winterSeverity;   // 1 or 2 hp per cycle
            bool warned = false;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
                if (e.state == S_GARRISONED || isNaval(e.type)) continue;
                if (e.aleTicks > 0) continue;   // ale in the blood keeps the cold out
                bool sheltered = false;
                for (auto& b : g.entities) {
                    if (!b.alive || b.owner != p || !isBuilding(b.type) || b.underConstruction) continue;
                    if (dist(e.x, e.y, b.x, b.y) <= 25) { sheltered = true; break; }
                }
                if (sheltered) continue;
                e.hp -= frost;
                if (e.hp <= 0) killEntity(e);
                else if (p == 0 && !warned) {
                    setStatus("Frostbite! Troops far from shelter are freezing.");
                    warned = true;
                }
            }
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
    // Decay: unused paving gradually returns to nature. Scattered loot is
    // carried off by crows and rain on the same slow clock.
    if (g.tick % 250 == 0) {
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Tile& t = g.map[y][x];
            if (t.lootGold > 0) t.lootGold -= std::max(1, t.lootGold/4);
            if (t.lootWood > 0) t.lootWood -= std::max(1, t.lootWood/4);
            if (t.lootFood > 0) t.lootFood -= std::max(1, t.lootFood/4);
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
        if (g.players[g.localPlayer].alive) setStatus("The rain turns to snow.");
        return;
    }
    bool lateAutumn = (s == AUTUMN && sp > 0.5f);
    if (!lateAutumn && s != WINTER && g.weather == W_SNOW) {
        g.weather = W_CLEAR;
        g.weatherTimer = 100;
        if (g.players[g.localPlayer].alive) setStatus("The skies clear.");
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
        // Spring is the wet season (mud — siege engines crawl, see
        // moveAlongPath); summer is dry campaign weather.
        int rainBias  = (s == AUTUMN) ? 50 : (s == SPRING) ? 55 : 15;
        int stormBias = (s == AUTUMN) ? 15 : (s == SPRING) ? 12 : 5;
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

    if (g.players[g.localPlayer].alive) {
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

        // Bears: solitary forest predators. Unlike wolves they have no fear of
        // settlements and a longer reach — anything that strays into the deep
        // woods gets mauled. They don't roam far from the trees (see wander).
        if (e.type == E_BEAR && (e.state==S_IDLE || (e.state==S_MOVING && e.path.empty()))) {
            for (auto& o : g.entities) {
                if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                if (o.state == S_GARRISONED) continue;
                if (dist(e.x, e.y, o.x, o.y) <= 6) { orderAttack(e, o.id); break; }
            }
        }

        // Wolf dens breed: while fewer than 2 wolves prowl within 10 tiles,
        // the den whelps a new one every ~600 ticks. Burn it out to stop them.
        if (e.type == E_WOLF_DEN && g.tick % 600 == 0) {
            int near = 0;
            for (auto& w : g.entities)
                if (w.alive && w.type == E_WOLF && dist(w.x, w.y, e.x, e.y) <= 10) near++;
            if (near < 2) {
                for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                    int nx = e.x+dx, ny = e.y+dy;
                    if ((dx||dy) && inBounds(nx,ny) && isPassable(nx,ny) && !entityAt(nx,ny)) {
                        spawnEntity(E_WOLF, OWNER_NATURE, nx, ny);
                        dy = 2; break;
                    }
                }
            }
        }

        // Random wander when idle (animals only — dens stay put). Wolves and
        // bears keep to the trees: they only drift to a forest tile, so they
        // live in the woods rather than spilling out onto open ground.
        if (isUnit(e.type) && e.state == S_IDLE && atick % (35 + (e.id%25)) == 0) {
            int wx = e.x + (simRand()%9)-4, wy = e.y + (simRand()%9)-4;
            wx = std::max(1, std::min(wx, MAP_W-2));
            wy = std::max(1, std::min(wy, MAP_H-2));
            bool ok = isPassable(wx, wy);
            if (e.type==E_WOLF || e.type==E_BEAR) {
                Terrain wt = g.map[wy][wx].terrain;
                ok = ok && (wt==T_FOREST||wt==T_PINE||wt==T_PALM||wt==T_DEAD_TREE);
            }
            if (ok) orderMove(e, wx, wy);
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
    // Match resolved: one side (or nobody) left standing. Both machines in a
    // network game reach this on the same tick — alive flags are sim state.
    if (aliveCount <= 1) { g.winner = lastAlive; g.mode = M_GAME_OVER; return; }

    // === SACRED-SITE DOMINATION ===
    // Count the map's claimable sites per holder. Hold the majority and a
    // countdown starts; keep it through SITE_HOLD_TICKS and the realm is
    // yours. Burning a site removes it from the count — denial is a play.
    {
        int total = 0, held[MAX_PLAYERS] = {};
        for (auto& e : g.entities) {
            if (!e.alive || !isClaimable(e.type)) continue;
            total++;
            if (e.owner >= 0 && e.owner < MAX_PLAYERS && !e.garrison.empty() ) held[e.owner]++;
            else if (e.owner >= 0 && e.owner < MAX_PLAYERS && e.type == E_RUIN) held[e.owner]++; // keeps stay claimed
        }
        int leader = -1;
        if (total >= 3)
            for (int p = 0; p < MAX_PLAYERS; p++)
                if (g.players[p].alive && held[p] * 2 > total) leader = p;
        if (leader != g.siteHoldOwner) {
            if (leader >= 0)
                setStatus(std::string(leader == g.localPlayer ? "You hold" : (std::string("The ") + CIVS[g.players[leader].civ].name + " (P" + std::to_string(leader+1) + ") hold"))
                          + " the sacred sites! The realm submits in "
                          + std::to_string(SITE_HOLD_TICKS * TICK_MS / 60000) + " minutes — break their grip!");
            else if (g.siteHoldOwner >= 0)
                setStatus("The grip on the sacred sites is broken.");
            g.siteHoldOwner = leader;
            g.siteHoldTicks = 0;
        } else if (leader >= 0) {
            int before = g.siteHoldTicks;
            g.siteHoldTicks += 100;   // checkWin cadence
            if (before < SITE_HOLD_TICKS/2 && g.siteHoldTicks >= SITE_HOLD_TICKS/2)
                setStatus(std::string(leader == g.localPlayer ? "Your" : "The enemy's")
                          + " claim on the sites is half sworn — the bells grow louder!");
            if (g.siteHoldTicks >= SITE_HOLD_TICKS) {
                g.winner = leader; g.mode = M_GAME_OVER;
                return;
            }
        }
    }
    // Solo-human defeat ends the match immediately — no point watching the AIs
    // fight each other. With two humans the fallen one spectates to the end
    // (halting the sim locally would stall the opponent's lockstep).
    bool soloHuman = (g.humanMask & (g.humanMask - 1)) == 0;
    if (soloHuman && !g.players[g.localPlayer].alive) { g.winner = -1; g.mode = M_GAME_OVER; }
}
