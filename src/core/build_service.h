#pragma once

#include "realm.h"

// Canonical building placement/resource-spend execution path shared by player
// commands and AI. UI event/status calls stay here temporarily until Phase 4
// introduces an event sink.

struct CanStartBuildResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

CanStartBuildResult canStartBuild(const Game& game, int player, const Entity& builder,
                                  EntityType buildingType, MapPos tile);

bool startBuild(Game& game, int player, int builderId, EntityType buildingType, MapPos tile);
bool startBuildLine(Game& game, int player, int builderId, EntityType buildingType,
                    MapPos start, MapPos end);
