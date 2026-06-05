#pragma once

#include "render/sdl/sdl_common.h"

struct IsoOffsetBounds {
    int minSx = 0;
    int maxSx = 0;
    int minSy = 0;
    int maxSy = 0;
};

bool isMobileGui();
bool mobilePortrait();
bool isAsciiMobileGui();
int mobileSafePad();
void asciiMobileCellMetrics(int& cellW, int& cellH);
int mobileHudExtent();
SDL_Rect mapRect();
SDL_Rect mapSafeRect();
SDL_Rect panelRect();
SDL_Rect miniMapRect();
int topDownSafeColumns();
int topDownSafeRows();
int isoHalfW();
int isoHalfH();
void isoOrigin(int& ox, int& oy);
float isoCameraViewX();
float isoCameraViewY();
void syncIsoCameraToView();
IsoOffsetBounds isoVisibleOffsetBounds();
IsoOffsetBounds isoSafeOffsetBounds();
void updateViewMetrics(bool keepCursor = true);
void clampView();
void centerViewOnTile(int mx, int my);
bool moveViewFromMiniMap(int px, int py, bool clampToMiniMap = false);
bool screenToMiniMapTile(int px, int py, int& mx, int& my, bool clampToMiniMap = false);
bool screenToMap(int px, int py, int& mx, int& my);
bool screenToMapOffset(int px, int py, int& sxOut, int& syOut);
bool mapTileScreenCenter(int mx, int my, int& px, int& py);
bool mapTileAtViewportCenter(int& mx, int& my, int& px, int& py);
void isoTileCenterFromScreenOffset(int sx, int sy, int& cx, int& cy);
void isoScreenToOffsetFloat(int px, int py, float& sx, float& sy);
int zoomDefaultTilePx();
int zoomMaxTilePx();
void resetZoomForDisplayMode();
void setZoom(int newTile, int anchorX = -1, int anchorY = -1);
void zoomBySteps(int steps, int anchorX = -1, int anchorY = -1);
void toggleFullscreen();
void startMiddlePan(int px, int py);
void updateMiddlePan(int px, int py);
void moveCursorToViewCenter();
