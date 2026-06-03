#pragma once

#include "render/sdl/sdl_common.h"

void clearTextCache();
void drawText(int x, int y, const std::string& text, Color col, TTF_Font* font = nullptr);
TTF_Font* openFont(const std::vector<std::string>& paths, int size, std::string* usedPath = nullptr);
SDL_Texture* cachedText(TTF_Font* font, const std::string& text, Color col, bool blended = true);
int textWidth(const std::string& text, TTF_Font* font = nullptr);
int textLineHeight(TTF_Font* font = nullptr);
bool rectHovered(SDL_Rect r);
void registerKeyHit(SDL_Rect r, int ch);
void drawHoverMark(SDL_Rect r, Color color);
void drawTextFit(int x, int y, const std::string& text, Color col, int maxW, TTF_Font* font = nullptr);
void drawCentered(const std::string& text, SDL_Rect rect, Color col, bool emoji, bool tint = false);
void drawKeyOptionText(int x, int y, const std::string& text, int ch,
                       Color color, int maxW, TTF_Font* font = nullptr);
void drawKeyTokensInText(int x, int y, const std::string& text,
                         const std::vector<std::pair<std::string, int>>& tokens,
                         Color color, int maxW, TTF_Font* font = nullptr);
