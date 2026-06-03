#pragma once

#include "core/game_types.h"
#include "core/service_result.h"

// Single source of truth for market exchange rates, shared by input handling and
// any future AI trading (see docs/implementation/refactor-plan.md phase 3.4).

struct WorldIndex;
struct Entity;
struct Game;

enum class TradeResource { Gold, Wood, Food };

enum class MarketTradeType {
    GoldForWood,
    WoodForGold,
    GoldForFood,
    FoodForGold,
};

struct MarketTradeDef {
    MarketTradeType type;
    char key;                 // hotkey used by the terminal UI
    TradeResource from;
    int fromAmount;           // resource spent
    TradeResource to;
    int toAmount;             // resource gained
    const char* successMessage;
};

const MarketTradeDef* marketTradeDefs(int& count);
const MarketTradeDef* marketTradeDef(MarketTradeType type);

struct CanTradeResult {
    bool ok;
    const char* reason; // non-null human-readable reason when !ok
};

CanTradeResult canTrade(const Game& game, int player, const Entity& market, MarketTradeType type);

// Validates and, on success, moves resources for the trade. Emits a status
// message for the human player (owner 0). Returns true when the trade happened.
bool executeTrade(Game& game, int player, int marketId, MarketTradeType type);
ServiceResult executeTradeService(Game& game, int player, int marketId, MarketTradeType type);
ServiceResult executeTradeService(Game& game, const WorldIndex& world, int player, int marketId, MarketTradeType type);
