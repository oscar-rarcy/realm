#include "realm.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float getBrightness() { return getBrightness(g); }
float getBrightness(const Game& game) { return std::max(0.0f, std::min(1.0f, sinf(game.dayPhase * M_PI))); }
Season getSeason() { return getSeason(g); }
Season getSeason(const Game& game) { return (Season)((int)game.seasonPhase % 4); }
float getSeasonProgress() { return getSeasonProgress(g); }
float getSeasonProgress(const Game& game) { return game.seasonPhase - (int)game.seasonPhase; }
const char* getSeasonName() {
    const char* n[] = {"Spring","Summer","Autumn","Winter"};
    return n[getSeason()];
}
const char* getTimeName() {
    float b = getBrightness();
    if (b > 0.85f) return "Noon";
    if (b > 0.6f)  return "Day";
    if (b > 0.35f) return g.dayPhase < 0.5f ? "Dawn" : "Dusk";
    if (b > 0.15f) return "Twilight";
    return "Night";
}
bool isNight() { return isNight(g); }
bool isNight(const Game& game) { return getBrightness(game) < 0.3f; }
bool isDusk()  { return isDusk(g); }
bool isDusk(const Game& game)  { float b = getBrightness(game); return b >= 0.3f && b < 0.55f && game.dayPhase > 0.5f; }
bool isDawn()  { return isDawn(g); }
bool isDawn(const Game& game)  { float b = getBrightness(game); return b >= 0.3f && b < 0.55f && game.dayPhase < 0.5f; }
