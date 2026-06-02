#include "game_events.h"

void LegacyUiEventSink::emit(const GameEvent& event) {
    if (event.player > 0) return;

    switch (event.type) {
        case GameEventType::StatusMessage:
        case GameEventType::CommandRejected:
        case GameEventType::ResearchStarted:
        case GameEventType::ResearchCompleted:
        case GameEventType::TrainingStarted:
        case GameEventType::TrainingQueued:
        case GameEventType::BuildingPlaced:
            if (!event.message.empty()) {
                g.statusMsg = event.message;
                g.statusTimer = 35;
            }
            break;
        case GameEventType::ActionMarker:
            if (event.markerGlyph && inBounds(event.tile.x, event.tile.y)) {
                g.actionMarkers.push_back({ event.tile.x, event.tile.y, 18, event.markerGlyph });
                if (g.actionMarkers.size() > 32) g.actionMarkers.erase(g.actionMarkers.begin());
            }
            break;
    }
}

EventSink& gameEvents() {
    static LegacyUiEventSink sink;
    return sink;
}

void emitGameEvent(const GameEvent& event) {
    gameEvents().emit(event);
}

void emitStatusEvent(int player, const std::string& message, GameEventType type) {
    emitGameEvent({ type, player, -1, { -1, -1 }, message, 0 });
}

void emitActionMarkerEvent(int player, MapPos tile, char glyph) {
    emitGameEvent({ GameEventType::ActionMarker, player, -1, tile, "", glyph });
}
