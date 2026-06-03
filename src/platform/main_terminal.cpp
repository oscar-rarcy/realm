#include "realm.h"
#include "view_state.h"
#include "env_config.h"
#include "entity_animation.h"
#include "input_keys.h"

#include <chrono>
#include <iostream>

#ifndef USE_SDL_RENDERER
// Full splash screen. Sets g.biomeChoice and displayMode. Returns numAIs.
static int showSplash() {
    static const char* biomeNames[] = {
        "Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"
    };
    int numAIs = 1;
    int biomeIdx = 7; // 7 = random
    const bool asciiOnly = realmVisualModeIsAsciiOnly();

    int maxY, maxX;
    while (true) {
        getmaxyx(stdscr, maxY, maxX);
        erase();

        int col = std::max(2, maxX/2 - 34);
        int row = std::max(0, maxY/2 - 15);

        auto pr = [&](int r, int c, const char* fmt, ...) {
            va_list ap; va_start(ap, fmt);
            char buf[256]; vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
            mvprintw(r, c, "%s", buf);
        };

        attron(A_BOLD);
        pr(row,   col+17, "R  E  A  L  M");
        pr(row+1, col+13, "-- Medieval Warlord --");
        attroff(A_BOLD);

        row += 3;
        pr(row++, col, "You are lord of a small settlement in a hostile");
        pr(row++, col, "realm. Gather resources, build an army, and");
        pr(row++, col, "outlast every rival. Survive winter or starve.");

        row++;
        attron(A_BOLD); pr(row++, col, "CONTROLS"); attroff(A_BOLD);
        pr(row++, col, "  Space/click    Select unit or building");
        pr(row++, col, "  Enter/R-click  Command (move/attack/gather)");
        pr(row++, col, "  B              Build menu");
        pr(row++, col, "  T              Train units");
        pr(row++, col, "  A              Select all military");
        pr(row++, col, "  H              Jump to town hall");
        pr(row++, col, "  1-9 / G        Control groups");
        pr(row++, col, "  P              Pause");

        row++;
        attron(A_BOLD); pr(row++, col, "TIPS"); attroff(A_BOLD);
        pr(row++, col, "  Stockpile food before winter (1 food/unit/8s).");
        pr(row++, col, "  Boars fight back. Wolves hunt in winter.");
        pr(row++, col, "  Catapults need 2+ tiles of standoff to fire.");

        row++;
        attron(A_BOLD); pr(row++, col, "OPPONENTS"); attroff(A_BOLD);
        pr(row++, col, "  [1] Duel       [2] Three-way     [3] Four-way");

        row++;
        attron(A_BOLD); pr(row++, col, "BIOME"); attroff(A_BOLD);
        pr(row++, col, "  [0] Random    [T] Temperate  [D] Desert");
        pr(row++, col, "  [S] Snow      [W] Swamp      [F] Forest");
        pr(row++, col, "  [C] Coastal");

        if (!asciiOnly) {
            row++;
            attron(A_BOLD); pr(row++, col, "DISPLAY"); attroff(A_BOLD);
            pr(row++, col, "  [4] ASCII     [5] Emoji");
            pr(row++, col, "  > Display: %s", displayMode == DM_EMOJI ? "Emoji" : "ASCII");
        }

        row++;
        attron(A_BOLD);
        pr(row++, col, "  > Opponents: %d    Biome: %s", numAIs, biomeNames[biomeIdx]);
        attroff(A_BOLD);

        row++;
        pr(row, col, "  [Enter] Start game            [Q/X] Quit");

        refresh();
        int ch = getch();
        if (ch=='q'||ch=='Q'||ch=='x'||ch=='X') { endwin(); exit(0); }
        if (ch=='\n'||ch==KEY_ENTER||ch=='\r') break;
        if (ch=='1') numAIs=1;
        else if (ch=='2') numAIs=2;
        else if (ch=='3') numAIs=3;
        else if (ch=='0') biomeIdx=7;
        else if (ch=='t'||ch=='T') biomeIdx=0;
        else if (ch=='d'||ch=='D') biomeIdx=1;
        else if (ch=='s'||ch=='S') biomeIdx=2;
        else if (ch=='w'||ch=='W') biomeIdx=3;
        else if (ch=='f'||ch=='F') biomeIdx=4;
        else if (ch=='c'||ch=='C') biomeIdx=6;
        else if (ch=='4') displayMode = DM_ASCII;
        else if (ch=='5' && !asciiOnly) displayMode = DM_EMOJI;
    }
    g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
    return numAIs;
}
#endif

#ifndef USE_SDL_RENDERER
int main(int argc, char** argv) {
    forceUtf8Locale();
    if (argc >= 2 && std::strcmp(argv[1], "--dump-missing-tileset-assets") == 0) {
        return dumpMissingTilesetAssets();
    }
    if (argc >= 2 && std::strcmp(argv[1], "--dump-animation-spec") == 0) {
        const char* entityArg = argc >= 3 ? argv[2] : "peasant";
        EntityType type = entityTypeForAnimationSlug(entityArg);
        if (!writeEntityAnimationSpecJson(std::cout, type)) {
            std::cerr << "unknown entity animation spec: " << entityArg << "\n";
            return 2;
        }
        return 0;
    }
    loadRealmEnvironmentFiles();
    displayMode = DM_ASCII;
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    // REPORT_MOUSE_POSITION gives continuous hover events for live cursor tracking
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    initColors();

    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;

    while (true) {
        int numAIs = showSplash();
        initGame(numAIs);
        setStatus("Dawn breaks over the realm. Select peasants [Space] and gather [Enter]. [A]=select all military.");

        auto nextTick = Clock::now() + Ms(TICK_MS);
        int lastCx = view.cursorX, lastCy = view.cursorY;
        bool lastDrag = view.dragging;

        while (!g.returnToMenu) {
            // Block only as long as needed to reach the next game tick
            int wait = (int)std::chrono::duration_cast<Ms>(nextTick - Clock::now()).count();
            timeout(std::max(0, wait));
            int ch = getch();
            handleInput(ch);

            // Drain any events that piled up (mouse moves, key repeats) without
            // running game logic for each one — keeps the cursor smooth
            timeout(0);
            int extra;
            while ((extra = getch()) != ERR) handleInput(extra);

            // Tick and render at fixed rate regardless of input volume
            bool ticked = false;
            if (Clock::now() >= nextTick) {
                nextTick += Ms(TICK_MS);
                if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
                    tickSimulationOnce(g, true);
                }
                render();
                ticked = true;
            }
            // Snappy cursor: redraw between ticks when the mouse moved or a drag updated.
            bool cursorMoved = (view.cursorX != lastCx || view.cursorY != lastCy || view.dragging != lastDrag);
            if (!ticked && cursorMoved) render();
            lastCx = view.cursorX; lastCy = view.cursorY; lastDrag = view.dragging;
        }
        // returnToMenu set — loop back to splash.
    }
    endwin();
    return 0;
}
#endif
