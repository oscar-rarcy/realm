#pragma once

#include "commands/command.h"
#include "core/research_defs.h"

#include <optional>
#include <vector>

struct Entity;
struct Game;
struct WorldIndex;

struct AIIntel {
    int playerArmy;
    int playerCastles;
    int playerWalls;
    int playerPeasants;
    int playerCatapults;
    std::optional<EntityId> playerTownCenterId;
    std::optional<MapPos> playerTownCenterPos;
};

struct AITuning {
    int minPeasantCap = 12;
    int earlyPeasantLead = 4;
    int latePeasantCapTick = 9000;
    int latePeasantCap = 18;
    int finalPeasantCapTick = 15000;
    int finalPeasantCap = 14;
    int minMilitiaCap = 8;
    int militiaLead = 4;
    int minArcherCap = 6;
    int minKnightCap = 4;
    int towerCapPeace = 2;
    int towerCapThreatened = 4;
    int towerThreatArmy = 6;
    int attackGraceTicks = 1500;
    int midGameTick = 6000;
    int lateGameTick = 12000;
    int earlyAttackThreshold = 7;
    int midAttackThreshold = 5;
    int lateAttackThreshold = 4;
    int midWaveCooldown = 10;
    int lateWaveCooldown = 6;
    int expansionTick = 7000;
    int aiThinkIntervalTicks = 12;
    int normalFarmCount = 5;
    int autumnFarmCount = 8;
    int fishingBoatCap = 3;
    int warshipCap = 2;
    int transportCap = 1;
    int expansionBaseCap = 2;
    int expansionPeasantMin = 9;
    int forwardAggressionArmy = 10;
    int forwardAggressionPercent = 65;
    int forwardHomeExclusionRadius = 10;
    int forwardAnchorRadius = 18;
    int beachheadHomeExclusionRadius = 30;
    int beachheadPlayerRadius = 30;
    int beachheadBaseRadius = 12;
    int defenseThreatRadius = 22;
};

const AITuning& defaultAITuning();

struct AIWorldView {
    int peas;
    int mil;
    int arch;
    int kni;
    int spr;
    int cat;
    int treb;
    int hous;
    int bar;
    int stb;
    int peasCap;
    int milCap;
    int archCap;
    int kniCap;
    int towerCap;
    AIIntel intel;
};

struct AIRejectedCommand {
    Command command;
    CommandResult result;
};

struct AIContext {
    PlayerId owner;
    GameContext& ctx;
    const AIWorldView& view;
    const AITuning& tuning;
    std::vector<Command> plannedCommands;
    std::vector<AIRejectedCommand> rejectedCommands;
    int rejectedBuildCommands = 0;
    int rejectedTrainCommands = 0;
    int rejectedResearchCommands = 0;
    int rejectedGatherCommands = 0;
    int rejectedAttackCommands = 0;
};

Entity* aiWorker(AIContext& context);
Entity* aiIdlePeasant(AIContext& context);
void aiGather(AIContext& context);
int aiCount(AIContext& context, EntityType type);
int aiCountAll(AIContext& context, EntityType type);
Entity* aiBldg(AIContext& context, EntityType type);
void aiBuildSpotNear(AIContext& context, EntityType type, int cx, int cy, int& ox, int& oy);
void aiBuildSpot(AIContext& context, EntityType type, int& ox, int& oy);
void aiBuildSpotWide(AIContext& context, EntityType type, int& ox, int& oy);
AIIntel aiScout(Game& game, const WorldIndex& world, int owner);
void aiTickTrebuchets(AIContext& context);
void aiTickTransports(AIContext& context);
int aiPickTarget(AIContext& context, Entity* attacker);
int aiPickSiegeTarget(AIContext& context, Entity* attacker);
AIWorldView buildAIWorldView(Game& game, const WorldIndex& world, int owner, const AITuning& tuning);
void runAIEconomy(AIContext& context);
void runAIProduction(AIContext& context);
void runAIDefenseInfrastructure(AIContext& context);
void runAIFoodEconomy(AIContext& context);
void runAINaval(AIContext& context);
void runAIExpansion(AIContext& context);
void runAIAttackAndDefense(AIContext& context);
void tickAIForOwner(Game& game, int owner);

void aiIssueBuild(AIContext& context, Entity& builder, EntityType buildingType, int x, int y);
void aiIssueTrain(AIContext& context, Entity& producer, EntityType unitType);
void aiIssueResearch(AIContext& context, Entity& producer, ResearchId researchId);
void aiIssueGather(AIContext& context, Entity& unit, int x, int y);
void aiIssueMove(AIContext& context, Entity& unit, int x, int y);
void aiIssueAttack(AIContext& context, Entity& unit, int targetId);
void aiIssueAttackMove(AIContext& context, Entity& unit, int x, int y);
void aiIssueGarrison(AIContext& context, Entity& unit, int buildingId);
void aiIssueEjectGarrison(AIContext& context, Entity& building);
void aiIssueContext(AIContext& context, Entity& unit, int x, int y);
void aiIssueToggleTrebuchetPacked(AIContext& context, Entity& trebuchet);
void executeAICommands(AIContext& context);
