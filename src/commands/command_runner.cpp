#include "commands/command_runner.h"

#include "core/game_context.h"
#include "core/world_index.h"

CommandResult dispatchCommandForLocalGame(Game& game, EventSink& events, const Command& command) {
    WorldIndex world = buildWorldIndex(game);
    GameContext context{ game, world, events };
    return dispatchCommand(context, command);
}
