#pragma once

#include "render/sdl/sdl_map.h"

std::vector<MobileButton> mobileHudButtons();
bool mobileHasSelectedWorker();
bool mobileHasSelectedMilitary();
bool isTrainProducer(EntityType t);
std::string trainPromptFor(const Entity* sel);
std::vector<std::pair<std::string, int>> trainOptionTokensFor(EntityType t);
std::vector<std::pair<std::string, int>> terminalBuildTokens();
std::string mobileSelectionSummary();
EntityType mobileDefaultTrainType(EntityType producer);
void drawMobileHud();
void drawTopBar();
void drawMiniMap(int x, int y, int w, int h);
void mobileDrawResources(int x, int y, int w);
void drawButton(const MobileButton& b, bool active = false, bool danger = false);
void drawConsoleButton(const MobileButton& b, bool active = false, bool danger = false);
void addGridButtons(std::vector<MobileButton>& out, int x, int y, int w,
                    const std::vector<std::pair<std::string, std::string>>& items,
                    int cols = 3);
void drawPanel();
void drawBottom();
bool devCaptureEnabled();
