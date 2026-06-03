#pragma once

#include "commands/command.h"

class EventSink;
struct Game;

CommandResult dispatchCommandForLocalGame(Game& game, EventSink& events, const Command& command);
CommandResult dispatchStopCommandForLocalSelection(Game& game, EventSink& events);
