#pragma once

// Display mode selected on the splash screen.
enum DisplayMode {
    DM_ASCII,
    DM_EMOJI
};

extern DisplayMode displayMode;

// Match pace selected on the splash screen. Scales the wall-clock tick period
// only — sim logic is fixed-step, so this never affects determinism/replays.
enum GameSpeed { GS_SLOW, GS_NORMAL, GS_FAST };
extern GameSpeed gameSpeed;
inline int tickPeriodMs() {
    switch (gameSpeed) { case GS_SLOW: return 120; case GS_FAST: return 55; default: return 80; }
}

const char* getCharEmoji(char ch);
const char* getEntityEmoji(int etype);
bool glyphIsWidth1(const char* str);

void putGlyph(int y, int x, char ascii_ch);
void putEntityGlyph(int y, int x, int etype);
