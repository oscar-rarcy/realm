#include "command.h"
#include "core/research_service.h"
#include "core/market_service.h"

void dispatchCommand(Game& game, const Command& command) {
    switch (command.type) {
    case CommandType::Context:
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (command.selection.ids.size() > 1) cmdAtTileGroup(command.targetTile.x, command.targetTile.y);
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
        orderBuild(*builder, command.entityType, command.targetTile.x, command.targetTile.y);
        break;
    }
    case CommandType::BuildLine: {
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (!inBounds(command.lineEnd.x, command.lineEnd.y)) return;
        Entity* builder = findEntity(command.selection.primaryId);
        if (!builder) return;
        orderBuildLine(*builder, command.entityType,
                       command.targetTile.x, command.targetTile.y,
                       command.lineEnd.x, command.lineEnd.y);
        break;
    }
    case CommandType::Train: {
        Entity* building = findEntity(command.selection.primaryId);
        if (!building) return;
        orderTrain(*building, command.entityType);
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
            setStatus("Rally point set.");
        }
        break;
    }
    case CommandType::AttackMove:
        if (!inBounds(command.targetTile.x, command.targetTile.y)) return;
        if (command.selection.ids.size() > 1) {
            orderGroupAttackMove(command.targetTile.x, command.targetTile.y);
        } else {
            Entity* selected = findEntity(command.selection.primaryId);
            if (selected && selected->alive && selected->owner == 0 && isUnit(selected->type)) {
                orderMove(*selected, command.targetTile.x, command.targetTile.y);
                selected->attackMove = 1;
                setStatus("Attack-moving.");
            }
        }
        break;
    default:
        break;
    }
}
