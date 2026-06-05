#pragma once

#include <utility>

struct Entity;

std::pair<int, int> entityFacingDeltaTowardTile(const Entity& entity, int targetX, int targetY);
std::pair<int, int> entityVisualFacingDelta(const Entity& entity);
void faceEntityTowardTile(Entity& entity, int targetX, int targetY);
