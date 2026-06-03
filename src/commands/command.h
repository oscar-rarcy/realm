#pragma once

#include "realm.h"
#include "core/game_context.h"
#include "core/research_defs.h"
#include "core/market_service.h"

#include <type_traits>
#include <variant>

struct Selection {
    int primaryId = -1;
    std::vector<int> ids;
};

enum class CommandType {
    None,
    Context,
    Move,
    Attack,
    AttackMove,
    Gather,
    Build,
    BuildLine,
    Train,
    Research,
    MarketTrade,
    Help,
    Garrison,
    EjectGarrison,
    SetRally,
    HoldPosition,
    Stop,
    Select,
    BoxSelect,
    SelectAllOfTypeInView,
    AssignControlGroup,
    RecallControlGroup,
    TogglePause,
    Save,
    Load,
    Resign,
    ToggleGate,
    ToggleTrebuchetPacked,
    ToggleDiagnostics,
    RevealMapDebug
};

enum class CommandStatus {
    Accepted,
    Rejected,
    NoOp,
    Error,
};

struct CommandResult {
    CommandStatus status = CommandStatus::NoOp;
    std::string reason;
    std::vector<GameEvent> events;
};

struct ContextCommand { Selection selection; MapPos target{-1, -1}; };
struct MoveCommand { Selection selection; MapPos target{-1, -1}; };
struct AttackCommand { Selection selection; EntityId targetId = -1; };
struct AttackMoveCommand { Selection selection; MapPos target{-1, -1}; };
struct GatherCommand { Selection selection; MapPos target{-1, -1}; };
struct BuildCommand { Selection selection; EntityType entityType = E_NONE; MapPos target{-1, -1}; };
struct BuildLineCommand { Selection selection; EntityType entityType = E_NONE; MapPos start{-1, -1}; MapPos end{-1, -1}; };
struct TrainCommand { Selection selection; EntityType entityType = E_NONE; };
struct ResearchCommand { Selection selection; ResearchId researchId{}; };
struct MarketTradeCommand { Selection selection; MarketTradeType trade{}; };
struct HelpCommand { Selection selection; EntityId targetId = -1; };
struct GarrisonCommand { Selection selection; EntityId targetId = -1; };
struct EjectGarrisonCommand { Selection selection; };
struct SetRallyCommand { Selection selection; MapPos target{-1, -1}; };
struct HoldPositionCommand { Selection selection; };
struct StopCommand { Selection selection; };
struct SelectCommand { MapPos target{-1, -1}; };
struct BoxSelectCommand { MapPos start{-1, -1}; MapPos end{-1, -1}; };
struct SelectAllOfTypeInViewCommand { MapPos target{-1, -1}; };
struct AssignControlGroupCommand { Selection selection; int slot = -1; };
struct RecallControlGroupCommand { int slot = -1; };
struct TogglePauseCommand {};
struct SaveCommand { int slot = -1; };
struct LoadCommand { int slot = -1; };
struct ResignCommand {};
struct ToggleGateCommand { Selection selection; };
struct ToggleTrebuchetPackedCommand { Selection selection; };
struct ToggleDiagnosticsCommand {};
struct RevealMapDebugCommand {};

using CommandPayload = std::variant<
    std::monostate,
    ContextCommand,
    MoveCommand,
    AttackCommand,
    AttackMoveCommand,
    GatherCommand,
    BuildCommand,
    BuildLineCommand,
    TrainCommand,
    ResearchCommand,
    MarketTradeCommand,
    HelpCommand,
    GarrisonCommand,
    EjectGarrisonCommand,
    SetRallyCommand,
    HoldPositionCommand,
    StopCommand,
    SelectCommand,
    BoxSelectCommand,
    SelectAllOfTypeInViewCommand,
    AssignControlGroupCommand,
    RecallControlGroupCommand,
    TogglePauseCommand,
    SaveCommand,
    LoadCommand,
    ResignCommand,
    ToggleGateCommand,
    ToggleTrebuchetPackedCommand,
    ToggleDiagnosticsCommand,
    RevealMapDebugCommand>;

struct Command {
    PlayerId issuer = 0;
    CommandPayload payload{};

    CommandType type() const {
        return std::visit([](const auto& p) -> CommandType {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, std::monostate>) return CommandType::None;
            else if constexpr (std::is_same_v<T, ContextCommand>) return CommandType::Context;
            else if constexpr (std::is_same_v<T, MoveCommand>) return CommandType::Move;
            else if constexpr (std::is_same_v<T, AttackCommand>) return CommandType::Attack;
            else if constexpr (std::is_same_v<T, AttackMoveCommand>) return CommandType::AttackMove;
            else if constexpr (std::is_same_v<T, GatherCommand>) return CommandType::Gather;
            else if constexpr (std::is_same_v<T, BuildCommand>) return CommandType::Build;
            else if constexpr (std::is_same_v<T, BuildLineCommand>) return CommandType::BuildLine;
            else if constexpr (std::is_same_v<T, TrainCommand>) return CommandType::Train;
            else if constexpr (std::is_same_v<T, ResearchCommand>) return CommandType::Research;
            else if constexpr (std::is_same_v<T, MarketTradeCommand>) return CommandType::MarketTrade;
            else if constexpr (std::is_same_v<T, HelpCommand>) return CommandType::Help;
            else if constexpr (std::is_same_v<T, GarrisonCommand>) return CommandType::Garrison;
            else if constexpr (std::is_same_v<T, EjectGarrisonCommand>) return CommandType::EjectGarrison;
            else if constexpr (std::is_same_v<T, SetRallyCommand>) return CommandType::SetRally;
            else if constexpr (std::is_same_v<T, HoldPositionCommand>) return CommandType::HoldPosition;
            else if constexpr (std::is_same_v<T, StopCommand>) return CommandType::Stop;
            else if constexpr (std::is_same_v<T, SelectCommand>) return CommandType::Select;
            else if constexpr (std::is_same_v<T, BoxSelectCommand>) return CommandType::BoxSelect;
            else if constexpr (std::is_same_v<T, SelectAllOfTypeInViewCommand>) return CommandType::SelectAllOfTypeInView;
            else if constexpr (std::is_same_v<T, AssignControlGroupCommand>) return CommandType::AssignControlGroup;
            else if constexpr (std::is_same_v<T, RecallControlGroupCommand>) return CommandType::RecallControlGroup;
            else if constexpr (std::is_same_v<T, TogglePauseCommand>) return CommandType::TogglePause;
            else if constexpr (std::is_same_v<T, SaveCommand>) return CommandType::Save;
            else if constexpr (std::is_same_v<T, LoadCommand>) return CommandType::Load;
            else if constexpr (std::is_same_v<T, ResignCommand>) return CommandType::Resign;
            else if constexpr (std::is_same_v<T, ToggleGateCommand>) return CommandType::ToggleGate;
            else if constexpr (std::is_same_v<T, ToggleTrebuchetPackedCommand>) return CommandType::ToggleTrebuchetPacked;
            else if constexpr (std::is_same_v<T, ToggleDiagnosticsCommand>) return CommandType::ToggleDiagnostics;
            else if constexpr (std::is_same_v<T, RevealMapDebugCommand>) return CommandType::RevealMapDebug;
        }, payload);
    }
};

Selection currentSelection();
Command resolveContextCommand(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);
Command resolveContextCommand(const Game& game, PlayerId issuer, const Selection& selection, MapPos target);
Command resolveContextCommand(const Game& game, const Selection& selection, MapPos target);
CommandResult dispatchCommand(Game& game, const Command& command);
CommandResult dispatchCommand(GameContext& context, const Command& command);
void selectAtTile(Game& game, int x, int y);
void selectAtTile(Game& game, const WorldIndex& world, PlayerId issuer, int x, int y);
void boxSelect(Game& game, int x0, int y0, int x1, int y1);
void boxSelect(Game& game, const WorldIndex& world, PlayerId issuer, int x0, int y0, int x1, int y1);
void boxSelect(Game& game, PlayerId issuer, int x0, int y0, int x1, int y1);
void selectAllOfTypeInView(Game& game, int x, int y);
void selectAllOfTypeInView(Game& game, const WorldIndex& world, PlayerId issuer, int x, int y);
Entity* selectNextIdleWorker(Game& game, const WorldIndex& world, PlayerId issuer, EntityId afterId);
Entity* selectNextUnit(Game& game, const WorldIndex& world, PlayerId issuer, EntityId afterId);
Entity* selectHomeBase(Game& game, const WorldIndex& world, PlayerId issuer);
int selectAllMilitary(Game& game, const WorldIndex& world, PlayerId issuer);
bool selectionContainsMilitary(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
bool beginControlGroupAssignment(Game& game, const WorldIndex& world, PlayerId issuer);
void clearSelection(Game& game);
bool assignControlGroup(Game& game, const WorldIndex& world, PlayerId issuer, int slot, const Selection& selection);
bool recallControlGroup(Game& game, const WorldIndex& world, PlayerId issuer, int slot);
int controlGroupSize(Game& game, PlayerId issuer, int slot);
