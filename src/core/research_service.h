#pragma once

#include "research_defs.h"
#include "core/service_result.h"

// Single validation + execution path for research, shared by player input and AI.

struct WorldIndex;
struct Entity;
struct Game;
class EventSink;

struct CanResearchResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

CanResearchResult canResearch(const Game& game, const WorldIndex& world, int player, const Entity& building, ResearchId id);

// Validates and, on success, spends resources and starts research on the building.
ServiceResult startResearchService(Game& game, const WorldIndex& world, EventSink& events, int player, int buildingId, ResearchId id);
