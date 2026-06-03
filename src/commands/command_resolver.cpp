#include "command.h"
#include "realm.h"
#include "core/entity_query.h"
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
        if (tgt && tgt->alive && tgt->owner == issuer
            && ((tgt->underConstruction && isBuilding(tgt->type)) || (tgt->type == E_FARM && !tgt->underConstruction))) {
            command.payload = HelpCommand{ selection, tgt->id };
            return command;
        }
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
        command.payload = HelpCommand{ selection, tgt->id };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        command.payload = HelpCommand{ selection, tgt->id };
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

Command resolveContextCommand(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return Command{};
    return typedContextCommand(game, world, issuer, selection, target);
}
