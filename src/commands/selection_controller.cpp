#include "command.h"
#include "view_state.h"

Selection currentSelection() {
    Selection selection;
    selection.primaryId = g.selectedId;
    selection.ids = g.selectedIds;
    if (selection.ids.empty() && selection.primaryId >= 0)
        selection.ids.push_back(selection.primaryId);
    return selection;
}

void selectAtTile(Game& game, int x, int y) {
    if (!inBounds(x, y)) return;
    Entity* ent = entityAtOwner(x, y, 0);
    if (ent) {
        game.selectedId = ent->id;
        game.selectedIds.clear();
        setStatus(std::string("Selected: ") + STATS[ent->type].name);
        return;
    }
    Entity* any = entityAt(x, y);
    if (any && any->alive && game.map[y][x].visible[0]) {
        game.selectedId = any->id;
        game.selectedIds.clear();
        setStatus(std::string(any->owner==OWNER_NATURE ? "Animal: " : "Enemy ") + STATS[any->type].name);
    } else {
        game.selectedId = -1;
        game.selectedIds.clear();
    }
}

void boxSelect(Game& game, int x0, int y0, int x1, int y1) {
    x0 = std::max(0, std::min(x0, MAP_W-1));
    x1 = std::max(0, std::min(x1, MAP_W-1));
    y0 = std::max(0, std::min(y0, MAP_H-1));
    y1 = std::max(0, std::min(y1, MAP_H-1));
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    game.selectedIds.clear();
    game.selectedId = -1;
    for (auto& e : game.entities) {
        if (!e.alive || e.owner != 0 || !isUnit(e.type)) continue;
        if (e.state == S_GARRISONED) continue;
        if (e.x >= x0 && e.x <= x1 && e.y >= y0 && e.y <= y1) {
            game.selectedIds.push_back(e.id);
            if (game.selectedId < 0) game.selectedId = e.id;
        }
    }
    if (!game.selectedIds.empty())
        setStatus(std::to_string(game.selectedIds.size()) + " units selected");
}

void selectAllOfTypeInView(Game& game, int x, int y) {
    if (!inBounds(x, y)) return;
    Entity* ent = entityAtOwner(x, y, 0);
    if (!ent || !isUnit(ent->type)) return;
    EntityType t = ent->type;
    game.selectedIds.clear();
    game.selectedId = -1;
    for (auto& e : game.entities) {
        if (!e.alive || e.owner != 0 || e.type != t) continue;
        if (e.state == S_GARRISONED) continue;
        if (e.x < view.viewX || e.x >= view.viewX + view.viewW) continue;
        if (e.y < view.viewY || e.y >= view.viewY + view.viewH) continue;
        game.selectedIds.push_back(e.id);
        if (game.selectedId < 0) game.selectedId = e.id;
    }
    if (!game.selectedIds.empty())
        setStatus(std::to_string(game.selectedIds.size()) + " " + STATS[t].name + "s selected");
}
