#include "build_service.h"
#include "realm.h"
#include "core/game_events.h"

#include <cstdlib>
#include <vector>

CanStartBuildResult canStartBuild(const Game& game, int player, const Entity& builder,
                                  EntityType buildingType, MapPos tile) {
    WorldIndex world = buildWorldIndex(game);
    return canStartBuild(game, world, player, builder, buildingType, tile);
}

CanStartBuildResult canStartBuild(const Game& game, const WorldIndex& world, int player,
                                  const Entity& builder, EntityType buildingType, MapPos tile) {
    if (!builder.alive || !canBuild(builder.type)) return { false, "Cannot build." };
    if (builder.owner != player) return { false, "Not your builder." };

    const Player& p = game.players[player];
    if (p.gold < STATS[buildingType].costGold || p.wood < STATS[buildingType].costWood)
        return { false, "Not enough resources!" };
    if (!canPlace(game, world, buildingType, tile.x, tile.y, player)) return { false, "Can't build there!" };

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

bool startBuild(Game& game, int player, int builderId, EntityType buildingType, MapPos tile) {
    WorldIndex world = buildWorldIndex(game);
    return startBuild(game, world, player, builderId, buildingType, tile);
}

bool startBuild(Game& game, WorldIndex& world, int player, int builderId, EntityType buildingType, MapPos tile) {
    return startBuildService(game, world, player, builderId, buildingType, tile).ok;
}

ServiceResult startBuildService(Game& game, WorldIndex& world, int player, int builderId, EntityType buildingType, MapPos tile) {
    if (!inBounds(tile.x, tile.y)) return { false, "Build target is out of bounds." };
    Entity* builder = findEntity(game, world, builderId);
    if (!builder) return { false, "Builder not found." };

    CanStartBuildResult result = canStartBuild(game, world, player, *builder, buildingType, tile);
    if (!result.ok) {
        emitStatusEvent(player, result.reason, GameEventType::CommandRejected);
        return { false, result.reason };
    }

    Player& p = game.players[player];
    p.gold -= STATS[buildingType].costGold;
    p.wood -= STATS[buildingType].costWood;
    int bid = spawnEntity(game, buildingType, player, tile.x, tile.y, false);
    emitActionMarkerEvent(player, tile, '#');
    emitGameEvent({ GameEventType::BuildingPlaced, player, bid, tile, "", 0 });
    world = buildWorldIndex(game);
    builder->state = S_BUILDING;
    builder->targetId = bid;
    builder->targetX = tile.x;
    builder->targetY = tile.y;
    pathBuilderToFootprint(game, *builder, buildingType, tile);
    return { true, nullptr };
}

bool startBuildLine(Game& game, int player, int builderId, EntityType buildingType,
                    MapPos start, MapPos end) {
    WorldIndex world = buildWorldIndex(game);
    return startBuildLine(game, world, player, builderId, buildingType, start, end);
}

bool startBuildLine(Game& game, WorldIndex& world, int player, int builderId, EntityType buildingType,
                    MapPos start, MapPos end) {
    return startBuildLineService(game, world, player, builderId, buildingType, start, end).ok;
}

ServiceResult startBuildLineService(Game& game, WorldIndex& world, int player, int builderId, EntityType buildingType,
                                    MapPos start, MapPos end) {
    Entity* builder = findEntity(game, world, builderId);
    if (!builder) return { false, "Builder not found." };
    if (!builder->alive || !canBuild(builder->type)) return { false, "Cannot build." };
    if (builder->owner != player) return { false, "Not your builder." };

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
        emitStatusEvent(player, "Can't build there!", GameEventType::CommandRejected);
        return { false, "Can't build there!" };
    }

    int totalGold = (int)tiles.size() * STATS[buildingType].costGold;
    int totalWood = (int)tiles.size() * STATS[buildingType].costWood;
    Player& p = game.players[player];
    if (p.gold < totalGold || p.wood < totalWood) {
        emitStatusEvent(player, "Not enough resources!", GameEventType::CommandRejected);
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
            pathBuilderToFootprint(game, *builder, buildingType, { first->x, first->y });
        }
        emitActionMarkerEvent(player, start, '#');
        emitStatusEvent(player, "Building walls...", GameEventType::BuildingPlaced);
    }
    return { true, nullptr };
}
