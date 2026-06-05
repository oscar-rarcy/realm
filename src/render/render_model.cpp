#include "render/render_model.h"
#include "realm.h"

#include <cmath>

static bool isAnimalEntityType(EntityType type) {
    return type == E_DEER || type == E_WOLF || type == E_SHEEP || type == E_BOAR;
}

static void tileGateState(const Game& game, int x, int y, bool& gateOpen, bool& gateLocked) {
    gateOpen = false;
    gateLocked = false;
    for (const Entity& entity : game.entities) {
        if (!entity.alive || entity.type != E_GATE) continue;
        if (entity.x != x || entity.y != y) continue;
        gateOpen = entity.gateOpen;
        gateLocked = entity.gateLocked;
        return;
    }
}

static const char* overlayAssetIdForMarkerGlyph(char glyph) {
    switch (glyph) {
        case '!': return "attack_marker";
        case '#': return "build_marker";
        case '+': return "gather_marker";
        case 'r': return "rally_marker";
        default: return "move_marker";
    }
}

RenderModel buildRenderModel(const Game& game, const std::vector<ActionMarker>& actionMarkers,
                             int observerOwner, int viewX, int viewY, int viewW, int viewH) {
    RenderModel model;
    int x0 = std::max(0, viewX);
    int y0 = std::max(0, viewY);
    int x1 = std::min(MAP_W, viewX + std::max(0, viewW));
    int y1 = std::min(MAP_H, viewY + std::max(0, viewH));
    if (x0 >= x1 || y0 >= y1) return model;
    model.viewX = x0;
    model.viewY = y0;
    model.viewW = x1 - x0;
    model.viewH = y1 - y0;
    model.mode = game.mode;
    model.buildPreviewType = game.mode == M_BUILD_PLACE || game.mode == M_WALL_DRAG ? game.local.buildPending : E_NONE;
    model.tiles.reserve((x1 - x0) * (y1 - y0));
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        const Tile& tile = game.map[y][x];
        TileRenderInfo info;
        info.x = x;
        info.y = y;
        info.terrain = tile.terrain;
        if (tile.terrain == T_CASTLE_GATE) {
            tileGateState(game, x, y, info.gateOpen, info.gateLocked);
            info.visualParts = visualPartsForTerrain(tile.terrain, tile.biome, tile.resources, tile.wear,
                                                     info.gateOpen, info.gateLocked);
        } else {
            info.visualParts = visualPartsForTile(tile);
        }
        if (observerOwner >= 0 && observerOwner < MAX_PLAYERS) {
            info.visible = tile.visible[observerOwner];
            info.explored = tile.explored[observerOwner];
        }
        model.tiles.push_back(info);
    }

    for (const Entity& entity : game.entities) {
        if (!entity.alive || entity.state == S_GARRISONED) continue;
        if (entity.x < x0 || entity.y < y0 || entity.x >= x1 || entity.y >= y1) continue;
        EntityRenderInfo info;
        info.id = entity.id;
        info.type = entity.type;
        info.owner = entity.owner;
        info.x = entity.x;
        info.y = entity.y;
        info.hp = entity.hp;
        info.maxHp = entity.maxHp;
        info.state = entity.state;
        info.targetId = entity.targetId;
        info.targetX = entity.targetX;
        info.targetY = entity.targetY;
        info.facingDx = entity.facingDx;
        info.facingDy = entity.facingDy;
        info.visualMoveFromX = entity.visualMoveFromX;
        info.visualMoveFromY = entity.visualMoveFromY;
        info.visualMoveToX = entity.visualMoveToX;
        info.visualMoveToY = entity.visualMoveToY;
        info.visualMoveStartedTick = entity.visualMoveStartedTick;
        info.visualMoveDurationTicks = entity.visualMoveDurationTicks;
        info.visualMoveSeq = entity.visualMoveSeq;
        info.alertTicks = entity.alertTicks;
        info.underConstruction = entity.underConstruction;
        info.attackMove = entity.attackMove != 0;
        info.holdPosition = entity.holdPosition != 0;
        info.packed = entity.packed != 0;
        info.packTicks = entity.packTicks;
        info.rallySet = entity.rallySet != 0;
        info.rallyX = entity.rallyX;
        info.rallyY = entity.rallyY;
        info.visible = observerOwner < 0 || observerOwner >= MAX_PLAYERS
            || game.map[entity.y][entity.x].visible[observerOwner];
        info.selected = entity.id == game.local.selectedId
            || std::find(game.local.selectedIds.begin(), game.local.selectedIds.end(), entity.id) != game.local.selectedIds.end();
        if (isBuilding(entity.type)) info.buildingState = buildingVisualState(entity);
        if (isAnimalEntityType(entity.type)) info.animalCarcassState = animalCarcassVisualState(entity);
        if (entity.type == E_TRANSPORT) info.transportState = transportVisualState(entity);
        model.entities.push_back(info);
    }
    for (const Projectile& projectile : game.projectiles) {
        if (!projectile.alive) continue;
        int mx = (int)std::lround(projectile.x);
        int my = (int)std::lround(projectile.y);
        if (mx < x0 || my < y0 || mx >= x1 || my >= y1) continue;
        ProjectileRenderInfo info;
        info.visualId = projectile.visualId;
        info.type = projectile.type;
        info.x = projectile.x;
        info.y = projectile.y;
        info.tx = projectile.tx;
        info.ty = projectile.ty;
        info.visualSpawnX = projectile.visualSpawnX;
        info.visualSpawnY = projectile.visualSpawnY;
        info.visualMoveFromX = projectile.visualMoveFromX;
        info.visualMoveFromY = projectile.visualMoveFromY;
        info.visualMoveToX = projectile.visualMoveToX;
        info.visualMoveToY = projectile.visualMoveToY;
        info.visualMoveStartedTick = projectile.visualMoveStartedTick;
        info.visualMoveDurationTicks = projectile.visualMoveDurationTicks;
        info.visualMoveSeq = projectile.visualMoveSeq;
        info.tileX = mx;
        info.tileY = my;
        info.glyph = projectile.glyph;
        info.color = projectile.color;
        info.life = projectile.life;
        info.alive = projectile.alive;
        info.visible = observerOwner < 0 || observerOwner >= MAX_PLAYERS
            || game.map[my][mx].visible[observerOwner];
        model.projectiles.push_back(info);
    }
    for (const ActionMarker& marker : actionMarkers) {
        if (marker.x < x0 || marker.y < y0 || marker.x >= x1 || marker.y >= y1) continue;
        model.actionMarkers.push_back({ marker.x, marker.y, marker.ticks, marker.glyph });
        model.uiOverlays.push_back({ overlayAssetIdForMarkerGlyph(marker.glyph), marker.x, marker.y, marker.ticks,
                                     marker.glyph, false });
    }
    return model;
}

RenderModel buildRenderModel(const Game& game, int observerOwner, int viewX, int viewY, int viewW, int viewH) {
    static const std::vector<ActionMarker> noActionMarkers;
    return buildRenderModel(game, noActionMarkers, observerOwner, viewX, viewY, viewW, viewH);
}
