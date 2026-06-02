#include "command.h"
#include "core/build_service.h"
#include "core/game_events.h"
#include "core/production_service.h"
#include "core/research_service.h"
#include "core/market_service.h"

void dispatchCommand(Game& game, const Command& command) {
    switch (command.type) {
    case CommandType::Context:
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (command.selection.ids.size() > 1) cmdAtTileGroup(command.selection, command.targetTile.x, command.targetTile.y);
        else cmdAtTileSingle(findEntity(command.selection.primaryId), command.targetTile.x, command.targetTile.y);
        break;
    case CommandType::Select:
        selectAtTile(game, command.targetTile.x, command.targetTile.y);
        break;
    case CommandType::BoxSelect:
        boxSelect(game, command.targetTile.x, command.targetTile.y, command.groupIndex >> 16, command.groupIndex & 0xffff);
        break;
    case CommandType::SelectAllOfTypeInView:
        selectAllOfTypeInView(game, command.targetTile.x, command.targetTile.y);
        break;
    case CommandType::Build: {
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        Entity* builder = findEntity(command.selection.primaryId);
        if (!builder) return;
        startBuild(game, builder->owner, builder->id, command.entityType, command.targetTile);
        break;
    }
    case CommandType::BuildLine: {
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (!inBounds(command.lineEnd.x, command.lineEnd.y)) return;
        Entity* builder = findEntity(command.selection.primaryId);
        if (!builder) return;
        startBuildLine(game, builder->owner, builder->id, command.entityType,
                       command.targetTile, command.lineEnd);
        break;
    }
    case CommandType::Train: {
        Entity* building = findEntity(command.selection.primaryId);
        if (!building) return;
        startTraining(game, building->owner, building->id, command.entityType);
        break;
    }
    case CommandType::Research: {
        Entity* building = findEntity(command.selection.primaryId);
        if (!building) return;
        startResearch(game, building->owner, building->id, command.researchId);
        break;
    }
    case CommandType::MarketTrade: {
        Entity* market = findEntity(command.selection.primaryId);
        if (!market) return;
        executeTrade(game, market->owner, market->id, command.marketTrade);
        break;
    }
    case CommandType::SetRally: {
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        Entity* selected = findEntity(command.selection.primaryId);
        if (selected && selected->alive && selected->owner == 0) {
            selected->rallyX = command.targetTile.x;
            selected->rallyY = command.targetTile.y;
            selected->rallySet = 1;
            emitStatusEvent(selected->owner, "Rally point set.");
        }
        break;
    }
    case CommandType::AttackMove:
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (command.selection.ids.size() > 1) {
            orderGroupAttackMove(command.selection, command.targetTile.x, command.targetTile.y);
        } else {
            Entity* selected = findEntity(command.selection.primaryId);
            if (selected && selected->alive && selected->owner == 0 && isUnit(selected->type)) {
                orderMove(*selected, command.targetTile.x, command.targetTile.y);
                selected->attackMove = 1;
                emitStatusEvent(selected->owner, "Attack-moving.");
            }
        }
        break;
    case CommandType::Move:
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (command.selection.ids.size() > 1) {
            orderGroupMove(command.selection, command.targetTile.x, command.targetTile.y);
        } else {
            Entity* selected = findEntity(command.selection.primaryId);
            if (selected && selected->alive && selected->owner == 0 && isUnit(selected->type))
                orderMove(*selected, command.targetTile.x, command.targetTile.y);
        }
        break;
    case CommandType::Attack:
        if (command.selection.ids.size() > 1) {
            orderGroupAttack(command.selection, command.targetEntity);
        } else {
            Entity* selected = findEntity(command.selection.primaryId);
            if (selected && selected->alive && selected->owner == 0 && isUnit(selected->type))
                orderAttack(*selected, command.targetEntity);
        }
        break;
    case CommandType::Gather: {
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        Entity* selected = findEntity(command.selection.primaryId);
        if (selected && selected->alive && selected->owner == 0 && canGather(selected->type))
            orderGather(*selected, command.targetTile.x, command.targetTile.y);
        break;
    }
    case CommandType::Garrison: {
        Entity* target = findEntity(command.targetEntity);
        if (!target || !target->alive || target->owner != 0 || target->underConstruction
            || !canGarrisonIn(target->type)) return;
        for (int id : command.selection.ids) {
            Entity* selected = findEntity(id);
            if (selected && selected->alive && selected->owner == 0 && isUnit(selected->type)
                && !isSiege(selected->type))
                orderGarrison(*selected, target->id);
        }
        break;
    }
    case CommandType::EjectGarrison: {
        Entity* selected = findEntity(command.selection.primaryId);
        if (selected && selected->alive && selected->owner == 0 && canGarrisonIn(selected->type))
            ejectGarrison(*selected);
        break;
    }
    case CommandType::HoldPosition: {
        for (int id : command.selection.ids) {
            Entity* selected = findEntity(id);
            if (!selected || !selected->alive || selected->owner != 0 || !isUnit(selected->type)) continue;
            selected->state = S_IDLE;
            selected->path.clear();
            selected->pathIdx = 0;
            selected->attackMove = 0;
            selected->holdPosition = 1;
            selected->targetId = -1;
        }
        break;
    }
    case CommandType::Stop: {
        for (int id : command.selection.ids) {
            Entity* selected = findEntity(id);
            if (!selected || !selected->alive || selected->owner != 0 || !isUnit(selected->type)) continue;
            selected->state = S_IDLE;
            selected->path.clear();
            selected->pathIdx = 0;
            selected->attackMove = 0;
            selected->holdPosition = 0;
            selected->targetId = -1;
        }
        break;
    }
    // The following command types are not (yet) routed through the dispatcher;
    // they are handled directly by the input/render layers. They are listed
    // explicitly (rather than via a default case) so that adding a new
    // CommandType produces a compiler warning here until it is handled.
    case CommandType::None:
    case CommandType::AssignControlGroup:
    case CommandType::RecallControlGroup:
    case CommandType::TogglePause:
    case CommandType::Save:
    case CommandType::Load:
    case CommandType::Resign:
        break;
    }
}
