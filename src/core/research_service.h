#pragma once

#include "research_defs.h"

// Single validation + execution path for research, shared by player input and AI.

struct CanResearchResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

CanResearchResult canResearch(const Game& game, int player, const Entity& building, ResearchId id);

// Validates and, on success, spends resources and starts research on the building.
// Emits a status message for the human player (owner 0) on success and failure,
// mirroring the other order* functions. Returns true when research started.
bool startResearch(Game& game, int player, int buildingId, ResearchId id);
