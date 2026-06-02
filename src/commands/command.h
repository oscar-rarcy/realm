#pragma once

#include "realm.h"
#include "core/game_context.h"
#include "core/research_defs.h"
#include "core/market_service.h"

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
    Resign
};

struct Command {
    CommandType type = CommandType::None;
    Selection selection;
    MapPos targetTile{-1, -1};
    MapPos lineEnd{-1, -1};
    int targetEntity = -1;
    EntityType entityType = E_NONE;
    ResearchId researchId{};
    MarketTradeType marketTrade{};
    int groupIndex = -1;
};

Selection currentSelection();
Command resolveContextCommand(const Game& game, const Selection& selection, MapPos target);
void dispatchCommand(Game& game, const Command& command);
void dispatchCommand(GameContext& context, const Command& command);
void cmdAtTileSingle(Entity* selected, int x, int y);
void cmdAtTileGroup(const Selection& selection, int x, int y);
void selectAtTile(Game& game, int x, int y);
void boxSelect(Game& game, int x0, int y0, int x1, int y1);
void selectAllOfTypeInView(Game& game, int x, int y);
