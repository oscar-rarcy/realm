#pragma once

#include "render/sdl/sdl_text.h"
#include "render/sdl/sdl_viewport.h"

int keyToInput(SDL_Keycode key);
const char* seasonNameSafe();
const char* timeNameSafe();
const char* weatherName();
std::string trimPanelLine(const std::string& s, size_t maxLen = 32);
struct Game;
struct WorldIndex;
struct Command;
struct CommandPreviewRequest;
Entity* renderFindEntity(Game& game, const WorldIndex& world, int id);
Entity* renderEntityAt(Game& game, const WorldIndex& world, int x, int y);
bool renderCanPlace(Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner, int ignoreEntityId = -1);
std::string cursorTileSummary();
std::string cursorStackSummary();
void drawMap(const WorldIndex& world);
void drawCommandContextMenu();
void commandContextMenuOpen(const Game& game, const WorldIndex& world, const CommandPreviewRequest& request, int anchorX, int anchorY);
void commandContextMenuClose();
bool commandContextMenuIsOpen();
int commandContextMenuOptionCount();
bool commandContextMenuTakeCommand(int px, int py, Command& outCommand);

void fillDiamond(int cx, int cy, int hw, int hh, Color c);
void drawDiamondOutline(int cx, int cy, int hw, int hh, Color c);
void hatchDiamond(int cx, int cy, int hw, int hh, Color c, int step);
void sparkleDiamond(int cx, int cy, int hw, int hh, Color c, int x, int y);
void applyTerrainTextureIso(int cx, int cy, int hw, int hh, const Tile& t, int x, int y);
bool drawUnknownGroundTextureIso(int cx, int cy, int hw, int hh, int x, int y);
const char* terrainGlyph(const Tile& t, int x, int y);
bool isResourceEmojiTerrain(Terrain t);
bool isSelected(const Entity* e);
void logMissingTerrainImageTile(Terrain t);
void logMissingVisualTileParts(const Tile& tile);
void drawFeatureOccluderIfNeeded(Game& game, const WorldIndex& world, int mx, int my, SDL_Rect rect);
bool drawEntityImageTile(const Game& game, const WorldIndex& world, const Entity& e, SDL_Rect dst, Color modulation,
                         const char* forcedAction = nullptr,
                         const char* forcedDirection = nullptr,
                         int explicitFrame = -1,
                         SDL_Color teamColor = SDL_Color{0,0,0,0},
                         TilesetAssetFrame* outFrame = nullptr,
                         double angleDegrees = 0.0,
                         SDL_Rect* outDrawRect = nullptr);
bool drawEntityImageAtAnchor(const Game& game, const WorldIndex& world, const Entity& e,
                             int anchorScreenX, int anchorScreenY, int targetWidth, int targetHeight,
                             Color modulation,
                             const char* forcedAction = nullptr,
                             const char* forcedDirection = nullptr,
                             int explicitFrame = -1,
                             SDL_Color teamColor = SDL_Color{0,0,0,0},
                             TilesetAssetFrame* outFrame = nullptr,
                             SDL_Rect* outDrawRect = nullptr,
                             double angleDegrees = 0.0);
std::string tilesetEntityVisual(const Game& game, const WorldIndex& world, const Entity& e, bool& usesSymbolFont);
Color glyphColorForTerrain(const Tile& t, int x, int y);
Color applyVisionAndLight(Color c, int x, int y);
Color applyVisionToGlyph(Color c, int x, int y);
void applyTerrainTexture(SDL_Rect r, const Tile& t, int x, int y);
bool drawUnknownGroundTexture(SDL_Rect r, int x, int y);
bool imageTilesetEnabled();
bool hasEntityImageTile(EntityType type);
bool hasTerrainImageTile(Terrain terrain);
std::string terrainAssetKey(Terrain terrain);
std::string entityAssetKey(EntityType type);
std::string effectAssetKey(const std::string& effectName);
