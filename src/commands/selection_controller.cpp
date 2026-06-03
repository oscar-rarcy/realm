#include "command.h"
#include "view_state.h"
#include "core/entity_defs.h"
#include "core/entity_query.h"
#include "core/game_events.h"
#include "core/game_state_types.h"
#include "core/rng.h"
#include "core/world_index.h"

#include <algorithm>

namespace {

bool validIssuer(PlayerId issuer) {
    return issuer >= 0 && issuer < MAX_PLAYERS;
}

bool validControlSlot(int slot) {
    return slot >= 0 && slot < 9;
}

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

std::vector<int>& controlGroupSlot(Game& game, PlayerId issuer, int slot) {
    return issuer == 0 ? game.controlGroups[slot] : game.controlGroupsByOwner[issuer][slot];
}

} // namespace

Selection currentSelection(const Game& game) {
    Selection selection;
    selection.primaryId = game.selectedId;
    selection.ids = game.selectedIds;
    if (selection.ids.empty() && selection.primaryId >= 0)
        selection.ids.push_back(selection.primaryId);
    return selection;
}

void selectAtTile(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x, int y) {
    if (!inBounds(x, y)) return;
    Entity* ent = entityAtOwner(game, world, x, y, issuer);
    if (ent) {
        game.selectedId = ent->id;
        game.selectedIds.clear();
        emitStatus(events, issuer, std::string("Selected: ") + STATS[ent->type].name);
        return;
    }
    Entity* any = entityAt(game, world, x, y);
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && game.map[y][x].visible[issuer];
    if (any && any->alive && visible) {
        game.selectedId = any->id;
        game.selectedIds.clear();
        emitStatus(events, issuer, std::string(any->owner==OWNER_NATURE ? "Animal: " : "Enemy ") + STATS[any->type].name);
    } else {
        game.selectedId = -1;
        game.selectedIds.clear();
    }
}

void boxSelect(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x0, int y0, int x1, int y1) {
    x0 = std::max(0, std::min(x0, MAP_W-1));
    x1 = std::max(0, std::min(x1, MAP_W-1));
    y0 = std::max(0, std::min(y0, MAP_H-1));
    y1 = std::max(0, std::min(y1, MAP_H-1));
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    game.selectedIds.clear();
    game.selectedId = -1;
    if (validIssuer(issuer)) {
        for (EntityId id : world.unitsByOwner[issuer]) {
            Entity* e = entityById(game, world, id);
            if (!e || e->state == S_GARRISONED) continue;
            if (e->x >= x0 && e->x <= x1 && e->y >= y0 && e->y <= y1) {
                game.selectedIds.push_back(e->id);
                if (game.selectedId < 0) game.selectedId = e->id;
            }
        }
    }
    if (!game.selectedIds.empty())
        emitStatus(events, issuer, std::to_string(game.selectedIds.size()) + " units selected");
}

void selectAllOfTypeInView(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x, int y) {
    if (!inBounds(x, y)) return;
    Entity* ent = entityAtOwner(game, world, x, y, issuer);
    if (!ent || !isUnit(ent->type)) return;
    EntityType t = ent->type;
    game.selectedIds.clear();
    game.selectedId = -1;
    if (validIssuer(issuer)) {
        for (EntityId id : world.unitsByOwner[issuer]) {
            Entity* e = entityById(game, world, id);
            if (!e || e->type != t || e->state == S_GARRISONED) continue;
            if (e->x < view.viewX || e->x >= view.viewX + view.viewW) continue;
            if (e->y < view.viewY || e->y >= view.viewY + view.viewH) continue;
            game.selectedIds.push_back(e->id);
            if (game.selectedId < 0) game.selectedId = e->id;
        }
    }
    if (!game.selectedIds.empty())
        emitStatus(events, issuer, std::to_string(game.selectedIds.size()) + " " + STATS[t].name + "s selected");
}

Entity* selectNextIdleWorker(Game& game, const WorldIndex& world, PlayerId issuer, EntityId afterId) {
    if (!validIssuer(issuer)) return nullptr;
    Entity* first = nullptr;
    Entity* pick = nullptr;
    bool past = afterId < 0;
    for (EntityId id : world.unitsByOwner[issuer]) {
        Entity* entity = entityById(game, world, id);
        if (!entity || entity->type != E_PEASANT || entity->state != S_IDLE) continue;
        if (!first) first = entity;
        if (!past) {
            if (entity->id == afterId) past = true;
            continue;
        }
        pick = entity;
        break;
    }
    if (!pick) pick = first;
    if (!pick) return nullptr;
    game.selectedId = pick->id;
    game.selectedIds.clear();
    return pick;
}

Entity* selectNextUnit(Game& game, const WorldIndex& world, PlayerId issuer, EntityId afterId) {
    if (!validIssuer(issuer)) return nullptr;
    Entity* first = nullptr;
    Entity* pick = nullptr;
    bool past = afterId < 0;
    for (EntityId id : world.unitsByOwner[issuer]) {
        Entity* entity = entityById(game, world, id);
        if (!entity || entity->state == S_GARRISONED || !isUnit(entity->type)) continue;
        if (!first) first = entity;
        if (!past) {
            if (entity->id == afterId) past = true;
            continue;
        }
        pick = entity;
        break;
    }
    if (!pick) pick = first;
    if (!pick) return nullptr;
    game.selectedId = pick->id;
    game.selectedIds.clear();
    return pick;
}

Entity* selectHomeBase(Game& game, const WorldIndex& world, PlayerId issuer) {
    if (!validIssuer(issuer)) return nullptr;
    for (EntityId id : world.buildingsByOwner[issuer]) {
        Entity* entity = entityById(game, world, id);
        if (!entity || (entity->type != E_TOWNHALL && entity->type != E_CASTLE)) continue;
        game.selectedId = entity->id;
        game.selectedIds.clear();
        return entity;
    }
    return nullptr;
}

int selectAllMilitary(Game& game, const WorldIndex& world, PlayerId issuer) {
    game.selectedIds.clear();
    game.selectedId = -1;
    if (!validIssuer(issuer)) return 0;
    for (EntityId id : world.unitsByOwner[issuer]) {
        Entity* entity = entityById(game, world, id);
        if (!entity || entity->state == S_GARRISONED || !isMilitary(entity->type)) continue;
        game.selectedIds.push_back(entity->id);
        if (game.selectedId < 0) game.selectedId = entity->id;
    }
    return (int)game.selectedIds.size();
}

bool selectionContainsMilitary(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    if (!validIssuer(issuer)) return false;
    if (!selection.ids.empty()) {
        for (EntityId id : selection.ids) {
            Entity* entity = entityById(game, world, id);
            if (entity && entity->owner == issuer && isMilitary(entity->type)) return true;
        }
        return false;
    }
    Entity* entity = entityById(game, world, selection.primaryId);
    return entity && entity->owner == issuer && isMilitary(entity->type);
}

bool beginControlGroupAssignment(Game& game, const WorldIndex& world, PlayerId issuer) {
    if (!validIssuer(issuer)) return false;
    Selection selection = currentSelection(game);
    std::vector<int> filtered;
    for (EntityId id : selection.ids) {
        Entity* entity = entityById(game, world, id);
        if (entity && entity->alive && entity->owner == issuer) filtered.push_back(id);
    }
    if (filtered.empty()) return false;
    game.selectedIds = filtered;
    game.selectedId = filtered.front();
    game.groupAssignPending = true;
    return true;
}

void clearSelection(Game& game) {
    game.selectedId = -1;
    game.selectedIds.clear();
    game.groupAssignPending = false;
}

bool assignControlGroup(Game& game, const WorldIndex& world, PlayerId issuer, int slot, const Selection& selection) {
    if (!validIssuer(issuer) || !validControlSlot(slot) || selection.ids.empty()) return false;
    std::vector<int> filtered;
    for (int id : selection.ids) {
        Entity* entity = entityById(game, world, id);
        if (entity && entity->alive && entity->owner == issuer)
            filtered.push_back(id);
    }
    if (filtered.empty()) return false;
    controlGroupSlot(game, issuer, slot) = filtered;
    game.groupAssignPending = false;
    return true;
}

bool recallControlGroup(Game& game, const WorldIndex& world, PlayerId issuer, int slot) {
    if (!validIssuer(issuer) || !validControlSlot(slot)) return false;
    std::vector<int>& group = controlGroupSlot(game, issuer, slot);
    group.erase(std::remove_if(group.begin(), group.end(), [&](int id) {
        Entity* entity = entityById(game, world, id);
        return !entity || !entity->alive || entity->owner != issuer;
    }), group.end());
    if (group.empty()) return false;
    game.selectedIds = group;
    game.selectedId = -1;
    for (int id : game.selectedIds) {
        Entity* entity = entityById(game, world, id);
        if (entity && entity->alive) { game.selectedId = entity->id; break; }
    }
    return true;
}

int controlGroupSize(Game& game, PlayerId issuer, int slot) {
    if (!validIssuer(issuer) || !validControlSlot(slot)) return 0;
    return (int)controlGroupSlot(game, issuer, slot).size();
}
