#include "order_service.h"
#include "core/entity_query.h"
#include "core/game_events.h"
#include "core/world_index.h"

namespace {

ServiceResult ok() {
    return { true, nullptr };
}

ServiceResult fail(const char* reason) {
    return { false, reason };
}

Entity* entityIn(Game& game, const WorldIndex& world, EntityId id) {
    return entityById(game, world, id);
}

const Entity* entityIn(const Game& game, const WorldIndex& world, EntityId id) {
    return entityById(game, world, id);
}

Entity* selectedEntity(Game& game, const WorldIndex& world, const Selection& selection) {
    return entityIn(game, world, selection.primaryId);
}

bool ownedUnit(const Entity* entity, PlayerId issuer) {
    return entity && entity->alive && entity->owner == issuer && isUnit(entity->type);
}

std::vector<Entity*> selectedOwnedUnits(Game& game, const WorldIndex& world, const Selection& selection, PlayerId issuer) {
    std::vector<Entity*> units;
    for (int id : selection.ids) {
        Entity* entity = entityIn(game, world, id);
        if (ownedUnit(entity, issuer)) units.push_back(entity);
    }
    return units;
}

} // namespace

ServiceResult canMove(const Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Move target is out of bounds.");
    const Entity* unit = entityIn(game, world, unitId);
    if (!ownedUnit(unit, issuer)) return fail("Selected unit cannot move.");
    if (unit->type == E_TREBUCHET && unit->packed == 0)
        return fail("Pack the trebuchet first [D].");
    return ok();
}

ServiceResult startMove(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Move target is out of bounds.");
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail("Selected unit cannot move.");
    if (selection.ids.size() > 1) {
        orderGroupMove(selection, target.x, target.y, issuer);
        return ok();
    }
    ServiceResult allowed = canMove(game, world, issuer, selection.primaryId, target);
    if (!allowed.ok) return allowed;
    orderMove(game, *units.front(), target.x, target.y);
    return ok();
}

ServiceResult startAttackMove(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Attack-move target is out of bounds.");
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail("Selected unit cannot attack-move.");
    if (selection.ids.size() > 1) {
        orderGroupAttackMove(selection, target.x, target.y, issuer);
        return ok();
    }
    ServiceResult moved = canMove(game, world, issuer, selection.primaryId, target);
    if (!moved.ok) return fail("Selected unit cannot attack-move.");
    orderMove(game, *units.front(), target.x, target.y);
    units.front()->attackMove = 1;
    emitStatusEvent(issuer, "Attack-moving.");
    return ok();
}

ServiceResult canAttack(const Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, EntityId targetId) {
    const Entity* unit = entityIn(game, world, unitId);
    if (!ownedUnit(unit, issuer)) return fail("Selected unit cannot attack.");
    const Entity* target = entityIn(game, world, targetId);
    if (!target) return fail("Attack target is invalid.");
    if (unit->type == E_RAM && !isBuilding(target->type)) return fail("Rams can only attack buildings.");
    if (unit->type == E_TREBUCHET && unit->packed == 1) return fail("Deploy the trebuchet first [D].");
    if (unit->type == E_TREBUCHET && unit->packTicks > 0) return fail("Trebuchet is already changing stance.");
    return ok();
}

ServiceResult startAttack(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, EntityId targetId) {
    const Entity* target = entityIn(game, world, targetId);
    if (!target) return fail("Attack target is invalid.");
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail("Selected unit cannot attack.");
    if (selection.ids.size() > 1) {
        orderGroupAttack(selection, targetId, issuer);
        return ok();
    }
    ServiceResult allowed = canAttack(game, world, issuer, selection.primaryId, targetId);
    if (!allowed.ok) return allowed;
    orderAttack(game, world, *units.front(), targetId);
    return ok();
}

ServiceResult canGatherAt(Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Gather target is out of bounds.");
    const Entity* unit = entityIn(game, world, unitId);
    if (!unit || unit->owner != issuer || !canGather(unit->type)) return fail("Selected unit cannot gather.");
    Entity* carcass = corpseAt(game, world, target.x, target.y);
    if (carcass && isHarvestableCarcass(*carcass) && unit->type == E_PEASANT) return ok();
    Terrain terrain = game.map[target.y][target.x].terrain;
    CargoResource resource = resourceForTerrain(terrain);
    if (!isNaval(unit->type) && (resource == CR_GOLD || resource == CR_WOOD || resource == CR_FOOD)) return ok();
    if (isNaval(unit->type) && resource == CR_FISH) return ok();
    return fail("Gather target has no compatible resource.");
}

ServiceResult startGather(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    ServiceResult allowed = canGatherAt(game, world, issuer, selection.primaryId, target);
    if (!allowed.ok) return allowed;
    Entity* unit = selectedEntity(game, world, selection);
    orderGather(game, world, *unit, target.x, target.y);
    return ok();
}

ServiceResult canHelp(Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, EntityId targetId) {
    const Entity* unit = entityIn(game, world, unitId);
    if (!ownedUnit(unit, issuer) || unit->type != E_PEASANT) return fail("Selected unit cannot help.");
    const Entity* target = entityIn(game, world, targetId);
    if (!target || !target->alive || target->owner != issuer) return fail("Help target is invalid.");
    if (target->underConstruction && isBuilding(target->type)) return ok();
    if (target->type == E_FARM && !target->underConstruction) return ok();
    return fail("Help target is invalid.");
}

ServiceResult startHelp(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, EntityId targetId) {
    const Entity* target = entityIn(game, world, targetId);
    if (!target || !target->alive || target->owner != issuer) return fail("Help target is invalid.");
    bool ordered = false;
    for (int id : selection.ids) {
        ServiceResult allowed = canHelp(game, world, issuer, id, targetId);
        if (!allowed.ok) continue;
        Entity* unit = entityIn(game, world, id);
        orderHelp(game, world, *unit, targetId);
        ordered = true;
    }
    if (!ordered) return fail("No selected units can help.");
    emitStatusEvent(issuer, target->underConstruction ? "Helping build..." : "Tending farm...");
    return ok();
}

ServiceResult canGarrison(const Game& game, const WorldIndex& world, PlayerId issuer, EntityId unitId, EntityId targetId) {
    const Entity* unit = entityIn(game, world, unitId);
    if (!ownedUnit(unit, issuer) || isSiege(unit->type) || isNaval(unit->type))
        return fail("Selected unit cannot garrison.");
    const Entity* target = entityIn(game, world, targetId);
    if (!target || target->owner != issuer || target->underConstruction || !canGarrisonIn(target->type))
        return fail("Garrison target is invalid.");
    if ((int)target->garrison.size() >= garrisonCap(target->type)) return fail("Garrison target is full.");
    return ok();
}

ServiceResult startGarrison(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, EntityId targetId) {
    const Entity* target = entityIn(game, world, targetId);
    if (!target || target->owner != issuer || target->underConstruction || !canGarrisonIn(target->type))
        return fail("Garrison target is invalid.");
    bool ordered = false;
    for (int id : selection.ids) {
        ServiceResult allowed = canGarrison(game, world, issuer, id, targetId);
        if (!allowed.ok) continue;
        Entity* unit = entityIn(game, world, id);
        orderGarrison(game, world, *unit, targetId);
        ordered = true;
    }
    return ordered ? ok() : fail("No selected units can garrison.");
}

ServiceResult ejectGarrisonService(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer || !canGarrisonIn(selected->type))
        return fail("Selected building cannot eject a garrison.");
    if (selected->garrison.empty()) return fail("No garrison to eject.");
    int n = (int)selected->garrison.size();
    ejectGarrison(game, *selected);
    emitStatusEvent(issuer, std::to_string(n) + " unit(s) ejected");
    return ok();
}

ServiceResult setRallyPoint(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Rally target is out of bounds.");
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer) return fail("Rally building is not owned by issuer.");
    selected->rallyX = target.x;
    selected->rallyY = target.y;
    selected->rallySet = 1;
    emitStatusEvent(issuer, "Rally point set.");
    return ok();
}

ServiceResult holdPosition(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    bool changed = false;
    for (Entity* unit : selectedOwnedUnits(game, world, selection, issuer)) {
        unit->state = S_IDLE;
        unit->path.clear();
        unit->pathIdx = 0;
        unit->attackMove = 0;
        unit->holdPosition = 1;
        unit->targetId = -1;
        changed = true;
    }
    if (changed) emitStatusEvent(issuer, "Hold position.");
    return changed ? ok() : fail("No selected units can hold position.");
}

ServiceResult stopUnits(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    bool changed = false;
    for (Entity* unit : selectedOwnedUnits(game, world, selection, issuer)) {
        unit->state = S_IDLE;
        unit->path.clear();
        unit->pathIdx = 0;
        unit->attackMove = 0;
        unit->holdPosition = 0;
        unit->targetId = -1;
        changed = true;
    }
    return changed ? ok() : fail("No selected units can stop.");
}

ServiceResult toggleGateMode(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer || selected->type != E_GATE || selected->underConstruction)
        return fail("Selected entity is not an owned gate.");
    if (!selected->gateLocked) {
        selected->gateLocked = true;
        selected->gateOpen = true;
        emitStatusEvent(issuer, "Gate locked open");
    } else if (selected->gateOpen) {
        selected->gateOpen = false;
        emitStatusEvent(issuer, "Gate locked closed");
    } else {
        selected->gateLocked = false;
        emitStatusEvent(issuer, "Gate auto");
    }
    return ok();
}

ServiceResult toggleTrebuchetPacked(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer || selected->type != E_TREBUCHET)
        return fail("Selected entity is not an owned trebuchet.");
    if (selected->packTicks > 0) return fail("Trebuchet is already changing stance.");
    int ticks = (game.players[issuer].research & R_COUNTERWEIGHT) ? 25 : 40;
    selected->packed = selected->packed ? 0 : 1;
    selected->packTicks = ticks;
    selected->state = S_IDLE;
    selected->targetId = -1;
    selected->path.clear();
    selected->pathIdx = 0;
    emitStatusEvent(issuer, selected->packed ? "Packing trebuchet..." : "Deploying trebuchet...");
    return ok();
}
