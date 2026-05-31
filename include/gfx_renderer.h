#pragma once

// Optional SDL2/SDL_ttf frontend. The normal ncurses build does not include
// this file. Build with `make gfx`, run `./realm-gfx`.

#include <string>

bool gfxInit();
void gfxShutdown();
int  gfxShowSplash();
void gfxOnNewGame();
void gfxPollInput(bool& quitRequested);
void gfxRender();
void gfxDelay(int ms);
void gfxSetProjection(bool isometric);
void gfxSetZoomForTest(int tilePx);
void gfxSetWindowSizeForTest(int width, int height);
bool gfxSaveScreenshot(const std::string& path);
