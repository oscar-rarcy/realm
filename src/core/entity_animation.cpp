#include "entity_animation.h"

#include <cstring>
#include <ostream>
#include <string>
#include <utility>

namespace {

constexpr AnimationFrameSpec PEASANT_IDLE_FRAMES[] = {
    {"relaxed", "Relaxed idle, arms at sides, both feet planted.", 20000},
    {"long_idle_arms_crossed", "Long-idle hold pose, arms crossed, both feet planted.", 0},
};

constexpr AnimationFrameSpec PEASANT_WALK_FRAMES[] = {
    {"near_leg_forward", "Walking gait with the front or near leg forward and the rear or far leg back.", 90},
    {"far_leg_forward", "Walking gait with the rear or far leg forward and the front or near leg back.", 90},
};

constexpr AnimationFrameSpec PEASANT_CHOP_WOOD_FRAMES[] = {
    {"axe_down", "Axe at the bottom or contact part of the chop, axe head low and forward.", 160},
    {"axe_up", "Axe raised high at the top of the swing.", 160},
};

constexpr AnimationFrameSpec PEASANT_MINE_GOLD_FRAMES[] = {
    {"pickaxe_down", "Pickaxe at the bottom or contact part of the mining swing, pick head low and forward.", 160},
    {"pickaxe_up", "Pickaxe raised high at the top of the swing.", 160},
};

constexpr AnimationFrameSpec PEASANT_GATHER_BERRIES_FRAMES[] = {
    {"hand_reaching", "One hand reaching out toward berries beside a basket.", 350},
    {"hand_in_basket", "Hand back in the basket with berries.", 350},
};

constexpr AnimationFrameSpec PEASANT_HOE_SOIL_FRAMES[] = {
    {"hoe_extended", "Arms outstretched with the hoe extended away from the body.", 260},
    {"hoe_pulled_in", "Arms pulled in after the hoe stroke while still holding the same farming hoe.", 260},
};

constexpr AnimationFrameSpec PEASANT_GATHER_WHEAT_FRAMES[] = {
    {"sickle_cut", "Using a sickle to cut wheat.", 260},
    {"free_hand_reach", "Still holding the sickle while the free hand reaches for wheat.", 260},
};

constexpr AnimationFrameSpec PEASANT_BUILD_FRAMES[] = {
    {"hammer_up", "Kneeling builder with hammer raised up.", 150},
    {"hammer_down", "Kneeling builder with hammer down.", 150},
};

constexpr AnimationFrameSpec PEASANT_CARRY_WOOD_FRAMES[] = {
    {"near_leg_forward", "Carrying bundled logs while walking, front or near leg forward.", 90},
    {"far_leg_forward", "Carrying bundled logs while walking, rear or far leg forward.", 90},
};

constexpr AnimationFrameSpec PEASANT_CARRY_GOLD_FRAMES[] = {
    {"near_leg_forward", "Carrying stones and gold ore while walking, front or near leg forward.", 90},
    {"far_leg_forward", "Carrying stones and gold ore while walking, rear or far leg forward.", 90},
};

constexpr AnimationFrameSpec PEASANT_CARRY_BERRIES_FRAMES[] = {
    {"near_leg_forward", "Carrying berries while walking, front or near leg forward.", 90},
    {"far_leg_forward", "Carrying berries while walking, rear or far leg forward.", 90},
};

constexpr AnimationFrameSpec PEASANT_CARRY_WHEAT_FRAMES[] = {
    {"near_leg_forward", "Carrying wheat while walking, front or near leg forward.", 90},
    {"far_leg_forward", "Carrying wheat while walking, rear or far leg forward.", 90},
};

constexpr AnimationFrameSpec PEASANT_GATHER_MEAT_FRAMES[] = {
    {"knife_cut", "Holding a knife while actively cutting or reaching toward meat.", 260},
    {"free_hand_take", "Still holding the knife while taking meat with the free hand.", 260},
};

constexpr AnimationFrameSpec PEASANT_CARRY_MEAT_FRAMES[] = {
    {"near_leg_forward", "Carrying meat while walking, front or near leg forward.", 90},
    {"far_leg_forward", "Carrying meat while walking, rear or far leg forward.", 90},
};

constexpr AnimationFrameSpec PEASANT_CLUB_ATTACK_FRAMES[] = {
    {"club_up", "Club at the top of the attack swing, held overhead but still inside the tile.", 130},
    {"club_down", "Club at the bottom or contact part of the attack swing, still fully inside the tile.", 130},
};

constexpr AnimationFrameSpec PEASANT_DEATH_FRAMES[] = {
    {"dead", "Dead villager body lying on the ground, not a skeleton.", 30000},
    {"decayed", "Skeleton remains of the same villager in the same ground area, with small clothing and tool scraps.", 0},
};

constexpr AnimationFrameSpec HUMAN_DEATH_FRAMES[] = {
    {"dead", "Dead human body lying on the ground with clothing, armour, weapons, and equipment still intact.", 30000},
    {"decayed", "Human skeleton remains in the same ground area, with armour, weapons, bows, shields, tools, and other equipment still intact and readable.", 0},
};

constexpr AnimationFrameSpec ANIMAL_DEATH_FRAMES[] = {
    {"dead", "Dead animal body lying on the ground, species silhouette still readable.", 30000},
    {"decayed", "Animal skeleton remains in the same ground area, species silhouette still readable.", 0},
};

constexpr AnimationFrameSpec WRECK_DEATH_FRAMES[] = {
    {"dead", "Destroyed unit wreck lying on the ground or water, broken but still recognizable.", 30000},
    {"decayed", "Weathered wreckage remains in the same area, with durable wood, metal, wheels, hull, or siege parts still readable.", 0},
};

constexpr EntityActionAnimationSpec HUMAN_DEATH_ACTION =
    {E_NONE, "death", "Human unit dies, then decays into skeleton remains while armour and equipment persist.",
     "one_shot", ActionTargetRelation::SelfTile, 0, false, true, 30000, "lying", "", "",
     HUMAN_DEATH_FRAMES, 2};

constexpr EntityActionAnimationSpec ANIMAL_DEATH_ACTION =
    {E_NONE, "death", "Animal dies, then decays into skeleton remains.",
     "one_shot", ActionTargetRelation::SelfTile, 0, false, true, 30000, "lying", "", "",
     ANIMAL_DEATH_FRAMES, 2};

constexpr EntityActionAnimationSpec WRECK_DEATH_ACTION =
    {E_NONE, "death", "Vehicle or siege unit is destroyed, then decays into persistent wreckage.",
     "one_shot", ActionTargetRelation::SelfTile, 0, false, true, 30000, "lying", "", "",
     WRECK_DEATH_FRAMES, 2};

constexpr EntityActionAnimationSpec PEASANT_ACTIONS[] = {
    {E_PEASANT, "idle", "Worker is standing idle, then settles into a long-idle arms-crossed pose.",
     "idle", ActionTargetRelation::SelfTile, 0, false, true, 20000, "standing", "", "",
     PEASANT_IDLE_FRAMES, 2},
    {E_PEASANT, "walk", "Worker walks across the map with an alternating two-step gait.",
     "gait", ActionTargetRelation::SelfTile, 0, true, false, 0, "standing", "", "",
     PEASANT_WALK_FRAMES, 2},
    {E_PEASANT, "chop_wood", "Worker chops an adjacent tree or wood resource with an axe.",
     "swing", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "wide_tool", "wood axe", "",
     PEASANT_CHOP_WOOD_FRAMES, 2},
    {E_PEASANT, "mine_gold", "Worker mines an adjacent gold deposit with a pickaxe.",
     "swing", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "wide_tool", "pickaxe", "",
     PEASANT_MINE_GOLD_FRAMES, 2},
    {E_PEASANT, "gather_berries", "Worker gathers berries from an adjacent berry resource.",
     "gather", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "kneeling", "", "",
     PEASANT_GATHER_BERRIES_FRAMES, 2},
    {E_PEASANT, "hoe_soil", "Worker tends a farm on an adjacent tile with a farming hoe.",
     "work_stroke", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "wide_tool",
     "long-handled farming hoe with a small flat rectangular blade, not an axe or pickaxe", "",
     PEASANT_HOE_SOIL_FRAMES, 2},
    {E_PEASANT, "gather_wheat", "Worker gathers wheat with a sickle.",
     "gather", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "wide_tool", "sickle", "",
     PEASANT_GATHER_WHEAT_FRAMES, 2},
    {E_PEASANT, "build", "Worker kneels and hammers an adjacent construction site.",
     "hammer", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "kneeling", "hammer", "",
     PEASANT_BUILD_FRAMES, 2},
    {E_PEASANT, "carry_wood", "Worker carries wood back to a drop-off while walking.",
     "carry_gait", ActionTargetRelation::SelfTile, 0, true, false, 0, "standing", "",
     "bundled logs held securely in both arms", PEASANT_CARRY_WOOD_FRAMES, 2},
    {E_PEASANT, "carry_gold", "Worker carries gold ore back to a drop-off while walking.",
     "carry_gait", ActionTargetRelation::SelfTile, 0, true, false, 0, "standing", "",
     "pile of grey stones and yellow gold ore held in both arms", PEASANT_CARRY_GOLD_FRAMES, 2},
    {E_PEASANT, "carry_berries", "Worker carries a basket of berries back to a drop-off while walking.",
     "carry_gait", ActionTargetRelation::SelfTile, 0, true, false, 0, "standing", "",
     "basket of red berries held in both arms", PEASANT_CARRY_BERRIES_FRAMES, 2},
    {E_PEASANT, "carry_wheat", "Worker carries wheat back to a drop-off while walking.",
     "carry_gait", ActionTargetRelation::SelfTile, 0, true, false, 0, "standing", "",
     "bundle of wheat held securely in both arms", PEASANT_CARRY_WHEAT_FRAMES, 2},
    {E_PEASANT, "gather_meat", "Worker gathers meat from an adjacent animal carcass with a knife.",
     "gather", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "kneeling", "knife", "",
     PEASANT_GATHER_MEAT_FRAMES, 2},
    {E_PEASANT, "carry_meat", "Worker carries meat back to a drop-off while walking.",
     "carry_gait", ActionTargetRelation::SelfTile, 0, true, false, 0, "standing", "",
     "large cut of meat held in both arms", PEASANT_CARRY_MEAT_FRAMES, 2},
    {E_PEASANT, "club_attack", "Worker attacks an adjacent target with a wooden club.",
     "swing", ActionTargetRelation::AdjacentTarget, 1, true, false, 0, "wide_tool", "wooden club", "",
     PEASANT_CLUB_ATTACK_FRAMES, 2},
    {E_PEASANT, "death", "Worker dies, then the body decays into skeleton remains.",
     "one_shot", ActionTargetRelation::SelfTile, 0, false, true, 30000, "lying", "", "",
     PEASANT_DEATH_FRAMES, 2},
};

const char* peasantCarryAction(const Entity& e) {
    if (e.cargo.type == CR_WOOD) return "carry_wood";
    if (e.cargo.type == CR_GOLD) return "carry_gold";
    if (e.cargo.type == CR_FISH) return "carry_meat";
    if (e.cargo.type == CR_FOOD) {
        Terrain sourceTerrain = inBounds(e.cargo.sourceX, e.cargo.sourceY)
            ? g.map[e.cargo.sourceY][e.cargo.sourceX].terrain
            : T_GRASS;
        if (sourceTerrain == T_BERRY) return "carry_berries";
        if (sourceTerrain == T_WHEAT) return "carry_wheat";
        return "carry_meat";
    }
    return "walk";
}

const char* peasantGatherAction(const Entity& e) {
    int rx = inBounds(e.resourceX, e.resourceY) ? e.resourceX : e.targetX;
    int ry = inBounds(e.resourceX, e.resourceY) ? e.resourceY : e.targetY;
    Terrain targetTerrain = inBounds(rx, ry) ? g.map[ry][rx].terrain : T_GRASS;
    switch (targetTerrain) {
        case T_FOREST:
        case T_PINE:
        case T_PALM:
        case T_DEAD_TREE:
            return "chop_wood";
        case T_GOLD:
            return "mine_gold";
        case T_BERRY:
            return "gather_berries";
        case T_WHEAT:
            return "gather_wheat";
        default:
            break;
    }
    if (e.cargo.type == CR_FOOD || e.cargo.type == CR_FISH) return "gather_meat";
    return "gather_berries";
}

const char* peasantBuildAction(const Entity& e) {
    Entity* target = findEntity(e.targetId);
    if (target && target->type == E_FARM && !target->underConstruction) return "hoe_soil";
    return "build";
}

void jsonString(std::ostream& out, const char* value) {
    out << '"';
    if (value) {
        for (const char* p = value; *p; ++p) {
            switch (*p) {
                case '"': out << "\\\""; break;
                case '\\': out << "\\\\"; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default: out << *p; break;
            }
        }
    }
    out << '"';
}

const char* entitySlug(EntityType type) {
    switch (type) {
        case E_PEASANT: return "peasant";
        case E_MILITIA: return "militia";
        case E_ARCHER: return "archer";
        case E_KNIGHT: return "knight";
        case E_SPEARMAN: return "spearman";
        case E_CATAPULT: return "catapult";
        case E_TREBUCHET: return "trebuchet";
        case E_FISHING_BOAT: return "fishing_boat";
        case E_WARSHIP: return "warship";
        case E_TRANSPORT: return "transport";
        case E_RAM: return "ram";
        case E_DEER: return "deer";
        case E_WOLF: return "wolf";
        case E_SHEEP: return "sheep";
        case E_BOAR: return "boar";
        default: return "unknown";
    }
}

const EntityActionAnimationSpec* deathActionSpecFor(EntityType type) {
    switch (type) {
        case E_PEASANT: return &PEASANT_ACTIONS[15];
        case E_MILITIA:
        case E_SPEARMAN:
        case E_ARCHER:
        case E_KNIGHT:
            return &HUMAN_DEATH_ACTION;
        case E_DEER:
        case E_WOLF:
        case E_SHEEP:
        case E_BOAR:
            return &ANIMAL_DEATH_ACTION;
        case E_CATAPULT:
        case E_TREBUCHET:
        case E_FISHING_BOAT:
        case E_WARSHIP:
        case E_TRANSPORT:
        case E_RAM:
            return &WRECK_DEATH_ACTION;
        default:
            return nullptr;
    }
}

} // namespace

const char* actionTargetRelationId(ActionTargetRelation relation) {
    switch (relation) {
        case ActionTargetRelation::SelfTile: return "self_tile";
        case ActionTargetRelation::AdjacentTarget: return "adjacent_target_tile_or_entity";
        case ActionTargetRelation::RangedTarget: return "ranged_target";
        case ActionTargetRelation::ProjectileToTarget: return "projectile_to_target";
        case ActionTargetRelation::HiddenOrInside: return "hidden_or_inside";
    }
    return "unknown";
}

EntityType entityTypeForAnimationSlug(const char* slug) {
    if (slug && std::strcmp(slug, "villager") == 0) return E_PEASANT;
    for (int i = E_PEASANT; i <= E_BOAR; ++i) {
        EntityType type = (EntityType)i;
        if (slug && std::strcmp(slug, entitySlug(type)) == 0) return type;
    }
    return E_NONE;
}

const EntityActionAnimationSpec* findEntityActionAnimationSpec(EntityType type, const char* action) {
    if (!action) return nullptr;
    if (action && std::strcmp(action, "death") == 0)
        if (const EntityActionAnimationSpec* spec = deathActionSpecFor(type)) return spec;
    if (type == E_PEASANT) {
        for (const auto& spec : PEASANT_ACTIONS) {
            if (std::strcmp(spec.action, action) == 0) return &spec;
        }
    }
    return nullptr;
}

const char* entityAnimationActionId(const Entity& e) {
    if (!e.alive || e.state == S_DEAD) return "death";
    if (e.type == E_PEASANT) {
        if (e.cargo.amount > 0 && (e.state == S_RETURNING || e.state == S_MOVING || e.pathIdx < (int)e.path.size()))
            return peasantCarryAction(e);
        if (e.state == S_MOVING || e.state == S_RETURNING || e.pathIdx < (int)e.path.size()) return "walk";
        if (e.state == S_GATHERING) return peasantGatherAction(e);
        if (e.state == S_BUILDING) return peasantBuildAction(e);
        if (e.state == S_ATTACKING) return "club_attack";
        return "idle";
    }
    return "idle";
}

const EntityActionAnimationSpec* entityActionAnimationSpecFor(const Entity& e) {
    const char* action = entityAnimationActionId(e);
    return findEntityActionAnimationSpec(e.type, action);
}

namespace {

int signum(int value) {
    return (value > 0) - (value < 0);
}

std::pair<int, int> entityAnimationFacingDelta(const Entity& e) {
    if (e.pathIdx < (int)e.path.size()) {
        return {signum(e.path[e.pathIdx].first - e.x), signum(e.path[e.pathIdx].second - e.y)};
    }
    if (inBounds(e.targetX, e.targetY) && (e.targetX != e.x || e.targetY != e.y)) {
        return {signum(e.targetX - e.x), signum(e.targetY - e.y)};
    }
    return {signum(e.facingDx), signum(e.facingDy)};
}

} // namespace

const char* entityAnimationDirectionBucket(const Entity& e) {
    auto [dx, dy] = entityAnimationFacingDelta(e);
    int screenY = dx + dy;
    return screenY < 0 ? "back" : "front";
}

bool entityAnimationMirrorHorizontal(const Entity& e) {
    auto [dx, dy] = entityAnimationFacingDelta(e);
    int screenX = dx - dy;
    return screenX < 0;
}

int entityActionAnimationSpecCount(EntityType type) {
    if (type == E_PEASANT) return (int)(sizeof(PEASANT_ACTIONS) / sizeof(PEASANT_ACTIONS[0]));
    if (deathActionSpecFor(type)) return 1;
    return 0;
}

const EntityActionAnimationSpec* entityActionAnimationSpecAt(EntityType type, int index) {
    if (type == E_PEASANT && index >= 0 && index < entityActionAnimationSpecCount(type)) {
        return &PEASANT_ACTIONS[index];
    }
    if (index == 0) return deathActionSpecFor(type);
    return nullptr;
}

bool writeEntityAnimationSpecJson(std::ostream& out, EntityType type) {
    int count = entityActionAnimationSpecCount(type);
    if (count <= 0) return false;
    out << "{\n";
    out << "  \"schema\": \"realm.unit_animation_spec.v1\",\n";
    out << "  \"entity\": ";
    jsonString(out, entitySlug(type));
    out << ",\n";
    out << "  \"directions\": [\"front\", \"back\"],\n";
    out << "  \"runtime_mirrors_horizontal\": true,\n";
    out << "  \"projection\": \"isometric terrain diamonds with upright sprites\",\n";
    out << "  \"actions\": [\n";
    for (int i = 0; i < count; ++i) {
        const EntityActionAnimationSpec& spec = *entityActionAnimationSpecAt(type, i);
        out << "    {\n";
        out << "      \"id\": ";
        jsonString(out, spec.action);
        out << ",\n";
        out << "      \"description\": ";
        jsonString(out, spec.description);
        out << ",\n";
        out << "      \"family\": ";
        jsonString(out, spec.family);
        out << ",\n";
        out << "      \"target_relation\": ";
        jsonString(out, actionTargetRelationId(spec.targetRelation));
        out << ",\n";
        out << "      \"range_tiles\": " << spec.rangeTiles << ",\n";
        out << "      \"loop\": " << (spec.loop ? "true" : "false") << ",\n";
        out << "      \"hold_last\": " << (spec.holdLast ? "true" : "false") << ",\n";
        out << "      \"transition_after_ms\": " << spec.transitionAfterMs << ",\n";
        out << "      \"fit_profile\": ";
        jsonString(out, spec.fitProfile);
        out << ",\n";
        out << "      \"tool\": ";
        jsonString(out, spec.tool);
        out << ",\n";
        out << "      \"carry\": ";
        jsonString(out, spec.carriedObject);
        out << ",\n";
        out << "      \"frames\": [\n";
        for (int f = 0; f < spec.frameCount; ++f) {
            const AnimationFrameSpec& frame = spec.frames[f];
            out << "        {\"id\": ";
            jsonString(out, frame.id);
            out << ", \"description\": ";
            jsonString(out, frame.description);
            out << ", \"duration_ms\": " << frame.durationMs << "}";
            if (f + 1 < spec.frameCount) out << ",";
            out << "\n";
        }
        out << "      ]\n";
        out << "    }";
        if (i + 1 < count) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}
