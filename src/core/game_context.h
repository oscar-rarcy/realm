#pragma once

#include "realm.h"
#include "core/game_events.h"
#include "core/world_index.h"
#include "view_state.h"

// New command/domain code should receive explicit context objects instead of
// reading global state directly. Legacy wrappers still bridge existing callers.

struct GameContext {
    Game& game;
    WorldIndex& world;
    EventSink& events;
};

struct UiContext {
    ViewState& view;
    EventSink& events;
};

inline GameContext makeGameContext(Game& game, WorldIndex& world, EventSink& events) {
    return { game, world, events };
}

inline GameContext legacyGameContext(Game& game, WorldIndex& world) {
    world = buildWorldIndex(game);
    return makeGameContext(game, world, gameEvents());
}

inline UiContext legacyUiContext(ViewState& viewState) {
    return { viewState, gameEvents() };
}
