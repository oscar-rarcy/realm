#pragma once

#include "core/game_types.h"
#include "core/service_result.h"

// Canonical training rules and execution path shared by player commands and AI.

struct WorldIndex;
struct Entity;
struct Game;
class EventSink;

struct ProductionRule {
    EntityType producer;
    const EntityType* allowedUnits;
    int allowedCount;
    const char* menuHotkeys;
    int queueLimit;
};

struct CanTrainResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

const ProductionRule* productionRules(int& count);
const ProductionRule* productionRule(EntityType producer);
bool canProducerTrain(EntityType producer, EntityType unitType);

CanTrainResult canTrain(const Game& game, int player, const Entity& producer, EntityType unitType);

// Validates and, on success, spends resources and starts/queues training.
ServiceResult startTrainingService(Game& game, const WorldIndex& world, EventSink& events, int player, int producerId, EntityType unitType);
