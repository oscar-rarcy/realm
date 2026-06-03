#include "realm.h"

const char* projectileTypeName(ProjectileType type) {
    switch (type) {
        case PT_ARROW: return "arrow";
        case PT_CROSSBOW_BOLT: return "crossbow_bolt";
        case PT_FLAMING_ARROW: return "flaming_arrow";
        case PT_TOWER_BOLT: return "tower_bolt";
        case PT_WARSHIP_ARROW_VOLLEY: return "warship_arrow_volley";
        case PT_CATAPULT_BOULDER: return "catapult_boulder";
        case PT_TREBUCHET_BOULDER: return "trebuchet_boulder";
    }
    return "arrow";
}

ProjectileType projectileTypeFromLegacyGlyphColor(char glyph, int color) {
    if (color == CP_PROJ_TOWER) return PT_TOWER_BOLT;
    if (color == CP_PROJ_BOULDER || glyph == 'o') return PT_CATAPULT_BOULDER;
    return PT_ARROW;
}

void spawnProjectile(Game& game, int sx, int sy, int tx, int ty, char gl, int col, ProjectileType type) {
    float dx = (float)(tx-sx), dy = (float)(ty-sy);
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.1f) return;
    Projectile p;
    p.type = type;
    p.x = (float)sx; p.y = (float)sy; p.tx = (float)tx; p.ty = (float)ty;
    p.glyph = gl; p.color = col; p.life = (int)(len/1.5f)+2; p.alive = true;
    game.projectiles.push_back(p);
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
