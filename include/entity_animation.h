#pragma once

#include "realm.h"

#include <iosfwd>

enum class ActionTargetRelation {
    SelfTile,
    AdjacentTarget,
    RangedTarget,
    ProjectileToTarget,
    HiddenOrInside
};

struct AnimationFrameSpec {
    const char* id;
    const char* description;
    int durationMs;
};

struct EntityActionAnimationSpec {
    EntityType entityType;
    const char* action;
    const char* description;
    const char* family;
    ActionTargetRelation targetRelation;
    int rangeTiles;
    bool loop;
    bool holdLast;
    int transitionAfterMs;
    const char* fitProfile;
    const char* tool;
    const char* carriedObject;
    const AnimationFrameSpec* frames;
    int frameCount;
};

const char* actionTargetRelationId(ActionTargetRelation relation);
EntityType entityTypeForAnimationSlug(const char* slug);
const EntityActionAnimationSpec* findEntityActionAnimationSpec(EntityType type, const char* action);
const EntityActionAnimationSpec* entityActionAnimationSpecFor(const Entity& e);
const char* entityAnimationActionId(const Entity& e);
const char* entityAnimationDirectionBucket(const Entity& e);
bool entityAnimationMirrorHorizontal(const Entity& e);
int entityActionAnimationSpecCount(EntityType type);
const EntityActionAnimationSpec* entityActionAnimationSpecAt(EntityType type, int index);
bool writeEntityAnimationSpecJson(std::ostream& out, EntityType type);
