#include "core/entity_motion.h"

#include "realm.h"

bool entityHasQueuedPathStep(const Entity& entity) {
    return entity.pathIdx < (int)entity.path.size();
}

bool entityIsCompletingTileStep(const Entity& entity) {
    return entity.moveCd > 0;
}

bool entityHasActivePathMotion(const Entity& entity) {
    return entity.state == S_MOVING || entity.state == S_RETURNING
        || (entity.state != S_IDLE && entityHasQueuedPathStep(entity))
        || entityIsCompletingTileStep(entity);
}

bool consumeEntityMoveCooldown(Entity& entity) {
    if (!entityIsCompletingTileStep(entity)) return false;
    entity.moveCd--;
    return true;
}
