#pragma once

#include "core/game_events.h"
#include "core/world_index.h"
#include "view_state.h"

// Command/domain code receives explicit context objects instead of reading
// global state directly.

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
