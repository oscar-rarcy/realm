#pragma once

#include "core/game_types.h"
#include "core/service_result.h"
#include "core/world_index.h"

struct Entity;
struct Game;
class EventSink;

struct CanStartBuildResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

struct BuildRule {
    char menuHotkey;
    EntityType buildingType;
};

const BuildRule* buildRules(int& count);
const BuildRule* buildRule(EntityType buildingType);

CanStartBuildResult canStartBuild(const Game& game, const WorldIndex& world, int player,
                                  const Entity& builder, EntityType buildingType, MapPos tile);

ServiceResult startBuildService(Game& game, WorldIndex& world, EventSink& events, int player, int builderId, EntityType buildingType, MapPos tile);
ServiceResult startBuildLineService(Game& game, WorldIndex& world, EventSink& events, int player, int builderId, EntityType buildingType,
                                    MapPos start, MapPos end);
