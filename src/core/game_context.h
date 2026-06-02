#pragma once

#include "realm.h"
#include "core/game_events.h"
#include "view_state.h"

// New command/domain code should receive explicit context objects instead of
// reading global state directly. Legacy wrappers still bridge existing callers.

struct GameContext {
    Game& game;
    EventSink& events;
};

struct UiContext {
    ViewState& view;
    EventSink& events;
};

inline GameContext legacyGameContext(Game& game) {
    return { game, gameEvents() };
}

inline UiContext legacyUiContext(ViewState& viewState) {
    return { viewState, gameEvents() };
}
