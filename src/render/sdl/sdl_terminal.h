#pragma once

#include "render/sdl/sdl_hud.h"

struct TerminalCell {
    char ch;
    Color fg;
    Color bg;
};

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
TerminalFrame buildAsciiTerminalFrame();
TerminalCell terminalMapCell(int mx, int my);
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
void drawAsciiTerminalFrame(bool present);
void drawAsciiMobileFrame(bool present);
bool gfxSaveAsciiTerminalText(const std::string& path);
