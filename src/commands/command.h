#pragma once

#include "core/game_context.h"
#include "core/research_defs.h"
#include "core/market_service.h"

#include <string>
#include <type_traits>
#include <variant>
#include <vector>

struct Entity;

struct Selection {
    int primaryId = -1;
    std::vector<int> ids;
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
};

struct ContextCommand { Selection selection; MapPos target{-1, -1}; };
struct MoveCommand { Selection selection; MapPos target{-1, -1}; };
struct WaypointCommand { Selection selection; MapPos target{-1, -1}; };
struct PatrolCommand { Selection selection; MapPos target{-1, -1}; };
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
struct CancelProductionCommand { Selection selection; };
struct SelectCommand { MapPos target{-1, -1}; bool toggle = false; };
struct BoxSelectCommand { MapPos start{-1, -1}; MapPos end{-1, -1}; bool additive = false; };
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
    WaypointCommand,
    PatrolCommand,
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
    CancelProductionCommand,
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
};

enum class CommandActionKind {
    None,
    Context,
    Move,
    Waypoint,
    Patrol,
    Attack,
    FirePosition,
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
    CancelProduction,
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
    RevealMapDebug,
    Invalid,
};

enum class CommandPreviewMode {
    Context,
    Waypoint,
    BuildPlace,
    Rally,
    AttackMove,
    Patrol,
};

struct CommandPreviewRequest {
    PlayerId issuer = 0;
    Selection selection{};
    MapPos target{-1, -1};
    CommandPreviewMode mode = CommandPreviewMode::Context;
    EntityType buildType = E_NONE;
};

struct CommandOption {
    CommandActionKind kind = CommandActionKind::None;
    Command command{};
    std::string label;
    int priority = 0;
    bool recommended = false;
    bool enabled = true;
    std::string disabledReason;
};

struct CommandOptions {
    std::vector<CommandOption> options;
    int recommendedIndex = -1;
};

template <typename Payload>
inline bool commandHasPayload(const Command& command) {
    return std::holds_alternative<Payload>(command.payload);
}

inline bool commandIsEmpty(const Command& command) {
    return commandHasPayload<std::monostate>(command);
}

inline bool commandIsContext(const Command& command) {
    return commandHasPayload<ContextCommand>(command);
}

inline CommandActionKind commandActionKind(const Command& command) {
    return std::visit([](const auto& p) -> CommandActionKind {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, std::monostate>) return CommandActionKind::None;
        else if constexpr (std::is_same_v<T, ContextCommand>) return CommandActionKind::Context;
        else if constexpr (std::is_same_v<T, MoveCommand>) return CommandActionKind::Move;
        else if constexpr (std::is_same_v<T, WaypointCommand>) return CommandActionKind::Waypoint;
        else if constexpr (std::is_same_v<T, PatrolCommand>) return CommandActionKind::Patrol;
        else if constexpr (std::is_same_v<T, AttackCommand>) return CommandActionKind::Attack;
        else if constexpr (std::is_same_v<T, AttackMoveCommand>) return CommandActionKind::AttackMove;
        else if constexpr (std::is_same_v<T, GatherCommand>) return CommandActionKind::Gather;
        else if constexpr (std::is_same_v<T, BuildCommand>) return CommandActionKind::Build;
        else if constexpr (std::is_same_v<T, BuildLineCommand>) return CommandActionKind::BuildLine;
        else if constexpr (std::is_same_v<T, TrainCommand>) return CommandActionKind::Train;
        else if constexpr (std::is_same_v<T, ResearchCommand>) return CommandActionKind::Research;
        else if constexpr (std::is_same_v<T, MarketTradeCommand>) return CommandActionKind::MarketTrade;
        else if constexpr (std::is_same_v<T, HelpCommand>) return CommandActionKind::Help;
        else if constexpr (std::is_same_v<T, GarrisonCommand>) return CommandActionKind::Garrison;
        else if constexpr (std::is_same_v<T, EjectGarrisonCommand>) return CommandActionKind::EjectGarrison;
        else if constexpr (std::is_same_v<T, SetRallyCommand>) return CommandActionKind::SetRally;
        else if constexpr (std::is_same_v<T, HoldPositionCommand>) return CommandActionKind::HoldPosition;
        else if constexpr (std::is_same_v<T, StopCommand>) return CommandActionKind::Stop;
        else if constexpr (std::is_same_v<T, CancelProductionCommand>) return CommandActionKind::CancelProduction;
        else if constexpr (std::is_same_v<T, SelectCommand>) return CommandActionKind::Select;
        else if constexpr (std::is_same_v<T, BoxSelectCommand>) return CommandActionKind::BoxSelect;
        else if constexpr (std::is_same_v<T, SelectAllOfTypeInViewCommand>) return CommandActionKind::SelectAllOfTypeInView;
        else if constexpr (std::is_same_v<T, AssignControlGroupCommand>) return CommandActionKind::AssignControlGroup;
        else if constexpr (std::is_same_v<T, RecallControlGroupCommand>) return CommandActionKind::RecallControlGroup;
        else if constexpr (std::is_same_v<T, TogglePauseCommand>) return CommandActionKind::TogglePause;
        else if constexpr (std::is_same_v<T, SaveCommand>) return CommandActionKind::Save;
        else if constexpr (std::is_same_v<T, LoadCommand>) return CommandActionKind::Load;
        else if constexpr (std::is_same_v<T, ResignCommand>) return CommandActionKind::Resign;
        else if constexpr (std::is_same_v<T, ToggleGateCommand>) return CommandActionKind::ToggleGate;
        else if constexpr (std::is_same_v<T, ToggleTrebuchetPackedCommand>) return CommandActionKind::ToggleTrebuchetPacked;
        else if constexpr (std::is_same_v<T, ToggleDiagnosticsCommand>) return CommandActionKind::ToggleDiagnostics;
        else if constexpr (std::is_same_v<T, RevealMapDebugCommand>) return CommandActionKind::RevealMapDebug;
        else return CommandActionKind::Invalid;
    }, command.payload);
}

inline const char* commandPayloadName(const Command& command) {
    return std::visit([](const auto& p) -> const char* {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "None";
        else if constexpr (std::is_same_v<T, ContextCommand>) return "Context";
        else if constexpr (std::is_same_v<T, MoveCommand>) return "Move";
        else if constexpr (std::is_same_v<T, WaypointCommand>) return "Waypoint";
        else if constexpr (std::is_same_v<T, PatrolCommand>) return "Patrol";
        else if constexpr (std::is_same_v<T, AttackCommand>) return "Attack";
        else if constexpr (std::is_same_v<T, AttackMoveCommand>) return "AttackMove";
        else if constexpr (std::is_same_v<T, GatherCommand>) return "Gather";
        else if constexpr (std::is_same_v<T, BuildCommand>) return "Build";
        else if constexpr (std::is_same_v<T, BuildLineCommand>) return "BuildLine";
        else if constexpr (std::is_same_v<T, TrainCommand>) return "Train";
        else if constexpr (std::is_same_v<T, ResearchCommand>) return "Research";
        else if constexpr (std::is_same_v<T, MarketTradeCommand>) return "MarketTrade";
        else if constexpr (std::is_same_v<T, HelpCommand>) return "Help";
        else if constexpr (std::is_same_v<T, GarrisonCommand>) return "Garrison";
        else if constexpr (std::is_same_v<T, EjectGarrisonCommand>) return "EjectGarrison";
        else if constexpr (std::is_same_v<T, SetRallyCommand>) return "SetRally";
        else if constexpr (std::is_same_v<T, HoldPositionCommand>) return "HoldPosition";
        else if constexpr (std::is_same_v<T, StopCommand>) return "Stop";
        else if constexpr (std::is_same_v<T, CancelProductionCommand>) return "CancelProduction";
        else if constexpr (std::is_same_v<T, SelectCommand>) return "Select";
        else if constexpr (std::is_same_v<T, BoxSelectCommand>) return "BoxSelect";
        else if constexpr (std::is_same_v<T, SelectAllOfTypeInViewCommand>) return "SelectAllOfTypeInView";
        else if constexpr (std::is_same_v<T, AssignControlGroupCommand>) return "AssignControlGroup";
        else if constexpr (std::is_same_v<T, RecallControlGroupCommand>) return "RecallControlGroup";
        else if constexpr (std::is_same_v<T, TogglePauseCommand>) return "TogglePause";
        else if constexpr (std::is_same_v<T, SaveCommand>) return "Save";
        else if constexpr (std::is_same_v<T, LoadCommand>) return "Load";
        else if constexpr (std::is_same_v<T, ResignCommand>) return "Resign";
        else if constexpr (std::is_same_v<T, ToggleGateCommand>) return "ToggleGate";
        else if constexpr (std::is_same_v<T, ToggleTrebuchetPackedCommand>) return "ToggleTrebuchetPacked";
        else if constexpr (std::is_same_v<T, ToggleDiagnosticsCommand>) return "ToggleDiagnostics";
        else if constexpr (std::is_same_v<T, RevealMapDebugCommand>) return "RevealMapDebug";
    }, command.payload);
}

Selection currentSelection(const Game& game);
const char* commandActionKindName(CommandActionKind kind);
int commandActionPriority(CommandActionKind kind);
const CommandOption* recommendedCommandOption(const CommandOptions& options);
CommandOptions resolveCommandOptions(const Game& game, const WorldIndex& world, const CommandPreviewRequest& request);
CommandOptions resolveContextCommandOptions(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);
Command resolveRecommendedCommand(const Game& game, const WorldIndex& world, const CommandPreviewRequest& request);
Command resolveContextCommand(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target);
CommandResult dispatchCommand(GameContext& context, const Command& command);
void selectAtTile(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x, int y, bool toggle = false);
void boxSelect(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x0, int y0, int x1, int y1, bool additive = false);
void selectAllOfTypeInView(Game& game, const WorldIndex& world, EventSink& events, PlayerId issuer, int x, int y);
Entity* selectNextIdleWorker(Game& game, const WorldIndex& world, PlayerId issuer, EntityId afterId);
Entity* selectNextUnit(Game& game, const WorldIndex& world, PlayerId issuer, EntityId afterId);
Entity* selectHomeBase(Game& game, const WorldIndex& world, PlayerId issuer);
int selectAllMilitary(Game& game, const WorldIndex& world, PlayerId issuer);
bool selectionContainsMilitary(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
bool selectionContainsLandUnits(Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection);
bool beginControlGroupAssignment(Game& game, const WorldIndex& world, PlayerId issuer);
void clearSelection(Game& game);
bool assignControlGroup(Game& game, const WorldIndex& world, PlayerId issuer, int slot, const Selection& selection);
bool recallControlGroup(Game& game, const WorldIndex& world, PlayerId issuer, int slot);
int controlGroupSize(Game& game, PlayerId issuer, int slot);
