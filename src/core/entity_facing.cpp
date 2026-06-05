#include "core/entity_facing.h"

#include "core/entity_motion.h"
#include "realm.h"

namespace {

int signum(int value) {
    return (value > 0) - (value < 0);
}

} // namespace

std::pair<int, int> entityFacingDeltaTowardTile(const Entity& entity, int targetX, int targetY) {
    return {signum(targetX - entity.x), signum(targetY - entity.y)};
}

std::pair<int, int> entityVisualFacingDelta(const Entity& entity) {
    if (entityHasActivePathMotion(entity) && entityHasQueuedPathStep(entity)) {
        return {signum(entity.path[entity.pathIdx].first - entity.x),
                signum(entity.path[entity.pathIdx].second - entity.y)};
    }
    if (inBounds(entity.targetX, entity.targetY)
        && (entity.targetX != entity.x || entity.targetY != entity.y)) {
        return entityFacingDeltaTowardTile(entity, entity.targetX, entity.targetY);
    }
    return {signum(entity.facingDx), signum(entity.facingDy)};
}

void faceEntityTowardTile(Entity& entity, int targetX, int targetY) {
    auto [dx, dy] = entityFacingDeltaTowardTile(entity, targetX, targetY);
    if (dx == 0 && dy == 0) return;
    entity.facingDx = dx;
    entity.facingDy = dy;
}
