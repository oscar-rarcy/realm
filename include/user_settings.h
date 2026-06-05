#pragma once

struct Game;

struct UserSettings {
    int playerColorHue = 200;
    bool asciiSquareMapCells = true;
};

UserSettings defaultUserSettings();
UserSettings loadUserSettings();
bool saveUserSettings(const UserSettings& settings);
void applyUserSettingsToGame(Game& game, const UserSettings& settings);
UserSettings userSettingsFromGame(const Game& game);
bool saveUserSettingsFromGame(const Game& game);
