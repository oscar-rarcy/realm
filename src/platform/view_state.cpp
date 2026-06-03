#include "view_state.h"

#include "realm.h"

#include <algorithm>

ViewState view;
UiState ui;

namespace {

void clampViewOrigin(ViewState& state) {
    state.viewX = std::max(0, std::min(state.viewX, MAP_W - state.viewW));
    state.viewY = std::max(0, std::min(state.viewY, MAP_H - state.viewH));
}

} // namespace

void resetViewState() {
    view = ViewState{};
}

void resetUiState() {
    ui = UiState{};
}

void tickUiState(UiState& state) {
    if (state.statusTimer > 0) state.statusTimer--;
    for (auto& marker : state.actionMarkers) if (marker.ticks > 0) marker.ticks--;
    state.actionMarkers.erase(std::remove_if(state.actionMarkers.begin(), state.actionMarkers.end(),
        [](const ActionMarker& marker){ return marker.ticks <= 0; }), state.actionMarkers.end());
}

void clampCursorToMap(ViewState& state) {
    state.cursorX = std::max(0, std::min(state.cursorX, MAP_W - 1));
    state.cursorY = std::max(0, std::min(state.cursorY, MAP_H - 1));
}

ViewportCell viewportCellAt(const ViewState& state, int screenX, int screenY, int mapTopY) {
    int mapScreenY = screenY - mapTopY;
    int mapX = state.viewX + screenX;
    int mapY = state.viewY + mapScreenY;
    return { mapX, mapY, mapScreenY >= 0 && state.viewW > 0 && screenX < state.viewW && inBounds(mapX, mapY) };
}

bool handleMinimapClick(ViewState& state, int screenWidth, int mouseX, int mouseY, bool activate) {
    int panelW = 24;
    int panelX = screenWidth - panelW;
    int mmX = panelX + 1;
    int mmY = 1;
    int mmW = panelW - 2;
    int mmH = std::min(state.viewH / 3, 14);
    if (mouseX < mmX || mouseX >= mmX + mmW || mouseY < mmY || mouseY >= mmY + mmH) return false;
    if (activate && mmW > 0 && mmH > 0) {
        int mx = (mouseX - mmX) * MAP_W / mmW;
        int my = (mouseY - mmY) * MAP_H / mmH;
        state.viewX = mx - state.viewW / 2;
        state.viewY = my - state.viewH / 2;
        state.cursorX = mx;
        state.cursorY = my;
        state.dragging = false;
        clampViewOrigin(state);
        clampCursorToMap(state);
    }
    return true;
}

void panViewportAtScreenEdge(ViewState& state, int screenX, int screenY, int mapTopY, int edgeMargin, int edgeStep) {
    int viewportY = screenY - mapTopY;
    int dx = 0;
    int dy = 0;
    if (screenX < edgeMargin) dx = -edgeStep;
    else if (screenX >= state.viewW - edgeMargin) dx = edgeStep;
    if (viewportY < edgeMargin) dy = -edgeStep;
    else if (viewportY >= state.viewH - edgeMargin) dy = edgeStep;
    if (!dx && !dy) return;
    state.viewX += dx;
    state.viewY += dy;
    clampViewOrigin(state);
}
