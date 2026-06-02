#include "command.h"
#include "core/entity_query.h"
#include "core/game_events.h"
#include "core/world_index.h"

namespace {

const Entity* selectedEntity(const Game& game, const WorldIndex& world, const Selection& selection) {
    return entityById(game, world, selection.primaryId);
}

const Entity* corpseAtTile(const Game& game, const WorldIndex& world, MapPos tile) {
    for (EntityId id : entitiesAt(world, tile)) {
        const Entity* entity = entityByIdAny(game, world, id);
        if (entity && !entity->alive && isHarvestableCarcass(*entity)) return entity;
    }
    return nullptr;
}

Command typedContextCommand(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return Command{};
    Command command;
    command.issuer = issuer;

    const Entity* tgt = entityById(game, world, topEntityAt(game, world, target));
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && game.map[target.y][target.x].visible[issuer];

    if (selection.ids.size() > 1) {
        if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
            command.payload = GarrisonCommand{ selection, tgt->id };
            return command;
        }
        if (tgt && tgt->alive && tgt->owner != issuer && visible) {
            command.payload = AttackCommand{ selection, tgt->id };
            return command;
        }
        command.payload = MoveCommand{ selection, target };
        return command;
    }

    const Entity* sel = selectedEntity(game, world, selection);
    if (!sel || sel->owner != issuer || !isUnit(sel->type)) return Command{};
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->underConstruction && sel->type == E_PEASANT) {
        command.payload = ContextCommand{ selection, target };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        command.payload = ContextCommand{ selection, target };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction
        && canGarrisonIn(tgt->type) && !isSiege(sel->type)) {
        command.payload = GarrisonCommand{ selection, tgt->id };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner != issuer && visible) {
        command.payload = AttackCommand{ selection, tgt->id };
        return command;
    }
    if (sel->type == E_PEASANT) {
        const Entity* carcass = corpseAtTile(game, world, target);
        if (carcass && isHarvestableCarcass(*carcass)) {
            command.payload = GatherCommand{ selection, target };
            return command;
        }
        Terrain terrain = game.map[target.y][target.x].terrain;
        bool wood = terrain == T_FOREST || terrain == T_PINE || terrain == T_PALM || terrain == T_DEAD_TREE;
        if ((terrain == T_GOLD || wood || terrain == T_BERRY) && game.map[target.y][target.x].resources > 0) {
            command.payload = GatherCommand{ selection, target };
            return command;
        }
        if (terrain == T_WHEAT && !tgt && canPlace(game, world, E_FARM, target.x, target.y, issuer)) {
            command.payload = BuildCommand{ selection, E_FARM, target };
            return command;
        }
        command.payload = MoveCommand{ selection, target };
        return command;
    }
    if (sel->type == E_FISHING_BOAT) {
        Terrain terrain = game.map[target.y][target.x].terrain;
        command.payload = (terrain == T_FISH && game.map[target.y][target.x].resources > 0)
            ? CommandPayload{ GatherCommand{ selection, target } }
            : CommandPayload{ MoveCommand{ selection, target } };
        return command;
    }
    command.payload = MoveCommand{ selection, target };
    return command;
}

} // namespace

// Single right-click / Enter command from a selected player unit onto (x,y).
// Picks the right verb based on what's at the tile: help-build / tend-farm /
// garrison / attack / gather / fish / move. Used by both the
// keyboard Enter handler and the mouse right-click handler so the two paths
// can never drift out of sync.
void cmdAtTileSingle(Entity* sel, int x, int y, PlayerId issuer) {
    if (!sel || sel->owner != issuer || !isUnit(sel->type)) return;
    Entity* tgt = entityAt(x, y);
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && g.map[y][x].visible[issuer];

    if (tgt && tgt->alive && tgt->owner == issuer && tgt->underConstruction && sel->type == E_PEASANT) {
        orderHelp(*sel, tgt->id); emitStatusEvent(sel->owner, "Helping build..."); return;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        orderHelp(*sel, tgt->id); emitStatusEvent(sel->owner, "Tending farm..."); return;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction
        && canGarrisonIn(tgt->type) && !isSiege(sel->type)) {
        orderGarrison(*sel, tgt->id); return;
    }
    if (tgt && tgt->alive && tgt->owner != issuer && visible) {
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
void cmdAtTileGroup(const Selection& selection, int x, int y, PlayerId issuer) {
    Entity* tgt = entityAt(x, y);
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && g.map[y][x].visible[issuer];
    if (tgt && tgt->alive && tgt->owner == issuer
        && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
        for (int id : selection.ids) {
            Entity* u = findEntity(id);
            if (u && u->alive && u->owner == issuer && isUnit(u->type) && !isSiege(u->type))
                orderGarrison(*u, tgt->id);
        }
        emitStatusEvent(issuer, "Garrisoning...");
        return;
    }
    if (tgt && tgt->alive && tgt->owner != issuer && visible) {
        orderGroupAttack(selection, tgt->id, issuer); return;
    }
    orderGroupMove(selection, x, y, issuer);
}

Command resolveContextCommand(const Game& game, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return Command{};
    WorldIndex world = buildWorldIndex(game);
    return typedContextCommand(game, world, issuer, selection, target);
}

Command resolveContextCommand(const Game& game, const Selection& selection, MapPos target) {
    return resolveContextCommand(game, 0, selection, target);
}
