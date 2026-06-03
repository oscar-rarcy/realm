#pragma once

#include "core/game_types.h"

#include <string>
#include <vector>

struct Game;

enum class ValidationSeverity { Recoverable, Error };
struct ValidationIssue {
    ValidationSeverity severity;
    std::string code;
    std::string message;
    int entityId = -1;
    MapPos tile{ -1, -1 };
};
struct RecoveryResult {
    bool recovered = false;
    int issuesProcessed = 0;
    std::vector<ValidationIssue> remainingIssues;
};

std::vector<ValidationIssue> validateGameStateIssues(const Game& game);
RecoveryResult recoverGameState(Game& game, const std::vector<ValidationIssue>& issues);
bool validateGameState(const Game& game, std::string* error);
bool validateMapInvariants(const Game& game, std::string* error);
