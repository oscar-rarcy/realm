#pragma once

#include "realm.h"
#include "core/types.h"

#include <optional>

enum class InputUtilityMode {
    None,
    MarketTrade,
    Research,
    Rally,
};

enum class InputTrainMenuEligibility {
    CanTrain,
    UnsupportedBuilding,
    NeedProductionBuilding,
};

void setInputMode(Game& game, GameMode mode);
void cancelInputMode(Game& game);
bool isInputBlockedByMode(GameMode mode);
void startWallBuildMode(Game& game);
bool toggleHelpOverlay(Game& game);
bool controlGroupAssignmentPending(const Game& game);
std::optional<MapPos> selectedEntityPosition(const Game& game, PlayerId issuer);
bool selectedPeasantCanBuild(const Game& game, PlayerId issuer);
bool selectedProducerCanTrain(const Game& game, PlayerId issuer);
std::optional<EntityType> selectedTrainProducerType(const Game& game, PlayerId issuer);
InputTrainMenuEligibility trainMenuEligibilityForSelected(const Game& game, PlayerId issuer);
bool selectedMarketCanTrade(const Game& game, PlayerId issuer);
bool selectedBlacksmithCanResearch(const Game& game, PlayerId issuer);
bool selectedTrebuchetCanToggle(const Game& game, PlayerId issuer);
InputUtilityMode utilityModeForSelectedBuilding(const Game& game, PlayerId issuer);
