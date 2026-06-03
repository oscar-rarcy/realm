#include "command.h"
#include "core/build_service.h"
#include "core/entity_query.h"
#include "core/game_state_types.h"
#include "core/order_service.h"
#include "core/production_service.h"
#include "core/rng.h"
#include "core/research_service.h"
#include "core/market_service.h"
#include "sim/save_service.h"

#include <iostream>

static std::string saveSlotPath(int slot) {
    return slot > 0 ? "realm-slot" + std::to_string(slot) + ".sav" : "realm-save.txt";
}

template<class... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };
template<class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

static CommandResult result(CommandStatus status, const std::string& reason = {}) {
    return { status, reason, {} };
}

static GameEvent emit(GameContext& context, GameEvent event) {
    context.events.emit(event);
    return event;
}

static CommandResult accepted(GameContext& context, PlayerId issuer) {
    CommandResult out = result(CommandStatus::Accepted);
    out.events.push_back(emit(context, { GameEventType::CommandAccepted, issuer, -1, { -1, -1 }, "" }));
    return out;
}

static CommandResult rejected(GameContext& context, PlayerId issuer, const std::string& reason) {
    CommandResult out = result(CommandStatus::Rejected, reason);
    out.events.push_back(emit(context, { GameEventType::CommandRejected, issuer, -1, { -1, -1 }, reason }));
    return out;
}

class CapturingEventSink : public EventSink {
public:
    explicit CapturingEventSink(EventSink& next) : next_(next) {}

    void emit(const GameEvent& event) override {
        events.push_back(event);
        next_.emit(event);
    }

    std::vector<GameEvent> events;

private:
    EventSink& next_;
};

static CommandResult noOp(const std::string& reason = {}) {
    return result(CommandStatus::NoOp, reason);
}

static CommandResult orderResult(GameContext& context, PlayerId issuer, ServiceResult serviceResult) {
    return serviceResult.ok ? accepted(context, issuer) : rejected(context, issuer, serviceResult.reason ? serviceResult.reason : "Command rejected.");
}

static CommandResult serviceResult(GameContext& context, PlayerId issuer, ServiceResult result, const char* fallback) {
    return result.ok ? accepted(context, issuer) : rejected(context, issuer, result.reason ? result.reason : fallback);
}

template <typename ServiceFn>
static CommandResult capturedOrderResult(GameContext& context, PlayerId issuer, ServiceFn&& serviceFn) {
    CapturingEventSink capture(context.events);
    ServiceResult result = serviceFn(capture);
    CommandResult out = orderResult(context, issuer, result);
    out.events.insert(out.events.begin(), capture.events.begin(), capture.events.end());
    return out;
}

template <typename ServiceFn>
static CommandResult capturedServiceResult(GameContext& context, PlayerId issuer, ServiceFn&& serviceFn, const char* fallback) {
    CapturingEventSink capture(context.events);
    ServiceResult result = serviceFn(capture);
    CommandResult out = serviceResult(context, issuer, result, fallback);
    out.events.insert(out.events.begin(), capture.events.begin(), capture.events.end());
    return out;
}

static Entity* selectedEntity(GameContext& context, const Selection& selection) {
    return entityById(context.game, context.world, selection.primaryId);
}

static bool validSlot(int slot) {
    return slot >= 0 && slot < 9;
}

CommandResult dispatchCommand(GameContext& context, const Command& command) {
    Game& game = context.game;
    PlayerId issuer = command.issuer;

    return std::visit(Overloaded{
        [&](const std::monostate&) -> CommandResult {
            return noOp();
        },
        [&](const ContextCommand& payload) -> CommandResult {
            if (!inBounds(payload.target.x, payload.target.y)) return rejected(context, issuer, "Target is out of bounds.");
            Command typed = resolveContextCommand(game, context.world, issuer, payload.selection, payload.target);
            if (!commandIsEmpty(typed) && !commandIsContext(typed)) {
                typed.issuer = issuer;
                return dispatchCommand(context, typed);
            }
            return rejected(context, issuer, "Context command could not be resolved.");
        },
        [&](const SelectCommand& payload) -> CommandResult {
            if (!inBounds(payload.target.x, payload.target.y)) return rejected(context, issuer, "Selection target is out of bounds.");
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                selectAtTile(game, context.world, events, issuer, payload.target.x, payload.target.y);
                return ServiceResult{ true, nullptr };
            });
        },
        [&](const BoxSelectCommand& payload) -> CommandResult {
            if (!inBounds(payload.start.x, payload.start.y) || !inBounds(payload.end.x, payload.end.y))
                return rejected(context, issuer, "Box selection target is out of bounds.");
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                boxSelect(game, context.world, events, issuer, payload.start.x, payload.start.y, payload.end.x, payload.end.y);
                return ServiceResult{ true, nullptr };
            });
        },
        [&](const SelectAllOfTypeInViewCommand& payload) -> CommandResult {
            if (!inBounds(payload.target.x, payload.target.y)) return rejected(context, issuer, "Selection target is out of bounds.");
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                selectAllOfTypeInView(game, context.world, events, issuer, payload.target.x, payload.target.y);
                return ServiceResult{ true, nullptr };
            });
        },
        [&](const BuildCommand& payload) -> CommandResult {
            if (!inBounds(payload.target.x, payload.target.y)) return rejected(context, issuer, "Build target is out of bounds.");
            Entity* builder = selectedEntity(context, payload.selection);
            if (!builder || builder->owner != issuer) return rejected(context, issuer, "Builder is not owned by issuer.");
            return capturedServiceResult(context, issuer, [&](EventSink& events) {
                return startBuildService(game, context.world, events, issuer, builder->id, payload.entityType, payload.target);
            }, "Build command rejected.");
        },
        [&](const BuildLineCommand& payload) -> CommandResult {
            if (!inBounds(payload.start.x, payload.start.y) || !inBounds(payload.end.x, payload.end.y))
                return rejected(context, issuer, "Build-line target is out of bounds.");
            Entity* builder = selectedEntity(context, payload.selection);
            if (!builder || builder->owner != issuer) return rejected(context, issuer, "Builder is not owned by issuer.");
            return capturedServiceResult(context, issuer, [&](EventSink& events) {
                return startBuildLineService(game, context.world, events, issuer, builder->id, payload.entityType, payload.start, payload.end);
            }, "Build-line command rejected.");
        },
        [&](const TrainCommand& payload) -> CommandResult {
            Entity* building = selectedEntity(context, payload.selection);
            if (!building || building->owner != issuer) return rejected(context, issuer, "Producer is not owned by issuer.");
            return capturedServiceResult(context, issuer, [&](EventSink& events) {
                return startTrainingService(game, context.world, events, issuer, building->id, payload.entityType);
            }, "Train command rejected.");
        },
        [&](const ResearchCommand& payload) -> CommandResult {
            Entity* building = selectedEntity(context, payload.selection);
            if (!building || building->owner != issuer) return rejected(context, issuer, "Research building is not owned by issuer.");
            return capturedServiceResult(context, issuer, [&](EventSink& events) {
                return startResearchService(game, context.world, events, issuer, building->id, payload.researchId);
            }, "Research command rejected.");
        },
        [&](const MarketTradeCommand& payload) -> CommandResult {
            Entity* market = selectedEntity(context, payload.selection);
            if (!market || market->owner != issuer) return rejected(context, issuer, "Market is not owned by issuer.");
            return capturedServiceResult(context, issuer, [&](EventSink& events) {
                return executeTradeService(game, context.world, events, issuer, market->id, payload.trade);
            }, "Market trade rejected.");
        },
        [&](const HelpCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return startHelp(game, context.world, events, issuer, payload.selection, payload.targetId);
            });
        },
        [&](const SetRallyCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return setRallyPoint(game, context.world, events, issuer, payload.selection, payload.target);
            });
        },
        [&](const AttackMoveCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return startAttackMove(game, context.world, events, issuer, payload.selection, payload.target);
            });
        },
        [&](const MoveCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return startMove(game, context.world, events, issuer, payload.selection, payload.target);
            });
        },
        [&](const AttackCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return startAttack(game, context.world, events, issuer, payload.selection, payload.targetId);
            });
        },
        [&](const GatherCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return startGather(game, context.world, events, issuer, payload.selection, payload.target);
            });
        },
        [&](const GarrisonCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return startGarrison(game, context.world, events, issuer, payload.selection, payload.targetId);
            });
        },
        [&](const EjectGarrisonCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return ejectGarrisonService(game, context.world, events, issuer, payload.selection);
            });
        },
        [&](const HoldPositionCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return holdPosition(game, context.world, events, issuer, payload.selection);
            });
        },
        [&](const StopCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return stopUnits(game, context.world, events, issuer, payload.selection);
            });
        },
        [&](const AssignControlGroupCommand& payload) -> CommandResult {
            if (!assignControlGroup(game, context.world, issuer, payload.slot, payload.selection)) return rejected(context, issuer, "Invalid control group assignment.");
            emit(context, { GameEventType::StatusMessage, issuer, -1, { -1, -1 },
                            "Group " + std::to_string(payload.slot + 1) + " assigned ("
                            + std::to_string(controlGroupSize(game, issuer, payload.slot)) + " units)" });
            return accepted(context, issuer);
        },
        [&](const RecallControlGroupCommand& payload) -> CommandResult {
            if (!validSlot(payload.slot)) return rejected(context, issuer, "Invalid control group.");
            if (!recallControlGroup(game, context.world, issuer, payload.slot)) return rejected(context, issuer, "Group " + std::to_string(payload.slot + 1) + " is empty");
            emit(context, { GameEventType::StatusMessage, issuer, -1, { -1, -1 },
                            "Group " + std::to_string(payload.slot + 1) + " recalled ("
                            + std::to_string(game.selectedIds.size()) + " units)" });
            return accepted(context, issuer);
        },
        [&](const TogglePauseCommand&) -> CommandResult {
            if (game.mode == M_NORMAL || game.mode == M_PAUSED)
                game.mode = game.mode == M_PAUSED ? M_NORMAL : M_PAUSED;
            return accepted(context, issuer);
        },
        [&](const SaveCommand& payload) -> CommandResult {
            std::string path = saveSlotPath(payload.slot);
            CapturingEventSink capture(context.events);
            GameContext serviceContext{ game, context.world, capture };
            SaveLoadResult save = saveGameService(serviceContext, { path, payload.slot, issuer });
            CommandResult out = save.ok ? accepted(context, issuer) : rejected(context, issuer, save.error);
            out.events.insert(out.events.begin(), capture.events.begin(), capture.events.end());
            return out;
        },
        [&](const LoadCommand& payload) -> CommandResult {
            std::string path = saveSlotPath(payload.slot);
            CapturingEventSink capture(context.events);
            GameContext serviceContext{ game, context.world, capture };
            SaveLoadResult load = loadGameService(serviceContext, { path, payload.slot, issuer });
            CommandResult out = load.ok ? accepted(context, issuer) : rejected(context, issuer, load.error);
            out.events.insert(out.events.begin(), capture.events.begin(), capture.events.end());
            return out;
        },
        [&](const ResignCommand&) -> CommandResult {
            game.returnToMenu = true;
            std::cerr << "realm: resign/return-to-menu requested tick=" << game.tick << "\n";
            return accepted(context, issuer);
        },
        [&](const ToggleGateCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return toggleGateMode(game, context.world, events, issuer, payload.selection);
            });
        },
        [&](const ToggleTrebuchetPackedCommand& payload) -> CommandResult {
            return capturedOrderResult(context, issuer, [&](EventSink& events) {
                return toggleTrebuchetPacked(game, context.world, events, issuer, payload.selection);
            });
        },
        [&](const ToggleDiagnosticsCommand&) -> CommandResult {
            game.diagnostics = !game.diagnostics;
            emit(context, { GameEventType::StatusMessage, issuer, -1, { -1, -1 },
                            game.diagnostics ? "Diagnostics on." : "Diagnostics off." });
            return accepted(context, issuer);
        },
        [&](const RevealMapDebugCommand&) -> CommandResult {
            if (issuer < 0 || issuer >= MAX_PLAYERS) return rejected(context, issuer, "Invalid reveal-map issuer.");
            for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
                game.map[y][x].visible[issuer] = true;
                game.map[y][x].explored[issuer] = true;
            }
            emit(context, { GameEventType::StatusMessage, issuer, -1, { -1, -1 }, "Debug: map revealed" });
            return accepted(context, issuer);
        }
    }, command.payload);
}
