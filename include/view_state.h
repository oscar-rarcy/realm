#pragma once

#include "core/game_types.h"

#include <string>
#include <vector>

struct ViewState {
    int cursorX = 0;
    int cursorY = 0;
    int viewX = 0;
    int viewY = 0;
    int viewW = 1;
    int viewH = 1;
    bool dragging = false;
    int dragStartX = 0;
    int dragStartY = 0;
    int wallDragX = 0;
    int wallDragY = 0;
};

struct UiState {
    std::string statusMsg;
    int statusTimer = 0;
    std::vector<ActionMarker> actionMarkers;
};

struct ViewportCell {
    int x = 0;
    int y = 0;
    bool inMap = false;
};

extern ViewState view;
extern UiState ui;

void resetViewState();
void resetUiState();
void tickUiState(UiState& state);
void clampCursorToMap(ViewState& state);
ViewportCell viewportCellAt(const ViewState& state, int screenX, int screenY, int mapTopY);
bool handleMinimapClick(ViewState& state, int screenWidth, int mouseX, int mouseY, bool activate);
