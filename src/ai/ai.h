#pragma once

#include "realm.h"
#include "commands/command.h"
#include "core/research_defs.h"

#include <optional>

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

Entity* aiWorker(int owner);
Entity* aiIdlePeasant(int owner);
void aiGather(int owner);
void aiBuildSpotNear(int owner, EntityType type, int cx, int cy, int& ox, int& oy);
void aiBuildSpotWide(int owner, EntityType type, int& ox, int& oy);
AIIntel aiScout(int owner);
void aiTickTrebuchets(int owner);
void aiTickTransports(int owner);
int aiPickTarget(int owner, Entity* attacker);
int aiPickSiegeTarget(int owner, Entity* attacker);
AIWorldView buildAIWorldView(int owner);
AIWorldView buildAIWorldView(int owner, const AITuning& tuning);
void runAIEconomy(int owner, Player& player, const AIWorldView& view);
void runAIProduction(int owner, Player& player, const AIWorldView& view);
void runAIDefenseInfrastructure(int owner, Player& player, const AIWorldView& view);
void runAIFoodEconomy(int owner, Player& player, const AIWorldView& view);
void runAINaval(int owner, Player& player, const AIWorldView& view);
void runAIExpansion(int owner, Player& player, const AIWorldView& view);
void runAIAttackAndDefense(int owner, Player& player, const AIWorldView& view);
void tickAIForOwner(int owner);

void aiIssueBuild(Entity& builder, EntityType buildingType, int x, int y);
void aiIssueTrain(Entity& producer, EntityType unitType);
void aiIssueResearch(Entity& producer, ResearchId researchId);
void aiIssueGather(Entity& unit, int x, int y);
void aiIssueMove(Entity& unit, int x, int y);
void aiIssueAttack(Entity& unit, int targetId);
void aiIssueAttackMove(Entity& unit, int x, int y);
void aiIssueGarrison(Entity& unit, int buildingId);
void aiIssueEjectGarrison(Entity& building);
void aiIssueContext(Entity& unit, int x, int y);
void aiIssueToggleTrebuchetPacked(Entity& trebuchet);
void setActiveAIContext(AIContext* context);
void executeAICommands(AIContext& context);
