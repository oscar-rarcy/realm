#include "realm.h"
#include "entity_animation.h"
#include "sim/save_migration.h"
#include "sim/save_reader.h"
#include "sim/save_writer.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <system_error>
#include <utility>

static std::string lowerAssetSlug(const std::string& text) {
    std::string out;
    bool underscore = false;
    for (unsigned char raw : text) {
        char ch = (char)std::tolower(raw);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            out.push_back(ch);
            underscore = false;
        } else if (!underscore && !out.empty()) {
            out.push_back('_');
            underscore = true;
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    return out.empty() ? "unknown" : out;
}

static bool featureUsesHarvestStates(FeatureType feature) {
    switch (feature) {
        case F_FOREST:
        case F_PINE:
        case F_PALM:
        case F_DEAD_TREE:
        case F_BERRY_BUSH:
        case F_WHEAT_CROP:
        case F_FISH_SHOAL:
        case F_GOLD_DEPOSIT:
            return true;
        default:
            return false;
    }
}

static std::vector<const char*> featureAuditStates(FeatureType feature) {
    if (featureUsesHarvestStates(feature)) {
        return {"full", "mostly_full", "mostly_empty", "depleted"};
    }
    if (feature == F_CASTLE_GATE) return {"open", "closed", "locked"};
    return {"default"};
}

int dumpMissingTilesetAssets() {
    namespace fs = std::filesystem;
    int missing = 0;
    auto report = [&](const std::string& kind, const std::string& key, const fs::path& path) {
        if (fs::exists(path)) return;
        std::cout << kind << " | " << key << " | missing=" << path.generic_string() << "\n";
        missing++;
    };

    bool sawGround[32] = {};
    bool sawFeature[32] = {};
    bool sawDecal[32] = {};
    for (int t = T_GRASS; t <= T_CASTLE_GATE; ++t) {
        Tile tile{};
        tile.terrain = (Terrain)t;
        tile.biome = (t == T_SNOW || t == T_PINE) ? B_SNOW : B_TEMPERATE;
        tile.resources = 100;
        VisualTileParts parts = visualPartsForTile(tile);
        if (!sawGround[(int)parts.ground]) {
            sawGround[(int)parts.ground] = true;
            std::string name = groundTypeName(parts.ground);
            report("ground", name, fs::path("assets") / "tiles" / "grounds" / (name + ".png"));
        }
        if (parts.feature != F_NONE && !sawFeature[(int)parts.feature]) {
            sawFeature[(int)parts.feature] = true;
            std::string name = featureTypeName(parts.feature);
            fs::path featureRoot = fs::path("assets") / "tiles" / "features" / name;
            report("feature", name, featureRoot / "manifest.json");
            for (const char* state : featureAuditStates(parts.feature)) {
                fs::path stateRoot = featureRoot / state;
                if (featureConceals(parts.feature)) {
                    report("feature", name + "/" + state + "/back", stateRoot / "back.png");
                    report("feature", name + "/" + state + "/front_occluder", stateRoot / "front_occluder.png");
                } else {
                    report("feature", name + "/" + state, stateRoot / "base.png");
                }
            }
        }
        for (VisualDecalType decal : parts.decals) {
            if (sawDecal[(int)decal]) continue;
            sawDecal[(int)decal] = true;
            std::string name = visualDecalName(decal);
            report("decal", name, fs::path("assets") / "tiles" / "decals" / (name + ".png"));
        }
    }
    for (int ground = (int)G_GRASS; ground <= (int)G_CASTLE_FLOOR; ++ground) {
        if (sawGround[ground]) continue;
        sawGround[ground] = true;
        std::string name = groundTypeName((GroundType)ground);
        report("ground", name, fs::path("assets") / "tiles" / "grounds" / (name + ".png"));
    }
    for (int feature = (int)F_FOREST; feature <= (int)F_CASTLE_GATE; ++feature) {
        if (sawFeature[feature]) continue;
        sawFeature[feature] = true;
        FeatureType featureType = (FeatureType)feature;
        std::string name = featureTypeName(featureType);
        fs::path featureRoot = fs::path("assets") / "tiles" / "features" / name;
        report("feature", name, featureRoot / "manifest.json");
        for (const char* state : featureAuditStates(featureType)) {
            fs::path stateRoot = featureRoot / state;
            if (featureConceals(featureType)) {
                report("feature", name + "/" + state + "/back", stateRoot / "back.png");
                report("feature", name + "/" + state + "/front_occluder", stateRoot / "front_occluder.png");
            } else {
                report("feature", name + "/" + state, stateRoot / "base.png");
            }
        }
    }
    for (int decal = (int)VD_ROAD; decal <= (int)VD_SNOW_TRAMPLED_PATH; ++decal) {
        if (sawDecal[decal]) continue;
        sawDecal[decal] = true;
        std::string name = visualDecalName((VisualDecalType)decal);
        report("decal", name, fs::path("assets") / "tiles" / "decals" / (name + ".png"));
    }

    const char* projectileSlugs[] = {
        "arrow", "crossbow_bolt", "flaming_arrow", "tower_bolt",
        "warship_arrow_volley", "catapult_boulder", "trebuchet_boulder"
    };
    for (const char* projectile : projectileSlugs) {
        report("projectile", projectile,
               fs::path("assets") / "tiles" / "projectiles" / projectile / "manifest.json");
    }

    const char* effects[] = {
        "arrow_projectile", "tower_bolt_projectile", "warship_shot_projectile",
        "catapult_boulder_projectile", "melee_hit_spark", "arrow_hit",
        "boulder_impact", "boulder_water_splash", "building_hit_dust",
        "rain_frame_1", "rain_frame_2", "storm_rain_frame_1", "storm_rain_frame_2",
        "snowfall_frame_1", "snowfall_frame_2", "move_marker", "attack_marker",
        "gather_marker", "build_marker", "rally_marker", "attack_move_marker",
        "hold_position_marker", "selection_ring", "group_selection_ring",
        "range_ring_dot", "build_preview_valid", "build_preview_invalid",
        "wall_preview", "garrison_indicator", "queued_unit_marker",
        "research_active_marker", "completed_research_icon_treatment"
    };
    for (const char* effect : effects)
        report("effect-ui", effect, fs::path("assets") / "tiles" / "effects-ui" / (std::string(effect) + ".png"));

    for (int type = E_PEASANT; type < E_TYPE_COUNT; ++type) {
        EntityType entityType = (EntityType)type;
        std::string slug = lowerAssetSlug(STATS[entityType].name ? STATS[entityType].name : "unknown");
        fs::path entityRoot = fs::path("assets") / "tiles" / "entities" / slug;
        report("entity", slug, entityRoot / "manifest.json");
        int count = entityActionAnimationSpecCount(entityType);
        for (int i = 0; i < count; ++i) {
            const EntityActionAnimationSpec* spec = entityActionAnimationSpecAt(entityType, i);
            if (!spec || !spec->action) continue;
            const char* dirs[] = {"front", "back"};
            for (const char* dir : dirs) {
                for (int f = 0; f < std::max(1, spec->frameCount); ++f) {
                    std::ostringstream frame;
                    frame << "frame_" << (f < 10 ? "0" : "") << f << "_base.png";
                    fs::path path = entityRoot / spec->action / dir / frame.str();
                    report("entity", slug + "/" + spec->action + "/" + dir, path);
                }
            }
        }
    }

    std::cout << "missing_tileset_assets=" << missing << "\n";
    return missing == 0 ? 0 : 1;
}

bool saveGame(Game& game, const std::string& path) {
    return writeSaveFile(game, path);
}

static void recalculateSupply(Game& game) {
    for (int owner = 0; owner < MAX_PLAYERS; owner++) {
        updateSupply(game, owner);
    }
}

static bool validateOrRecoverLoadedGame(Game& game) {
    std::vector<ValidationIssue> issues = validateGameStateIssues(game);
    if (issues.empty()) return true;
    for (const ValidationIssue& issue : issues)
        if (issue.severity == ValidationSeverity::Error) return false;
    RecoveryResult recovery = recoverGameState(game, issues);
    return recovery.recovered;
}

static bool hydrateLoadedGameInto(Game& target, Game&& ng, int version) {
    if (!migrateLoadedGame(ng, version)) return false;
    recalculateSupply(ng);
    if (!validateOrRecoverLoadedGame(ng)) return false;
    setHumanPlayerColorHue(ng, target.playerColorHue[0]);
    configurePlayerColorHues(ng, ng.startupAIs);
    target = std::move(ng);
    return true;
}

static bool loadParsedGame(const std::string& path, Game& parsed, int& version) {
    return readSaveFile(path, parsed, version);
}

bool loadGame(Game& game, const std::string& path) {
    auto parsed = std::make_unique<Game>();
    int version = 0;
    if (!loadParsedGame(path, *parsed, version)) return false;
    return hydrateLoadedGameInto(game, std::move(*parsed), version);
}
