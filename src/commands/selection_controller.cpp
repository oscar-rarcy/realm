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

bool selectionContains(const Game& game, EntityId id) {
    if (id < 0) return false;
    if (game.local.selectedId == id) return true;
    return std::find(game.local.selectedIds.begin(), game.local.selectedIds.end(), id) != game.local.selectedIds.end();
}

void addToSelection(Game& game, EntityId id) {
    if (id < 0 || selectionContains(game, id)) return;
    if (game.local.selectedIds.empty() && game.local.selectedId >= 0)
        game.local.selectedIds.push_back(game.local.selectedId);
    game.local.selectedIds.push_back(id);
    if (game.local.selectedId < 0) game.local.selectedId = id;
}

void removeFromSelection(Game& game, EntityId id) {
    if (id < 0) return;
    if (!game.local.selectedIds.empty()) {
        game.local.selectedIds.erase(std::remove(game.local.selectedIds.begin(), game.local.selectedIds.end(), id),
                                     game.local.selectedIds.end());
        if (game.local.selectedIds.size() == 1) {
            game.local.selectedId = game.local.selectedIds.front();
            game.local.selectedIds.clear();
        } else if (game.local.selectedIds.empty()) {
            game.local.selectedId = -1;
        } else if (game.local.selectedId == id) {
            game.local.selectedId = game.local.selectedIds.front();
        }
    } else if (game.local.selectedId == id) {
        game.local.selectedId = -1;
    }
}

std::vector<int>& controlGroupSlot(Game& game, PlayerId issuer, int slot) {
    return game.controlGroupsByOwner[issuer][slot];
}

} // namespace

Selection currentSelection(const Game& game) {
    Selection selection;
    selection.primaryId = game.local.selectedId;
    selection.ids = game.local.selectedIds;
    if (selection.ids.empty() && selection.primaryId >= 0)
        selection.ids.push_back(selection.primaryId);
    return selection;
}

void selectAtTile(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x, int y, bool toggle) {
    if (!inBounds(x, y)) return;
    Entity* ent = entityAtOwner(game, world, x, y, issuer);
    if (ent) {
        if (toggle && isUnit(ent->type)) {
            if (selectionContains(game, ent->id)) {
                removeFromSelection(game, ent->id);
                emitStatus(events, issuer, "Removed from selection");
            } else {
                addToSelection(game, ent->id);
                emitStatus(events, issuer, "Added to selection");
            }
            return;
        }
        game.local.selectedId = ent->id;
        game.local.selectedIds.clear();
        emitStatus(events, issuer, std::string("Selected: ") + STATS[ent->type].name);
        return;
    }
    Entity* any = entityAt(game, world, x, y);
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && game.map[y][x].visible[issuer];
    if (any && any->alive && visible) {
        game.local.selectedId = any->id;
        game.local.selectedIds.clear();
        emitStatus(events, issuer, std::string(any->owner==OWNER_NATURE ? "Animal: " : "Enemy ") + STATS[any->type].name);
    } else {
        game.local.selectedId = -1;
        game.local.selectedIds.clear();
    }
}

void boxSelect(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x0, int y0, int x1, int y1, bool additive) {
    x0 = std::max(0, std::min(x0, MAP_W-1));
    x1 = std::max(0, std::min(x1, MAP_W-1));
    y0 = std::max(0, std::min(y0, MAP_H-1));
    y1 = std::max(0, std::min(y1, MAP_H-1));
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    std::vector<int> hits;
    bool sawMilitary = false;
    bool sawPeasant = false;
    if (validIssuer(issuer)) {
        for (EntityId id : world.unitsByOwner[issuer]) {
            Entity* e = entityById(game, world, id);
            if (!e || e->state == S_GARRISONED) continue;
            if (e->x >= x0 && e->x <= x1 && e->y >= y0 && e->y <= y1) {
                if (e->type == E_PEASANT) sawPeasant = true;
                else sawMilitary = true;
                hits.push_back(e->id);
            }
        }
    }
    bool filterPeasants = !additive && sawMilitary && sawPeasant;
    if (!additive) {
        game.local.selectedIds.clear();
        game.local.selectedId = -1;
    }
    for (EntityId id : hits) {
        Entity* e = entityById(game, world, id);
        if (!e) continue;
        if (filterPeasants && e->type == E_PEASANT) continue;
        addToSelection(game, id);
    }
    int count = game.local.selectedIds.empty() && game.local.selectedId >= 0 ? 1 : (int)game.local.selectedIds.size();
    if (count > 0) emitStatus(events, issuer, std::to_string(count) + " units selected");
}

void selectAllOfTypeInView(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x, int y) {
    if (!inBounds(x, y)) return;
    Entity* ent = entityAtOwner(game, world, x, y, issuer);
    if (!ent || !isUnit(ent->type)) return;
    EntityType t = ent->type;
    game.local.selectedIds.clear();
    game.local.selectedId = -1;
    if (validIssuer(issuer)) {
        for (EntityId id : world.unitsByOwner[issuer]) {
            Entity* e = entityById(game, world, id);
            if (!e || e->type != t || e->state == S_GARRISONED) continue;
            if (e->x < view.viewX || e->x >= view.viewX + view.viewW) continue;
            if (e->y < view.viewY || e->y >= view.viewY + view.viewH) continue;
            game.local.selectedIds.push_back(e->id);
            if (game.local.selectedId < 0) game.local.selectedId = e->id;
        }
    }
    if (!game.local.selectedIds.empty())
        emitStatus(events, issuer, std::to_string(game.local.selectedIds.size()) + " " + STATS[t].name + "s selected");
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
    game.local.selectedId = pick->id;
    game.local.selectedIds.clear();
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
    game.local.selectedId = pick->id;
    game.local.selectedIds.clear();
    return pick;
}

Entity* selectHomeBase(Game& game, const WorldIndex& world, PlayerId issuer) {
    if (!validIssuer(issuer)) return nullptr;
    for (EntityId id : world.buildingsByOwner[issuer]) {
        Entity* entity = entityById(game, world, id);
        if (!entity || (entity->type != E_TOWNHALL && entity->type != E_CASTLE)) continue;
        game.local.selectedId = entity->id;
        game.local.selectedIds.clear();
        return entity;
    }
    return nullptr;
}

int selectAllMilitary(Game& game, const WorldIndex& world, PlayerId issuer) {
    game.local.selectedIds.clear();
    game.local.selectedId = -1;
    if (!validIssuer(issuer)) return 0;
    for (EntityId id : world.unitsByOwner[issuer]) {
        Entity* entity = entityById(game, world, id);
        if (!entity || entity->state == S_GARRISONED || !isMilitary(entity->type)) continue;
        game.local.selectedIds.push_back(entity->id);
        if (game.local.selectedId < 0) game.local.selectedId = entity->id;
    }
    return (int)game.local.selectedIds.size();
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
    game.local.selectedIds = filtered;
    game.local.selectedId = filtered.front();
    game.local.groupAssignPending = true;
    return true;
}

void clearSelection(Game& game) {
    game.local.selectedId = -1;
    game.local.selectedIds.clear();
    game.local.groupAssignPending = false;
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
    game.local.groupAssignPending = false;
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
    game.local.selectedIds = group;
    game.local.selectedId = -1;
    for (int id : game.local.selectedIds) {
        Entity* entity = entityById(game, world, id);
        if (entity && entity->alive) { game.local.selectedId = entity->id; break; }
    }
    return true;
}

int controlGroupSize(Game& game, PlayerId issuer, int slot) {
    if (!validIssuer(issuer) || !validControlSlot(slot)) return 0;
    return (int)controlGroupSlot(game, issuer, slot).size();
}
