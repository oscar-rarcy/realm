#include "realm.h"
#include "gfx_renderer.h"
#include "env_config.h"

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>

int main(int, char**) {
    std::freopen("realm-lab.log", "w", stderr);
    std::cerr << "realm: lab process started\n";

    forceUtf8Locale();
    loadRealmEnvironmentFiles();
    displayMode = DM_EMOJI;

    if (!gfxInit()) return 1;
    gfxSetAsciiOnly(false);
    gfxSetProjection(true);
    std::cerr << "realm: lab gfxInit ok\n";

    int code = gfxRunTilesetLab();
    gfxShutdown();
    return code;
}
