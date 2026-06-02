#pragma once

#include "realm.h"
#include "core/service_result.h"

// Canonical training rules and execution path shared by player commands and AI.

struct WorldIndex;

struct ProductionRule {
    EntityType producer;
    const EntityType* allowedUnits;
    int allowedCount;
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
// Emits a status message for the human player (owner 0). Returns true when the
// unit was started or queued.
bool startTraining(Game& game, int player, int producerId, EntityType unitType);
ServiceResult startTrainingService(Game& game, int player, int producerId, EntityType unitType);
ServiceResult startTrainingService(Game& game, const WorldIndex& world, int player, int producerId, EntityType unitType);
