#include "command.h"
#include "core/entity_defs.h"
#include "core/entity_query.h"
#include "core/game_state_types.h"
#include "core/order_service.h"
#include "core/rng.h"
#include "core/terrain_defs.h"
#include "core/world_index.h"

#include <algorithm>
#include <utility>

namespace {

const Entity* selectedEntity(const Game& game, const WorldIndex& world, const Selection& selection) {
    return entityById(game, world, selection.primaryId);
}

const Entity* corpseAtTile(const Game& game, const WorldIndex& world, MapPos tile) {
    for (EntityId id : entitiesAt(world, tile)) {
        const Entity* entity = entityByIdAny(game, world, id);
        if (entity && !entity->alive && isHarvestableCarcass(*entity)) return entity;
    }
    return nullptr;
}

template <typename Payload>
Command makeCommand(PlayerId issuer, Payload payload) {
    Command command;
    command.issuer = issuer;
    command.payload = std::move(payload);
    return command;
}

bool samePos(MapPos a, MapPos b) {
    return a.x == b.x && a.y == b.y;
}

bool sameCommandTarget(const Command& a, const Command& b) {
    CommandActionKind kind = commandActionKind(a);
    if (kind != commandActionKind(b)) return false;

    switch (kind) {
        case CommandActionKind::Move:
            return samePos(std::get<MoveCommand>(a.payload).target, std::get<MoveCommand>(b.payload).target);
        case CommandActionKind::Waypoint:
            return samePos(std::get<WaypointCommand>(a.payload).target, std::get<WaypointCommand>(b.payload).target);
        case CommandActionKind::Patrol:
            return samePos(std::get<PatrolCommand>(a.payload).target, std::get<PatrolCommand>(b.payload).target);
        case CommandActionKind::Attack:
            return std::get<AttackCommand>(a.payload).targetId == std::get<AttackCommand>(b.payload).targetId;
        case CommandActionKind::AttackMove:
            return samePos(std::get<AttackMoveCommand>(a.payload).target, std::get<AttackMoveCommand>(b.payload).target);
        case CommandActionKind::Gather:
            return samePos(std::get<GatherCommand>(a.payload).target, std::get<GatherCommand>(b.payload).target);
        case CommandActionKind::Build: {
            const BuildCommand& lhs = std::get<BuildCommand>(a.payload);
            const BuildCommand& rhs = std::get<BuildCommand>(b.payload);
            return lhs.entityType == rhs.entityType && samePos(lhs.target, rhs.target);
        }
        case CommandActionKind::BuildLine: {
            const BuildLineCommand& lhs = std::get<BuildLineCommand>(a.payload);
            const BuildLineCommand& rhs = std::get<BuildLineCommand>(b.payload);
            return lhs.entityType == rhs.entityType && samePos(lhs.start, rhs.start) && samePos(lhs.end, rhs.end);
        }
        case CommandActionKind::Help:
            return std::get<HelpCommand>(a.payload).targetId == std::get<HelpCommand>(b.payload).targetId;
        case CommandActionKind::Garrison:
            return std::get<GarrisonCommand>(a.payload).targetId == std::get<GarrisonCommand>(b.payload).targetId;
        case CommandActionKind::SetRally:
            return samePos(std::get<SetRallyCommand>(a.payload).target, std::get<SetRallyCommand>(b.payload).target);
        case CommandActionKind::Select:
            return samePos(std::get<SelectCommand>(a.payload).target, std::get<SelectCommand>(b.payload).target);
        case CommandActionKind::SelectAllOfTypeInView:
            return samePos(std::get<SelectAllOfTypeInViewCommand>(a.payload).target, std::get<SelectAllOfTypeInViewCommand>(b.payload).target);
        default:
            return true;
    }
}

void addOption(CommandOptions& options, Command command, bool recommended = false, bool enabled = true, std::string disabledReason = {}) {
    CommandActionKind kind = commandActionKind(command);
    if (kind == CommandActionKind::None || kind == CommandActionKind::Context || kind == CommandActionKind::Invalid) return;
    for (CommandOption& existing : options.options) {
        if (!sameCommandTarget(existing.command, command)) continue;
        existing.recommended = existing.recommended || recommended;
        existing.enabled = existing.enabled || enabled;
        if (existing.disabledReason.empty()) existing.disabledReason = std::move(disabledReason);
        return;
    }

    CommandOption option;
    option.kind = kind;
    option.command = std::move(command);
    option.label = commandActionKindName(kind);
    option.priority = commandActionPriority(kind);
    option.recommended = recommended;
    option.enabled = enabled;
    option.disabledReason = std::move(disabledReason);
    options.options.push_back(std::move(option));
}

void finalizeOptions(CommandOptions& options) {
    std::stable_sort(options.options.begin(), options.options.end(), [](const CommandOption& a, const CommandOption& b) {
        if (a.enabled != b.enabled) return a.enabled;
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.label < b.label;
    });
    options.recommendedIndex = -1;
    for (int i = 0; i < (int)options.options.size(); ++i) {
        if (options.options[i].recommended && options.options[i].enabled) {
            options.recommendedIndex = i;
            break;
        }
    }
    if (options.recommendedIndex < 0) {
        for (int i = 0; i < (int)options.options.size(); ++i) {
            if (options.options[i].enabled) {
                options.options[i].recommended = true;
                options.recommendedIndex = i;
                break;
            }
        }
    }
}

bool selectionCanReceiveMapOrder(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection) {
    if (selection.ids.size() > 1) return true;
    const Entity* sel = selectedEntity(game, world, selection);
    return sel && sel->owner == issuer && isUnit(sel->type);
}

Command typedContextCommand(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    if (!inBounds(target.x, target.y)) return Command{};
    Command command;
    command.issuer = issuer;

    const Entity* tgt = entityById(game, world, topEntityAt(game, world, target));
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && game.map[target.y][target.x].visible[issuer];

    if (selection.ids.size() > 1) {
        if (tgt && tgt->alive && tgt->owner == issuer
            && ((tgt->underConstruction && isBuilding(tgt->type)) || (tgt->type == E_FARM && !tgt->underConstruction))) {
            command.payload = HelpCommand{ selection, tgt->id };
            return command;
        }
        if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
            command.payload = GarrisonCommand{ selection, tgt->id };
            return command;
        }
        if (tgt && tgt->alive && tgt->owner != issuer && visible) {
            command.payload = AttackCommand{ selection, tgt->id };
            return command;
        }
        command.payload = MoveCommand{ selection, target };
        return command;
    }

    const Entity* sel = selectedEntity(game, world, selection);
    if (!sel || sel->owner != issuer || !isUnit(sel->type)) return Command{};
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->underConstruction && sel->type == E_PEASANT) {
        command.payload = HelpCommand{ selection, tgt->id };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        command.payload = HelpCommand{ selection, tgt->id };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction
        && canGarrisonIn(tgt->type) && !isSiege(sel->type)) {
        command.payload = GarrisonCommand{ selection, tgt->id };
        return command;
    }
    if (tgt && tgt->alive && tgt->owner != issuer && visible) {
        command.payload = AttackCommand{ selection, tgt->id };
        return command;
    }
    if (sel->type == E_PEASANT) {
        const Entity* carcass = corpseAtTile(game, world, target);
        if (carcass && isHarvestableCarcass(*carcass)) {
            command.payload = GatherCommand{ selection, target };
            return command;
        }
        Terrain terrain = game.map[target.y][target.x].terrain;
        if (terrainHasDirectGatherResource(terrain)
            && !terrainMatchesResource(terrain, CR_FISH)
            && game.map[target.y][target.x].resources > 0) {
            command.payload = GatherCommand{ selection, target };
            return command;
        }
        if (terrain == T_WHEAT && !tgt && canPlace(game, world, E_FARM, target.x, target.y, issuer)) {
            command.payload = BuildCommand{ selection, E_FARM, target };
            return command;
        }
        command.payload = MoveCommand{ selection, target };
        return command;
    }
    if (sel->type == E_FISHING_BOAT) {
        Terrain terrain = game.map[target.y][target.x].terrain;
        command.payload = (terrainMatchesResource(terrain, CR_FISH) && game.map[target.y][target.x].resources > 0)
            ? CommandPayload{ GatherCommand{ selection, target } }
            : CommandPayload{ MoveCommand{ selection, target } };
        return command;
    }
    command.payload = MoveCommand{ selection, target };
    return command;
}

CommandOptions typedContextOptions(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    CommandOptions options;
    if (!inBounds(target.x, target.y)) return options;

    Command recommended = typedContextCommand(game, world, issuer, selection, target);
    addOption(options, recommended, true);

    const Entity* tgt = entityById(game, world, topEntityAt(game, world, target));
    bool visible = issuer >= 0 && issuer < MAX_PLAYERS && game.map[target.y][target.x].visible[issuer];
    bool canMapOrder = selectionCanReceiveMapOrder(game, world, issuer, selection);

    if (selection.ids.size() > 1) {
        if (tgt && tgt->alive && tgt->owner == issuer
            && ((tgt->underConstruction && isBuilding(tgt->type)) || (tgt->type == E_FARM && !tgt->underConstruction))) {
            addOption(options, makeCommand(issuer, HelpCommand{ selection, tgt->id }));
        }
        if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
            addOption(options, makeCommand(issuer, GarrisonCommand{ selection, tgt->id }));
        }
        if (tgt && tgt->alive && tgt->owner != issuer && visible) {
            addOption(options, makeCommand(issuer, AttackCommand{ selection, tgt->id }));
        }
        addOption(options, makeCommand(issuer, MoveCommand{ selection, target }));
        finalizeOptions(options);
        return options;
    }

    const Entity* sel = selectedEntity(game, world, selection);
    if (!sel || sel->owner != issuer || !isUnit(sel->type)) {
        finalizeOptions(options);
        return options;
    }

    if (tgt && tgt->alive && tgt->owner == issuer && tgt->underConstruction && sel->type == E_PEASANT) {
        addOption(options, makeCommand(issuer, HelpCommand{ selection, tgt->id }));
    }
    if (tgt && tgt->alive && tgt->owner == issuer && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        addOption(options, makeCommand(issuer, HelpCommand{ selection, tgt->id }));
    }
    if (tgt && tgt->alive && tgt->owner == issuer && !tgt->underConstruction
        && canGarrisonIn(tgt->type) && !isSiege(sel->type)) {
        addOption(options, makeCommand(issuer, GarrisonCommand{ selection, tgt->id }));
    }
    if (tgt && tgt->alive && tgt->owner != issuer && visible) {
        addOption(options, makeCommand(issuer, AttackCommand{ selection, tgt->id }));
    }
    if (sel->type == E_PEASANT) {
        const Entity* carcass = corpseAtTile(game, world, target);
        if (carcass && isHarvestableCarcass(*carcass)) {
            addOption(options, makeCommand(issuer, GatherCommand{ selection, target }));
        }
        Terrain terrain = game.map[target.y][target.x].terrain;
        if (!tgt && terrainHasDirectGatherResource(terrain)
            && !terrainMatchesResource(terrain, CR_FISH)
            && game.map[target.y][target.x].resources > 0) {
            addOption(options, makeCommand(issuer, GatherCommand{ selection, target }));
        }
        if (terrain == T_WHEAT && !tgt && canPlace(game, world, E_FARM, target.x, target.y, issuer)) {
            addOption(options, makeCommand(issuer, BuildCommand{ selection, E_FARM, target }));
        }
    }
    if (sel->type == E_FISHING_BOAT) {
        Terrain terrain = game.map[target.y][target.x].terrain;
        if (terrainMatchesResource(terrain, CR_FISH) && game.map[target.y][target.x].resources > 0) {
            addOption(options, makeCommand(issuer, GatherCommand{ selection, target }));
        }
    }
    if (canMapOrder) {
        addOption(options, makeCommand(issuer, MoveCommand{ selection, target }));
    }
    finalizeOptions(options);
    return options;
}

} // namespace

const char* commandActionKindName(CommandActionKind kind) {
    switch (kind) {
        case CommandActionKind::None: return "None";
        case CommandActionKind::Context: return "Context";
        case CommandActionKind::Move: return "Move";
        case CommandActionKind::Waypoint: return "Waypoint";
        case CommandActionKind::Patrol: return "Patrol";
        case CommandActionKind::Attack: return "Attack";
        case CommandActionKind::FirePosition: return "Fire position";
        case CommandActionKind::AttackMove: return "Attack move";
        case CommandActionKind::Gather: return "Gather";
        case CommandActionKind::Build: return "Build";
        case CommandActionKind::BuildLine: return "Build line";
        case CommandActionKind::Train: return "Train";
        case CommandActionKind::Research: return "Research";
        case CommandActionKind::MarketTrade: return "Market trade";
        case CommandActionKind::Help: return "Help";
        case CommandActionKind::Garrison: return "Garrison";
        case CommandActionKind::EjectGarrison: return "Eject garrison";
        case CommandActionKind::SetRally: return "Set rally";
        case CommandActionKind::HoldPosition: return "Hold position";
        case CommandActionKind::Stop: return "Stop";
        case CommandActionKind::CancelProduction: return "Cancel production";
        case CommandActionKind::Select: return "Select";
        case CommandActionKind::BoxSelect: return "Box select";
        case CommandActionKind::SelectAllOfTypeInView: return "Select all of type";
        case CommandActionKind::AssignControlGroup: return "Assign control group";
        case CommandActionKind::RecallControlGroup: return "Recall control group";
        case CommandActionKind::TogglePause: return "Toggle pause";
        case CommandActionKind::Save: return "Save";
        case CommandActionKind::Load: return "Load";
        case CommandActionKind::Resign: return "Resign";
        case CommandActionKind::ToggleGate: return "Toggle gate";
        case CommandActionKind::ToggleTrebuchetPacked: return "Toggle trebuchet packed";
        case CommandActionKind::ToggleDiagnostics: return "Toggle diagnostics";
        case CommandActionKind::RevealMapDebug: return "Reveal map";
        case CommandActionKind::Invalid: return "Invalid";
    }
    return "Invalid";
}

int commandActionPriority(CommandActionKind kind) {
    switch (kind) {
        case CommandActionKind::Attack: return 100;
        case CommandActionKind::FirePosition: return 90;
        case CommandActionKind::Help: return 86;
        case CommandActionKind::Garrison: return 84;
        case CommandActionKind::Gather: return 80;
        case CommandActionKind::Build:
        case CommandActionKind::BuildLine: return 76;
        case CommandActionKind::SetRally: return 72;
        case CommandActionKind::AttackMove: return 70;
        case CommandActionKind::Patrol: return 68;
        case CommandActionKind::Waypoint: return 64;
        case CommandActionKind::Move: return 40;
        case CommandActionKind::Select:
        case CommandActionKind::BoxSelect:
        case CommandActionKind::SelectAllOfTypeInView: return 30;
        case CommandActionKind::Train:
        case CommandActionKind::Research:
        case CommandActionKind::MarketTrade:
        case CommandActionKind::EjectGarrison:
        case CommandActionKind::HoldPosition:
        case CommandActionKind::Stop:
        case CommandActionKind::CancelProduction:
        case CommandActionKind::AssignControlGroup:
        case CommandActionKind::RecallControlGroup:
        case CommandActionKind::TogglePause:
        case CommandActionKind::Save:
        case CommandActionKind::Load:
        case CommandActionKind::Resign:
        case CommandActionKind::ToggleGate:
        case CommandActionKind::ToggleTrebuchetPacked:
        case CommandActionKind::ToggleDiagnostics:
        case CommandActionKind::RevealMapDebug: return 20;
        case CommandActionKind::None:
        case CommandActionKind::Context:
        case CommandActionKind::Invalid: return 0;
    }
    return 0;
}

const CommandOption* recommendedCommandOption(const CommandOptions& options) {
    if (options.recommendedIndex < 0 || options.recommendedIndex >= (int)options.options.size()) return nullptr;
    return &options.options[(size_t)options.recommendedIndex];
}

CommandOptions resolveCommandOptions(const Game& game, const WorldIndex& world, const CommandPreviewRequest& request) {
    CommandOptions options;
    if (!inBounds(request.target.x, request.target.y)) return options;

    switch (request.mode) {
        case CommandPreviewMode::Context:
            return typedContextOptions(game, world, request.issuer, request.selection, request.target);
        case CommandPreviewMode::Waypoint:
            addOption(options, makeCommand(request.issuer, WaypointCommand{ request.selection, request.target }), true);
            break;
        case CommandPreviewMode::BuildPlace: {
            bool enabled = request.buildType != E_NONE
                && canPlace(game, world, request.buildType, request.target.x, request.target.y, request.issuer, request.selection.primaryId);
            addOption(options,
                      makeCommand(request.issuer, BuildCommand{ request.selection, request.buildType, request.target }),
                      true,
                      enabled,
                      enabled ? std::string{} : std::string{"Cannot build there."});
            break;
        }
        case CommandPreviewMode::Rally:
            addOption(options, makeCommand(request.issuer, SetRallyCommand{ request.selection, request.target }), true);
            break;
        case CommandPreviewMode::AttackMove:
            addOption(options, makeCommand(request.issuer, AttackMoveCommand{ request.selection, request.target }), true);
            break;
        case CommandPreviewMode::Patrol:
            addOption(options, makeCommand(request.issuer, PatrolCommand{ request.selection, request.target }), true);
            break;
    }
    finalizeOptions(options);
    return options;
}

CommandOptions resolveContextCommandOptions(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    CommandPreviewRequest request;
    request.issuer = issuer;
    request.selection = selection;
    request.target = target;
    request.mode = CommandPreviewMode::Context;
    return resolveCommandOptions(game, world, request);
}

Command resolveRecommendedCommand(const Game& game, const WorldIndex& world, const CommandPreviewRequest& request) {
    CommandOptions options = resolveCommandOptions(game, world, request);
    const CommandOption* option = recommendedCommandOption(options);
    return option ? option->command : Command{};
}

Command resolveContextCommand(const Game& game, const WorldIndex& world, PlayerId issuer, const Selection& selection, MapPos target) {
    CommandOptions options = resolveContextCommandOptions(game, world, issuer, selection, target);
    const CommandOption* option = recommendedCommandOption(options);
    return option ? option->command : Command{};
}
