#include "build_service.h"
#include "realm.h"
#include "core/game_events.h"

#include <cstdlib>
#include <vector>

static const BuildRule BUILD_RULES[] = {
    {'\0', E_TOWNHALL},
    {'h', E_HOUSE},
    {'b', E_BARRACKS},
    {'s', E_STABLE},
    {'t', E_TOWER},
    {'f', E_FARM},
    {'w', E_WALL},
    {'a', E_BLACKSMITH},
    {'c', E_CHURCH},
    {'m', E_MARKET},
    {'k', E_CASTLE},
    {'l', E_LUMBER_CAMP},
    {'n', E_MINING_CAMP},
    {'i', E_MILL},
    {'g', E_GATE},
    {'d', E_DOCK},
};

static const int BUILD_RULE_COUNT = (int)(sizeof(BUILD_RULES) / sizeof(BUILD_RULES[0]));

static void emitActionMarker(EventSink& events, int player, MapPos tile, char glyph) {
    events.emit({ GameEventType::ActionMarker, player, -1, tile, "", glyph });
}

static void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

const BuildRule* buildRules(int& count) {
    count = BUILD_RULE_COUNT;
    return BUILD_RULES;
}

const BuildRule* buildRule(EntityType buildingType) {
    for (int i = 0; i < BUILD_RULE_COUNT; i++)
        if (BUILD_RULES[i].buildingType == buildingType) return &BUILD_RULES[i];
    return nullptr;
}

CanStartBuildResult canStartBuild(const Game& game, const WorldIndex& world, int player,
                                  const Entity& builder, EntityType buildingType, MapPos tile) {
    if (!builder.alive || !canBuild(builder.type)) return { false, "Cannot build." };
    if (builder.owner != player) return { false, "Not your builder." };
    if (!buildRule(buildingType)) return { false, "Unknown building type." };

    const Player& p = game.players[player];
    if (p.gold < STATS[buildingType].costGold || p.wood < STATS[buildingType].costWood)
        return { false, "Not enough resources!" };
    if (!canPlace(game, world, buildingType, tile.x, tile.y, player, builder.id)) return { false, "Can't build there!" };

    return { true, nullptr };
}

static void pathBuilderToFootprint(const Game& game, Entity& builder, EntityType buildingType, MapPos tile) {
    int bldW = STATS[buildingType].sizeW, bldH = STATS[buildingType].sizeH;
    int bestAX = tile.x - 1, bestAY = tile.y, bestAD = 99999;
    for (int dy = -1; dy <= bldH; dy++) for (int dx = -1; dx <= bldW; dx++) {
        if (dx >= 0 && dx < bldW && dy >= 0 && dy < bldH) continue;
        int nx = tile.x + dx, ny = tile.y + dy;
        if (inBounds(nx, ny) && isPassable(game, nx, ny)) {
            int d = mdist(builder.x, builder.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    builder.path = findPath(game, builder.x, builder.y, bestAX, bestAY, 300, isNaval(builder.type));
    builder.pathIdx = 0;
}

ServiceResult startBuildService(Game& game, WorldIndex& world, EventSink& events, int player, int builderId, EntityType buildingType, MapPos tile) {
    if (!inBounds(tile.x, tile.y)) return { false, "Build target is out of bounds." };
    Entity* builder = findEntity(game, world, builderId);
    if (!builder) return { false, "Builder not found." };

    CanStartBuildResult result = canStartBuild(game, world, player, *builder, buildingType, tile);
    if (!result.ok) {
        return { false, result.reason };
    }

    Player& p = game.players[player];
    p.gold -= STATS[buildingType].costGold;
    p.wood -= STATS[buildingType].costWood;
    int bid = spawnEntity(game, buildingType, player, tile.x, tile.y, false);
    emitActionMarker(events, player, tile, '#');
    events.emit({ GameEventType::BuildingPlaced, player, bid, tile, "", 0 });
    world = buildWorldIndex(game);
    builder = findEntity(game, world, builderId);
    if (!builder) return { false, "Builder not found." };
    builder->state = S_BUILDING;
    builder->targetId = bid;
    builder->targetX = tile.x;
    builder->targetY = tile.y;
    builder->waypoints.clear();
    builder->patrolMode = false;
    pathBuilderToFootprint(game, *builder, buildingType, tile);
    return { true, nullptr };
}

ServiceResult startBuildLineService(Game& game, WorldIndex& world, EventSink& events, int player, int builderId, EntityType buildingType,
                                    MapPos start, MapPos end) {
    Entity* builder = findEntity(game, world, builderId);
    if (!builder) return { false, "Builder not found." };
    if (!builder->alive || !canBuild(builder->type)) return { false, "Cannot build." };
    if (builder->owner != player) return { false, "Not your builder." };
    if (!buildRule(buildingType)) return { false, "Unknown building type." };

    std::vector<MapPos> tiles;
    int cx = start.x, cy = start.y;
    int dx = std::abs(end.x - start.x), sx = start.x < end.x ? 1 : -1;
    int dy = -std::abs(end.y - start.y), sy = start.y < end.y ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if (inBounds(cx, cy) && canPlace(game, world, buildingType, cx, cy, player)) tiles.push_back({ cx, cy });
        if (cx == end.x && cy == end.y) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
    }

    if (tiles.empty()) {
        return { false, "Can't build there!" };
    }

    int totalGold = (int)tiles.size() * STATS[buildingType].costGold;
    int totalWood = (int)tiles.size() * STATS[buildingType].costWood;
    Player& p = game.players[player];
    if (p.gold < totalGold || p.wood < totalWood) {
        return { false, "Not enough resources!" };
    }

    p.gold -= totalGold;
    p.wood -= totalWood;
    int firstId = -1;
    for (MapPos tile : tiles) {
        int bid = spawnEntity(game, buildingType, player, tile.x, tile.y, false);
        if (firstId < 0) firstId = bid;
    }
    world = buildWorldIndex(game);
    if (firstId >= 0) {
        builder = findEntity(game, world, builderId);
        Entity* first = findEntity(game, world, firstId);
        if (builder && first) {
            builder->state = S_BUILDING;
            builder->targetId = firstId;
            builder->targetX = first->x;
            builder->targetY = first->y;
            builder->waypoints.clear();
            builder->patrolMode = false;
            pathBuilderToFootprint(game, *builder, buildingType, { first->x, first->y });
        }
        emitActionMarker(events, player, start, '#');
        emitStatus(events, player, "Building walls...", GameEventType::BuildingPlaced);
    }
    return { true, nullptr };
}
