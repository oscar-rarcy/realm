#include "command.h"
#include "core/build_service.h"
#include "core/game_events.h"
#include "core/production_service.h"
#include "core/research_service.h"
#include "core/market_service.h"

#include <iostream>

static std::string saveSlotPath(int slot) {
    return slot > 0 ? "realm-slot" + std::to_string(slot) + ".sav" : "realm-save.txt";
}

void dispatchCommand(GameContext& context, const Command& command) {
    Game& game = context.game;
    (void)context;
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
        boxSelect(game, command.targetTile.x, command.targetTile.y, command.boxEnd.x, command.boxEnd.y);
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
        if (selected && selected->alive && selected->owner == 0 && canGarrisonIn(selected->type)) {
            int n = (int)selected->garrison.size();
            if (n > 0) {
            ejectGarrison(*selected);
                emitStatusEvent(0, std::to_string(n) + " unit(s) ejected");
            } else {
                emitStatusEvent(0, "No garrison to eject", GameEventType::CommandRejected);
            }
        }
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
        if (!command.selection.ids.empty()) emitStatusEvent(0, "Hold position.");
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
    case CommandType::AssignControlGroup:
        if (command.slot >= 0 && command.slot < 9 && !command.selection.ids.empty()) {
            game.controlGroups[command.slot] = command.selection.ids;
            game.groupAssignPending = false;
            emitStatusEvent(0, "Group " + std::to_string(command.slot + 1) + " assigned ("
                            + std::to_string(command.selection.ids.size()) + " units)");
        }
        break;
    case CommandType::RecallControlGroup:
        if (command.slot >= 0 && command.slot < 9) {
            if (game.controlGroups[command.slot].empty()) {
                emitStatusEvent(0, "Group " + std::to_string(command.slot + 1) + " is empty",
                                GameEventType::CommandRejected);
            } else {
                game.selectedIds = game.controlGroups[command.slot];
                game.selectedId = -1;
                for (int id : game.selectedIds) {
                    Entity* entity = findEntity(id);
                    if (entity && entity->alive) { game.selectedId = entity->id; break; }
                }
                emitStatusEvent(0, "Group " + std::to_string(command.slot + 1) + " recalled ("
                                + std::to_string(game.selectedIds.size()) + " units)");
            }
        }
        break;
    case CommandType::TogglePause:
        if (game.mode == M_NORMAL || game.mode == M_PAUSED)
            game.mode = game.mode == M_PAUSED ? M_NORMAL : M_PAUSED;
        break;
    case CommandType::Save: {
        std::string path = saveSlotPath(command.slot);
        bool ok = saveGame(path);
        if (ok) {
            emitStatusEvent(0, command.slot > 0 ? "Saved to slot " + std::to_string(command.slot) + "."
                                                : "Saved realm-save.txt");
            std::cerr << "realm: saved " << path << " tick=" << game.tick << "\n";
        } else {
            emitStatusEvent(0, "Save failed.", GameEventType::CommandRejected);
            std::cerr << "realm: save failed " << path << " tick=" << game.tick << "\n";
        }
        break;
    }
    case CommandType::Load: {
        std::string path = saveSlotPath(command.slot);
        bool ok = loadGame(path);
        if (ok) {
            emitStatusEvent(0, command.slot > 0 ? "Loaded slot " + std::to_string(command.slot) + "."
                                                : "Loaded realm-save.txt");
            std::cerr << "realm: loaded " << path << " tick=" << game.tick << "\n";
        } else {
            emitStatusEvent(0, "Load failed.", GameEventType::CommandRejected);
            std::cerr << "realm: load failed " << path << "\n";
        }
        break;
    }
    case CommandType::Resign:
        game.returnToMenu = true;
        std::cerr << "realm: resign/return-to-menu requested tick=" << game.tick << "\n";
        break;
    case CommandType::ToggleGate: {
        Entity* selected = findEntity(command.selection.primaryId);
        if (selected && selected->alive && selected->owner == 0 && selected->type == E_GATE && !selected->underConstruction) {
            if (!selected->gateLocked) {
                selected->gateLocked = true;
                selected->gateOpen = true;
                emitStatusEvent(0, "Gate locked open");
            } else if (selected->gateOpen) {
                selected->gateOpen = false;
                emitStatusEvent(0, "Gate locked closed");
            } else {
                selected->gateLocked = false;
                emitStatusEvent(0, "Gate auto");
            }
        }
        break;
    }
    case CommandType::ToggleTrebuchetPacked: {
        Entity* selected = findEntity(command.selection.primaryId);
        if (selected && selected->alive && selected->owner == 0 && selected->type == E_TREBUCHET) {
            if (selected->packTicks > 0) {
                emitStatusEvent(0, "Trebuchet is already changing stance.", GameEventType::CommandRejected);
                break;
            }
            int ticks = (game.players[0].research & R_COUNTERWEIGHT) ? 25 : 40;
            selected->packed = selected->packed ? 0 : 1;
            selected->packTicks = ticks;
            selected->state = S_IDLE;
            selected->path.clear();
            selected->pathIdx = 0;
            emitStatusEvent(0, selected->packed ? "Packing trebuchet..." : "Deploying trebuchet...");
        }
        break;
    }
    case CommandType::ToggleDiagnostics:
        game.diagnostics = !game.diagnostics;
        emitStatusEvent(0, game.diagnostics ? "Diagnostics on." : "Diagnostics off.");
        break;
    case CommandType::RevealMapDebug:
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            game.map[y][x].visible[0] = true;
            game.map[y][x].explored[0] = true;
        }
        emitStatusEvent(0, "Debug: map revealed");
        break;
    // The following command types are not (yet) routed through the dispatcher;
    // they are handled directly by the input/render layers. They are listed
    // explicitly (rather than via a default case) so that adding a new
    // CommandType produces a compiler warning here until it is handled.
    case CommandType::None:
        break;
    }
}

void dispatchCommand(Game& game, const Command& command) {
    GameContext context = legacyGameContext(game);
    dispatchCommand(context, command);
}
