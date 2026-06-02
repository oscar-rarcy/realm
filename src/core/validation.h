#pragma once

#include "realm.h"

std::vector<ValidationIssue> validateGameStateIssues();
std::vector<ValidationIssue> validateGameStateIssues(const Game& game);
RecoveryResult recoverGameState(Game& game, const std::vector<ValidationIssue>& issues);
bool validateGameState(std::string* error);
bool validateMapInvariants(const Game& game, std::string* error);
