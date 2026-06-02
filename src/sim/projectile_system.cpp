#include "realm.h"

void spawnProjectile(int sx, int sy, int tx, int ty, char gl, int col) {
    spawnProjectile(g, sx, sy, tx, ty, gl, col);
}

void spawnProjectile(Game& game, int sx, int sy, int tx, int ty, char gl, int col) {
    float dx = (float)(tx-sx), dy = (float)(ty-sy);
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.1f) return;
    Projectile p;
    p.x = (float)sx; p.y = (float)sy; p.tx = (float)tx; p.ty = (float)ty;
    p.glyph = gl; p.color = col; p.life = (int)(len/1.5f)+2; p.alive = true;
    game.projectiles.push_back(p);
}

void tickProjectiles() {
    tickProjectiles(g);
}

void tickProjectiles(Game& game) {
    for (auto& p : game.projectiles) {
        if (!p.alive) continue;
        float dx = p.tx-p.x, dy = p.ty-p.y, len = sqrtf(dx*dx+dy*dy);
        if (len < 1.5f || p.life <= 0) { p.alive = false; continue; }
        p.x += (dx/len)*1.5f; p.y += (dy/len)*1.5f; p.life--;
    }
    if (game.tick % 30 == 0)
        game.projectiles.erase(std::remove_if(game.projectiles.begin(), game.projectiles.end(),
            [](const Projectile& p){ return !p.alive; }), game.projectiles.end());
}
