#include "build_service.h"
#include "core/game_events.h"

#include <cstdlib>
#include <vector>

CanStartBuildResult canStartBuild(const Game& game, int player, const Entity& builder,
                                  EntityType buildingType, MapPos tile) {
    if (!builder.alive || !canBuild(builder.type)) return { false, "Cannot build." };
    if (builder.owner != player) return { false, "Not your builder." };

    const Player& p = game.players[player];
    if (p.gold < STATS[buildingType].costGold || p.wood < STATS[buildingType].costWood)
        return { false, "Not enough resources!" };
    if (!canPlace(buildingType, tile.x, tile.y, player)) return { false, "Can't build there!" };

    return { true, nullptr };
}

static void pathBuilderToFootprint(Entity& builder, EntityType buildingType, MapPos tile) {
    int bldW = STATS[buildingType].sizeW, bldH = STATS[buildingType].sizeH;
    int bestAX = tile.x - 1, bestAY = tile.y, bestAD = 99999;
    for (int dy = -1; dy <= bldH; dy++) for (int dx = -1; dx <= bldW; dx++) {
        if (dx >= 0 && dx < bldW && dy >= 0 && dy < bldH) continue;
        int nx = tile.x + dx, ny = tile.y + dy;
        if (inBounds(nx, ny) && isPassable(nx, ny)) {
            int d = mdist(builder.x, builder.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    builder.path = findPath(builder.x, builder.y, bestAX, bestAY, 300, isNaval(builder.type));
    builder.pathIdx = 0;
}

bool startBuild(Game& game, int player, int builderId, EntityType buildingType, MapPos tile) {
    if (!inBounds(tile.x, tile.y)) return false;
    Entity* builder = findEntity(builderId);
    if (!builder) return false;

    CanStartBuildResult result = canStartBuild(game, player, *builder, buildingType, tile);
    if (!result.ok) {
        emitStatusEvent(player, result.reason, GameEventType::CommandRejected);
        return false;
    }

    Player& p = game.players[player];
    p.gold -= STATS[buildingType].costGold;
    p.wood -= STATS[buildingType].costWood;
    int bid = spawnEntity(buildingType, player, tile.x, tile.y, false);
    emitActionMarkerEvent(player, tile, '#');
    emitGameEvent({ GameEventType::BuildingPlaced, player, bid, tile, "", 0 });
    builder->state = S_BUILDING;
    builder->targetId = bid;
    builder->targetX = tile.x;
    builder->targetY = tile.y;
    pathBuilderToFootprint(*builder, buildingType, tile);
    return true;
}

bool startBuildLine(Game& game, int player, int builderId, EntityType buildingType,
                    MapPos start, MapPos end) {
    Entity* builder = findEntity(builderId);
    if (!builder) return false;
    if (!builder->alive || !canBuild(builder->type)) return false;
    if (builder->owner != player) return false;

    std::vector<MapPos> tiles;
    int cx = start.x, cy = start.y;
    int dx = std::abs(end.x - start.x), sx = start.x < end.x ? 1 : -1;
    int dy = -std::abs(end.y - start.y), sy = start.y < end.y ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if (inBounds(cx, cy) && canPlace(buildingType, cx, cy, player)) tiles.push_back({ cx, cy });
        if (cx == end.x && cy == end.y) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; cx += sx; }
        if (e2 <= dx) { err += dx; cy += sy; }
    }

    if (tiles.empty()) {
        emitStatusEvent(player, "Can't build there!", GameEventType::CommandRejected);
        return false;
    }

    int totalGold = (int)tiles.size() * STATS[buildingType].costGold;
    int totalWood = (int)tiles.size() * STATS[buildingType].costWood;
    Player& p = game.players[player];
    if (p.gold < totalGold || p.wood < totalWood) {
        emitStatusEvent(player, "Not enough resources!", GameEventType::CommandRejected);
        return false;
    }

    p.gold -= totalGold;
    p.wood -= totalWood;
    int firstId = -1;
    for (MapPos tile : tiles) {
        int bid = spawnEntity(buildingType, player, tile.x, tile.y, false);
        if (firstId < 0) firstId = bid;
    }
    if (firstId >= 0) {
        orderHelp(*builder, firstId);
        emitActionMarkerEvent(player, start, '#');
        emitStatusEvent(player, "Building walls...", GameEventType::BuildingPlaced);
    }
    return true;
}
