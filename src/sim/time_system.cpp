#include "realm.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float getBrightness(const Game& game) { return std::max(0.0f, std::min(1.0f, sinf(game.dayPhase * M_PI))); }
Season getSeason(const Game& game) { return (Season)((int)game.seasonPhase % 4); }
float getSeasonProgress(const Game& game) { return game.seasonPhase - (int)game.seasonPhase; }
const char* getSeasonName(const Game& game) {
    const char* n[] = {"Spring","Summer","Autumn","Winter"};
    return n[getSeason(game)];
}
const char* getTimeName(const Game& game) {
    float b = getBrightness(game);
    if (b > 0.85f) return "Noon";
    if (b > 0.6f)  return "Day";
    if (b > 0.35f) return game.dayPhase < 0.5f ? "Dawn" : "Dusk";
    if (b > 0.15f) return "Twilight";
    return "Night";
}
bool isNight(const Game& game) { return getBrightness(game) < 0.3f; }
bool isDusk(const Game& game)  { float b = getBrightness(game); return b >= 0.3f && b < 0.55f && game.dayPhase > 0.5f; }
bool isDawn(const Game& game)  { float b = getBrightness(game); return b >= 0.3f && b < 0.55f && game.dayPhase < 0.5f; }
