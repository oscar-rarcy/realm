#include "order_service.h"
#include "realm.h"
#include "core/entity_facing.h"
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

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

void emitActionMarker(EventSink& events, int player, MapPos tile, char glyph) {
    events.emit({ GameEventType::ActionMarker, player, -1, tile, "", glyph });
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

void clearQueuedMovement(Entity& unit) {
    unit.waypoints.clear();
    unit.patrolMode = false;
}

int rolePriority(EntityType type) {
    switch (type) {
        case E_KNIGHT:    return 0;
        case E_MILITIA:   return 1;
        case E_SPEARMAN:  return 1;
        case E_PEASANT:   return 2;
        case E_ARCHER:    return 3;
        case E_CATAPULT:  return 4;
        case E_TREBUCHET: return 4;
        default:          return 5;
    }
}

std::vector<Entity*> selectedOwnedUnits(Game& game, const WorldIndex& world, const Selection& selection, PlayerId issuer) {
    std::vector<Entity*> units;
    for (int id : selection.ids) {
        Entity* entity = entityIn(game, world, id);
        if (ownedUnit(entity, issuer)) units.push_back(entity);
    }
    return units;
}

ServiceResult startSingleMove(Game& game, const WorldIndex& world, EventSink& events, Entity& unit, MapPos target, bool attackMove, bool clearQueued = true) {
    if (unit.type == E_TREBUCHET && unit.packed == 0)
        return fail("Pack the trebuchet first [D].");
    if (clearQueued) clearQueuedMovement(unit);
    unit.state = S_MOVING;
    unit.targetX = target.x;
    unit.targetY = target.y;
    unit.targetId = -1;
    unit.stuckTicks = 0;
    unit.attackMove = attackMove ? 1 : 0;
    unit.holdPosition = 0;
    unit.path = findPath(game, world, unit.x, unit.y, target.x, target.y, 300, isNaval(unit.type));
    unit.pathIdx = 0;
    if (unit.path.empty() && (unit.x != target.x || unit.y != target.y)) {
        unit.state = S_IDLE;
        return fail("Can't reach there.");
    }
    emitActionMarker(events, unit.owner, target, attackMove ? '!' : 'x');
    return ok();
}

ServiceResult startGroupMove(Game& game, const WorldIndex& world, EventSink& events, const Selection& selection, MapPos target, bool attackMove, PlayerId issuer) {
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail(attackMove ? "Selected unit cannot attack-move." : "Selected unit cannot move.");
    std::sort(units.begin(), units.end(), [](Entity* a, Entity* b) {
        return rolePriority(a->type) < rolePriority(b->type);
    });
    int unitCount = (int)units.size();
    int cols = std::max(1, (int)std::ceil(std::sqrt((double)unitCount)));
    int rows = (unitCount + cols - 1) / cols;
    int half = (cols - 1) / 2;
    int cx = 0, cy = 0;
    for (Entity* unit : units) {
        cx += unit->x;
        cy += unit->y;
    }
    cx /= unitCount;
    cy /= unitCount;
    int dx = target.x - cx;
    int dy = target.y - cy;
    bool horizontal = std::abs(dx) >= std::abs(dy);
    int sx = (dx > 0) ? 1 : (dx < 0 ? -1 : 1);
    int sy = (dy > 0) ? 1 : (dy < 0 ? -1 : 1);

    std::vector<MapPos> slots;
    slots.reserve(unitCount);
    for (int r = 0; r < rows && (int)slots.size() < unitCount; r++) {
        for (int c = 0; c < cols && (int)slots.size() < unitCount; c++) {
            int slotX = horizontal ? target.x - sx * r : target.x + (c - half);
            int slotY = horizontal ? target.y + (c - half) : target.y - sy * r;
            slots.push_back({
                std::max(0, std::min(slotX, MAP_W - 1)),
                std::max(0, std::min(slotY, MAP_H - 1)),
            });
        }
    }

    bool ordered = false;
    const char* firstFailure = nullptr;
    for (int i = 0; i < unitCount && i < (int)slots.size(); i++) {
        ServiceResult moved = startSingleMove(game, world, events, *units[i], slots[i], attackMove);
        if (moved.ok) ordered = true;
        else if (!firstFailure) firstFailure = moved.reason;
    }
    if (!ordered) return fail(firstFailure ? firstFailure : "Selected unit cannot move.");
    emitStatus(events, issuer, attackMove ? "Attack-move in formation!" : "Group moving in formation...");
    return ok();
}

ServiceResult startSingleAttack(Game& game, const WorldIndex& world, EventSink& events, Entity& unit, EntityId targetId) {
    Entity* target = findEntity(game, world, targetId);
    if (!target) return fail("Attack target is invalid.");
    if (unit.type == E_RAM && !isBuilding(target->type)) return fail("Rams can only attack buildings.");
    if (unit.type == E_TREBUCHET && unit.packed == 1) return fail("Deploy the trebuchet first [D].");
    if (unit.type == E_TREBUCHET && unit.packTicks > 0) return fail("Trebuchet is already changing stance.");
    clearQueuedMovement(unit);
    unit.holdPosition = 0;
    unit.state = S_ATTACKING;
    unit.targetId = targetId;
    unit.targetX = target->x;
    unit.targetY = target->y;
    unit.stuckTicks = 0;
    faceEntityTowardTile(unit, target->x, target->y);
    if (dist(unit.x, unit.y, target->x, target->y) > unitRange(game, unit)) {
        unit.path = findPathFor(game, unit, target->x, target->y);
    } else {
        unit.path.clear();
    }
    unit.pathIdx = 0;
    emitActionMarker(events, unit.owner, { target->x, target->y }, '!');
    return ok();
}

ServiceResult startSingleGather(Game& game, const WorldIndex& world, EventSink& events, Entity& unit, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Gather target is out of bounds.");
    Entity* carcass = corpseAt(game, world, target.x, target.y);
    if (carcass && isHarvestableCarcass(*carcass) && unit.type == E_PEASANT && canGather(unit.type)) {
        clearQueuedMovement(unit);
        unit.cargo.type = CR_FOOD;
        unit.cargo.sourceX = target.x;
        unit.cargo.sourceY = target.y;
        unit.resourceX = target.x;
        unit.resourceY = target.y;
        unit.state = S_GATHERING;
        unit.targetX = target.x;
        unit.targetY = target.y;
        unit.targetId = -1;
        faceEntityTowardTile(unit, target.x, target.y);
        emitActionMarker(events, unit.owner, target, '+');
        int bestAX = target.x, bestAY = target.y, bestAD = 99999;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = target.x + dx, ny = target.y + dy;
            if (!inBounds(nx, ny) || !isPassable(game, nx, ny)) continue;
            int d = mdist(unit.x, unit.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
        unit.path = findPath(game, world, unit.x, unit.y, bestAX, bestAY, 300, false);
        unit.pathIdx = 0;
        unit.gatherCd = 0;
        unit.cargo.amount = 0;
        return ok();
    }

    Terrain terrain = game.map[target.y][target.x].terrain;
    CargoResource resource = resourceForTerrain(terrain);
    if (canGather(unit.type) && !isNaval(unit.type)) {
        if (resource != CR_GOLD && resource != CR_WOOD && resource != CR_FOOD)
            return fail("Gather target has no compatible resource.");
    } else if (canGather(unit.type) && isNaval(unit.type)) {
        if (resource != CR_FISH) return fail("Gather target has no compatible resource.");
    } else {
        return fail("Selected unit cannot gather.");
    }

    clearQueuedMovement(unit);
    unit.cargo.type = resource;
    unit.cargo.sourceX = target.x;
    unit.cargo.sourceY = target.y;
    unit.resourceX = target.x;
    unit.resourceY = target.y;
    unit.state = S_GATHERING;
    unit.targetX = target.x;
    unit.targetY = target.y;
    unit.targetId = -1;
    faceEntityTowardTile(unit, target.x, target.y);
    emitActionMarker(events, unit.owner, target, '+');
    bool naval = isNaval(unit.type);
    int bestAX = target.x, bestAY = target.y, bestAD = 99999;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;
        int nx = target.x + dx, ny = target.y + dy;
        if (!inBounds(nx, ny)) continue;
        bool passable = naval ? isPassableWater(game, nx, ny) : isPassable(game, nx, ny);
        if (!passable) continue;
        int d = mdist(unit.x, unit.y, nx, ny);
        if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
    }
    unit.path = findPath(game, world, unit.x, unit.y, bestAX, bestAY, 300, naval);
    unit.pathIdx = 0;
    unit.gatherCd = 0;
    unit.cargo.amount = 0;
    return ok();
}

ServiceResult startSingleHelp(Game& game, const WorldIndex& world, Entity& unit, EntityId targetId) {
    if (!canBuild(unit.type)) return fail("Selected unit cannot help.");
    Entity* target = findEntity(game, world, targetId);
    if (!target || !target->alive) return fail("Help target is invalid.");
    if (!target->underConstruction && target->type != E_FARM) return fail("Help target is invalid.");
    clearQueuedMovement(unit);
    unit.state = S_BUILDING;
    unit.targetId = targetId;
    unit.targetX = target->x;
    unit.targetY = target->y;
    faceEntityTowardTile(unit, target->x, target->y);
    int targetW = STATS[target->type].sizeW;
    int targetH = STATS[target->type].sizeH;
    int bestAX = target->x - 1, bestAY = target->y, bestAD = 99999;
    for (int dy = -1; dy <= targetH; dy++) for (int dx = -1; dx <= targetW; dx++) {
        if (dx >= 0 && dx < targetW && dy >= 0 && dy < targetH) continue;
        int nx = target->x + dx, ny = target->y + dy;
        if (inBounds(nx, ny) && isPassable(game, nx, ny)) {
            int d = mdist(unit.x, unit.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    unit.path = findPath(game, world, unit.x, unit.y, bestAX, bestAY, 300, isNaval(unit.type));
    unit.pathIdx = 0;
    return ok();
}

ServiceResult startSingleGarrison(Game& game, const WorldIndex& world, Entity& unit, EntityId targetId) {
    Entity* target = findEntity(game, world, targetId);
    if (!target || !target->alive || target->underConstruction) return fail("Garrison target is invalid.");
    if (target->owner != unit.owner || !canGarrisonIn(target->type)) return fail("Garrison target is invalid.");
    if (!isUnit(unit.type) || isSiege(unit.type) || isNaval(unit.type)) return fail("Selected unit cannot garrison.");
    if ((int)target->garrison.size() >= garrisonCap(target->type)) return fail("Garrison target is full.");
    clearQueuedMovement(unit);
    unit.state = S_ENTERING;
    unit.targetId = targetId;
    unit.targetX = target->x;
    unit.targetY = target->y;
    unit.stuckTicks = 0;
    int targetW = STATS[target->type].sizeW;
    int targetH = STATS[target->type].sizeH;
    int bestAX = target->x - 1, bestAY = target->y, bestAD = 99999;
    for (int dy = -1; dy <= targetH; dy++) for (int dx = -1; dx <= targetW; dx++) {
        if (dx >= 0 && dx < targetW && dy >= 0 && dy < targetH) continue;
        int nx = target->x + dx, ny = target->y + dy;
        if (inBounds(nx, ny) && isPassable(game, nx, ny)) {
            int d = mdist(unit.x, unit.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    unit.path = findPath(game, world, unit.x, unit.y, bestAX, bestAY, 300, isNaval(unit.type));
    unit.pathIdx = 0;
    return ok();
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

ServiceResult startMove(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Move target is out of bounds.");
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail("Selected unit cannot move.");
    if (selection.ids.size() > 1) {
        return startGroupMove(game, world, events, selection, target, false, issuer);
    }
    ServiceResult allowed = canMove(game, world, issuer, selection.primaryId, target);
    if (!allowed.ok) return allowed;
    return startSingleMove(game, world, events, *units.front(), target, false);
}

ServiceResult appendWaypoint(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Waypoint target is out of bounds.");
    int count = 0;
    for (Entity* unit : selectedOwnedUnits(game, world, selection, issuer)) {
        if (isNaval(unit->type)) continue;
        unit->waypoints.push_back({ target.x, target.y });
        count++;
    }
    if (count == 0) return fail("No selected land units can queue a waypoint.");
    emitStatus(events, issuer, "Waypoint queued (" + std::to_string(count) + " units)");
    emitActionMarker(events, issuer, target, 'x');
    return ok();
}

ServiceResult startPatrol(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Patrol target is out of bounds.");
    int count = 0;
    for (Entity* unit : selectedOwnedUnits(game, world, selection, issuer)) {
        if (isNaval(unit->type)) continue;
        if (unit->x == target.x && unit->y == target.y) continue;
        unit->waypoints.clear();
        unit->patrolMode = true;
        unit->waypoints.push_back({ target.x, target.y });
        unit->waypoints.push_back({ unit->x, unit->y });
        MapPos first{ unit->waypoints.front().first, unit->waypoints.front().second };
        unit->waypoints.erase(unit->waypoints.begin());
        unit->waypoints.push_back({ first.x, first.y });
        if (startSingleMove(game, world, events, *unit, first, false, false).ok) count++;
    }
    if (count == 0) return fail("No valid units for patrol.");
    emitStatus(events, issuer, std::to_string(count) + " unit(s) on patrol");
    return ok();
}

ServiceResult startAttackMove(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Attack-move target is out of bounds.");
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail("Selected unit cannot attack-move.");
    if (selection.ids.size() > 1) {
        return startGroupMove(game, world, events, selection, target, true, issuer);
    }
    ServiceResult moved = canMove(game, world, issuer, selection.primaryId, target);
    if (!moved.ok) return fail("Selected unit cannot attack-move.");
    ServiceResult ordered = startSingleMove(game, world, events, *units.front(), target, true);
    if (!ordered.ok) return ordered;
    emitStatus(events, issuer, "Attack-moving.");
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

ServiceResult startAttack(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, EntityId targetId) {
    const Entity* target = entityIn(game, world, targetId);
    if (!target) return fail("Attack target is invalid.");
    std::vector<Entity*> units = selectedOwnedUnits(game, world, selection, issuer);
    if (units.empty()) return fail("Selected unit cannot attack.");
    if (selection.ids.size() > 1) {
        bool ordered = false;
        for (Entity* unit : units) {
            ServiceResult attacked = startSingleAttack(game, world, events, *unit, targetId);
            ordered = ordered || attacked.ok;
        }
        if (ordered) emitStatus(events, issuer, "Group attacking!");
        return ordered ? ok() : fail("No selected units can attack that target.");
    }
    ServiceResult allowed = canAttack(game, world, issuer, selection.primaryId, targetId);
    if (!allowed.ok) return allowed;
    return startSingleAttack(game, world, events, *units.front(), targetId);
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

ServiceResult startGather(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, MapPos target) {
    ServiceResult allowed = canGatherAt(game, world, issuer, selection.primaryId, target);
    if (!allowed.ok) return allowed;
    Entity* unit = selectedEntity(game, world, selection);
    return startSingleGather(game, world, events, *unit, target);
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

ServiceResult startHelp(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, EntityId targetId) {
    const Entity* target = entityIn(game, world, targetId);
    if (!target || !target->alive || target->owner != issuer) return fail("Help target is invalid.");
    bool ordered = false;
    for (int id : selection.ids) {
        ServiceResult allowed = canHelp(game, world, issuer, id, targetId);
        if (!allowed.ok) continue;
        Entity* unit = entityIn(game, world, id);
        if (!startSingleHelp(game, world, *unit, targetId).ok) continue;
        ordered = true;
    }
    if (!ordered) return fail("No selected units can help.");
    emitStatus(events, issuer, target->underConstruction ? "Helping build..." : "Tending farm...");
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

ServiceResult startGarrison(Game& game, const WorldIndex& world, EventSink&, PlayerId issuer, const Selection& selection, EntityId targetId) {
    const Entity* target = entityIn(game, world, targetId);
    if (!target || target->owner != issuer || target->underConstruction || !canGarrisonIn(target->type))
        return fail("Garrison target is invalid.");
    bool ordered = false;
    for (int id : selection.ids) {
        ServiceResult allowed = canGarrison(game, world, issuer, id, targetId);
        if (!allowed.ok) continue;
        Entity* unit = entityIn(game, world, id);
        if (!startSingleGarrison(game, world, *unit, targetId).ok) continue;
        ordered = true;
    }
    return ordered ? ok() : fail("No selected units can garrison.");
}

ServiceResult ejectGarrisonService(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection) {
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer || !canGarrisonIn(selected->type))
        return fail("Selected building cannot eject a garrison.");
    if (selected->garrison.empty()) return fail("No garrison to eject.");
    int n = (int)selected->garrison.size();
    ejectGarrison(game, *selected);
    emitStatus(events, issuer, std::to_string(n) + " unit(s) ejected");
    return ok();
}

ServiceResult setRallyPoint(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return fail("Rally target is out of bounds.");
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer) return fail("Rally building is not owned by issuer.");
    selected->rallyX = target.x;
    selected->rallyY = target.y;
    selected->rallySet = 1;
    emitStatus(events, issuer, "Rally point set.");
    return ok();
}

ServiceResult holdPosition(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection) {
    bool changed = false;
    for (Entity* unit : selectedOwnedUnits(game, world, selection, issuer)) {
        clearQueuedMovement(*unit);
        unit->state = S_IDLE;
        unit->path.clear();
        unit->pathIdx = 0;
        unit->attackMove = 0;
        unit->holdPosition = 1;
        unit->targetId = -1;
        changed = true;
    }
    if (changed) emitStatus(events, issuer, "Hold position.");
    return changed ? ok() : fail("No selected units can hold position.");
}

ServiceResult stopUnits(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection) {
    bool changed = false;
    for (Entity* unit : selectedOwnedUnits(game, world, selection, issuer)) {
        clearQueuedMovement(*unit);
        unit->state = S_IDLE;
        unit->path.clear();
        unit->pathIdx = 0;
        unit->attackMove = 0;
        unit->holdPosition = 0;
        unit->targetId = -1;
        changed = true;
    }
    if (changed) emitStatus(events, issuer, "Stopped.", GameEventType::UnitOrdered);
    return changed ? ok() : fail("No selected units can stop.");
}

ServiceResult toggleGateMode(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection) {
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer || selected->type != E_GATE || selected->underConstruction)
        return fail("Selected entity is not an owned gate.");
    if (!selected->gateLocked) {
        selected->gateLocked = true;
        selected->gateOpen = true;
        emitStatus(events, issuer, "Gate locked open");
    } else if (selected->gateOpen) {
        selected->gateOpen = false;
        emitStatus(events, issuer, "Gate locked closed");
    } else {
        selected->gateLocked = false;
        emitStatus(events, issuer, "Gate auto");
    }
    return ok();
}

ServiceResult toggleTrebuchetPacked(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, const Selection& selection) {
    Entity* selected = selectedEntity(game, world, selection);
    if (!selected || selected->owner != issuer || selected->type != E_TREBUCHET)
        return fail("Selected entity is not an owned trebuchet.");
    if (selected->packTicks > 0) return fail("Trebuchet is already changing stance.");
    int ticks = (game.players[issuer].research & R_COUNTERWEIGHT) ? 25 : 40;
    selected->packed = selected->packed ? 0 : 1;
    selected->packTicks = ticks;
    clearQueuedMovement(*selected);
    selected->state = S_IDLE;
    selected->targetId = -1;
    selected->path.clear();
    selected->pathIdx = 0;
    emitStatus(events, issuer, selected->packed ? "Packing trebuchet..." : "Deploying trebuchet...");
    return ok();
}
