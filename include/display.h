#pragma once

// Display mode selected on the splash screen.
enum DisplayMode {
    DM_ASCII,
    DM_EMOJI
};

extern DisplayMode displayMode;

const char* getCharEmoji(char ch);
const char* getEntityEmoji(int etype);
bool glyphIsWidth1(const char* str);

void putGlyph(int y, int x, char ascii_ch);
void putEntityGlyph(int y, int x, int etype);
