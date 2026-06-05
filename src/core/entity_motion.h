#pragma once

struct Entity;

bool entityHasQueuedPathStep(const Entity& entity);
bool entityIsCompletingTileStep(const Entity& entity);
bool entityHasActivePathMotion(const Entity& entity);
bool consumeEntityMoveCooldown(Entity& entity);
