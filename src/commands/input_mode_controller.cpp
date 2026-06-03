#include "commands/input_mode_controller.h"
#include "core/entity_defs.h"
#include "core/game_state_types.h"
#include "core/production_service.h"

void setInputMode(Game& game, GameMode mode) {
    game.mode = mode;
}

void cancelInputMode(Game& game) {
    if (game.mode == M_BUILD_SELECT || game.mode == M_BUILD_PLACE || game.mode == M_WALL_DRAG)
        game.local.buildPending = E_NONE;
    setInputMode(game, M_NORMAL);
}

bool isInputBlockedByMode(GameMode mode) {
    return mode == M_PAUSED || mode == M_GAME_OVER;
}

void startWallBuildMode(Game& game) {
    game.local.buildPending = E_WALL;
    setInputMode(game, M_WALL_DRAG);
}

bool toggleHelpOverlay(Game& game) {
    game.local.helpOverlay = !game.local.helpOverlay;
    return game.local.helpOverlay;
}

bool controlGroupAssignmentPending(const Game& game) {
    return game.local.groupAssignPending;
}

static const Entity* selectedEntity(const Game& game) {
    for (const Entity& entity : game.entities) {
        if (entity.alive && entity.id == game.local.selectedId) return &entity;
    }
    return nullptr;
}

std::optional<MapPos> selectedEntityPosition(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    if (!entity || entity->owner != issuer) return std::nullopt;
    return MapPos{ entity->x, entity->y };
}

bool selectedPeasantCanBuild(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    return entity && entity->alive && entity->owner == issuer && entity->type == E_PEASANT;
}

bool selectedProducerCanTrain(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    if (!entity || !entity->alive || entity->owner != issuer || entity->underConstruction) return false;
    return productionRule(entity->type) != nullptr;
}

std::optional<EntityType> selectedTrainProducerType(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    if (!entity || !selectedProducerCanTrain(game, issuer)) return std::nullopt;
    return entity->type;
}

InputTrainMenuEligibility trainMenuEligibilityForSelected(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    if (selectedProducerCanTrain(game, issuer)) return InputTrainMenuEligibility::CanTrain;
    if (entity && entity->alive && entity->owner == issuer && isBuilding(entity->type) && !entity->underConstruction) {
        return InputTrainMenuEligibility::UnsupportedBuilding;
    }
    return InputTrainMenuEligibility::NeedProductionBuilding;
}

bool selectedMarketCanTrade(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    return entity && entity->alive && entity->owner == issuer && entity->type == E_MARKET && !entity->underConstruction;
}

bool selectedBlacksmithCanResearch(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    return entity && entity->alive && entity->owner == issuer && entity->type == E_BLACKSMITH && !entity->underConstruction;
}

bool selectedTrebuchetCanToggle(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    return entity && entity->alive && entity->owner == issuer && entity->type == E_TREBUCHET;
}

InputUtilityMode utilityModeForSelectedBuilding(const Game& game, PlayerId issuer) {
    const Entity* entity = selectedEntity(game);
    if (!entity || !entity->alive || entity->owner != issuer || !isBuilding(entity->type) || entity->underConstruction) {
        return InputUtilityMode::None;
    }
    if (entity->type == E_MARKET) return InputUtilityMode::MarketTrade;
    if (entity->type == E_BLACKSMITH) return InputUtilityMode::Research;
    if (productionRule(entity->type)) {
        return InputUtilityMode::Rally;
    }
    return InputUtilityMode::None;
}
