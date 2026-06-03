#include "production_service.h"
#include "realm.h"
#include "core/entity_query.h"
#include "core/game_events.h"
#include "core/world_index.h"

static const EntityType TOWN_HALL_UNITS[] = { E_PEASANT };
static const EntityType BARRACKS_UNITS[] = { E_MILITIA, E_ARCHER, E_SPEARMAN, E_CATAPULT, E_RAM };
static const EntityType STABLE_UNITS[] = { E_KNIGHT };
static const EntityType CASTLE_UNITS[] = { E_PEASANT, E_TREBUCHET };
static const EntityType DOCK_UNITS[] = { E_FISHING_BOAT, E_WARSHIP, E_TRANSPORT };

static const ProductionRule RULES[] = {
    { E_TOWNHALL, TOWN_HALL_UNITS, (int)(sizeof(TOWN_HALL_UNITS) / sizeof(TOWN_HALL_UNITS[0])), "p", 5 },
    { E_BARRACKS, BARRACKS_UNITS, (int)(sizeof(BARRACKS_UNITS) / sizeof(BARRACKS_UNITS[0])), "mascr", 5 },
    { E_STABLE, STABLE_UNITS, (int)(sizeof(STABLE_UNITS) / sizeof(STABLE_UNITS[0])), "k", 5 },
    { E_CASTLE, CASTLE_UNITS, (int)(sizeof(CASTLE_UNITS) / sizeof(CASTLE_UNITS[0])), "pt", 5 },
    { E_DOCK, DOCK_UNITS, (int)(sizeof(DOCK_UNITS) / sizeof(DOCK_UNITS[0])), "bwt", 5 },
};

static const int RULE_COUNT = (int)(sizeof(RULES) / sizeof(RULES[0]));

static void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

const ProductionRule* productionRules(int& count) {
    count = RULE_COUNT;
    return RULES;
}

const ProductionRule* productionRule(EntityType producer) {
    for (int i = 0; i < RULE_COUNT; i++)
        if (RULES[i].producer == producer) return &RULES[i];
    return nullptr;
}

bool canProducerTrain(EntityType producer, EntityType unitType) {
    const ProductionRule* rule = productionRule(producer);
    if (!rule) return false;
    for (int i = 0; i < rule->allowedCount; i++)
        if (rule->allowedUnits[i] == unitType) return true;
    return false;
}

CanTrainResult canTrain(const Game& game, int player, const Entity& producer, EntityType unitType) {
    if (!producer.alive) return { false, "Producer not available." };
    if (producer.owner != player) return { false, "Not your building." };
    if (!isBuilding(producer.type) || producer.underConstruction) return { false, "Building not complete." };

    const ProductionRule* rule = productionRule(producer.type);
    if (!rule || !canProducerTrain(producer.type, unitType)) return { false, "Cannot train that unit." };
    if (producer.producing != E_NONE && (int)producer.queue.size() >= rule->queueLimit)
        return { false, "Queue full!" };

    const Player& p = game.players[player];
    const EntityStats& stats = STATS[unitType];
    if (p.gold < stats.costGold || p.wood < stats.costWood) return { false, "Not enough resources!" };
    if (reservedSupply(game, player) + stats.supplyUsed > p.supplyMax) return { false, "Need more houses!" };
    if (p.food < stats.costFood) return { false, "Need more food!" };

    return { true, nullptr };
}

ServiceResult startTrainingService(Game& game, const WorldIndex& world, EventSink& events, int player, int producerId, EntityType unitType) {
    Entity* producer = findEntity(game, world, producerId);
    if (!producer) return { false, "Producer not found." };

    CanTrainResult result = canTrain(game, player, *producer, unitType);
    if (!result.ok) {
        return { false, result.reason };
    }

    Player& p = game.players[player];
    const EntityStats& stats = STATS[unitType];
    spendPlayerFood(game, player, stats.costFood);
    p.gold -= stats.costGold;
    p.wood -= stats.costWood;

    if (producer->producing == E_NONE) {
        producer->producing = unitType;
        producer->trainProgress = 0;
        producer->trainTime = stats.trainTime;
        producer->state = S_TRAINING;
        events.emit({ GameEventType::TrainingStarted, player, producer->id, { -1, -1 }, "", 0 });
    } else {
        producer->queue.push_back((int)unitType);
        emitStatus(events, player, "Queued.", GameEventType::TrainingQueued);
    }
    return { true, nullptr };
}
