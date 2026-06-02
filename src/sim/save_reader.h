#pragma once

#include <string>

struct Game;

struct SaveHeaderInfo {
    bool ok = false;
    int version = 0;
    std::string error;
};

SaveHeaderInfo inspectSaveHeader(const std::string& path);
bool readSaveFile(const std::string& path, Game& game, int& version);
