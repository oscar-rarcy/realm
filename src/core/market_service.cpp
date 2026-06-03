#include "market_service.h"
#include "realm.h"
#include "core/entity_query.h"
#include "core/game_events.h"
#include "core/world_index.h"

// Canonical exchange rates. These mirror the rates that were previously inline in
// the input handler so the player-facing behavior is unchanged.
static const MarketTradeDef DEFS[] = {
    { MarketTradeType::GoldForWood, 'g', TradeResource::Gold, 40, TradeResource::Wood, 30, "Traded gold for wood." },
    { MarketTradeType::WoodForGold, 'w', TradeResource::Wood, 40, TradeResource::Gold, 30, "Traded wood for gold." },
    { MarketTradeType::GoldForFood, 'f', TradeResource::Gold, 50, TradeResource::Food, 30, "Bought food." },
    { MarketTradeType::FoodForGold, 'v', TradeResource::Food, 40, TradeResource::Gold, 30, "Sold food." },
};

static const int DEF_COUNT = (int)(sizeof(DEFS) / sizeof(DEFS[0]));

const MarketTradeDef* marketTradeDefs(int& count) {
    count = DEF_COUNT;
    return DEFS;
}

const MarketTradeDef* marketTradeDef(MarketTradeType type) {
    for (int i = 0; i < DEF_COUNT; i++)
        if (DEFS[i].type == type) return &DEFS[i];
    return nullptr;
}

static int getResource(const Player& p, TradeResource r) {
    switch (r) {
        case TradeResource::Gold: return p.gold;
        case TradeResource::Wood: return p.wood;
        case TradeResource::Food: return p.food;
    }
    return 0;
}

static void addResource(Player& p, TradeResource r, int delta) {
    switch (r) {
        case TradeResource::Gold: p.gold += delta; break;
        case TradeResource::Wood: p.wood += delta; break;
        case TradeResource::Food: p.food += delta; break;
    }
}

CanTradeResult canTrade(const Game& game, int player, const Entity& market, MarketTradeType type) {
    const MarketTradeDef* def = marketTradeDef(type);
    if (!def) return {false, "Unknown trade."};
    if (market.type != E_MARKET) return {false, "Not a market."};
    if (market.underConstruction) return {false, "Market not complete."};
    if (market.owner != player) return {false, "Not your market."};
    if (getResource(game.players[player], def->from) < def->fromAmount)
        return {false, "Not enough resources."};
    return {true, nullptr};
}

bool executeTrade(Game& game, int player, int marketId, MarketTradeType type) {
    return executeTradeService(game, player, marketId, type).ok;
}

ServiceResult executeTradeService(Game& game, int player, int marketId, MarketTradeType type) {
    WorldIndex world = buildWorldIndex(game);
    return executeTradeService(game, world, player, marketId, type);
}

ServiceResult executeTradeService(Game& game, const WorldIndex& world, int player, int marketId, MarketTradeType type) {
    Entity* market = findEntity(game, world, marketId);
    if (!market) return { false, "Market not found." };

    CanTradeResult result = canTrade(game, player, *market, type);
    if (!result.ok) {
        emitStatusEvent(player, result.reason, GameEventType::CommandRejected);
        return { false, result.reason };
    }

    const MarketTradeDef* def = marketTradeDef(type);
    Player& p = game.players[player];
    addResource(p, def->from, -def->fromAmount);
    addResource(p, def->to, def->toAmount);
    emitStatusEvent(player, def->successMessage);
    return { true, nullptr };
}
