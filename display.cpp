#include "display.h"
#include "realm.h"
#include <cstring>
#include <cstdint>

DisplayMode displayMode = DM_ASCII;

// Type-level default glyphs. render.cpp overrides peasants on the map with
// state/owner-specific standing/walking/kneeling/working people.
static const char* ENTITY_EMOJI[] = {
    "  ",       // E_NONE
    u8"🧍‍♂️",   // E_PEASANT default/idle preview
    u8"🤺",     // E_MILITIA
    u8"🏹",     // E_ARCHER
    u8"🐎",     // E_KNIGHT / cavalry
    u8"🔱",     // E_SPEARMAN
    u8"🛞",     // E_CATAPULT
    u8"🏗",      // E_TREBUCHET
    u8"🛶",     // E_FISHING_BOAT
    u8"🚢",     // E_WARSHIP
    u8"⛴",      // E_TRANSPORT
    u8"🪵",     // E_RAM
    u8"🎯",     // E_CROSSBOWMAN
    u8"🏇",     // E_HUSSAR
    u8"🙏",     // E_MONK
    u8"💣",     // E_SAPPER
    u8"🛒",     // E_WAGON
    u8"🏛",      // E_TOWNHALL
    u8"🏠",     // E_HOUSE
    u8"🏕",      // E_BARRACKS
    u8"🐴",     // E_STABLE
    u8"🗼",     // E_TOWER
    u8"🌾",     // E_FARM
    u8"🔨",     // E_BLACKSMITH
    u8"⛪",     // E_CHURCH
    u8"🏪",     // E_MARKET
    u8"🧱",     // E_WALL
    u8"🚪",     // E_GATE
    u8"🏰",     // E_CASTLE
    u8"🪵",     // E_LUMBER_CAMP
    u8"⛏",      // E_MINING_CAMP
    u8"⚙",      // E_MILL
    u8"⚓",     // E_DOCK
    u8"🧺",     // E_GRANARY
    u8"🍺",     // E_TAVERN
    u8"⛲",     // E_WELL
    u8"🏡",     // E_MANOR
    u8"⚒",      // E_STONEMASON
    u8"🕯",      // E_SHRINE
    u8"💦",     // E_WATERMILL
    u8"⚖",      // E_TRADING_POST
    u8"🕳",      // E_WOLF_DEN
    u8"🏚",      // E_RUIN
    u8"🌉",     // E_BRIDGE
    u8"🦌",     // E_DEER
    u8"🐺",     // E_WOLF
    u8"🐑",     // E_SHEEP
    u8"🐗",     // E_BOAR
};
static_assert(sizeof(ENTITY_EMOJI)/sizeof(ENTITY_EMOJI[0]) == (size_t)E_BOAR + 1,
    "ENTITY_EMOJI must have an entry for every EntityType (0..E_BOAR)");

const char* getCharEmoji(char ch) {
    if (displayMode == DM_ASCII) {
        static char bufs[4][2];
        static int idx = 0;
        int i = (idx++) & 3;
        bufs[i][0] = ch;
        bufs[i][1] = '\0';
        return bufs[i];
    }

    // Generic fallback mapping. Terrain rendering uses a terrain-aware helper
    // in render.cpp so e.g. berries can be 🫐 while gravel remains a symbol.
    switch (ch) {
    case '.': return u8"·";
    case '"': return u8"⁝";
    case '*': return u8"✿";
    case ',': return u8"∙";
    case 'T': return u8"🌳";
    case 'Y': return u8"🌲";
    case 'y': return u8"🌴";
    case 't': return u8"🪵";
    case '^': return u8"▲";
    case 'n': return u8"⌒";
    case 'o': return u8"▪";
    case '~': return u8"≈";
    case '=': return u8"≋";
    case '|': return u8"╿";
    case '$': return u8"🪙";
    case '%': return u8"🌾";
    case ':': return u8"⁘";
    case '&': return u8"⌂";
    case '#': return u8"▓";
    case '-': return u8"─";
    case '/': return u8"╱";
    case '\\': return u8"╲";
    case 'c': return u8"🛞";
    case 'r': return u8"🪵";
    case '!': return u8"!";
    case ' ': return "  ";
    default: {
        static char fallback[2] = {0,0};
        fallback[0] = ch;
        return fallback;
    }
    }
}

const char* getEntityEmoji(int etype) {
    if (displayMode == DM_ASCII) {
        static char bufs[4][2];
        static int idx = 0;
        int i = (idx++) & 3;
        bufs[i][0] = STATS[etype].glyph;
        bufs[i][1] = '\0';
        return bufs[i];
    }
    if (etype < 0 || etype >= (int)(sizeof(ENTITY_EMOJI)/sizeof(ENTITY_EMOJI[0])))
        return u8"?";
    return ENTITY_EMOJI[etype];
}

bool glyphIsWidth1(const char* str) {
    (void)str;
    return false;
}

void putGlyph(int y, int x, char ascii_ch) {
    if (displayMode == DM_ASCII) {
        mvaddch(y, x, (chtype)(unsigned char)ascii_ch);
    } else {
        mvaddstr(y, x, "  ");
        mvprintw(y, x, "%s", getCharEmoji(ascii_ch));
    }
}

void putEntityGlyph(int y, int x, int etype) {
    if (displayMode == DM_ASCII) {
        mvaddch(y, x, (chtype)(unsigned char)STATS[etype].glyph);
    } else {
        mvaddstr(y, x, "  ");
        mvprintw(y, x, "%s", getEntityEmoji(etype));
    }
}
