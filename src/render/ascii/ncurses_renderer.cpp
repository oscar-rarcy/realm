#include "realm.h"
#include "input_keys.h"

static void renderHelpOverlay() {
    if (!g.helpOverlay) return;
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    int w = std::min(maxX - 4, 78);
    int h = std::min(maxY - 4, 24);
    int x = std::max(1, (maxX - w) / 2);
    int y = std::max(1, (maxY - h) / 2);
    attron(COLOR_PAIR(CP_UI_BAR));
    for (int yy = 0; yy < h; yy++) mvhline(y + yy, x, ' ', w);
    mvhline(y, x, '-', w);
    mvhline(y + h - 1, x, '-', w);
    mvvline(y, x, '|', h);
    mvvline(y, x + w - 1, '|', h);
    mvaddch(y, x, '+');
    mvaddch(y, x + w - 1, '+');
    mvaddch(y + h - 1, x, '+');
    mvaddch(y + h - 1, x + w - 1, '+');
    attroff(COLOR_PAIR(CP_UI_BAR));

    int row = y + 1;
    attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
    mvprintw(row++, x + 2, "Realm Help");
    attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    attron(COLOR_PAIR(CP_UI_TEXT));
    for (int i = 0; i < n && row < y + h - 6; i++)
        mvprintw(row++, x + 2, "%-16s %-14s %.36s", commands[i].keys, commands[i].label, commands[i].help);
    row++;
    mvprintw(row++, x + 2, "Food: berries, hunting, farms/mills, wheat, fishing.");
    mvprintw(row++, x + 2, "Winter drains food; starvation damages units.");
    mvprintw(row++, x + 2, "Owner backgrounds mark factions; animals are neutral.");
    mvprintw(row++, x + 2, "! means recent combat; x/+/# are command markers.");
    attroff(COLOR_PAIR(CP_UI_TEXT));
    attron(COLOR_PAIR(CP_UI_HIGH));
    mvprintw(y + h - 2, x + 2, "Press ? to close");
    attroff(COLOR_PAIR(CP_UI_HIGH));
}

void render() { erase(); renderMap(); renderUI(); renderHelpOverlay(); refresh(); }
