#pragma once

#include "research_defs.h"
#include "core/service_result.h"

// Single validation + execution path for research, shared by player input and AI.

struct WorldIndex;
struct Entity;
struct Game;

struct CanResearchResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

CanResearchResult canResearch(const Game& game, int player, const Entity& building, ResearchId id);
CanResearchResult canResearch(const Game& game, const WorldIndex& world, int player, const Entity& building, ResearchId id);

// Validates and, on success, spends resources and starts research on the building.
// Emits a status message for the human player (owner 0) on success and failure,
// mirroring the other order* functions. Returns true when research started.
bool startResearch(Game& game, int player, int buildingId, ResearchId id);
ServiceResult startResearchService(Game& game, int player, int buildingId, ResearchId id);
ServiceResult startResearchService(Game& game, const WorldIndex& world, int player, int buildingId, ResearchId id);
