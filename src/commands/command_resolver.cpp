#include "command.h"
#include "core/game_events.h"

// Single right-click / Enter command from a selected player unit onto (x,y).
// Picks the right verb based on what's at the tile: help-build / tend-farm /
// garrison / attack / gather / sow-farm / fish / move. Used by both the
// keyboard Enter handler and the mouse right-click handler so the two paths
// can never drift out of sync.
void cmdAtTileSingle(Entity* sel, int x, int y) {
    if (!sel || sel->owner != 0 || !isUnit(sel->type)) return;
    Entity* tgt = entityAt(x, y);
    bool visible = g.map[y][x].visible[0];

    if (tgt && tgt->alive && tgt->owner == 0 && tgt->underConstruction && sel->type == E_PEASANT) {
        orderHelp(*sel, tgt->id); emitStatusEvent(sel->owner, "Helping build..."); return;
    }
    if (tgt && tgt->alive && tgt->owner == 0 && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        orderHelp(*sel, tgt->id); emitStatusEvent(sel->owner, "Tending farm..."); return;
    }
    if (tgt && tgt->alive && tgt->owner == 0 && !tgt->underConstruction
        && canGarrisonIn(tgt->type) && !isSiege(sel->type)) {
        orderGarrison(*sel, tgt->id); return;
    }
    if (tgt && tgt->alive && tgt->owner != 0 && visible) {
        orderAttack(*sel, tgt->id); emitStatusEvent(sel->owner, "Attacking!"); return;
    }
    if (sel->type == E_PEASANT) {
        Entity* carcass = corpseAt(x, y);
        if (carcass && isHarvestableCarcass(*carcass)) {
            orderGather(*sel, x, y);
            emitStatusEvent(sel->owner, "Harvesting carcass...");
            return;
        }
        Terrain ter = g.map[y][x].terrain;
        bool isW = (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
        if ((ter==T_GOLD||isW||ter==T_BERRY) && g.map[y][x].resources > 0) {
            orderGather(*sel, x, y);
            emitStatusEvent(sel->owner, ter==T_GOLD ? "Mining gold..."
                    : ter==T_BERRY ? "Picking berries..." : "Chopping wood...");
            return;
        }
        if (ter == T_WHEAT && !tgt && canPlace(E_FARM, x, y, 0)) {
            int fid = spawnEntity(E_FARM, 0, x, y, true);
            orderHelp(*sel, fid); emitStatusEvent(sel->owner, "Working wheat field...");
            return;
        }
        orderMove(*sel, x, y); emitStatusEvent(sel->owner, "Moving..."); return;
    }
    if (sel->type == E_FISHING_BOAT) {
        Terrain ter = g.map[y][x].terrain;
        if (ter == T_FISH && g.map[y][x].resources > 0) {
            orderGather(*sel, x, y); emitStatusEvent(sel->owner, "Fishing..."); return;
        }
        orderMove(*sel, x, y); emitStatusEvent(sel->owner, "Moving..."); return;
    }
    orderMove(*sel, x, y); emitStatusEvent(sel->owner, "Moving...");
}

// Group right-click / Enter command - applies to the command's explicit
// selection. Same path for keyboard and mouse.
void cmdAtTileGroup(const Selection& selection, int x, int y) {
    Entity* tgt = entityAt(x, y);
    bool visible = g.map[y][x].visible[0];
    if (tgt && tgt->alive && tgt->owner == 0
        && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
        for (int id : selection.ids) {
            Entity* u = findEntity(id);
            if (u && u->alive && u->owner == 0 && isUnit(u->type) && !isSiege(u->type))
                orderGarrison(*u, tgt->id);
        }
        emitStatusEvent(0, "Garrisoning...");
        return;
    }
    if (tgt && tgt->alive && tgt->owner != 0 && visible) {
        orderGroupAttack(selection, tgt->id); return;
    }
    orderGroupMove(selection, x, y);
}

Command resolveContextCommand(const Game& game, const Selection& selection, MapPos target) {
    Command command;
    if (!inBounds(target.x, target.y)) return command;
    command.type = CommandType::Context;
    command.selection = selection;
    command.targetTile = target;
    Entity* targetEntity = entityAt(target.x, target.y);
    if (targetEntity && game.map[target.y][target.x].visible[0]) command.targetEntity = targetEntity->id;
    return command;
}
