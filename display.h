#pragma once

// Match pace selected on the splash screen. Scales the wall-clock tick period
// only — sim logic is fixed-step, so this never affects determinism/replays.
enum GameSpeed { GS_SLOW, GS_NORMAL, GS_FAST };
extern GameSpeed gameSpeed;
inline int tickPeriodMs() {
    switch (gameSpeed) { case GS_SLOW: return 120; case GS_FAST: return 55; default: return 80; }
}

// Returns the entity's ASCII glyph as a NUL-terminated string (for %s prints).
const char* entityGlyphStr(int etype);
