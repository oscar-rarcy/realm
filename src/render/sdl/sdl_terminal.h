#pragma once

#include "render/sdl/sdl_hud.h"

struct TerminalCell {
    char ch;
    Color fg;
    Color bg;
};

struct WorldIndex;

struct TerminalFrame {
    int cols = 0;
    int rows = 0;
    int cellW = 0;
    int cellH = 0;
    std::vector<TerminalCell> cells;

    TerminalCell& at(int x, int y) {
        return cells[(size_t)y * (size_t)cols + (size_t)x];
    }

    const TerminalCell& at(int x, int y) const {
        return cells[(size_t)y * (size_t)cols + (size_t)x];
    }
};

TerminalFrame makeBlankTerminalFrame();
TerminalFrame buildAsciiTerminalFrame(const WorldIndex& world);
TerminalCell terminalMapCell(const WorldIndex& world, int mx, int my);
Color termBg();
Color termFg();
Color termDim();
Color termHigh();
Color termAccent();
void terminalMapCellMetrics(int& cellW, int& cellH);
SDL_Rect terminalMapPixelRect(const TerminalFrame& frame);
void clampTerminalView();
void updateTerminalCamera(int cols, int rows, bool keepCursor = true);
void registerTerminalKeyTokens(const TerminalFrame& frame);
void drawAsciiTerminalFrame(const WorldIndex& world, bool present);
void drawAsciiMobileFrame(const WorldIndex& world, bool present);
bool gfxSaveAsciiTerminalText(const std::string& path);
