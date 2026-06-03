#pragma once

#include "core/game_types.h"

#include <string>
#include <vector>

struct Game;

enum class GameEventType {
    StatusMessage,
    ActionMarker,
    CommandAccepted,
    CommandRejected,
    ResourcesChanged,
    EntitySpawned,
    EntityDestroyed,
    UnitOrdered,
    GarrisonChanged,
    ResearchStarted,
    ResearchCompleted,
    TrainingStarted,
    TrainingQueued,
    BuildingPlaced,
    SaveCompleted,
    LoadCompleted,
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

EventSink& gameEvents();
std::vector<GameEvent> drainGameEvents();
void flushGameEventsToUi(Game& game, int viewerPlayer = 0);
void emitGameEvent(const GameEvent& event);
void emitStatusEvent(int player, const std::string& message, GameEventType type = GameEventType::StatusMessage);
void emitActionMarkerEvent(int player, MapPos tile, char glyph);
