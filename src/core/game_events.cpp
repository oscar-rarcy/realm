#include "game_events.h"
#include "realm.h"
#include "view_state.h"

namespace {

class QueuedGameEventSink : public EventSink {
public:
    void emit(const GameEvent& event) override {
        events.push_back(event);
    }

    std::vector<GameEvent> events;
};

QueuedGameEventSink& eventQueue() {
    static QueuedGameEventSink sink;
    return sink;
}

bool visibleToViewer(const GameEvent& event, int viewerPlayer) {
    return viewerPlayer < 0 || event.player < 0 || event.player == viewerPlayer;
}

void applyGameEventToUi(UiState& uiState, const GameEvent& event) {
    switch (event.type) {
        case GameEventType::StatusMessage:
        case GameEventType::CommandAccepted:
        case GameEventType::CommandRejected:
        case GameEventType::ResourcesChanged:
        case GameEventType::EntitySpawned:
        case GameEventType::EntityDestroyed:
        case GameEventType::UnitOrdered:
        case GameEventType::GarrisonChanged:
        case GameEventType::ResearchStarted:
        case GameEventType::ResearchCompleted:
        case GameEventType::TrainingStarted:
        case GameEventType::TrainingQueued:
        case GameEventType::BuildingPlaced:
        case GameEventType::SaveCompleted:
        case GameEventType::LoadCompleted:
            if (!event.message.empty()) {
                uiState.statusMsg = event.message;
                uiState.statusTimer = 35;
            }
            break;
        case GameEventType::ActionMarker:
            if (event.markerGlyph && inBounds(event.tile.x, event.tile.y)) {
                uiState.actionMarkers.push_back({ event.tile.x, event.tile.y, 18, event.markerGlyph });
                if (uiState.actionMarkers.size() > 32) uiState.actionMarkers.erase(uiState.actionMarkers.begin());
            }
            break;
    }
}

} // namespace

EventSink& gameEvents() {
    return eventQueue();
}

std::vector<GameEvent> drainGameEvents() {
    std::vector<GameEvent> events;
    events.swap(eventQueue().events);
    return events;
}

void flushGameEventsToUi(UiState& uiState, int viewerPlayer) {
    for (const GameEvent& event : drainGameEvents()) {
        if (visibleToViewer(event, viewerPlayer)) applyGameEventToUi(uiState, event);
    }
}

void emitGameEvent(const GameEvent& event) {
    gameEvents().emit(event);
}

void emitUiStatusEvent(int player, const std::string& message, GameEventType type) {
    emitGameEvent({ type, player, -1, { -1, -1 }, message, 0 });
}
