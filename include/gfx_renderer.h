#pragma once

// Optional SDL2/SDL_ttf frontend. The normal ncurses build does not include
// this file. Build with `make gfx`, run `./realm-gfx`.

bool gfxInit();
void gfxShutdown();
int  gfxShowSplash();
void gfxOnNewGame();
void gfxPollInput(bool& quitRequested);
void gfxRender();
void gfxDelay(int ms);
