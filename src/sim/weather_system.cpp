#include "realm.h"
#include "core/game_events.h"

namespace {

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

} // namespace

void tickWeather(Game& game, EventSink& events) {
    Season s = getSeason(game);
    float sp = getSeasonProgress(game);

    // Snow doesn't create mud; rain/storm do.
    if (game.tick % 50 == 0) {
        if (game.weather == W_RAIN || game.weather == W_STORM) {
            int hits = (game.weather == W_STORM) ? 60 : 30;
            for (int i = 0; i < hits; i++) {
                int x = realmRand(game) % MAP_W, y = realmRand(game) % MAP_H;
                Tile& t = game.map[y][x];
                if (t.terrain == T_GRASS || t.terrain == T_MEADOW
                 || t.terrain == T_DIRT  || t.terrain == T_TALL_GRASS) {
                    t.terrain = T_MUD;
                }
            }
        } else {
            // Drying — mud reverts to dirt once skies clear (or freeze over).
            for (int i = 0; i < 40; i++) {
                int x = realmRand(game) % MAP_W, y = realmRand(game) % MAP_H;
                Tile& t = game.map[y][x];
                if (t.terrain == T_MUD) t.terrain = T_DIRT;
            }
        }
    }

    // Season-appropriate weather: rain/storm can't persist into winter; snow can't persist into spring/summer.
    if (s == WINTER && (game.weather == W_RAIN || game.weather == W_STORM)) {
        game.weather = W_SNOW;
        game.weatherTimer = 300;
        if (game.players[0].alive) emitStatus(events, 0, "The rain turns to snow.");
        return;
    }
    bool lateAutumn = (s == AUTUMN && sp > 0.5f);
    if (!lateAutumn && s != WINTER && game.weather == W_SNOW) {
        game.weather = W_CLEAR;
        game.weatherTimer = 100;
        if (game.players[0].alive) emitStatus(events, 0, "The skies clear.");
        return;
    }

    if (game.weatherTimer > 0) { game.weatherTimer--; return; }

    int roll = realmRand(game) % 100;

    if (s == WINTER) {
        // Winter: only clear or snow.
        if (game.weather == W_CLEAR) {
            if (roll < 40) { game.weather = W_SNOW; game.weatherTimer = 500 + realmRand(game) % 900; }
            else             game.weatherTimer = 300 + realmRand(game) % 500;
        } else { // W_SNOW
            if (roll < 50) game.weather = W_CLEAR;
            game.weatherTimer = 300 + realmRand(game) % 600;
        }
    } else if (lateAutumn) {
        // Late autumn: rain fades, first snows begin. Progress 0.5→1 maps to 0→1 of this range.
        float late = (sp - 0.5f) * 2.0f;
        int snowBias  = (int)(late * 30);           // up to 30% snow chance by end of autumn
        int rainBias  = (int)(50 * (1.0f - late * 0.6f)); // rain fades 50→20
        int stormBias = (int)(15 * (1.0f - late));  // storms fade out entirely
        if (game.weather == W_CLEAR) {
            if      (roll < stormBias)              game.weather = W_STORM;
            else if (roll < rainBias)               game.weather = W_RAIN;
            else if (roll < rainBias + snowBias)    game.weather = W_SNOW;
            game.weatherTimer = 400 + realmRand(game) % 800;
        } else {
            if (roll < 60) game.weather = W_CLEAR;
            else if (game.weather == W_RAIN  && roll < 75) game.weather = W_STORM;
            else if (game.weather == W_STORM && roll < 80) game.weather = W_RAIN;
            // snow just clears, doesn't escalate
            game.weatherTimer = 300 + realmRand(game) % 600;
        }
    } else {
        // Spring / summer / early autumn: rain and storms only.
        int rainBias  = (s == AUTUMN) ? 50 : (s == SPRING) ? 35 : 25;
        int stormBias = (s == AUTUMN) ? 15 : 8;
        if (game.weather == W_CLEAR) {
            if (roll < stormBias)     game.weather = W_STORM;
            else if (roll < rainBias) game.weather = W_RAIN;
            game.weatherTimer = 400 + realmRand(game) % 800;
        } else {
            if (roll < 60) game.weather = W_CLEAR;
            else if (game.weather == W_RAIN  && roll < 75) game.weather = W_STORM;
            else if (game.weather == W_STORM && roll < 80) game.weather = W_RAIN;
            game.weatherTimer = 300 + realmRand(game) % 600;
        }
    }

    if (game.players[0].alive) {
        if      (game.weather == W_RAIN)  emitStatus(events, 0, "Rain begins.");
        else if (game.weather == W_STORM) emitStatus(events, 0, "A storm rolls in!");
        else if (game.weather == W_SNOW)  emitStatus(events, 0, "Snow begins to fall.");
        else                              emitStatus(events, 0, "The skies clear.");
    }
}
