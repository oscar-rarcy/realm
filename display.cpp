#include "display.h"
#include "realm.h"

GameSpeed gameSpeed = GS_NORMAL;

const char* entityGlyphStr(int etype) {
    static char bufs[4][2];
    static int idx = 0;
    int i = (idx++) & 3;
    bufs[i][0] = STATS[etype].glyph;
    bufs[i][1] = '\0';
    return bufs[i];
}
