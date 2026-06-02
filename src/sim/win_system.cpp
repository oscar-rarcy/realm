#include "realm.h"

void checkWin() {
    checkWin(g);
}

void checkWin(Game& game) {
    int aliveCount = 0; int lastAlive = -1;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!game.players[p].alive) continue;
        bool hasBase = false;
        for (auto& e : game.entities)
            if (e.alive && e.owner==p && (e.type==E_TOWNHALL||e.type==E_CASTLE)) { hasBase=true; break; }
        if (!hasBase) game.players[p].alive = false;
        else { aliveCount++; lastAlive = p; }
    }
    // Human defeat ends the match immediately — no point watching the AIs fight
    // each other after the player's been eliminated.
    if (!game.players[0].alive) { game.winner = -1; game.mode = M_GAME_OVER; return; }
    if (aliveCount <= 1) { game.winner = lastAlive; game.mode = M_GAME_OVER; }
}

// AI moved to ai.cpp.
