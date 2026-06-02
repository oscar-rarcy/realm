#pragma once

#include "realm.h"

// Canonical research catalogue. This is the single source of truth for research
// cost, duration, the building that performs it, and any required owned building.
// Both the player input path and the AI consume these definitions so the two can
// never drift apart (see docs/implementation/refactor-plan.md phases 1.3 / 3.3).

enum class ResearchId {
    IronWeapons,
    Crossbows,
    Pikes,
    Counterweight,
    PlateHelm,
};

struct ResearchDef {
    ResearchId id;
    int bit;                       // R_* bitmask value (save-compatible)
    int costGold;
    int costWood;
    int ticks;
    EntityType requiredBuilding;       // building that runs the research (E_BLACKSMITH)
    EntityType requiredOwnedBuilding;  // E_NONE, or a completed building the player must own
    const char* startMessage;
};

const ResearchDef* researchDefs(int& count);
const ResearchDef* researchDef(ResearchId id);
const ResearchDef* researchDefFromBit(int bit);
int researchBit(ResearchId id);
bool researchIdFromBit(int bit, ResearchId& out);
