#pragma once

#include "commands/command.h"

class EventSink;
struct Game;

CommandResult dispatchCommandForLocalGame(Game& game, EventSink& events, const Command& command);
CommandResult stopCurrentSelection(Game& game, EventSink& events, PlayerId issuer);
