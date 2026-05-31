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
void gfxSetZoomAnchoredForTest(int tilePx, int anchorX, int anchorY);
bool gfxMapTileAtScreenForTest(int px, int py, int& mx, int& my);
void gfxSetWindowSizeForTest(int width, int height);
bool gfxSaveScreenshot(const std::string& path);
