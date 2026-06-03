#include "commands/command_runner.h"

#include "core/game_context.h"
#include "core/world_index.h"

CommandResult dispatchCommandForLocalGame(Game& game, EventSink& events, const Command& command) {
    WorldIndex world = buildWorldIndex(game);
    GameContext context{ game, world, events };
    return dispatchCommand(context, command);
}

CommandResult dispatchStopCommandForLocalSelection(Game& game, EventSink& events) {
    Command command;
    command.issuer = 0;
    command.payload = StopCommand{ currentSelection(game) };
    return dispatchCommandForLocalGame(game, events, command);
}
