#include "realm.h"

const EntityStats STATS[E_TYPE_COUNT] = {
    {"None",       ' ',   0, 0,0,0,0,  0,  0,  0, 1,1,  0,0, false, 0},
    {"Peasant",    'p',  35, 3,1,3,8, 50,  0, 40, 1,1,  0,1, false, TR_WORKER|TR_GATHERER|TR_BUILDER},
    {"Militia",    'm',  70, 8,1,3,6, 60,  0, 50, 1,1,  0,1, false, TR_MILITARY|TR_INFANTRY},
    {"Archer",     'a',  45, 6,5,3,7, 70,  0, 60, 1,1,  0,1, false, TR_MILITARY|TR_INFANTRY|TR_RANGED},
    {"Knight",     'k', 110,14,1,1,5,120,  0, 80, 1,1,  0,2, false, TR_MILITARY},
    {"Spearman",   's',  55, 5,1,3,6, 40,  0, 55, 1,1,  0,1, false, TR_MILITARY|TR_INFANTRY},
    {"Catapult",   'c',  70,25,8,8,12,150, 40, 90, 1,1,  0,3, false, TR_MILITARY|TR_RANGED|TR_SIEGE},
    {"Trebuchet",  'q',  90,35,12,8,18,200,250,140, 1,1, 0,4, false, TR_MILITARY|TR_RANGED|TR_SIEGE},
    {"Fishing Boat",'b', 40, 0,0,5, 0, 80, 50, 60, 1,1,  0,1, false, TR_GATHERER|TR_NAVAL},
    {"Warship",    'V',  70, 9,5,4, 7,150, 80,100, 1,1,  0,3, false, TR_MILITARY|TR_RANGED|TR_NAVAL},
    {"Transport",  'F',  80, 0,0,5, 0, 80, 40, 70, 1,1,  0,2, false, TR_NAVAL|TR_GARRISON},
    {"Ram",        '-',  80,28,1,6, 9, 70, 80, 80, 1,1,  0,3, false, TR_MILITARY|TR_SIEGE},
    {"Town Hall",  'H', 240, 0,0,0,0,200,150,100, 3,3, 10,0, true, TR_DROPOFF|TR_TRAINS_UNITS|TR_GARRISON},
    {"House",      'h', 100, 0,0,0,0,  0, 50, 60, 2,2,  5,0, true, TR_GARRISON},
    {"Barracks",   'B', 120, 0,0,0,0,  0,150, 80, 3,2,  0,0, true, TR_TRAINS_UNITS},
    {"Stable",     'S', 140, 0,0,0,0,  0,200,100, 3,2,  0,0, true, TR_TRAINS_UNITS},
    {"Tower",      'X', 130,10,7,0,7, 50,100, 70, 1,1,  0,0, true, TR_DEFENSE|TR_RANGED|TR_GARRISON},
    {"Farm",       '%',  20, 0,0,0,0,  0,  0, 15, 1,1,  0,0, true, 0},
    {"Blacksmith", 'A', 100, 0,0,0,0,  0,120, 70, 2,2,  0,0, true, 0},
    {"Church",     '+', 120, 0,0,0,0, 80,100, 90, 2,2,  0,0, true, 0},
    {"Market",     'M', 100, 0,0,0,0,  0,100, 60, 2,2,  0,0, true, 0},
    {"Wall",       '#', 120, 0,0,0,0,  0, 20, 15, 1,1,  0,0, true, 0},
    {"Gate",       'G',  90, 0,0,0,0,  0, 30, 20, 1,1,  0,0, true, 0},
    {"Castle",     'W', 350, 0,0,0,0,100,250,150, 4,4, 15,0, true, TR_TRAINS_UNITS|TR_DEFENSE|TR_GARRISON|TR_DROPOFF},
    {"Lumber Camp",'L',  80, 0,0,0,0,  0, 80,  0, 2,2,  0,0, true, TR_DROPOFF},
    {"Mining Camp",'N',  80, 0,0,0,0, 30, 60,  0, 2,2,  0,0, true, TR_DROPOFF},
    {"Mill",       'O', 100, 0,0,0,0,  0,100, 80, 2,2,  0,0, true, TR_DROPOFF},
    {"Dock",       'D', 100, 0,0,0,0,  0,100, 70, 2,2,  0,0, true, TR_DROPOFF|TR_TRAINS_UNITS},
    {"Deer",       'd',  20, 0,0,2,0,  0,  0,  0, 1,1,  0,0, false, TR_WILD_ANIMAL},
    {"Wolf",       'w',  35, 4,1,2,8,  0,  0,  0, 1,1,  0,0, false, TR_WILD_ANIMAL|TR_HOSTILE_WILDLIFE},
    {"Sheep",      's',  12, 0,0,3,0,  0,  0,  0, 1,1,  0,0, false, TR_WILD_ANIMAL},
    {"Boar",       'o',  25, 3,1,2,6,  0,  0,  0, 1,1,  0,0, false, TR_WILD_ANIMAL|TR_HOSTILE_WILDLIFE},
};

static_assert(sizeof(STATS) / sizeof(STATS[0]) == E_TYPE_COUNT, "STATS must match EntityType");

const EntityDefinition& entityDef(EntityType type) {
    if (type < E_NONE || type >= E_TYPE_COUNT) return STATS[E_NONE];
    return STATS[type];
}

BuildingVisualState buildingVisualState(const Entity& e) {
    if (e.underConstruction) {
        int pct = e.hp * 100 / std::max(1, e.maxHp);
        if (pct <= 33) return BVS_CONSTRUCTION_0_FOUNDATION;
        if (pct <= 66) return BVS_CONSTRUCTION_1_FRAME;
        return BVS_CONSTRUCTION_2_NEARLY_COMPLETE;
    }
    if (e.researching == R_IRON_WEAPONS) return BVS_RESEARCHING_IRON_WEAPONS;
    if (e.researching == R_CROSSBOWS) return BVS_RESEARCHING_CROSSBOWS;
    if (e.researching == R_PIKES) return BVS_RESEARCHING_PIKES;
    if (e.researching == R_COUNTERWEIGHT) return BVS_RESEARCHING_COUNTERWEIGHT;
    if (e.researching == R_PLATE_HELM) return BVS_RESEARCHING_PLATE_HELM;
    if (e.producing != E_NONE) {
        if (e.producing == E_PEASANT) return BVS_TRAINING_PEASANT;
        if (e.producing == E_MILITIA || e.producing == E_ARCHER || e.producing == E_SPEARMAN) return BVS_TRAINING_INFANTRY;
        if (e.producing == E_KNIGHT) return BVS_TRAINING_CAVALRY;
        if (isNaval(e.producing)) return BVS_TRAINING_SHIP;
    }
    if (!e.garrison.empty()) return e.atkCd > 0 ? BVS_GARRISON_FIRING : BVS_GARRISONED;
    if (e.hp > 0 && e.hp * 2 <= std::max(1, e.maxHp)) return BVS_DAMAGED;
    return BVS_COMPLETE;
}

AnimalCarcassVisualState animalCarcassVisualState(const Entity& e) {
    if (e.alive && e.state != S_DEAD) return ACVS_ALIVE;
    if (e.carcassFoodMax <= 0 || e.carcassFoodRemaining <= 0 || e.deathTicks >= DEATH_DECAY_TICKS)
        return ACVS_DEPLETED_SKELETON;
    int pct = e.carcassFoodRemaining * 100 / std::max(1, e.carcassFoodMax);
    if (pct >= 76) return ACVS_DEAD_UNHARVESTED;
    if (pct >= 36) return ACVS_PARTLY_HARVESTED;
    return ACVS_MOSTLY_HARVESTED;
}

TransportVisualState transportVisualState(const Entity& e) {
    if (!e.alive || e.state == S_DEAD)
        return e.deathTicks >= DEATH_DECAY_TICKS ? TVS_DECAYED_WRECK : TVS_WRECK;
    if (e.state == S_ENTERING) return TVS_LOAD_UNLOAD;
    int cap = garrisonCap(E_TRANSPORT);
    if (e.garrison.empty()) return TVS_EMPTY;
    if ((int)e.garrison.size() >= cap) return TVS_LOADED_FULL;
    return TVS_LOADED_PARTIAL;
}

const char* buildingVisualStateName(BuildingVisualState state) {
    switch (state) {
        case BVS_CONSTRUCTION_0_FOUNDATION: return "construction_0_foundation";
        case BVS_CONSTRUCTION_1_FRAME: return "construction_1_frame";
        case BVS_CONSTRUCTION_2_NEARLY_COMPLETE: return "construction_2_nearly_complete";
        case BVS_COMPLETE: return "complete";
        case BVS_DAMAGED: return "damaged";
        case BVS_GARRISONED: return "garrisoned";
        case BVS_GARRISON_FIRING: return "garrison_firing";
        case BVS_TRAINING_PEASANT: return "training_peasant";
        case BVS_TRAINING_INFANTRY: return "training_infantry";
        case BVS_TRAINING_CAVALRY: return "training_cavalry";
        case BVS_TRAINING_SHIP: return "training_ship";
        case BVS_RESEARCHING_IRON_WEAPONS: return "researching_iron_weapons";
        case BVS_RESEARCHING_CROSSBOWS: return "researching_crossbows";
        case BVS_RESEARCHING_PIKES: return "researching_pikes";
        case BVS_RESEARCHING_COUNTERWEIGHT: return "researching_counterweight";
        case BVS_RESEARCHING_PLATE_HELM: return "researching_plate_helm";
    }
    return "unknown";
}

const char* animalCarcassVisualStateName(AnimalCarcassVisualState state) {
    switch (state) {
        case ACVS_ALIVE: return "alive";
        case ACVS_DEAD_UNHARVESTED: return "dead_unharvested";
        case ACVS_PARTLY_HARVESTED: return "partly_harvested";
        case ACVS_MOSTLY_HARVESTED: return "mostly_harvested";
        case ACVS_DEPLETED_SKELETON: return "depleted_skeleton";
    }
    return "unknown";
}

const char* transportVisualStateName(TransportVisualState state) {
    switch (state) {
        case TVS_EMPTY: return "empty";
        case TVS_LOADED_PARTIAL: return "loaded_partial";
        case TVS_LOADED_FULL: return "loaded_full";
        case TVS_LOAD_UNLOAD: return "load_unload";
        case TVS_WRECK: return "wreck";
        case TVS_DECAYED_WRECK: return "decayed_wreck";
    }
    return "unknown";
}

const char* stateName(EntityState s) {
    switch (s) {
        case S_IDLE:       return "Idle";
        case S_MOVING:     return "Moving";
        case S_ATTACKING:  return "Attacking";
        case S_GATHERING:  return "Gathering";
        case S_BUILDING:   return "Building";
        case S_TRAINING:   return "Training";
        case S_RETURNING:  return "Returning";
        case S_DEAD:       return "Dead";
        case S_ENTERING:   return "Boarding";
        case S_GARRISONED: return "Garrisoned";
    }
    return "Unknown";
}

const char* terrainName(Terrain t) {
    switch (t) {
        case T_GRASS: return "Grass"; case T_TALL_GRASS: return "Tall grass";
        case T_FLOWERS: return "Flowers"; case T_MEADOW: return "Meadow";
        case T_FOREST: return "Forest"; case T_PINE: return "Pine";
        case T_PALM: return "Palm"; case T_DEAD_TREE: return "Dead tree";
        case T_MOUNTAIN: return "Mountain"; case T_HILLS: return "Hills";
        case T_STONE: return "Stone"; case T_WATER: return "Water";
        case T_SHALLOWS: return "Shallows"; case T_MARSH: return "Marsh";
        case T_REEDS: return "Reeds"; case T_GOLD: return "Gold";
        case T_SAND: return "Sand"; case T_DUNES: return "Dunes";
        case T_SNOW: return "Snow"; case T_ICE: return "Ice";
        case T_DIRT: return "Dirt"; case T_ROAD: return "Road";
        case T_MUD: return "Mud"; case T_WHEAT: return "Wheat";
        case T_BERRY: return "Berries"; case T_FISH: return "Fish";
        case T_RUINS: return "Ruins"; case T_GRAVEL: return "Gravel";
        case T_LAVA: return "Lava"; case T_ASH: return "Ash";
        case T_CASTLE_WALL: return "Castle wall";
        case T_CASTLE_FLOOR: return "Castle floor";
        case T_CASTLE_GATE: return "Castle gate";
        case TERRAIN_COUNT: break;
    }
    return "Unknown";
}

const char* biomeName(Biome b) {
    switch (b) {
        case B_TEMPERATE: return "Temperate"; case B_DESERT: return "Desert";
        case B_SNOW: return "Snow"; case B_SWAMP: return "Swamp";
        case B_FOREST: return "Forest"; case B_VOLCANIC: return "Volcanic";
        case B_OCEAN: return "Coastal";
    }
    return "Unknown";
}

const char* modeName(GameMode m) {
    switch (m) {
        case M_NORMAL: return "Normal"; case M_BUILD_SELECT: return "Build";
        case M_TRAIN_SELECT: return "Train"; case M_WALL_DRAG: return "Wall";
        case M_PAUSED: return "Paused"; case M_GAME_OVER: return "Game over";
        case M_RALLY_SET: return "Rally"; case M_RESEARCH_SELECT: return "Research";
        case M_ATTACK_MOVE: return "Attack move";
        case M_MARKET_TRADE: return "Market trade";
    }
    return "Unknown";
}

const CommandBinding* gameplayCommands(int& count) {
    static const CommandBinding commands[] = {
        {"select", "Select", "Space/click", "Normal", "Select unit or building"},
        {"command", "Command", "Enter/R-click", "Normal", "Move, attack, gather, help, or garrison"},
        {"help", "Help", "?", "Any", "Toggle in-game help"},
        {"build", "Build", "B", "Normal", "Open peasant build menu"},
        {"train", "Train", "T", "Normal", "Open production queue; repeat unit keys to queue"},
        {"attack_move", "Attack move", "A", "Normal", "Select all military or set attack-move target"},
        {"rally", "Rally/research", "R", "Normal", "Set rally point or open blacksmith research"},
        {"groups", "Groups", "G, 1-9", "Normal", "Assign and recall control groups"},
        {"hold", "Hold position", "X", "Normal", "Stop and hold selected units in place"},
        {"pause", "Pause", "P", "Normal", "Toggle pause"},
        {"diagnostics", "Diagnostics", "D/F8", "Normal", "Toggle debug diagnostics"},
        {"save", "Save", "V/F5", "Normal", "Save current match"},
        {"load", "Load", "L/F9", "Normal", "Load saved match"},
        {"resign", "Resign", "Q", "Normal", "Return to main menu"},
        {"exit", "Exit", "X", "Game over", "Exit the application from the game-over screen"},
        {"cancel", "Cancel", "Esc", "Command modes", "Cancel build, train, wall, rally, attack-move, or research"},
        {"zoom", "Zoom", "+/-/wheel", "SDL", "Zoom map"},
        {"pan", "Pan", "Middle-drag", "SDL", "Pan map"}
    };
    count = (int)(sizeof(commands) / sizeof(commands[0]));
    return commands;
}

std::string commandHelpLine() {
    int n = 0;
    const CommandBinding* c = gameplayCommands(n);
    std::string out;
    for (int i = 0; i < n; i++) {
        if (i) out += "  ";
        out += c[i].keys;
        out += ':';
        out += c[i].label;
    }
    return out;
}
