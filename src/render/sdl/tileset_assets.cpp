#include "tileset_assets.h"
#include "realm.h"
#include "render/entity_visual_defs.h"

#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
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
    int width = 0;
    int height = 0;
    std::string basePath;
    std::string maskPath;
    std::string status;
};

std::unordered_map<std::string, TextureRecord> gTextureCache;

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
    ss << (int)request.type << '|' << request.action << '|' << request.direction << '|'
       << request.frameIndex << '|' << (int)request.teamColor.r << ','
       << (int)request.teamColor.g << ',' << (int)request.teamColor.b;
    return ss.str();
}

TilesetAssetFrame copyFrame(const TextureRecord& record) {
    return TilesetAssetFrame{record.texture, record.baseLoaded, record.maskLoaded, record.placeholder,
                             record.width, record.height, record.basePath, record.maskPath, record.status};
}

} // namespace

std::string tilesetEntitySlug(EntityType type) {
    if (type <= E_NONE || type > E_BOAR) return "unknown";
    return entityAssetSlug(type);
}

TilesetAssetFrame tilesetLoadEntityFrame(SDL_Renderer* renderer, const TilesetAssetRequest& request) {
    if (!renderer || request.type == E_NONE || request.action.empty() || request.direction.empty()) {
        return {};
    }

    std::string key = cacheKey(request);
    auto found = gTextureCache.find(key);
    if (found != gTextureCache.end()) return copyFrame(found->second);

    TextureRecord record;
    record.basePath = entityFramePath(request.type, request.action, request.direction,
                                      request.frameIndex, "_base.png").generic_string();
    record.maskPath = entityFramePath(request.type, request.action, request.direction,
                                      request.frameIndex, "_teammask.png").generic_string();

    Image base;
    Image mask;
    std::string baseError;
    std::string maskError;
    record.baseLoaded = readPngRgba(record.basePath, base, baseError);
    if (!record.baseLoaded) {
        base = makePlaceholder(48, 48);
        record.placeholder = true;
        record.status = "missing base: " + baseError;
    }

    record.maskLoaded = readPngRgba(record.maskPath, mask, maskError);
    if (record.maskLoaded) {
        compositeTeamMask(base, mask, request.teamColor);
    } else if (record.status.empty()) {
        record.status = "mask missing: " + maskError;
    }

    record.width = base.width;
    record.height = base.height;
    record.texture = textureFromImage(renderer, base);
    if (!record.texture) {
        record.status = "texture creation failed";
    } else if (record.status.empty()) {
        record.status = record.maskLoaded ? "base + team mask loaded" : "base loaded";
    }

    auto inserted = gTextureCache.emplace(key, std::move(record));
    return copyFrame(inserted.first->second);
}

bool tilesetEntityFrameExists(EntityType type, const std::string& action,
                              const std::string& direction, int frameIndex) {
    return std::filesystem::exists(entityFramePath(type, action, direction, frameIndex, "_base.png"));
}

void tilesetAssetsClear() {
    for (auto& item : gTextureCache) {
        if (item.second.texture) SDL_DestroyTexture(item.second.texture);
    }
    gTextureCache.clear();
}
