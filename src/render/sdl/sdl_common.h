#pragma once

#include "core/game_types.h"
#include "display.h"
#include "input_keys.h"
#include "gfx_renderer.h"
#include "entity_animation.h"
#include "tileset_assets.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if defined(REALM_WEB)
#include <emscripten/emscripten.h>
#endif

struct Tile;

struct Color { Uint8 r, g, b, a; };

struct MobileButton {
    SDL_Rect r;
    std::string id;
    std::string label;
};

struct KeyHit {
    SDL_Rect r;
    int ch = 0;
};

struct Gfx {
    SDL_Window*   win = nullptr;
    SDL_Renderer* ren = nullptr;
    TTF_Font* mono = nullptr;
    TTF_Font* monoSmall = nullptr;
    TTF_Font* emoji = nullptr;
    bool emojiFontLoaded = false;
    std::string monoPath;
    std::string emojiPath;

    int winW = 1280;
    int winH = 800;
    int tile = 24;
    int topH = 32;
    int bottomH = 48;
    int panelW = 286;

    bool isometric = true;
    bool asciiOnly = false;
    bool asciiSquareMapCells = true;
    bool fullscreen = false;

    bool leftDown = false;
    bool rightDown = false;
    bool rightHoldMenuOpened = false;
    bool rightDownOnMiniMap = false;
    bool middleDown = false;
    bool miniMapDown = false;
    int dragStartX = 0, dragStartY = 0;
    int rightDownX = 0, rightDownY = 0;
    int rightDownMapX = -1, rightDownMapY = -1;
    bool rightDownShift = false;
    Uint32 rightDownTicks = 0;
    bool rightDragPathActive = false;
    std::vector<std::pair<int, int>> rightDragPath;
    int panStartMouseX = 0, panStartMouseY = 0;
    int panStartViewX = 0, panStartViewY = 0;
    bool isoCameraActive = false;
    float isoViewX = 0.0f, isoViewY = 0.0f;
    float panStartIsoViewX = 0.0f, panStartIsoViewY = 0.0f;
    int lastMouseMapX = -9999, lastMouseMapY = -9999;
    int mouseX = -10000, mouseY = -10000;
    std::vector<KeyHit> keyHits;

    int mobileOrientation = 0;
    float mobileUiScale = 1.0f;
    bool mobileMinimapTap = true;
    bool mobileConfirmCommands = false;
    bool mobileEdgeScroll = false;
    bool mobileSplashSettings = false;
    bool mobileSplashHelp = false;
    bool loadGameRequested = false;
    EntityType mobileBuildType = E_NONE;
    int mobileBuildPage = 0;

    bool touchDown = false;
    bool touchOnMap = false;
    bool touchPanning = false;
    bool suppressNextMouse = false;
    Uint32 touchDownTicks = 0;
    int touchStartX = 0, touchStartY = 0;
    int touchLastX = 0, touchLastY = 0;

    std::unordered_map<std::string, SDL_Texture*> textCache;
    std::unordered_map<std::string, TTF_Font*> sizedMonoFonts;
    std::unordered_set<std::string> missingTileKeys;
    bool missingTileLogStarted = false;
};

struct LabLightOverride {
    bool enabled = false;
    int x = 0;
    int y = 0;
    float strength = 0.0f;
    float radius = 0.0f;
};

extern Gfx s;
extern bool labForcesImageTileset;
extern LabLightOverride labLightOverride;

Color rgb(int r, int g, int b, int a = 255);
Color scale(Color c, float f);
Color blend(Color a, Color b, float t);
std::string lowerSlug(const std::string& text);
bool envFlagEnabled(const char* name, bool fallback);
bool localTilesetAuditEnabled();
float clamp01(float v);
unsigned hash2(int x, int y, unsigned salt);
void logMissingTile(const std::string& kind, const std::string& key,
                    const std::string& name, const std::string& fallback,
                    const std::string& suggestedAsset);
void setDraw(Color c);
void applyRendererOutputScale();
float noisePatch(int x, int y, unsigned salt);
int terrainFamily(Terrain t);
Color seasonTint(Color base);
Color timeTint(Color base);
Color biomeBase(Biome b);
Color terrainBg(const Tile& t, int x, int y);
Color colorFromHue(int hue);
Color ownerBg(int owner);
bool pointInRect(int x, int y, SDL_Rect r);
