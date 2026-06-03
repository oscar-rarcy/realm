#include "render/entity_visual_defs.h"

static const EntityVisualDef ENTITY_VISUAL_DEFS[] = {
    { E_NONE,         " ",          nullptr,      "unknown" },
    { E_PEASANT,      "\xe2\x99\x9f", u8"🧍",     "peasant" },
    { E_MILITIA,      "\xe2\x99\x99", u8"🤺",     "militia" },
    { E_ARCHER,       "\xe2\x99\x97", u8"🏹",     "archer" },
    { E_KNIGHT,       "\xe2\x99\x9e", u8"🐎",     "knight" },
    { E_SPEARMAN,     "\xe2\x96\xb3", u8"🗡",     "spearman" },
    { E_CATAPULT,     "\xe2\x8a\x99", u8"🛞",     "catapult" },
    { E_TREBUCHET,    "\xe2\x8c\x90", u8"🎯",     "trebuchet" },
    { E_FISHING_BOAT, "\xe2\x88\xaa", u8"🛶",     "fishing_boat" },
    { E_WARSHIP,      "\xe2\x96\xbc", u8"🚢",     "warship" },
    { E_TRANSPORT,    "\xe2\x96\xbd", u8"⛴",     "transport" },
    { E_RAM,          "\xe2\x96\xac", u8"🪵",     "ram" },
    { E_TOWNHALL,     "\xe2\x99\x96", u8"🏛",     "town_hall" },
    { E_HOUSE,        "\xe2\x96\xa1", u8"🏠",     "house" },
    { E_BARRACKS,     "\xe2\x96\xa6", u8"🏕",     "barracks" },
    { E_STABLE,       "\xe2\x99\x98", u8"🐴",     "stable" },
    { E_TOWER,        "\xe2\x96\xa3", u8"🗼",     "tower" },
    { E_FARM,         "\xc2\xa7",     u8"🌾",     "farm" },
    { E_BLACKSMITH,   "\xe2\x96\xb3", u8"⚒",     "blacksmith" },
    { E_CHURCH,       "\xe2\x9c\x9a", u8"⛪",     "church" },
    { E_MARKET,       "\xe2\x97\x86", u8"🏪",     "market" },
    { E_WALL,         "\xe2\x96\xa0", u8"🧱",     "wall" },
    { E_GATE,         "\xe2\x96\xac", u8"🧱",     "gate" },
    { E_CASTLE,       "\xe2\x99\x9a", u8"🏰",     "castle" },
    { E_LUMBER_CAMP,  "\xe2\x99\xa3", u8"🪵",     "lumber_camp" },
    { E_MINING_CAMP,  "\xe2\x97\x87", u8"⛏",     "mining_camp" },
    { E_MILL,         "\xe2\x97\x8b", u8"⚙",      "mill" },
    { E_DOCK,         "\xe2\x88\xa9", u8"⚓",     "dock" },
    { E_DEER,         "\xe2\x96\xb7", u8"🦌",     "deer" },
    { E_WOLF,         "\xe2\x97\x81", u8"🐺",     "wolf" },
    { E_SHEEP,        "\xe2\x97\x8c", u8"🐑",     "sheep" },
    { E_BOAR,         "\xe2\x97\x8f", u8"🐗",     "boar" },
};

static_assert(sizeof(ENTITY_VISUAL_DEFS) / sizeof(ENTITY_VISUAL_DEFS[0]) == E_TYPE_COUNT,
              "ENTITY_VISUAL_DEFS must have one row per EntityType");

const EntityVisualDef& entityVisualDef(EntityType type) {
    if (type < E_NONE || type >= E_TYPE_COUNT) return ENTITY_VISUAL_DEFS[E_NONE];
    return ENTITY_VISUAL_DEFS[type];
}

const char* entityTerminalEmoji(EntityType type) {
    return entityVisualDef(type).terminalEmoji;
}

const char* entitySdlGlyph(EntityType type) {
    return entityVisualDef(type).sdlGlyph;
}

const char* entityAssetSlug(EntityType type) {
    return entityVisualDef(type).assetSlug;
}
