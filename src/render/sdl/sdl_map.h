#pragma once

#include "render/sdl/sdl_text.h"
#include "render/sdl/sdl_viewport.h"

int keyToInput(SDL_Keycode key);
const char* seasonNameSafe();
const char* timeNameSafe();
const char* weatherName();
std::string trimPanelLine(const std::string& s, size_t maxLen = 32);
std::string cursorTileSummary();
std::string cursorStackSummary();
void drawMap();

void fillDiamond(int cx, int cy, int hw, int hh, Color c);
void drawDiamondOutline(int cx, int cy, int hw, int hh, Color c);
void hatchDiamond(int cx, int cy, int hw, int hh, Color c, int step);
void sparkleDiamond(int cx, int cy, int hw, int hh, Color c, int x, int y);
void applyTerrainTextureIso(int cx, int cy, int hw, int hh, const Tile& t, int x, int y);
char terrainAscii(Terrain t);
const char* terrainGlyph(const Tile& t, int x, int y);
bool isResourceEmojiTerrain(Terrain t);
bool isSelected(const Entity* e);
void logMissingTerrainImageTile(Terrain t);
void logMissingVisualTileParts(const Tile& tile);
void drawFeatureOccluderIfNeeded(int mx, int my, SDL_Rect rect);
bool drawEntityImageTile(const Entity& e, SDL_Rect dst, Color modulation,
                         const char* forcedAction = nullptr,
                         const char* forcedDirection = nullptr,
                         int explicitFrame = -1,
                         SDL_Color teamColor = SDL_Color{0,0,0,0},
                         TilesetAssetFrame* outFrame = nullptr);
std::string tilesetEntityVisual(const Entity& e, bool& usesSymbolFont);
Color glyphColorForTerrain(const Tile& t, int x, int y);
Color applyVisionAndLight(Color c, int x, int y);
Color applyVisionToGlyph(Color c, int x, int y);
void applyTerrainTexture(SDL_Rect r, const Tile& t, int x, int y);
bool imageTilesetEnabled();
bool hasEntityImageTile(EntityType type);
bool hasTerrainImageTile(Terrain terrain);
std::string terrainAssetKey(Terrain terrain);
std::string entityAssetKey(EntityType type);
std::string effectAssetKey(const std::string& effectName);
