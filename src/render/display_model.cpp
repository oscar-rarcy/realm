#include "display.h"
#include "realm.h"
#include "input_keys.h"
#include "render/entity_visual_defs.h"
#include <cstring>
#include <cstdint>

// ============================================================
// GLOBAL STATE
// ============================================================
DisplayMode displayMode = DM_ASCII;

// ============================================================
// EMOJI MAPPING — RAW CHARS
//
// Maps the ASCII glyphs produced by getTerrainVisual() and the
// miscellaneous chars used for projectiles, animations, and UI.
//
// All target glyphs verified width-1:
//   U+00A7  §  SECTION SIGN        (2-byte UTF-8: C2 A7)
//   U+00B6  ¶  PILCROW SIGN        (2-byte UTF-8: C2 B6)
//   U+00B7  ·  MIDDLE DOT          (2-byte UTF-8: C2 B7)
//   U+2020  †  DAGGER              (3-byte: E2 80 A0)
//   U+2021  ‡  DOUBLE DAGGER       (3-byte: E2 80 A1)
//   U+2025  ‥  TWO DOT LEADER      (3-byte: E2 80 A5)
//   U+2191  ↑  UPWARDS ARROW       (3-byte: E2 86 91)
//   U+2219  ∙  BULLET OPERATOR     (3-byte: E2 88 99)
//   U+2229  ∩  INTERSECTION        (3-byte: E2 88 A9)
//   U+2237  ∷  PROPORTION          (3-byte: E2 88 B7)
//   U+2248  ≈  ALMOST EQUAL TO     (3-byte: E2 89 88)
//   U+2261  ≡  IDENTICAL TO        (3-byte: E2 89 A1)
//   U+2551  ║  BOX DRAWINGS DOUBLE VERTICAL (3-byte: E2 95 91)
//   U+2571  ╱  BOX LIGHT DIAGONAL  (3-byte: E2 95 B1)
//   U+2572  ╲  BOX LIGHT DIAGONAL  (3-byte: E2 95 B2)
//   U+25A0  ■  BLACK SQUARE        (3-byte: E2 96 A0)
//   U+25AC  ▬  BLACK RECTANGLE     (3-byte: E2 96 AC)
//   U+25B2  ▲  BLACK UP-POINTING TRIANGLE (3-byte: E2 96 B2)
//   U+25B7  ▷  WHITE RIGHT-POINTING TRIANGLE (3-byte: E2 96 B7)
//   U+25BA  ►  BLACK RIGHT-POINTING POINTER (3-byte: E2 96 BA)
//   U+25C1  ◁  WHITE LEFT-POINTING TRIANGLE (3-byte: E2 97 81)
//   U+25C4  ◄  BLACK LEFT-POINTING POINTER  (3-byte: E2 97 84)
//   U+25CF  ●  BLACK CIRCLE        (3-byte: E2 97 8F)
//   U+2663  ♣  BLACK CLUB SUIT     (3-byte: E2 99 A3)
//   U+2666  ♦  BLACK DIAMOND SUIT  (3-byte: E2 99 A6)
//   U+2726  ✦  BLACK FOUR POINTED STAR (3-byte: E2 9C A6)
// ============================================================
const char* getCharEmoji(char ch) {
    if (displayMode == DM_ASCII) {
        // Return pointer into a small rotating buffer so callers can store
        // the pointer briefly without it being immediately overwritten.
        static char bufs[4][2];
        static int  idx = 0;
        int i = (idx++) & 3;
        bufs[i][0] = ch;
        bufs[i][1] = '\0';
        return bufs[i];
    }
    // Emoji mode: map every char that appears as a tile glyph.
    switch (ch) {
    // Terrain / ground
    case '.': return "\xc2\xb7";       // · U+00B7  MIDDLE DOT       (bare ground)
    case '"': return "\xe2\x80\xa5";   // ‥ U+2025  TWO DOT LEADER   (tall grass)
    case '*': return "\xe2\x9c\xa6";   // ✦ U+2726  FOUR POINTED STAR (flowers/lava-hot)
    case ',': return "\xe2\x88\x99";   // ∙ U+2219  BULLET OPERATOR  (meadow/mud)
    case 'T': return "\xe2\x99\xa3";   // ♣ U+2663  CLUB SUIT        (oak forest)
    case 'Y': return "\xe2\x86\x91";   // ↑ U+2191  UPWARDS ARROW    (pine)
    case 'y': return "\xe2\x80\xa1";   // ‡ U+2021  DOUBLE DAGGER    (palm)
    case 't': return "\xe2\x80\xa0";   // † U+2020  DAGGER           (dead tree)
    case '^': return "\xe2\x96\xb2";   // ▲ U+25B2  UP TRIANGLE      (mountain)
    case 'n': return "\xe2\x88\xa9";   // ∩ U+2229  INTERSECTION     (hills)
    case 'o': return "\xe2\x97\x8f";   // ● U+25CF  BLACK CIRCLE     (stone/boar-projectile)
    case '~': return "\xe2\x89\x88";   // ≈ U+2248  ALMOST EQUAL     (water/waves)
    case '=': return "\xe2\x89\xa1";   // ≡ U+2261  IDENTICAL TO     (ice/marsh/frozen)
    case '|': return "\xe2\x95\x91";   // ║ U+2551  DOUBLE VERTICAL  (reeds/gate closed)
    case '$': return "\xe2\x99\xa6";   // ♦ U+2666  DIAMOND SUIT     (gold)
    case '%': return "\xc2\xa7";       // § U+00A7  SECTION SIGN     (wheat/farm)
    case ':': return "\xe2\x88\xb7";   // ∷ U+2237  PROPORTION       (berries/gravel)
    case '&': return "\xc2\xb6";       // ¶ U+00B6  PILCROW          (ruins)
    case '#': return "\xe2\x96\xa0";   // ■ U+25A0  BLACK SQUARE     (wall/road/castle wall)
    // Siege / movement chars
    case '-': return "\xe2\x96\xac";   // ▬ U+25AC  BLACK RECTANGLE  (arm-ready / gate-open / arrow-proj)
    case '/': return "\xe2\x95\xb1";   // ╱ U+2571  LIGHT DIAGONAL   (catapult-firing arm)
    case '\\': return "\xe2\x95\xb2";  // ╲ U+2572  LIGHT DIAGONAL   (reeds animation)
    // Catapult/ram body chars (appear adjacent to the arm)
    case 'c': return "\xe2\x8a\x99";   // ⊙ U+2299  CIRCLED DOT      (catapult body)
    case 'r': return "\xe2\x96\xac";   // ▬ U+25AC  BLACK RECTANGLE  (ram body)
    // Alert / combat
    case '!': return "!";              // keep — universal alert symbol
    // Projectiles / weather
    // '.' already handled above (rain dot → middle dot)
    // '*' already handled above (snow flake → four-pointed star)
    // Minimap / UI chars (passthrough)
    case ' ': return " ";
    default:  {
        // Unknown char: pass through as-is (safe fallback)
        static char fallback[2] = {0, 0};
        fallback[0] = ch;
        return fallback;
    }
    }
}

// ============================================================
// ENTITY EMOJI
// ============================================================
const char* getEntityEmoji(int etype) {
    if (displayMode == DM_ASCII) {
        // Return the single-char ASCII glyph from STATS[].
        static char bufs[4][2];
        static int  idx = 0;
        int i = (idx++) & 3;
        bufs[i][0] = STATS[etype].glyph;
        bufs[i][1] = '\0';
        return bufs[i];
    }
    return entityTerminalEmoji((EntityType)etype);
}

// ============================================================
// WIDTH VALIDATOR
//
// Decodes the first UTF-8 codepoint and checks whether it falls
// in any of the East Asian Wide or Fullwidth blocks.  All of the
// symbols we use are outside those blocks and return true.
// 4-byte UTF-8 (emoji U+1F000+) always returns false — we never
// use them.
// ============================================================
bool glyphIsWidth1(const char* str) {
    if (!str || !*str) return false;
    unsigned char c0 = (unsigned char)str[0];
    uint32_t cp;
    if (c0 < 0x80) {
        cp = c0;                        // ASCII
    } else if ((c0 & 0xE0) == 0xC0) {
        if (!str[1]) return false;
        cp = ((uint32_t)(c0 & 0x1F) << 6)
           | ((uint32_t)((unsigned char)str[1] & 0x3F));
    } else if ((c0 & 0xF0) == 0xE0) {
        if (!str[1] || !str[2]) return false;
        cp = ((uint32_t)(c0 & 0x0F) << 12)
           | ((uint32_t)((unsigned char)str[1] & 0x3F) << 6)
           | ((uint32_t)((unsigned char)str[2] & 0x3F));
    } else {
        return false;  // 4-byte sequences are always double-width emoji
    }
    // Known wide Unicode ranges (East_Asian_Width = Wide or Fullwidth):
    if (cp >= 0x1100 && cp <= 0x115F) return false;  // Hangul Jamo
    if (cp >= 0x2E80 && cp <= 0x303E) return false;  // CJK Radicals / Kangxi
    if (cp >= 0x3040 && cp <= 0xA4CF) return false;  // Hiragana … Yi
    if (cp >= 0xA960 && cp <= 0xA97F) return false;  // Hangul Jamo Extended-A
    if (cp >= 0xAC00 && cp <= 0xD7FF) return false;  // Hangul Syllables
    if (cp >= 0xF900 && cp <= 0xFAFF) return false;  // CJK Compat Ideographs
    if (cp >= 0xFE10 && cp <= 0xFE19) return false;  // Vertical forms
    if (cp >= 0xFE30 && cp <= 0xFE6F) return false;  // CJK Compat Forms
    if (cp >= 0xFF00 && cp <= 0xFF60) return false;  // Fullwidth Latin/Katakana
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return false;  // Fullwidth signs
    return true;
}

// ============================================================
// LOW-LEVEL GLYPH OUTPUT
//
// Writes one map cell at (y, x) using the current display mode.
// Caller is responsible for setting colour attrs before this call.
// We use mvprintw with explicit coordinates so ncurses' internal
// cursor model doesn't accumulate for multi-byte strings — every
// subsequent cell is always positioned absolutely via mv*.
// ============================================================
#ifndef USE_SDL_RENDERER
void putGlyph(int y, int x, char ascii_ch) {
    if (displayMode == DM_ASCII) mvaddch(y, x, (chtype)(unsigned char)ascii_ch);
    else mvprintw(y, x, "%s", getCharEmoji(ascii_ch));
}
void putEntityGlyph(int y, int x, int etype) {
    if (displayMode == DM_ASCII) mvaddch(y, x, (chtype)(unsigned char)STATS[etype].glyph);
    else mvprintw(y, x, "%s", getEntityEmoji(etype));
}
#else
void putGlyph(int, int, char) {}
void putEntityGlyph(int, int, int) {}
#endif
