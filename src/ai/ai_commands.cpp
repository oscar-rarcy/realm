#include "ai/ai.h"
#include "commands/command.h"
#include "core/world_index.h"

#include <iostream>
#include <utility>

static AIContext* activeAIContext = nullptr;

static Selection aiSelection(Entity& entity) {
    Selection selection;
    selection.primaryId = entity.id;
    selection.ids = { entity.id };
    return selection;
}

void setActiveAIContext(AIContext* context) {
    activeAIContext = context;
}

static void aiDispatch(Command command) {
    if (activeAIContext && command.issuer == activeAIContext->owner) {
        activeAIContext->plannedCommands.push_back(std::move(command));
        return;
    }
    const AITuning& tuning = defaultAITuning();
    AIWorldView view = buildAIWorldView(command.issuer, tuning);
    WorldIndex world = buildWorldIndex(g);
    GameContext gameContext{ g, world, gameEvents() };
    AIContext context{ command.issuer, gameContext, view, tuning, {}, {} };
    context.plannedCommands.push_back(std::move(command));
    executeAICommands(context);
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
    aiDispatch(command);
}

void aiIssueTrain(Entity& producer, EntityType unitType) {
    Command command;
    command.issuer = producer.owner;
    command.payload = TrainCommand{ aiSelection(producer), unitType };
    aiDispatch(command);
}

void aiIssueResearch(Entity& producer, ResearchId researchId) {
    Command command;
    command.issuer = producer.owner;
    command.payload = ResearchCommand{ aiSelection(producer), researchId };
    aiDispatch(command);
}

void aiIssueGather(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = GatherCommand{ aiSelection(unit), { x, y } };
    aiDispatch(command);
}

void aiIssueMove(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = MoveCommand{ aiSelection(unit), { x, y } };
    aiDispatch(command);
}

void aiIssueAttack(Entity& unit, int targetId) {
    Command command;
    command.issuer = unit.owner;
    command.payload = AttackCommand{ aiSelection(unit), targetId };
    aiDispatch(command);
}

void aiIssueAttackMove(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = AttackMoveCommand{ aiSelection(unit), { x, y } };
    aiDispatch(command);
}

void aiIssueGarrison(Entity& unit, int buildingId) {
    Command command;
    command.issuer = unit.owner;
    command.payload = GarrisonCommand{ aiSelection(unit), buildingId };
    aiDispatch(command);
}

void aiIssueEjectGarrison(Entity& building) {
    Command command;
    command.issuer = building.owner;
    command.payload = EjectGarrisonCommand{ aiSelection(building) };
    aiDispatch(command);
}

void aiIssueContext(Entity& unit, int x, int y) {
    Command command;
    command.issuer = unit.owner;
    command.payload = ContextCommand{ aiSelection(unit), { x, y } };
    aiDispatch(command);
}

void aiIssueToggleTrebuchetPacked(Entity& trebuchet) {
    Command command;
    command.issuer = trebuchet.owner;
    command.payload = ToggleTrebuchetPackedCommand{ aiSelection(trebuchet) };
    aiDispatch(command);
}

void executeAICommands(AIContext& context) {
    for (const Command& command : context.plannedCommands) {
        context.ctx.world = buildWorldIndex(context.ctx.game);
        CommandResult result = dispatchCommand(context.ctx, command);
        if (result.status != CommandStatus::Rejected && result.status != CommandStatus::Error) continue;
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
