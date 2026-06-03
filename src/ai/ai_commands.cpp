#include "ai/ai.h"
#include "commands/command.h"
#include "core/world_index.h"

#include <iostream>
#include <utility>

static Selection aiSelection(Entity& entity) {
    Selection selection;
    selection.primaryId = entity.id;
    selection.ids = { entity.id };
    return selection;
}

static void aiQueue(AIContext& context, Command command) {
    context.plannedCommands.push_back(std::move(command));
}

static void aiDispatchImmediate(Command command) {
    const AITuning& tuning = defaultAITuning();
    AIWorldView view = buildAIWorldView(command.issuer, tuning);
    WorldIndex world = buildWorldIndex(g);
    GameContext gameContext{ g, world, gameEvents() };
    AIContext context{ command.issuer, gameContext, view, tuning, {}, {} };
    context.plannedCommands.push_back(std::move(command));
    executeAICommands(context);
}

void aiIssueBuild(AIContext& context, Entity& builder, EntityType buildingType, int x, int y) {
    Command command;
    command.issuer = context.owner;
    command.payload = BuildCommand{ aiSelection(builder), buildingType, { x, y } };
    aiQueue(context, std::move(command));
}

void aiIssueTrain(AIContext& context, Entity& producer, EntityType unitType) {
    Command command;
    command.issuer = context.owner;
    command.payload = TrainCommand{ aiSelection(producer), unitType };
    aiQueue(context, std::move(command));
}

void aiIssueResearch(AIContext& context, Entity& producer, ResearchId researchId) {
    Command command;
    command.issuer = context.owner;
    command.payload = ResearchCommand{ aiSelection(producer), researchId };
    aiQueue(context, std::move(command));
}

void aiIssueGather(AIContext& context, Entity& unit, int x, int y) {
    Command command;
    command.issuer = context.owner;
    command.payload = GatherCommand{ aiSelection(unit), { x, y } };
    aiQueue(context, std::move(command));
}

void aiIssueMove(AIContext& context, Entity& unit, int x, int y) {
    Command command;
    command.issuer = context.owner;
    command.payload = MoveCommand{ aiSelection(unit), { x, y } };
    aiQueue(context, std::move(command));
}

void aiIssueAttack(AIContext& context, Entity& unit, int targetId) {
    Command command;
    command.issuer = context.owner;
    command.payload = AttackCommand{ aiSelection(unit), targetId };
    aiQueue(context, std::move(command));
}

void aiIssueAttackMove(AIContext& context, Entity& unit, int x, int y) {
    Command command;
    command.issuer = context.owner;
    command.payload = AttackMoveCommand{ aiSelection(unit), { x, y } };
    aiQueue(context, std::move(command));
}

void aiIssueGarrison(AIContext& context, Entity& unit, int buildingId) {
    Command command;
    command.issuer = context.owner;
    command.payload = GarrisonCommand{ aiSelection(unit), buildingId };
    aiQueue(context, std::move(command));
}

void aiIssueEjectGarrison(AIContext& context, Entity& building) {
    Command command;
    command.issuer = context.owner;
    command.payload = EjectGarrisonCommand{ aiSelection(building) };
    aiQueue(context, std::move(command));
}

void aiIssueContext(AIContext& context, Entity& unit, int x, int y) {
    Command command;
    command.issuer = context.owner;
    command.payload = ContextCommand{ aiSelection(unit), { x, y } };
    aiQueue(context, std::move(command));
}

void aiIssueToggleTrebuchetPacked(AIContext& context, Entity& trebuchet) {
    Command command;
    command.issuer = context.owner;
    command.payload = ToggleTrebuchetPackedCommand{ aiSelection(trebuchet) };
    aiQueue(context, std::move(command));
}

static void countRejected(AIContext& context, CommandType type) {
    switch (type) {
        case CommandType::Build:
        case CommandType::BuildLine:
            context.rejectedBuildCommands++;
            break;
        case CommandType::Train:
            context.rejectedTrainCommands++;
            break;
        case CommandType::Research:
            context.rejectedResearchCommands++;
            break;
        case CommandType::Gather:
            context.rejectedGatherCommands++;
            break;
        case CommandType::Attack:
        case CommandType::AttackMove:
            context.rejectedAttackCommands++;
            break;
        default:
            break;
    }
}

void aiIssueBuild(Entity& builder, EntityType buildingType, int x, int y) {
    Command command;
    command.issuer = builder.owner;
    command.payload = BuildCommand{ aiSelection(builder), buildingType, { x, y } };
    aiDispatchImmediate(std::move(command));
}

void aiIssueTrain(Entity& producer, EntityType unitType) {
    Command command;
    command.issuer = producer.owner;
    command.payload = TrainCommand{ aiSelection(producer), unitType };
    aiDispatchImmediate(std::move(command));
}

void aiIssueResearch(Entity& producer, ResearchId researchId) {
    Command command;
    command.issuer = producer.owner;
    command.payload = ResearchCommand{ aiSelection(producer), researchId };
    aiDispatchImmediate(std::move(command));
}

void aiIssueGather(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = GatherCommand{ aiSelection(unit), { x, y } };
    aiDispatchImmediate(std::move(command));
}

void aiIssueMove(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = MoveCommand{ aiSelection(unit), { x, y } };
    aiDispatchImmediate(std::move(command));
}

void aiIssueAttack(Entity& unit, int targetId) {
    Command command;
    command.issuer = unit.owner;
    command.payload = AttackCommand{ aiSelection(unit), targetId };
    aiDispatchImmediate(std::move(command));
}

void aiIssueAttackMove(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = AttackMoveCommand{ aiSelection(unit), { x, y } };
    aiDispatchImmediate(std::move(command));
}

void aiIssueGarrison(Entity& unit, int buildingId) {
    Command command;
    command.issuer = unit.owner;
    command.payload = GarrisonCommand{ aiSelection(unit), buildingId };
    aiDispatchImmediate(std::move(command));
}

void aiIssueEjectGarrison(Entity& building) {
    Command command;
    command.issuer = building.owner;
    command.payload = EjectGarrisonCommand{ aiSelection(building) };
    aiDispatchImmediate(std::move(command));
}

void aiIssueContext(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = ContextCommand{ aiSelection(unit), { x, y } };
    aiDispatchImmediate(std::move(command));
}

void aiIssueToggleTrebuchetPacked(Entity& trebuchet) {
    Command command;
    command.issuer = trebuchet.owner;
    command.payload = ToggleTrebuchetPackedCommand{ aiSelection(trebuchet) };
    aiDispatchImmediate(std::move(command));
}

void executeAICommands(AIContext& context) {
    bool worldFresh = true;
    for (const Command& command : context.plannedCommands) {
        if (!worldFresh) {
            context.ctx.world = buildWorldIndex(context.ctx.game);
            worldFresh = true;
        }
        CommandResult result = dispatchCommand(context.ctx, command);
        if (result.status != CommandStatus::Rejected && result.status != CommandStatus::Error) {
            worldFresh = false;
            continue;
        }
        context.rejectedCommands.push_back({ command, result });
        countRejected(context, command.type());
        if (context.ctx.game.diagnostics) {
            std::cerr << "realm: ai command rejected owner=" << context.owner
                      << " type=" << (int)command.type()
                      << " tick=" << context.ctx.game.tick
                      << " reason=\"" << result.reason << "\"\n";
        }
    }
    context.plannedCommands.clear();
}
