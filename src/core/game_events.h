#pragma once

#include "realm.h"

#include <string>

enum class GameEventType {
    StatusMessage,
    ActionMarker,
    CommandRejected,
    ResearchStarted,
    ResearchCompleted,
    TrainingStarted,
    TrainingQueued,
    BuildingPlaced,
};

struct GameEvent {
    GameEventType type;
    int player = -1;
    int entityId = -1;
    MapPos tile{ -1, -1 };
    std::string message;
    char markerGlyph = 0;
};

class EventSink {
public:
    virtual ~EventSink() = default;
    virtual void emit(const GameEvent& event) = 0;
};

class LegacyUiEventSink : public EventSink {
public:
    void emit(const GameEvent& event) override;
};

EventSink& gameEvents();
void emitGameEvent(const GameEvent& event);
void emitStatusEvent(int player, const std::string& message, GameEventType type = GameEventType::StatusMessage);
void emitActionMarkerEvent(int player, MapPos tile, char glyph);
