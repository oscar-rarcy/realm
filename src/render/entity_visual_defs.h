#pragma once

#include "core/game_types.h"

struct EntityVisualDef {
    EntityType type;
    const char* terminalEmoji;
    const char* sdlGlyph;
    const char* assetSlug;
};

const EntityVisualDef& entityVisualDef(EntityType type);
const char* entityTerminalEmoji(EntityType type);
const char* entitySdlGlyph(EntityType type);
const char* entityAssetSlug(EntityType type);
