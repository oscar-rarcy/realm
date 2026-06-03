#include "realm.h"
#include "core/game_events.h"

namespace {

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

} // namespace

static void applyWinter(Game& game, EventSink& events) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = game.map[y][x];
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
    for (auto& e : game.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;
        if (e.type != E_DEER && e.type != E_SHEEP && e.type != E_BOAR) continue;
        if (realmRand(game) % 100 < 35) killEntity(game, events, e);
    }
    if (game.players[0].alive) emitStatus(events, 0, "Winter falls. The land freezes over.");
}

void tickSeasons(Game& game, EventSink& events) {
    if (game.attackNotifyCd > 0) game.attackNotifyCd--;
    int s = (int)getSeason(game);
    if (s != game.prevSeason) {
        if (s == WINTER) applyWinter(game, events);
        else if (s == SUMMER && game.players[0].alive) emitStatus(events, 0, "Summer crowns the fields in gold.");
        else if (s == AUTUMN && game.players[0].alive) emitStatus(events, 0, "Autumn reddens the woods.");
        if (s == SPRING && game.prevSeason == WINTER && game.players[0].alive)
            emitStatus(events, 0, "Spring stirs. The thaw begins.");
        game.prevSeason = s;
    }
    int phase = 0;
    if (isDusk(game)) phase = 1;
    else if (isNight(game)) phase = 2;
    else if (isDawn(game)) phase = 3;
    if (phase != game.prevTimePhase) {
        if (game.players[0].alive) {
            if (phase == 1) emitStatus(events, 0, "Evening gathers over the realm.");
            else if (phase == 2) emitStatus(events, 0, "Night settles. Torches flicker.");
            else if (phase == 3) emitStatus(events, 0, "Dawn breaks over the realm.");
        }
        game.prevTimePhase = phase;
    }
}

void tickThaw(Game& game) {
    if (game.tick % 5 != 0) return;
    if (getSeason(game) != SPRING) return;
    float progress = getSeasonProgress(game);
    // Patchy melt completes by ~40% of spring — earlier sessions felt snowy way too
    // deep into the season. Tiles thaw faster, world greens up quickly.
    int threshold = std::max(0, (int)(progress * 2600.0f));
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = game.map[y][x];
        if (t.terrain != T_SNOW && t.terrain != T_ICE) continue;
        if (t.preWinterTerrain == t.terrain) continue;
        unsigned h = ((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u);
        if ((int)(h & 0x3ff) < threshold) t.terrain = t.preWinterTerrain;
    }
}

void tickWinter(Game& game, EventSink& events) {
    if (getSeason(game) != WINTER) return;
    if (game.tick % 100 != 0) return;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!game.players[p].alive) continue;
        int unitCount = 0;
        for (auto& e : game.entities) {
            if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
            unitCount++;
        }
        if (unitCount == 0) continue;
        Player& pl = game.players[p];
        int drain = unitCount; // 1 food per unit per 100 ticks
        if (pl.food >= drain) {
            spendPlayerFood(game, p, drain);
        } else {
            int starve = drain - pl.food;
            spendPlayerFood(game, p, pl.food);
            // Damage `starve` random units. If any die, they're gone.
            int hits = 0;
            for (auto& e : game.entities) {
                if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
                e.hp -= 3;
                if (e.hp <= 0) killEntity(game, events, e);
                if (++hits >= starve) break;
            }
            emitStatus(events, p, "Starvation! Units are losing health.");
        }
    }
}

// ============================================================
// PAVING — building creep + path wear + decay
// ============================================================
void tickPaving(Game& game) {
    // Buildings emit creep into adjacent natural ground.
    if (game.tick % 100 == 0) {
        for (auto& e : game.entities) {
            if (!e.alive || !isBuilding(e.type) || e.underConstruction) continue;
            auto& s = STATS[e.type];
            for (int dy = -3; dy <= s.sizeH+2; dy++) for (int dx = -3; dx <= s.sizeW+2; dx++) {
                if (dx >= 0 && dx < s.sizeW && dy >= 0 && dy < s.sizeH) continue;
                int nx = e.x+dx, ny = e.y+dy;
                if (!inBounds(nx,ny)) continue;
                int ringDist = std::max(std::max(0, -dx), std::max(0, dx-s.sizeW+1))
                             + std::max(std::max(0, -dy), std::max(0, dy-s.sizeH+1));
                if (ringDist > 3) continue;
                Tile& t = game.map[ny][nx];
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
    if (game.tick % 250 == 0) {
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Tile& t = game.map[y][x];
            if (t.wear > 0) t.wear--;
            if (t.wear == 0 && t.terrain == T_ROAD) {
                t.terrain = T_DIRT; t.preWinterTerrain = T_DIRT;
            }
            // Dirt slowly regrows — patches of grass return after long disuse
            if (t.wear == 0 && t.terrain == T_DIRT && (realmRand(game) % 500) == 0) {
                t.terrain = T_GRASS; t.preWinterTerrain = T_GRASS;
            }
        }
    }
}
