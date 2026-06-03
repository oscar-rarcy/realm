#pragma once

#include "render/sdl/sdl_capture.h"

bool handleSplashKeyHit(int px, int py, int& numAIs, int& biomeIdx, int& result);
bool handleMobileSplashTap(int px, int py, int& numAIs, int& biomeIdx, bool& done);
int applySplashChoice(int ch, int& numAIs, int& biomeIdx);
void drawSplash(int numAIs, int biomeIdx);
void drawHelpOverlay();
