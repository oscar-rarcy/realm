#pragma once

#include "render/sdl/sdl_map.h"

std::vector<MobileButton> mobileHudButtons(const WorldIndex& world);
bool mobileHasSelectedWorker(const WorldIndex& world);
bool mobileHasSelectedMilitary(const WorldIndex& world);
bool isTrainProducer(EntityType t);
std::string trainPromptFor(const Entity* sel);
std::vector<std::pair<std::string, int>> trainOptionTokensFor(EntityType t);
std::vector<std::pair<std::string, int>> terminalBuildTokens();
std::string mobileSelectionSummary(const WorldIndex& world);
EntityType mobileDefaultTrainType(EntityType producer);
void drawMobileHud(const WorldIndex& world);
void drawTopBar();
void drawMiniMap(const WorldIndex& world, int x, int y, int w, int h);
void mobileDrawResources(int x, int y, int w);
void drawButton(const MobileButton& b, bool active = false, bool danger = false);
void drawConsoleButton(const MobileButton& b, bool active = false, bool danger = false);
void addGridButtons(std::vector<MobileButton>& out, int x, int y, int w,
                    const std::vector<std::pair<std::string, std::string>>& items,
                    int cols = 3);
void drawPanel(const WorldIndex& world);
void drawBottom(const WorldIndex& world);
bool devCaptureEnabled();
bool tilesetHudEnabled();
SDL_Rect tilesetHudOverlayRect();
SDL_Rect tilesetHudMiniMapRect();
bool tilesetHudConsumesPointer(int px, int py);
bool tilesetHudClickableAt(int px, int py);
void drawTilesetHud(const WorldIndex& world);
void clearTilesetHudCaches();
