#include "tileset_assets.h"
#include "realm.h"
#include "render/entity_visual_defs.h"
#include "render/sdl/sdl_profiler.h"

#include <png.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<unsigned char> rgba;
};

struct TextureRecord {
    SDL_Texture* texture = nullptr;
    bool baseLoaded = false;
    bool maskLoaded = false;
    bool placeholder = false;
    bool hasAnchor = false;
    TilesetPlacement placement;
    int width = 0;
    int height = 0;
    int anchorX = 0;
    int anchorY = 0;
    int anchorSourceWidth = 0;
    int anchorSourceHeight = 0;
    std::string basePath;
    std::string maskPath;
    std::string status;
};

struct EntityFramePlacement {
    bool found = false;
    TilesetPlacement placement;
};

std::unordered_map<std::string, TextureRecord> gTextureCache;
std::unordered_map<std::string, Image> gDecodedImageCache;
std::unordered_map<std::string, EntityFramePlacement> gEntityPlacementCache;

std::filesystem::path groundTilePath(const std::string& slug) {
    return std::filesystem::path("assets") / "tiles" / "grounds" / (slug + ".png");
}

std::filesystem::path groundTilePath(GroundType ground) {
    return groundTilePath(groundTypeName(ground));
}

std::string frameName(int frameIndex, const char* suffix) {
    std::ostringstream ss;
    ss << "frame_";
    if (frameIndex < 10) ss << '0';
    ss << std::max(0, frameIndex) << suffix;
    return ss.str();
}

std::filesystem::path entityFramePath(EntityType type, const std::string& action,
                                      const std::string& direction, int frameIndex,
                                      const char* suffix) {
    return std::filesystem::path("assets") / "tiles" / "entities" / tilesetEntitySlug(type)
        / action / direction / frameName(frameIndex, suffix);
}

std::filesystem::path entityFrameDir(EntityType type, const std::string& action,
                                     const std::string& direction) {
    return std::filesystem::path("assets") / "tiles" / "entities" / tilesetEntitySlug(type)
        / action / direction;
}

std::filesystem::path entityManifestPath(EntityType type) {
    return std::filesystem::path("assets") / "tiles" / "entities" / tilesetEntitySlug(type)
        / "manifest.json";
}

std::filesystem::path featureTilePath(FeatureType feature, FeatureState state, const std::string& layer) {
    return std::filesystem::path("assets") / "tiles" / "features" / featureTypeName(feature)
        / featureStateName(state) / (layer + ".png");
}

std::filesystem::path decalTilePath(VisualDecalType decal) {
    return std::filesystem::path("assets") / "tiles" / "decals"
        / (std::string(visualDecalName(decal)) + ".png");
}

std::filesystem::path projectileManifestPath(ProjectileType projectile) {
    return std::filesystem::path("assets") / "tiles" / "projectiles"
        / projectileTypeName(projectile) / "manifest.json";
}

std::filesystem::path effectUiTilePath(const std::string& assetId) {
    return std::filesystem::path("assets") / "tiles" / "effects-ui" / (assetId + ".png");
}

std::filesystem::path screenUiTilePath(const std::string& assetId) {
    return std::filesystem::path("assets") / "tiles" / "ui" / (assetId + ".png");
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int parseIntField(const std::string& text, const std::string& field, int fallback) {
    size_t key = text.find("\"" + field + "\"");
    if (key == std::string::npos) return fallback;
    size_t colon = text.find(':', key);
    if (colon == std::string::npos) return fallback;
    size_t pos = colon + 1;
    while (pos < text.size() && std::isspace((unsigned char)text[pos])) ++pos;
    size_t end = pos;
    if (end < text.size() && (text[end] == '-' || text[end] == '+')) ++end;
    while (end < text.size() && std::isdigit((unsigned char)text[end])) ++end;
    if (end <= pos) return fallback;
    try {
        return std::stoi(text.substr(pos, end - pos));
    } catch (...) {
        return fallback;
    }
}

std::string parseStringField(const std::string& text, const std::string& field, const std::string& fallback = "") {
    size_t key = text.find("\"" + field + "\"");
    if (key == std::string::npos) return fallback;
    size_t colon = text.find(':', key);
    if (colon == std::string::npos) return fallback;
    size_t quote = text.find('"', colon + 1);
    if (quote == std::string::npos) return fallback;
    std::string out;
    bool escape = false;
    for (size_t i = quote + 1; i < text.size(); ++i) {
        char c = text[i];
        if (escape) {
            out.push_back(c);
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            return out;
        } else {
            out.push_back(c);
        }
    }
    return fallback;
}

size_t findMatchingBrace(const std::string& text, size_t open) {
    if (open == std::string::npos || open >= text.size() || text[open] != '{') return std::string::npos;
    bool inString = false;
    bool escape = false;
    int depth = 0;
    for (size_t i = open; i < text.size(); ++i) {
        char c = text[i];
        if (inString) {
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
        } else if (c == '{') {
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string::npos;
}

std::string parseObjectField(const std::string& text, const std::string& field) {
    size_t key = text.find("\"" + field + "\"");
    if (key == std::string::npos) return {};
    size_t open = text.find('{', key);
    if (open == std::string::npos) return {};
    size_t close = findMatchingBrace(text, open);
    if (close == std::string::npos) return {};
    return text.substr(open, close - open + 1);
}

std::filesystem::path normalizedRelativeTo(const std::filesystem::path& base, const std::string& raw) {
    if (raw.empty()) return {};
    std::filesystem::path path(raw);
    if (path.is_absolute()) return path.lexically_normal();
    return (base / path).lexically_normal();
}

std::filesystem::path projectileTilePath(ProjectileType projectile) {
    std::filesystem::path manifestPath = projectileManifestPath(projectile);
    std::string manifest = readTextFile(manifestPath);
    std::string image = parseStringField(manifest, "image");
    std::filesystem::path resolved = normalizedRelativeTo(manifestPath.parent_path(), image);
    if (!resolved.empty() && std::filesystem::exists(resolved)) return resolved;
    if (!image.empty()) {
        std::filesystem::path named(image);
        if (named.filename() != named && image.find("effects-ui") != std::string::npos) {
            std::filesystem::path effectPath = std::filesystem::path("assets") / "tiles" / "effects-ui" / named.filename();
            if (std::filesystem::exists(effectPath)) return effectPath;
        }
        if (!resolved.empty()) return resolved;
    }
    return effectUiTilePath(std::string(projectileTypeName(projectile)) + "_projectile");
}

bool parseIntPairAfter(const std::string& text, size_t from, const std::string& field, int& x, int& y) {
    size_t key = text.find("\"" + field + "\"", from);
    if (key == std::string::npos) return false;
    size_t open = text.find('[', key);
    size_t close = text.find(']', open);
    if (open == std::string::npos || close == std::string::npos) return false;
    std::string body = text.substr(open + 1, close - open - 1);
    size_t comma = body.find(',');
    if (comma == std::string::npos) return false;
    try {
        x = std::stoi(body.substr(0, comma));
        y = std::stoi(body.substr(comma + 1));
        return true;
    } catch (...) {
        return false;
    }
}

TilesetProjection parseProjection(const std::string& value, TilesetProjection fallback) {
    if (value == "tile_space") return TilesetProjection::TileSpace;
    if (value == "tile_overlay") return TilesetProjection::TileOverlay;
    if (value == "upright_world") return TilesetProjection::UprightWorld;
    if (value == "footprint_world") return TilesetProjection::FootprintWorld;
    if (value == "projectile_world") return TilesetProjection::ProjectileWorld;
    if (value == "screen_ui") return TilesetProjection::ScreenUi;
    return fallback;
}

TilesetAnchorKind parseAnchorKind(const std::string& value, TilesetAnchorKind fallback) {
    if (value == "none") return TilesetAnchorKind::None;
    if (value == "center") return TilesetAnchorKind::Center;
    if (value == "feet") return TilesetAnchorKind::Feet;
    if (value == "footprint_origin") return TilesetAnchorKind::FootprintOrigin;
    if (value == "world_position") return TilesetAnchorKind::WorldPosition;
    if (value == "screen_position") return TilesetAnchorKind::ScreenPosition;
    return fallback;
}

TilesetScalePolicy parseScalePolicy(const std::string& value, TilesetScalePolicy fallback) {
    if (value == "source_pixels") return TilesetScalePolicy::SourcePixels;
    if (value == "entity_tile_zoom_1_55") return TilesetScalePolicy::EntityTileZoom155;
    if (value == "tile_fill") return TilesetScalePolicy::TileFill;
    if (value == "projectile") return TilesetScalePolicy::Projectile;
    if (value == "ui") return TilesetScalePolicy::Ui;
    return fallback;
}

TilesetDepthLayer parseDepthLayer(const std::string& value, TilesetDepthLayer fallback) {
    if (value == "ground") return TilesetDepthLayer::Ground;
    if (value == "decal") return TilesetDepthLayer::Decal;
    if (value == "feature_back") return TilesetDepthLayer::FeatureBack;
    if (value == "entity") return TilesetDepthLayer::Entity;
    if (value == "feature_front") return TilesetDepthLayer::FeatureFront;
    if (value == "projectile") return TilesetDepthLayer::Projectile;
    if (value == "overlay") return TilesetDepthLayer::Overlay;
    if (value == "ui") return TilesetDepthLayer::Ui;
    return fallback;
}

TilesetPlacement defaultEntityPlacement(const std::string& assetType, int spriteSize) {
    spriteSize = std::max(1, spriteSize);
    TilesetPlacement placement;
    placement.valid = true;
    placement.sourceWidth = spriteSize;
    placement.sourceHeight = spriteSize;
    placement.anchorX = spriteSize / 2;
    placement.anchorY = (int)std::lround(spriteSize * (39.0 / 48.0));
    placement.footprintWidth = 1;
    placement.footprintHeight = 1;
    placement.visualEnvelopeWidth = 1;
    placement.visualEnvelopeHeight = 1;
    placement.projection = TilesetProjection::UprightWorld;
    placement.anchorKind = TilesetAnchorKind::Feet;
    placement.scalePolicy = TilesetScalePolicy::EntityTileZoom155;
    placement.depth = TilesetDepthLayer::Entity;
    if (assetType == "building" || assetType == "buildings") {
        placement.projection = TilesetProjection::FootprintWorld;
        placement.anchorKind = TilesetAnchorKind::FootprintOrigin;
    } else if (assetType == "projectile" || assetType == "projectiles") {
        placement.projection = TilesetProjection::ProjectileWorld;
        placement.anchorKind = TilesetAnchorKind::WorldPosition;
        placement.scalePolicy = TilesetScalePolicy::Projectile;
        placement.depth = TilesetDepthLayer::Projectile;
    }
    return placement;
}

void applyPlacementObject(TilesetPlacement& placement, const std::string& objectText) {
    if (objectText.empty()) return;
    placement.projection = parseProjection(parseStringField(objectText, "projection"), placement.projection);
    placement.anchorKind = parseAnchorKind(parseStringField(objectText, "anchor_kind"), placement.anchorKind);
    placement.scalePolicy = parseScalePolicy(parseStringField(objectText, "scale_policy"), placement.scalePolicy);
    placement.depth = parseDepthLayer(parseStringField(objectText, "depth"), placement.depth);
    int x = 0, y = 0;
    if (parseIntPairAfter(objectText, 0, "source_size", x, y)) {
        placement.sourceWidth = std::max(1, x);
        placement.sourceHeight = std::max(1, y);
    }
    if (parseIntPairAfter(objectText, 0, "anchor", x, y)) {
        placement.anchorX = x;
        placement.anchorY = y;
    }
    if (parseIntPairAfter(objectText, 0, "footprint", x, y)) {
        placement.footprintWidth = std::max(1, x);
        placement.footprintHeight = std::max(1, y);
    }
    if (parseIntPairAfter(objectText, 0, "visual_envelope", x, y)) {
        placement.visualEnvelopeWidth = std::max(1, x);
        placement.visualEnvelopeHeight = std::max(1, y);
    }
}

EntityFramePlacement loadEntityFramePlacement(const TilesetAssetRequest& request) {
    std::ostringstream cache;
    cache << (int)request.type << '|' << request.action << '|' << request.direction << '|' << request.frameIndex;
    std::string key = cache.str();
    auto cached = gEntityPlacementCache.find(key);
    if (cached != gEntityPlacementCache.end()) return cached->second;

    EntityFramePlacement resolved;
    std::filesystem::path manifestPath = entityManifestPath(request.type);
    std::string manifest = readTextFile(manifestPath);
    if (!manifest.empty()) {
        int spriteSize = parseIntField(manifest, "sprite_size", 48);
        std::string assetType = parseStringField(manifest, "asset_type", "unit");
        resolved.found = true;
        resolved.placement = defaultEntityPlacement(assetType, spriteSize);
        applyPlacementObject(resolved.placement, parseObjectField(manifest, "placement"));

        size_t actionsKey = manifest.find("\"actions\"");
        size_t actionKey = actionsKey == std::string::npos ? std::string::npos
            : manifest.find("\"" + request.action + "\"", actionsKey);
        if (actionKey != std::string::npos) {
            size_t actionOpen = manifest.find('{', actionKey);
            size_t actionClose = findMatchingBrace(manifest, actionOpen);
            if (actionOpen != std::string::npos && actionClose != std::string::npos) {
                std::string actionText = manifest.substr(actionOpen, actionClose - actionOpen + 1);
                int ax = 0, ay = 0;
                applyPlacementObject(resolved.placement, parseObjectField(actionText, "placement"));
                if (parseIntPairAfter(actionText, 0, "anchor", ax, ay)) {
                    resolved.placement.anchorX = ax;
                    resolved.placement.anchorY = ay;
                }
            }
        }

        std::string frameBase = (request.action + "/" + request.direction + "/" +
                                 frameName(request.frameIndex, "_base.png"));
        size_t baseValue = manifest.find("\"" + frameBase + "\"");
        if (baseValue != std::string::npos) {
            size_t frameOpen = manifest.rfind('{', baseValue);
            size_t frameClose = findMatchingBrace(manifest, frameOpen);
            if (frameOpen != std::string::npos && frameClose != std::string::npos) {
                int ax = 0, ay = 0;
                std::string frameText = manifest.substr(frameOpen, frameClose - frameOpen + 1);
                applyPlacementObject(resolved.placement, parseObjectField(frameText, "placement"));
                if (parseIntPairAfter(frameText, 0, "anchor", ax, ay)) {
                    resolved.placement.anchorX = ax;
                    resolved.placement.anchorY = ay;
                }
            }
        }
    }

    auto inserted = gEntityPlacementCache.emplace(key, resolved);
    return inserted.first->second;
}

std::string zoomFrameName(int frameIndex, int stopSize, const char* suffix) {
    std::ostringstream ss;
    ss << "frame_";
    if (frameIndex < 10) ss << '0';
    ss << std::max(0, frameIndex) << "_zoom_";
    if (stopSize < 100) ss << '0';
    if (stopSize < 10) ss << '0';
    ss << std::max(1, stopSize) << suffix;
    return ss.str();
}

std::filesystem::path entityZoomFramePath(EntityType type, const std::string& action,
                                          const std::string& direction, int frameIndex,
                                          int stopSize, const char* suffix) {
    return entityFrameDir(type, action, direction) / zoomFrameName(frameIndex, stopSize, suffix);
}

int parseZoomFrameSize(const std::string& filename, int frameIndex, const char* suffix) {
    std::string prefix = frameName(frameIndex, "_zoom_");
    std::string tail = suffix;
    if (filename.rfind(prefix, 0) != 0 || filename.size() <= prefix.size() + tail.size()) return 0;
    if (filename.compare(filename.size() - tail.size(), tail.size(), tail) != 0) return 0;
    std::string raw = filename.substr(prefix.size(), filename.size() - prefix.size() - tail.size());
    if (raw.empty() || !std::all_of(raw.begin(), raw.end(), [](unsigned char c) { return std::isdigit(c); })) return 0;
    try {
        return std::max(1, std::stoi(raw));
    } catch (...) {
        return 0;
    }
}

int exactZoomStopSize(EntityType type, const std::string& action,
                      const std::string& direction, int frameIndex, int targetSize) {
    if (targetSize <= 0) return 0;
    std::filesystem::path dir = entityFrameDir(type, action, direction);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || ec) return 0;

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec) || ec) continue;
        int size = parseZoomFrameSize(entry.path().filename().generic_string(), frameIndex, "_base.png");
        if (size == targetSize) return size;
    }
    return 0;
}

struct EntityFramePaths {
    std::filesystem::path basePath;
    std::filesystem::path maskPath;
    int zoomStopSize = 0;
    bool usesZoomBase = false;
    bool usesZoomMask = false;
};

struct EntityFrameCandidate {
    std::string action;
    std::string direction;
    int frameIndex = 0;
};

std::vector<EntityFrameCandidate> entityFrameCandidates(const TilesetAssetRequest& request) {
    std::vector<EntityFrameCandidate> candidates;
    auto addCandidate = [&](std::string action, std::string direction, int frameIndex) {
        for (const EntityFrameCandidate& candidate : candidates) {
            if (candidate.action == action && candidate.direction == direction && candidate.frameIndex == frameIndex) {
                return;
            }
        }
        candidates.push_back({std::move(action), std::move(direction), frameIndex});
    };
    bool building = request.type > E_NONE && request.type < E_TYPE_COUNT && STATS[request.type].isBuilding;
    if (building && request.action == "idle") {
        addCandidate("complete", "south", 0);
    } else if (building && request.action == "death") {
        addCandidate("ruin_footprint", "south", 0);
    }
    addCandidate(request.action, request.direction, request.frameIndex);
    if (request.direction != "south") {
        addCandidate(request.action, "south", request.frameIndex);
    }
    if (building && request.action != "complete") {
        addCandidate("complete", "south", 0);
    }
    return candidates;
}

EntityFramePaths selectEntityFramePaths(const TilesetAssetRequest& request) {
    EntityFramePaths paths;
    EntityFrameCandidate selected{request.action, request.direction, request.frameIndex};
    for (const EntityFrameCandidate& candidate : entityFrameCandidates(request)) {
        if (std::filesystem::exists(entityFramePath(request.type, candidate.action, candidate.direction,
                                                    candidate.frameIndex, "_base.png"))) {
            selected = candidate;
            break;
        }
    }
    paths.basePath = entityFramePath(request.type, selected.action, selected.direction,
                                     selected.frameIndex, "_base.png");
    paths.maskPath = entityFramePath(request.type, selected.action, selected.direction,
                                     selected.frameIndex, "_teammask.png");

    if (request.targetWidth <= 0 || request.targetHeight <= 0 || request.targetWidth != request.targetHeight) {
        return paths;
    }

    int stopSize = exactZoomStopSize(request.type, selected.action, selected.direction,
                                     selected.frameIndex, request.targetWidth);
    if (stopSize <= 0) return paths;

    std::filesystem::path zoomBase = entityZoomFramePath(request.type, selected.action, selected.direction,
                                                        selected.frameIndex, stopSize, "_base.png");
    std::filesystem::path zoomMask = entityZoomFramePath(request.type, selected.action, selected.direction,
                                                        selected.frameIndex, stopSize, "_teammask.png");
    if (std::filesystem::exists(zoomBase)) {
        paths.basePath = zoomBase;
        paths.zoomStopSize = stopSize;
        paths.usesZoomBase = true;
    }
    if (std::filesystem::exists(zoomMask)) {
        paths.maskPath = zoomMask;
        paths.usesZoomMask = true;
    }
    return paths;
}

bool readPngRgba(const std::string& path, Image& image, std::string& error) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) {
        error = "open failed";
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        std::fclose(fp);
        error = "png_create_read_struct failed";
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        std::fclose(fp);
        error = "png_create_info_struct failed";
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        error = "png decode failed";
        return false;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);
    png_byte colorType = png_get_color_type(png, info);
    png_byte bitDepth = png_get_bit_depth(png, info);

    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_RGB || colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xff, PNG_FILLER_AFTER);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    image.width = (int)width;
    image.height = (int)height;
    image.rgba.assign((size_t)image.width * (size_t)image.height * 4u, 0);

    std::vector<png_bytep> rows((size_t)image.height);
    for (int y = 0; y < image.height; ++y) {
        rows[(size_t)y] = image.rgba.data() + (size_t)y * (size_t)image.width * 4u;
    }
    png_read_image(png, rows.data());

    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    return true;
}

bool loadDecodedImage(const std::string& path, Image& image, std::string& error) {
    auto found = gDecodedImageCache.find(path);
    if (found != gDecodedImageCache.end()) {
        image = found->second;
        return true;
    }
    if (!readPngRgba(path, image, error)) return false;
    gDecodedImageCache.emplace(path, image);
    return true;
}

Image makePlaceholder(int width, int height) {
    Image img;
    img.width = width;
    img.height = height;
    img.rgba.assign((size_t)width * (size_t)height * 4u, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool border = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            bool checker = ((x / 6) + (y / 6)) % 2 == 0;
            size_t i = ((size_t)y * (size_t)width + (size_t)x) * 4u;
            img.rgba[i + 0] = border ? 255 : checker ? 38 : 64;
            img.rgba[i + 1] = border ? 0 : checker ? 42 : 48;
            img.rgba[i + 2] = border ? 255 : checker ? 50 : 58;
            img.rgba[i + 3] = 210;
        }
    }
    return img;
}

void sampleSourcePixel(const Image& source, float sx, float sy, bool smooth, unsigned char* out);

Image resizeImageBilinear(const Image& source, int width, int height) {
    Image out;
    out.width = std::max(1, width);
    out.height = std::max(1, height);
    out.rgba.assign((size_t)out.width * (size_t)out.height * 4u, 0);
    if (source.width <= 0 || source.height <= 0) return out;

    float scaleX = out.width > 1 ? (float)(source.width - 1) / (float)(out.width - 1) : 0.0f;
    float scaleY = out.height > 1 ? (float)(source.height - 1) / (float)(out.height - 1) : 0.0f;
    for (int y = 0; y < out.height; ++y) {
        for (int x = 0; x < out.width; ++x) {
            size_t di = ((size_t)y * (size_t)out.width + (size_t)x) * 4u;
            sampleSourcePixel(source, x * scaleX, y * scaleY, true, &out.rgba[di]);
        }
    }
    return out;
}

Image resizeImageArea(const Image& source, int width, int height) {
    Image out;
    out.width = std::max(1, width);
    out.height = std::max(1, height);
    out.rgba.assign((size_t)out.width * (size_t)out.height * 4u, 0);
    if (source.width <= 0 || source.height <= 0) return out;

    const double scaleX = (double)source.width / (double)out.width;
    const double scaleY = (double)source.height / (double)out.height;
    for (int y = 0; y < out.height; ++y) {
        double sy0 = y * scaleY;
        double sy1 = (y + 1) * scaleY;
        int iy0 = std::max(0, (int)std::floor(sy0));
        int iy1 = std::min(source.height - 1, (int)std::ceil(sy1) - 1);
        for (int x = 0; x < out.width; ++x) {
            double sx0 = x * scaleX;
            double sx1 = (x + 1) * scaleX;
            int ix0 = std::max(0, (int)std::floor(sx0));
            int ix1 = std::min(source.width - 1, (int)std::ceil(sx1) - 1);

            double premulR = 0.0, premulG = 0.0, premulB = 0.0, alphaSum = 0.0, weightSum = 0.0;
            for (int sy = iy0; sy <= iy1; ++sy) {
                double oy = std::max(0.0, std::min(sy1, (double)sy + 1.0) - std::max(sy0, (double)sy));
                if (oy <= 0.0) continue;
                for (int sx = ix0; sx <= ix1; ++sx) {
                    double ox = std::max(0.0, std::min(sx1, (double)sx + 1.0) - std::max(sx0, (double)sx));
                    double w = ox * oy;
                    if (w <= 0.0) continue;
                    size_t si = ((size_t)sy * (size_t)source.width + (size_t)sx) * 4u;
                    double a = source.rgba[si + 3] / 255.0;
                    premulR += source.rgba[si + 0] * a * w;
                    premulG += source.rgba[si + 1] * a * w;
                    premulB += source.rgba[si + 2] * a * w;
                    alphaSum += a * w;
                    weightSum += w;
                }
            }

            size_t di = ((size_t)y * (size_t)out.width + (size_t)x) * 4u;
            double alpha = weightSum > 0.0 ? alphaSum / weightSum : 0.0;
            out.rgba[di + 3] = (unsigned char)std::clamp((int)std::lround(alpha * 255.0), 0, 255);
            if (alphaSum <= 0.000001) continue;
            out.rgba[di + 0] = (unsigned char)std::clamp((int)std::lround(premulR / alphaSum), 0, 255);
            out.rgba[di + 1] = (unsigned char)std::clamp((int)std::lround(premulG / alphaSum), 0, 255);
            out.rgba[di + 2] = (unsigned char)std::clamp((int)std::lround(premulB / alphaSum), 0, 255);
        }
    }
    return out;
}

Image resizeImage(const Image& source, int width, int height) {
    width = std::max(1, width);
    height = std::max(1, height);
    if (source.width == width && source.height == height) return source;
    if (width < source.width || height < source.height) return resizeImageArea(source, width, height);
    return resizeImageBilinear(source, width, height);
}

Image cropImageFraction(const Image& source, float fraction) {
    fraction = std::clamp(fraction, 0.0f, 0.24f);
    if (fraction <= 0.001f || source.width <= 2 || source.height <= 2) return source;
    int cropX = std::min(source.width / 4, (int)std::lround(source.width * fraction));
    int cropY = std::min(source.height / 4, (int)std::lround(source.height * fraction));
    int width = std::max(1, source.width - cropX * 2);
    int height = std::max(1, source.height - cropY * 2);
    Image out;
    out.width = width;
    out.height = height;
    out.rgba.assign((size_t)width * (size_t)height * 4u, 0);
    for (int y = 0; y < height; ++y) {
        const unsigned char* src = source.rgba.data() + (((size_t)y + (size_t)cropY) * (size_t)source.width + (size_t)cropX) * 4u;
        unsigned char* dst = out.rgba.data() + (size_t)y * (size_t)width * 4u;
        std::copy(src, src + (size_t)width * 4u, dst);
    }
    return out;
}

void compositeTeamMask(Image& base, const Image& mask, SDL_Color teamColor) {
    if (base.width != mask.width || base.height != mask.height) return;
    for (int y = 0; y < base.height; ++y) {
        for (int x = 0; x < base.width; ++x) {
            size_t i = ((size_t)y * (size_t)base.width + (size_t)x) * 4u;
            float alpha = mask.rgba[i + 3] / 255.0f;
            if (alpha <= 0.0f) continue;
            float shade = (mask.rgba[i + 0] + mask.rgba[i + 1] + mask.rgba[i + 2]) / (255.0f * 3.0f);
            unsigned char r = (unsigned char)std::clamp((int)std::lround(teamColor.r * shade), 0, 255);
            unsigned char g = (unsigned char)std::clamp((int)std::lround(teamColor.g * shade), 0, 255);
            unsigned char b = (unsigned char)std::clamp((int)std::lround(teamColor.b * shade), 0, 255);
            base.rgba[i + 0] = (unsigned char)std::clamp((int)std::lround(base.rgba[i + 0] * (1.0f - alpha) + r * alpha), 0, 255);
            base.rgba[i + 1] = (unsigned char)std::clamp((int)std::lround(base.rgba[i + 1] * (1.0f - alpha) + g * alpha), 0, 255);
            base.rgba[i + 2] = (unsigned char)std::clamp((int)std::lround(base.rgba[i + 2] * (1.0f - alpha) + b * alpha), 0, 255);
            base.rgba[i + 3] = std::max(base.rgba[i + 3], mask.rgba[i + 3]);
        }
    }
}

SDL_Texture* textureFromImage(SDL_Renderer* renderer, const Image& image) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, image.width, image.height);
    if (!texture) return nullptr;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    if (SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4) != 0) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }
    return texture;
}

std::string cacheKey(const TilesetAssetRequest& request) {
    std::ostringstream ss;
    ss << "entity|" << (int)request.type << '|' << request.action << '|' << request.direction << '|'
       << request.frameIndex << '|' << (int)request.teamColor.r << ','
       << (int)request.teamColor.g << ',' << (int)request.teamColor.b << '|'
       << request.targetWidth << 'x' << request.targetHeight;
    return ss.str();
}

std::string imageCacheKey(const std::string& kind, const std::string& path, int width = 0, int height = 0) {
    std::ostringstream ss;
    ss << kind << '|' << path << '|' << width << 'x' << height;
    return ss.str();
}

TilesetAssetFrame copyFrame(const TextureRecord& record) {
    return TilesetAssetFrame{record.texture, record.baseLoaded, record.maskLoaded, record.placeholder,
                             record.hasAnchor, record.placement, record.width, record.height,
                             record.anchorX, record.anchorY,
                             record.anchorSourceWidth, record.anchorSourceHeight,
                             record.basePath, record.maskPath, record.status};
}

void alignEntityPlacementToDecodedImage(TextureRecord& record, int imageWidth, int imageHeight) {
    if (!record.hasAnchor || !record.placement.valid || imageWidth <= 0 || imageHeight <= 0) return;

    int sourceW = record.placement.sourceWidth > 0 ? record.placement.sourceWidth : record.anchorSourceWidth;
    int sourceH = record.placement.sourceHeight > 0 ? record.placement.sourceHeight : record.anchorSourceHeight;
    if (sourceW <= 0 || sourceH <= 0 || (sourceW == imageWidth && sourceH == imageHeight)) return;

    record.placement.anchorX = (int)std::lround(record.placement.anchorX * (imageWidth / (double)sourceW));
    record.placement.anchorY = (int)std::lround(record.placement.anchorY * (imageHeight / (double)sourceH));
    record.placement.sourceWidth = imageWidth;
    record.placement.sourceHeight = imageHeight;
    record.anchorX = record.placement.anchorX;
    record.anchorY = record.placement.anchorY;
    record.anchorSourceWidth = imageWidth;
    record.anchorSourceHeight = imageHeight;
}

bool isGroundImageKind(const std::string& kind) {
    return kind.rfind("ground", 0) == 0;
}

float zoomedOutGroundCropFraction(int tilePx) {
    if (tilePx >= 48) return 0.0f;
    if (tilePx <= 24) return 0.045f;
    return 0.045f * (48.0f - tilePx) / 24.0f;
}

void sampleSourcePixel(const Image& source, float sx, float sy, bool smooth, unsigned char* out) {
    if (!smooth) {
        int ix = std::clamp((int)std::lround(sx), 0, source.width - 1);
        int iy = std::clamp((int)std::lround(sy), 0, source.height - 1);
        size_t si = ((size_t)iy * (size_t)source.width + (size_t)ix) * 4u;
        out[0] = source.rgba[si + 0];
        out[1] = source.rgba[si + 1];
        out[2] = source.rgba[si + 2];
        out[3] = source.rgba[si + 3];
        return;
    }

    sx = std::clamp(sx, 0.0f, (float)(source.width - 1));
    sy = std::clamp(sy, 0.0f, (float)(source.height - 1));
    int x0 = std::clamp((int)std::floor(sx), 0, source.width - 1);
    int y0 = std::clamp((int)std::floor(sy), 0, source.height - 1);
    int x1 = std::min(source.width - 1, x0 + 1);
    int y1 = std::min(source.height - 1, y0 + 1);
    float tx = sx - x0;
    float ty = sy - y0;
    for (int channel = 0; channel < 4; ++channel) {
        float c00 = source.rgba[((size_t)y0 * (size_t)source.width + (size_t)x0) * 4u + channel];
        float c10 = source.rgba[((size_t)y0 * (size_t)source.width + (size_t)x1) * 4u + channel];
        float c01 = source.rgba[((size_t)y1 * (size_t)source.width + (size_t)x0) * 4u + channel];
        float c11 = source.rgba[((size_t)y1 * (size_t)source.width + (size_t)x1) * 4u + channel];
        float top = c00 + (c10 - c00) * tx;
        float bottom = c01 + (c11 - c01) * tx;
        out[channel] = (unsigned char)std::clamp((int)std::lround(top + (bottom - top) * ty), 0, 255);
    }
}

Image projectSquareToDiamond(const Image& source, int width, int height, float sourceCropFraction = 0.0f) {
    Image out;
    out.width = std::max(1, width);
    out.height = std::max(1, height);
    out.rgba.assign((size_t)out.width * (size_t)out.height * 4u, 0);
    float cx = (out.width - 1) * 0.5f;
    float cy = (out.height - 1) * 0.5f;
    float hw = std::max(1.0f, cx);
    float hh = std::max(1.0f, cy);
    float crop = std::clamp(sourceCropFraction, 0.0f, 0.18f);
    float sx0 = crop * (source.width - 1);
    float sy0 = crop * (source.height - 1);
    float sxRange = std::max(1.0f, (source.width - 1) * (1.0f - crop * 2.0f));
    float syRange = std::max(1.0f, (source.height - 1) * (1.0f - crop * 2.0f));
    bool smooth = true;
    for (int y = 0; y < out.height; ++y) {
        float b = (y - cy) / hh;
        for (int x = 0; x < out.width; ++x) {
            float a = (x - cx) / hw;
            if (std::abs(a) + std::abs(b) > 1.01f) continue;
            float u = (a + b + 1.0f) * 0.5f;
            float v = (b - a + 1.0f) * 0.5f;
            size_t di = ((size_t)y * (size_t)out.width + (size_t)x) * 4u;
            sampleSourcePixel(source, sx0 + u * sxRange, sy0 + v * syRange, smooth, &out.rgba[di]);
        }
    }
    return out;
}

enum class ImageTransform {
    Original,
    Scaled,
    ProjectedIso,
};

TilesetAssetFrame loadImageTexture(SDL_Renderer* renderer, const std::filesystem::path& path,
                                   const std::string& kind, int targetWidth = 0, int targetHeight = 0,
                                   ImageTransform transform = ImageTransform::Original) {
    if (!renderer) return {};
    std::string pathString = path.generic_string();
    std::string key = imageCacheKey(kind, pathString, targetWidth, targetHeight);
    auto found = gTextureCache.find(key);
    if (found != gTextureCache.end()) return copyFrame(found->second);
    RealmProfileScope cacheMissScope("assets.image_cache_miss");

    TextureRecord record;
    record.basePath = pathString;
    Image image;
    std::string error;
    {
        RealmProfileScope scope("assets.decode_image");
        record.baseLoaded = loadDecodedImage(pathString, image, error);
    }
    if (!record.baseLoaded) {
        record.status = "missing image: " + error;
        record.placeholder = true;
        auto inserted = gTextureCache.emplace(key, std::move(record));
        return copyFrame(inserted.first->second);
    }

    if (isGroundImageKind(kind) && (image.width < 128 || image.height < 128)) {
        record.width = image.width;
        record.height = image.height;
        record.placeholder = true;
        record.status = "placeholder-sized ground image ignored";
        auto inserted = gTextureCache.emplace(key, std::move(record));
        return copyFrame(inserted.first->second);
    }

    if (targetWidth > 0 && targetHeight > 0 && transform == ImageTransform::ProjectedIso) {
        RealmProfileScope scope("assets.project_iso");
        float crop = isGroundImageKind(kind) ? zoomedOutGroundCropFraction((targetWidth - 1) / 2) : 0.0f;
        int mipSize = std::max(targetWidth, targetHeight);
        Image projectionSource = resizeImage(cropImageFraction(image, crop), mipSize, mipSize);
        image = projectSquareToDiamond(projectionSource, targetWidth, targetHeight, 0.0f);
    } else if (targetWidth > 0 && targetHeight > 0 && transform == ImageTransform::Scaled) {
        RealmProfileScope scope("assets.scale_image");
        float crop = isGroundImageKind(kind) ? zoomedOutGroundCropFraction(std::min(targetWidth, targetHeight)) : 0.0f;
        image = resizeImage(cropImageFraction(image, crop), targetWidth, targetHeight);
    }
    record.width = image.width;
    record.height = image.height;
    {
        RealmProfileScope scope("assets.create_texture");
        record.texture = textureFromImage(renderer, image);
    }
    if (!record.texture) {
        record.status = "texture creation failed";
    } else {
        if (targetWidth > 0 && targetHeight > 0) {
            record.status = transform == ImageTransform::ProjectedIso ? "image projected and cached at draw size"
                                                                      : "image scaled and cached at draw size";
        } else {
            record.status = "image loaded";
        }
    }
    auto inserted = gTextureCache.emplace(key, std::move(record));
    return copyFrame(inserted.first->second);
}

} // namespace

std::string tilesetEntitySlug(EntityType type) {
    if (type <= E_NONE || type >= E_TYPE_COUNT) return "unknown";
    return entityAssetSlug(type);
}

TilesetPlacement tilesetResolveEntityFramePlacement(EntityType type, const std::string& action,
                                                    const std::string& direction, int frameIndex) {
    TilesetAssetRequest request{type, action, direction, frameIndex, SDL_Color{0, 0, 0, 0}, 0, 0};
    EntityFramePlacement resolved = loadEntityFramePlacement(request);
    return resolved.placement;
}

TilesetAssetFrame tilesetLoadEntityFrame(SDL_Renderer* renderer, const TilesetAssetRequest& request) {
    if (!renderer || request.type == E_NONE || request.action.empty() || request.direction.empty()) {
        return {};
    }

    std::string key = cacheKey(request);
    auto found = gTextureCache.find(key);
    if (found != gTextureCache.end()) return copyFrame(found->second);
    RealmProfileScope cacheMissScope("assets.entity_cache_miss");

    EntityFramePaths paths = selectEntityFramePaths(request);

    TextureRecord record;
    record.basePath = paths.basePath.generic_string();
    record.maskPath = paths.maskPath.generic_string();
    EntityFramePlacement placement = loadEntityFramePlacement(request);
    if (placement.found && placement.placement.valid) {
        record.hasAnchor = true;
        record.placement = placement.placement;
        record.anchorX = placement.placement.anchorX;
        record.anchorY = placement.placement.anchorY;
        record.anchorSourceWidth = placement.placement.sourceWidth;
        record.anchorSourceHeight = placement.placement.sourceHeight;
    }

    Image base;
    Image mask;
    std::string baseError;
    std::string maskError;
    {
        RealmProfileScope scope("assets.decode_entity_base");
        record.baseLoaded = loadDecodedImage(record.basePath, base, baseError);
    }
    if (!record.baseLoaded) {
        base = makePlaceholder(48, 48);
        record.placeholder = true;
        record.status = "missing base: " + baseError;
    }
    alignEntityPlacementToDecodedImage(record, base.width, base.height);

    {
        RealmProfileScope scope("assets.decode_entity_mask");
        record.maskLoaded = loadDecodedImage(record.maskPath, mask, maskError);
    }
    if (record.maskLoaded) {
        if (mask.width != base.width || mask.height != base.height) {
            mask = resizeImage(mask, base.width, base.height);
        }
        compositeTeamMask(base, mask, request.teamColor);
    } else if (record.status.empty()) {
        record.status = "mask missing: " + maskError;
    }

    if (request.targetWidth > 0 && request.targetHeight > 0) {
        RealmProfileScope scope("assets.scale_entity");
        base = resizeImage(base, request.targetWidth, request.targetHeight);
    }
    record.width = base.width;
    record.height = base.height;
    {
        RealmProfileScope scope("assets.create_entity_texture");
        record.texture = textureFromImage(renderer, base);
    }
    if (!record.texture) {
        record.status = "texture creation failed";
    } else if (paths.usesZoomBase) {
        std::ostringstream status;
        status << "zoom stop " << paths.zoomStopSize;
        status << (record.maskLoaded ? " base + team mask loaded" : " base loaded");
        if (record.maskLoaded && !paths.usesZoomMask) {
            status << " with scaled fallback mask";
        } else if (!record.maskLoaded && !maskError.empty()) {
            status << "; mask missing: " << maskError;
        }
        record.status = status.str();
    } else if (record.status.empty()) {
        record.status = record.maskLoaded ? "base + team mask loaded" : "base loaded";
    }

    auto inserted = gTextureCache.emplace(key, std::move(record));
    return copyFrame(inserted.first->second);
}

bool tilesetEntityFrameExists(EntityType type, const std::string& action,
                              const std::string& direction, int frameIndex) {
    TilesetAssetRequest request{type, action, direction, frameIndex, SDL_Color{255,255,255,255}, 0, 0};
    for (const EntityFrameCandidate& candidate : entityFrameCandidates(request)) {
        if (std::filesystem::exists(entityFramePath(type, candidate.action, candidate.direction,
                                                    candidate.frameIndex, "_base.png"))) {
            return true;
        }
    }
    return false;
}

TilesetAssetFrame tilesetLoadGroundTile(SDL_Renderer* renderer, GroundType ground) {
    return loadImageTexture(renderer, groundTilePath(ground), "ground");
}

TilesetAssetFrame tilesetLoadUnknownGroundTile(SDL_Renderer* renderer) {
    return loadImageTexture(renderer, groundTilePath("unknown"), "ground.unknown");
}

TilesetAssetFrame tilesetLoadGroundTileScaled(SDL_Renderer* renderer, GroundType ground,
                                              int width, int height) {
    return loadImageTexture(renderer, groundTilePath(ground), "ground.scaled", width, height,
                            ImageTransform::Scaled);
}

TilesetAssetFrame tilesetLoadUnknownGroundTileScaled(SDL_Renderer* renderer,
                                                     int width, int height) {
    return loadImageTexture(renderer, groundTilePath("unknown"), "ground.unknown.scaled", width, height,
                            ImageTransform::Scaled);
}

TilesetAssetFrame tilesetLoadGroundTileIso(SDL_Renderer* renderer, GroundType ground,
                                           int width, int height) {
    return loadImageTexture(renderer, groundTilePath(ground), "ground.iso", width, height,
                            ImageTransform::ProjectedIso);
}

TilesetAssetFrame tilesetLoadUnknownGroundTileIso(SDL_Renderer* renderer,
                                                  int width, int height) {
    return loadImageTexture(renderer, groundTilePath("unknown"), "ground.unknown.iso", width, height,
                            ImageTransform::ProjectedIso);
}

TilesetAssetFrame tilesetLoadFeatureTileScaled(SDL_Renderer* renderer, FeatureType feature,
                                               FeatureState state, const std::string& layer,
                                               int width, int height) {
    return loadImageTexture(renderer, featureTilePath(feature, state, layer), "feature." + layer,
                            width, height, ImageTransform::Scaled);
}

TilesetAssetFrame tilesetLoadDecalTileScaled(SDL_Renderer* renderer, VisualDecalType decal,
                                             int width, int height) {
    return loadImageTexture(renderer, decalTilePath(decal), "decal", width, height,
                            ImageTransform::Scaled);
}

TilesetAssetFrame tilesetLoadProjectileTileScaled(SDL_Renderer* renderer, ProjectileType projectile,
                                                  int width, int height) {
    return loadImageTexture(renderer, projectileTilePath(projectile), "projectile", width, height,
                            ImageTransform::Scaled);
}

TilesetAssetFrame tilesetLoadEffectUiTileScaled(SDL_Renderer* renderer, const std::string& assetId,
                                                int width, int height) {
    return loadImageTexture(renderer, effectUiTilePath(assetId), "effects-ui", width, height,
                            ImageTransform::Scaled);
}

TilesetAssetFrame tilesetLoadScreenUiTileScaled(SDL_Renderer* renderer, const std::string& assetId,
                                                int width, int height) {
    return loadImageTexture(renderer, screenUiTilePath(assetId), "screen-ui", width, height,
                            ImageTransform::Scaled);
}

void tilesetAssetsClear() {
    for (auto& item : gTextureCache) {
        if (item.second.texture) SDL_DestroyTexture(item.second.texture);
    }
    gTextureCache.clear();
    gDecodedImageCache.clear();
    gEntityPlacementCache.clear();
}
