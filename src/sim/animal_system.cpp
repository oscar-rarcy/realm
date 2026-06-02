#include "realm.h"
#include "core/world_index.h"

void tickAnimals() {
    tickAnimals(g);
}

void tickAnimals(Game& game) {
    game.animalTimer++;
    for (auto& e : game.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;

        // Deer flee in herds: one spooked deer panics nearby deer in the same direction.
        if (e.type == E_DEER) {
            if (e.state != S_MOVING || e.path.empty()) {
                for (auto& o : game.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 5) {
                        int fx = std::max(1, std::min(e.x + (e.x-o.x)*4, MAP_W-2));
                        int fy = std::max(1, std::min(e.y + (e.y-o.y)*4, MAP_H-2));
                        if (isPassable(game, fx, fy)) {
                            orderMove(game, e, fx, fy);
                            // Spook nearby herd members to bolt the same way.
                            for (auto& nb : game.entities) {
                                if (!nb.alive || nb.type != E_DEER || nb.id == e.id) continue;
                                if (nb.state == S_MOVING && !nb.path.empty()) continue;
                                if (dist(e.x, e.y, nb.x, nb.y) > 6) continue;
                                int nbfx = std::max(1, std::min(nb.x+(nb.x-o.x)*4, MAP_W-2));
                                int nbfy = std::max(1, std::min(nb.y+(nb.y-o.y)*4, MAP_H-2));
                                if (isPassable(game, nbfx, nbfy)) orderMove(game, nb, nbfx, nbfy);
                            }
                        }
                        break;
                    }
                }
            }
        }

        // Sheep still flee individually.
        if (e.type == E_SHEEP) {
            if (e.state != S_MOVING || e.path.empty()) {
                for (auto& o : game.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 4) {
                        int fx = std::max(1, std::min(e.x + (e.x-o.x)*4, MAP_W-2));
                        int fy = std::max(1, std::min(e.y + (e.y-o.y)*4, MAP_H-2));
                        if (isPassable(game, fx, fy)) orderMove(game, e, fx, fy);
                        break;
                    }
                }
            }
        }

        // Boars forage peacefully until struck, then retaliate briefly.
        if (e.type == E_BOAR) {
            int chargeRange = (e.alertTicks > 0) ? 4 : 0;
            if (e.state == S_IDLE || (e.state == S_MOVING && e.path.empty())) {
                for (auto& o : game.entities) {
                    if (!o.alive || o.owner == OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= chargeRange) {
                        WorldIndex world = buildWorldIndex(game);
                        orderAttack(game, world, e, o.id);
                        break;
                    }
                }
            }
        }

        // Wolves: give buildings a modest berth in summer/spring; bolder in autumn/winter.
        if (e.type == E_WOLF) {
            bool winter = (getSeason(game) == WINTER);
            int huntRange = winter ? 8 : 5;
            int settleAvoid = winter ? 0 : 8; // smaller avoid radius — wolves press closer
            bool nearSettlement = false;
            int fleeX = -1, fleeY = -1;
            if (settleAvoid > 0) {
                for (auto& o : game.entities) {
                    if (!o.alive || o.owner == OWNER_NATURE || !isBuilding(o.type)) continue;
                    int d = dist(e.x, e.y, o.x, o.y);
                    if (d <= settleAvoid) {
                        nearSettlement = true;
                        fleeX = std::max(1, std::min(e.x + (e.x - o.x)*3, MAP_W-2));
                        fleeY = std::max(1, std::min(e.y + (e.y - o.y)*3, MAP_H-2));
                        break;
                    }
                }
            }
            if (nearSettlement) {
                if (e.state == S_ATTACKING) { e.state = S_IDLE; e.path.clear(); }
                if (e.state == S_IDLE && fleeX >= 0 && isPassable(game, fleeX, fleeY))
                    orderMove(game, e, fleeX, fleeY);
            } else if (e.state==S_IDLE || (e.state==S_MOVING && e.path.empty())) {
                for (auto& o : game.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= huntRange) {
                        WorldIndex world = buildWorldIndex(game);
                        orderAttack(game, world, e, o.id);
                        break;
                    }
                }
            }
        }

        // Random wander when idle
        if (e.state == S_IDLE && game.animalTimer % (35 + (e.id%25)) == 0) {
            int wx = e.x + (realmRand(game)%9)-4, wy = e.y + (realmRand(game)%9)-4;
            wx = std::max(1, std::min(wx, MAP_W-2));
            wy = std::max(1, std::min(wy, MAP_H-2));
            if (isPassable(game, wx, wy)) orderMove(game, e, wx, wy);
        }
    }
}
