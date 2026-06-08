#pragma once

// Optional SDL2/SDL_ttf frontend. The normal ncurses build does not include
// this file. Build with `make gfx`, run `./realm-gfx`.

#include <string>

bool gfxInit();
void gfxShutdown();
int  gfxShowSplash();
int  gfxSplashFrame(int& numAIs, int& biomeIdx);
void gfxSetAsciiOnly(bool asciiOnly);
void gfxSetViewportOnly(bool viewportOnly);
void gfxSetEdgeScrollEnabled(bool enabled);
bool gfxConsumeLoadGameRequest();
void gfxOnNewGame();
void gfxPollInput(bool& quitRequested);
void gfxRender();
void gfxRenderNoPresentForTest();
void gfxDelay(int ms);
void gfxResetZoomForDisplayMode();
void gfxSetAsciiSquareMapCells(bool enabled);
void gfxSetProjection(bool isometric);
void gfxSetZoomForTest(int tilePx);
void gfxSetZoomAnchoredForTest(int tilePx, int anchorX, int anchorY);
bool gfxMapTileAtScreenForTest(int px, int py, int& mx, int& my);
bool gfxScreenCenterForMapTileForTest(int mx, int my, int& px, int& py);
void gfxSetWindowSizeForTest(int width, int height);
bool gfxSaveScreenshot(const std::string& path);
bool gfxSaveAsciiTerminalReference(const std::string& path);
bool gfxSaveAsciiTerminalText(const std::string& path);
bool gfxSaveSplashScreenshot(const std::string& path, int numAIs, int biomeIdx);
int  gfxRunTilesetLab();
