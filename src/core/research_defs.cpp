#include "research_defs.h"

// Costs and durations are the canonical (player-path) values. The AI previously
// used shorter, free research; it now shares these exact definitions.
static const ResearchDef DEFS[] = {
    { ResearchId::IronWeapons,   R_IRON_WEAPONS,  100, 100, 940, E_BLACKSMITH, E_NONE,   "Researching Iron Weapons..." },
    { ResearchId::Crossbows,     R_CROSSBOWS,      80,  80, 820, E_BLACKSMITH, E_NONE,   "Researching Crossbows..." },
    { ResearchId::Pikes,         R_PIKES,         100, 100, 900, E_BLACKSMITH, E_NONE,   "Researching Pikes..." },
    { ResearchId::Counterweight, R_COUNTERWEIGHT, 120, 150, 980, E_BLACKSMITH, E_CASTLE, "Researching Counterweight..." },
    { ResearchId::PlateHelm,     R_PLATE_HELM,    120, 100, 980, E_BLACKSMITH, E_NONE,   "Researching Plate Helm..." },
};

static const int DEF_COUNT = (int)(sizeof(DEFS) / sizeof(DEFS[0]));

const ResearchDef* researchDefs(int& count) {
    count = DEF_COUNT;
    return DEFS;
}

const ResearchDef* researchDef(ResearchId id) {
    for (int i = 0; i < DEF_COUNT; i++)
        if (DEFS[i].id == id) return &DEFS[i];
    return nullptr;
}

const ResearchDef* researchDefFromBit(int bit) {
    for (int i = 0; i < DEF_COUNT; i++)
        if (DEFS[i].bit == bit) return &DEFS[i];
    return nullptr;
}

int researchBit(ResearchId id) {
    const ResearchDef* def = researchDef(id);
    return def ? def->bit : 0;
}

bool researchIdFromBit(int bit, ResearchId& out) {
    const ResearchDef* def = researchDefFromBit(bit);
    if (!def) return false;
    out = def->id;
    return true;
}
