#pragma once
// Backend switch: the game speaks a small ncurses-shaped API. The default
// build uses real ncurses (terminal); -DUSE_SDL_SHIM swaps in the SDL2
// implementation (standalone window, vector-font glyphs, native mouse).
#ifdef USE_SDL_SHIM
#include "sdl_shim.h"
#else
#include <ncurses.h>
// Decorative title attribute is SDL-only; terminal renders it as bold.
#ifndef A_TITLE
#define A_TITLE A_BOLD
#endif
#endif
#include <vector>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <cstring>
#include "display.h"

// ============================================================
// CONSTANTS
// ============================================================
const int MAP_W        = 180;
const int MAP_H        = 110;
const int TICK_MS      = 80;   // base sim period; the splash speed knob scales it
const int COMBAT_PACE  = 150;  // attack-cooldown scale (%). >100 = slower, more readable fights
const int FOG_RADIUS   = 7;
const int GATHER_RATE  = 8;
const int GATHER_TICKS = 15;
const int DAY_LENGTH   = 1500;
const int SEASON_LENGTH= 3000;
const int CARRY_MAX    = 20;
const int WAGON_CAP    = 100;  // supply wagon hold — five peasant-loads per trip
const int FARM_CAP     = 40;   // ripe grain a 2x2 field holds awaiting pickup
const int MAX_PLAYERS  = 4;
const int OWNER_NATURE = MAX_PLAYERS;

// ============================================================
// ENUMS
// ============================================================
enum Terrain {
    T_GRASS, T_TALL_GRASS, T_FLOWERS, T_MEADOW,
    T_FOREST, T_PINE, T_PALM, T_DEAD_TREE,
    T_MOUNTAIN, T_HILLS, T_STONE,
    T_WATER, T_SHALLOWS, T_MARSH, T_REEDS,
    T_GOLD,
    T_SAND, T_DUNES,
    T_SNOW, T_ICE,
    T_DIRT, T_ROAD, T_MUD,
    T_WHEAT, T_BERRY, T_FISH,
    T_RUINS, T_GRAVEL,
    T_LAVA, T_ASH,
    T_CASTLE_WALL, T_CASTLE_FLOOR, T_CASTLE_GATE,
    T_BRIDGE,  // built over water; land-passable (fast), blocks boats
    T_MONOLITH,// standing stone: a unit on it sees +6 (hilltop beacon)
    T_HEATH    // moorland heather: open, hardy purple scrub (appended: saved as int)
};

enum EntityType {
    E_NONE = 0,
    E_PEASANT, E_MILITIA, E_ARCHER, E_KNIGHT, E_SPEARMAN, E_CATAPULT, E_TREBUCHET,
    E_FISHING_BOAT, E_WARSHIP, E_TRANSPORT, E_RAM,
    E_CROSSBOWMAN,  // Barracks + Blacksmith: armoured ranged, Thrust — the knight answer
    E_HUSSAR,       // Stable: fastest unit in the game, light raider cavalry
    E_MONK,         // Church: no attack; heals an adjacent friendly while idle
    E_SAPPER,       // Barracks + Blacksmith: suicide petard vs buildings
    E_WAGON,        // Mill/Granary: hauls up to 100 resources between depots; drops loot if killed
    E_TOWNHALL, E_HOUSE, E_BARRACKS, E_STABLE, E_TOWER,
    E_FARM, E_BLACKSMITH, E_CHURCH, E_MARKET, E_WALL, E_GATE, E_CASTLE,
    E_LUMBER_CAMP, E_MINING_CAMP, E_MILL, E_DOCK,
    E_GRANARY,      // big food store; halves winter drain for units near it
    E_TAVERN,       // brews grain into ale; ale-warms passing soldiers; feast ability
    E_WELL,         // peasant heal trickle; nearby buildings take less damage (bucket line)
    E_MANOR,        // +supply, garrison, small tax on nearby worked farms
    E_STONEMASON,   // stone construction (2x wall/gate/tower HP) + auto-repair from stone deposits
    E_STOCKYARD,    // open-air hoard: stores pile up tile by tile — and can be raided
    E_SHRINE,       // neutral: heals adjacent; a garrisoned monk projects the aura
    E_WATERMILL,    // neutral riverside: claim by garrison — half-rate mill + food dropoff
    E_TRADING_POST, // neutral on roads: claim by garrison — trickle gold + market trades
    E_WOLF_DEN,     // neutral: spawns wolves until destroyed
    E_RUIN,    // neutral ruined keep: garrison to capture (shelter + vision)
    E_BRIDGE,  // construction scaffold; completion converts the tile to T_BRIDGE
    E_DEER, E_WOLF, E_SHEEP, E_BOAR,
    E_BEAR          // rare forest predator: tough, hits hard, doesn't fear settlements
};

// Food is a larder, not a number: each kind stores, spoils, and is eaten
// differently. Ale is brewed from grain and never auto-eaten.
enum FoodKind { F_GRAIN = 0, F_MEAT, F_FISH, F_BERRY, F_ALE, F_COUNT };

enum EntityState {
    S_IDLE, S_MOVING, S_ATTACKING, S_GATHERING,
    S_BUILDING, S_TRAINING, S_RETURNING, S_DEAD,
    S_ENTERING, S_GARRISONED,
    S_ROUTING,  // morale broke: fleeing, unorderable until it rallies
    S_RAIDING   // marching on an enemy stockyard to steal from its piles
};
enum GameMode  { M_NORMAL, M_BUILD_SELECT, M_BUILD_PLACE, M_TRAIN_SELECT, M_WALL_DRAG, M_PAUSED, M_GAME_OVER, M_RALLY_SET, M_RESEARCH_SELECT, M_ATTACK_MOVE, M_MARKET_TRADE, M_PATROL_SET, M_HELP, M_SAVELOAD, M_STATS };

// Sacred-site domination: hold a MAJORITY of the map's claimable sites
// (shrines, watermills, trading posts, ruined keeps) for this many ticks
// and the realm submits — a match can end without grinding the last keep.
const int SITE_HOLD_TICKS = 2500;   // ~3.3 min at base speed

// Research bits stored in Player.research
enum Research {
    R_IRON_WEAPONS = 1, R_CROSSBOWS = 2, R_PIKES = 4, R_COUNTERWEIGHT = 8, R_PLATE_HELM = 16,
    R_FLETCHING = 32,       // Blacksmith, Township:   archers/crossbowmen +1 atk
    R_HEAVY_PLOUGH = 64,    // Mill, Township:         farms yield +1 per harvest
    R_HORSE_BREEDING = 128, // Stable, Stronghold:     cavalry musters with +15 HP
    R_MASONRY = 256,        // Stonemason, Stronghold: buildings take 20% less damage
    // Sentinel carried in Entity.researching while a Town Hall/Castle is
    // advancing the era; never set in Player.research.
    R_ERA_ADVANCE = 1 << 20,
};

// ============================================================
// ERAS — the match arc. Each era gates buildings, units and research;
// advancing is a long, expensive upgrade at the Town Hall / Castle.
// ============================================================
enum Era { ERA_HAMLET = 0, ERA_TOWNSHIP = 1, ERA_STRONGHOLD = 2, ERA_COUNT };
const char* eraName(int era);
int  eraOf(EntityType t);                       // minimum era to make t
bool eraUpCost(int fromEra, int& food, int& gold, int& wood, int& ticks);

// ============================================================
// CIVILISATIONS — light asymmetry: each civ bends costs, speeds and
// rates, and is DENIED something (that's where the countering lives).
// Player.civ indexes CIVS[] (globals.cpp).
// ============================================================
struct CivDef { const char* name; const char* bonus; const char* lack; };
extern const CivDef CIVS[];
inline constexpr int NUM_CIVS = 4;
enum Civ { CIV_FREEHOLDERS = 0, CIV_FENLANDERS, CIV_HILLFOLK, CIV_MARCHERS };
// One research: where it's bought, which era unlocks it, what it costs.
struct ResearchDef {
    int bit; EntityType building; int era;
    int gold, wood, ticks;
    char key; const char* name; const char* effect;
};
const ResearchDef* researchTable(int& n);   // commands.cpp owns the table

// Gate: 0 = may make it, 1 = locked by era, 2 = denied by civilisation.
int  makeGate(int owner, EntityType t);
// Civ-adjusted costs / training time (UI prints these; orders charge these).
int  costGoldOf(int owner, EntityType t);
int  costWoodOf(int owner, EntityType t);
int  trainTimeOf(int owner, EntityType t);
// Climate = the tile palette (what open ground / trees / etc. look like). This
// is the per-tile `biome` and the climate axis of map setup. Values 5-8 are
// legacy and no longer used as climates — topology now lives in `Layout`.
enum Biome     { B_TEMPERATE, B_DESERT, B_SNOW, B_SWAMP, B_FOREST,
                 B_OCEAN, B_HIGHLANDS, B_DEEPWOODS, B_RIVER,
                 // New CLIMATES appended (stored ints must stay stable):
                 B_STEPPE,  // dry golden grass-sea: salt pans, kurgan barrows
                 B_MOOR };  // heather upland: peat bogs, tors, stone circles
// Layout = the map topology, independent of climate. Each layout emits a
// neutral terrain template that applyClimateSkin() then themes to the chosen
// climate, so e.g. Highlands+Snow = alpine, Riverlands+Desert = a Nile.
enum Layout    { L_CONTINENTAL, L_HIGHLANDS, L_DEEPWOODS, L_RIVER, L_ISLANDS, L_PLAINS,
                 L_DELTA,    // one great river fanning into braided channels + silt isles
                 L_VALE,     // rift valley: fertile corridor between two cliff plateaus
                 L_CANYONS,  // badlands maze of stone gorges and gulches
                 LAYOUT_COUNT };
enum Season    { SPRING = 0, SUMMER, AUTUMN, WINTER };
enum Weather   { W_CLEAR = 0, W_RAIN, W_STORM, W_SNOW };

// ============================================================
// COLOR PAIR IDS  (used in both entity.cpp and render.cpp)
// ============================================================
enum {
    CP_GRASS = 1, CP_GRASS_LIGHT, CP_GRASS_DRY, CP_TALL_GRASS,
    CP_FLOWERS, CP_FLOWERS_BLUE, CP_FLOWERS_YELLOW, CP_FLOWERS_RED, CP_MEADOW,
    CP_FOREST, CP_FOREST_DARK, CP_PINE, CP_PALM, CP_DEAD_TREE,
    CP_MOUNTAIN, CP_HILLS, CP_STONE,
    CP_WATER, CP_WATER_SHIMMER, CP_SHALLOWS, CP_MARSH, CP_REEDS,
    CP_GOLD, CP_GOLD_SHIMMER,
    CP_SAND, CP_DUNES, CP_SNOW_GROUND, CP_ICE,
    CP_DIRT, CP_ROAD,
    CP_WHEAT, CP_WHEAT_GOLD, CP_BERRY,
    CP_RUINS, CP_GRAVEL,
    CP_CASTLE_WALL, CP_CASTLE_FLOOR, CP_CASTLE_GATE,
    CP_AUT_TREE_EARLY, CP_AUT_TREE_MID, CP_AUT_TREE_LATE, CP_AUT_TREE_GOLD, CP_AUT_TREE_RED,
    CP_AUT_GRASS, CP_AUT_GRASS_LATE,
    CP_WIN_GROUND, CP_WIN_TREE, CP_WIN_PINE, CP_WIN_ICE,
    CP_NIGHT_GRASS, CP_NIGHT_TREE, CP_NIGHT_WATER,
    CP_NIGHT_GROUND, CP_NIGHT_GOLD, CP_NIGHT_SNOW,
    CP_DAWN_SKY, CP_DUSK_SKY,
    CP_PLAYER, CP_PLAYER_NIGHT, CP_ENEMY, CP_ENEMY_NIGHT,
    CP_SHIP_PLAYER, CP_SHIP_ENEMY,
    // Per-player ship hulls: brown deck, owner-coloured glyph.
    CP_SHIP_P0, CP_SHIP_P1, CP_SHIP_P2, CP_SHIP_P3,
    CP_PROJ_ARROW, CP_PROJ_BOULDER, CP_PROJ_TOWER,
    CP_RAIN, CP_SNOW_FALL,
    CP_UI_BAR, CP_UI_TEXT, CP_UI_HIGH, CP_UI_DIM, CP_UI_ACCENT,
    CP_FOG, CP_FOG_EXPLORED, CP_CURSOR,
    CP_HP_GREEN, CP_HP_YELLOW, CP_HP_RED,
    CP_SUN, CP_MOON,
    CP_MM_PLAYER, CP_MM_ENEMY, CP_MM_WATER, CP_MM_FOREST,
    CP_MM_GOLD, CP_MM_SAND, CP_MM_SNOW, CP_MM_MTN, CP_MM_CASTLE,
    CP_SPRING_FLOWER,
    CP_DEER, CP_WOLF, CP_SHEEP, CP_BOAR, CP_MM_ANIMAL,
    CP_LAVA, CP_LAVA_HOT, CP_ASH,
    // Ownership background colours: background = owner, foreground = glyph.
    // Used for all land units and buildings (ships keep CP_SHIP_* wood bg).
    // One set per player slot (0=human, 1-3=AI); separate night variants.
    CP_OWN_P0, CP_OWN_P1, CP_OWN_P2, CP_OWN_P3,
    CP_OWN_P0_NIGHT, CP_OWN_P1_NIGHT, CP_OWN_P2_NIGHT, CP_OWN_P3_NIGHT,
    CP_BUILD_OK, CP_BUILD_BAD,
    CP_CLIFF,   // plateau rim escarpment
    CP_CORPSE,  // fallen-soldier marker (dim blood-red)
    CP_HEATH, CP_MM_HEATH,   // moorland heather (map + minimap/preview)
    CP_TORCHLIT,             // torch-glow core: warm amber pool around buildings
    CP_TORCHLIT_DIM,         // glow fringe flicker: embers at the edge of the light
    CP_COUNT
};

// ============================================================
// ENTITY STATS
// ============================================================
struct EntityStats {
    const char* name; char glyph;
    int maxHp, atk, range, speed, atkSpeed, costGold, costWood, trainTime;
    int sizeW, sizeH, supplyProvided, supplyUsed; bool isBuilding;
};
extern const EntityStats STATS[];

inline bool isUnit(EntityType t)     { return (t>=E_PEASANT&&t<=E_WAGON)||(t>=E_DEER&&t<=E_BEAR); }
inline bool isBuilding(EntityType t) { return t>=E_TOWNHALL&&t<=E_BRIDGE; }
inline bool isRanged(EntityType t)   { return t==E_ARCHER||t==E_CATAPULT||t==E_TREBUCHET||t==E_WARSHIP||t==E_CROSSBOWMAN; }
inline bool isNaval(EntityType t)    { return t==E_FISHING_BOAT||t==E_WARSHIP||t==E_TRANSPORT; }

// ============================================================
// COMBAT GRAMMAR — every unit has an armour class; every attack a damage
// type. damageVs() resolves them through one table (combat.cpp), so the
// whole counter triangle is legible and tunable in one place.
// ============================================================
enum ArmorClass { ARM_LIGHT, ARM_ARMORED, ARM_SIEGE };
enum DamageType { DMG_SLASH, DMG_PIERCE, DMG_THRUST, DMG_CRUSH };
ArmorClass armorClassOf(EntityType t);
DamageType damageTypeOf(EntityType t);

// ============================================================
// DATA STRUCTURES
// ============================================================
struct Projectile { float x,y,tx,ty; char glyph; int color,life; bool alive; };

struct Tile {
    Terrain terrain; int resources;
    bool visible[MAX_PLAYERS], explored[MAX_PLAYERS]; Biome biome;
    Terrain preWinterTerrain; // snapshot taken when winter arrives; restored during spring thaw
    int wear;        // 0-100: traffic + creep. Drives dirt/road transitions and decay.
    int elev;        // 0 lowland, 1 highland plateau. Steps across the boundary
                     // need a T_HILLS ramp; everywhere else the rim is a cliff.
    // Plunder: a destroyed depot scatters part of its stores onto its
    // footprint. Any land unit standing by picks loot up and hauls it home.
    int lootGold, lootWood, lootFood;
};

struct Entity {
    int id; EntityType type; int owner, x, y, hp, maxHp;
    EntityState state; int targetId, targetX, targetY;
    std::vector<std::pair<int,int>> path; int pathIdx;
    int moveCd, atkCd, gatherCd, gatherType;
    EntityType producing; int prodProgress, prodTime;
    bool underConstruction, alive; int rallyX, rallyY;
    int carrying;   // Peasants: resource units in inventory (gold/wood/food). Farms: harvest waiting for pickup. Transports: unused (garrison vector is the cargo).
    int stuckTicks;
    int alertTicks; // > 0 = recently in combat; render flashes '!'
    int rallySet;   // 0 = default, 1 = player-set rally point honoured on training
    int researching; // Research bit currently being researched (Blacksmith only); 0 = none
    int attackMove;  // 1 = engage enemies opportunistically while moving
    int holdPosition;// 1 = ignore auto-aggro; only attack when explicitly ordered
    bool gateOpen;   // E_GATE only: open (passable) vs closed (blocks pathing)
    bool gateLocked; // E_GATE only: manual mode — don't auto-toggle on ally proximity
    int convertTicks; // accumulated exposure to an enemy church; convert when threshold met
    int retreating;   // >0 while fleeing to safety at low HP; suppresses auto-aggro
    int packed;       // E_TREBUCHET: 1 = mobile/packed, 0 = deployed/firing
    int packTicks;    // E_TREBUCHET: ticks remaining in pack/unpack transition
    std::vector<int> queue;    // pending EntityTypes to train (FIFO, max 5)
    std::vector<int> garrison; // unit ids currently inside this building
    std::vector<std::pair<int,int>> waypoints; // queued move targets (Shift+RClick) and patrol loop
    bool patrolMode;           // when true, completed waypoints are re-queued so the unit loops
    // Stockpiles: wealth physically lives at depots. Player totals are the
    // cached sum; these say WHERE — and burn or scatter with the building.
    int storeGold, storeWood;
    int storeFood[F_COUNT];
    int foodKind;   // FoodKind of the food this unit is carrying (peasant/wagon)
    int aleTicks;   // >0: ale-warmed — +1 atk, -1 ranged range, no frostbite
    // Combat feel (docs/combat-feel-proposals.md):
    int morale;        // 0-100; at 0 the unit breaks and routs
    int routTicks;     // >0 while routing; rallies when it expires / reaches safety
    int chargeSteps;   // consecutive cavalry steps — >=4 means the next hit is a charge
    int stamina;       // 0-100; <30 = -25% damage, slower steps
    int kills;         // military kills; militia with 3+ become a veteran banner
    int prisoner;      // 1 = captured soldier: can be marched, ransomed, or rescued
    int origOwner;     // who a prisoner belonged to (-1 otherwise)
    int captureTicks;  // routing while cornered by enemies: counts up to capture
    int entrenchTicks; // catapult standing still: >=200 entrenched (+1 range)
};

struct Player {
    int gold, wood, food, supply, supplyMax;
    bool alive;
    int research;     // bitmask of completed upgrades (R_*)
    int aiWaveCd;     // per-AI rate-limit for wave dispatch
    int era;          // Era ladder position (ERA_HAMLET..ERA_STRONGHOLD)
    int civ;          // index into CIVS[]
    int aiPersona;    // AI temperament: 0 none, 1 Raider, 2 Builder, 3 Warlord
    int aiRaidCd;     // per-AI rate-limit for plunder squads
};

// ============================================================
// COMMANDS — the only way player intent reaches the sim.
// Input resolves a click/key into a Command and queues it; the tick
// applies the queue. The AI issues Commands too (applied immediately,
// inside the sim). In lockstep multiplayer this struct is what goes
// over the wire; in replays it's what goes to disk. UI-only state
// (selection, camera, cursor) never appears here — a Command must be
// meaningful on a machine that has no idea what the issuer had selected.
// ============================================================
enum CmdType {
    CMD_NONE = 0,
    CMD_MOVE,        // units → (x,y); >1 unit = formation move
    CMD_ATTACK,      // units → entity `target`; arg!=0 = attack-move stance
    CMD_ATTACK_MOVE, // units → (x,y), engaging en route
    CMD_GATHER,      // units → resource tile (x,y)
    CMD_BUILD,       // units[0] places building `arg` at (x,y)
    CMD_BUILD_WALL,  // units[0] builds wall line (x,y)..(x2,y2)
    CMD_SOW_FARM,    // units[0] sows a farm on wheat at (x,y)
    CMD_TRAIN,       // building `target` trains unit type `arg`
    CMD_HELP,        // units[0] helps construct / tends building `target`
    CMD_GARRISON,    // units enter building/transport `target`
    CMD_UNGARRISON,  // building/transport `target` ejects all
    CMD_STOP,        // units halt, keep auto-aggro
    CMD_HOLD,        // units halt, suppress auto-aggro
    CMD_PATROL,      // units bounce between current pos and (x,y)
    CMD_WAYPOINT,    // units append (x,y) to waypoint queue
    CMD_RALLY,       // building `target` rally point = (x,y)
    CMD_GATE,        // gate `target` cycles auto/open/closed
    CMD_PACK,        // trebuchet units[0] toggles pack/deploy
    CMD_RESEARCH,    // blacksmith `target` starts research bit `arg`
    CMD_TRADE,       // market `target` trade: arg 0=g→w 1=w→g 2=g→f 3=f→g
    CMD_REVEAL,      // debug: reveal map for issuing player
    CMD_FEAST,       // tavern `target` throws a feast: 10 ale, heal nearby units
    CMD_HAUL,        // wagon units[0] ↔ depot `target`: load if empty, unload if laden
    CMD_ERA_UP,      // town hall/castle `target` begins advancing to the next era
    CMD_RAID,        // land units march on enemy stockyard `target` and steal from it
};

struct Command {
    int type   = CMD_NONE;
    int player = -1;          // issuer; apply rejects units/buildings not owned by them
    int x = 0, y = 0;         // primary tile
    int x2 = 0, y2 = 0;       // secondary tile (wall line end)
    int target = -1;          // target entity id
    int arg    = 0;           // EntityType / research bit / trade index / flags
    std::vector<int> units;   // acting unit ids
};

struct Game {
    Tile map[MAP_H][MAP_W];
    std::vector<Entity> entities;
    std::vector<Projectile> projectiles;
    int nextId; Player players[MAX_PLAYERS + 1]; int tick;
    GameMode mode; int cursorX, cursorY, viewX, viewY, viewW, viewH;
    int selectedId;
    std::vector<int> selectedIds;
    std::vector<int> controlGroups[9];
    bool groupAssignPending;
    bool dragging; int dragStartX, dragStartY;
    std::string statusMsg; int statusTimer;
    EntityType buildPending; int wallDragX, wallDragY;
    int winner, aiTimer, farmTimer;
    float dayPhase, seasonPhase;
    int year;             // campaign year; seasonPhase wraps at 4.0, this counts the wraps
    int prevSeason;       // for detecting season transitions
    int prevTimePhase;    // 0=day 1=dusk 2=night 3=dawn; for transition messages
    int attackNotifyCd;  // ticks until next "Under attack" message is allowed
    int weather;          // current Weather state
    int weatherTimer;     // ticks until next weather change roll
    // Civ choices per seat: -1 = roll one from the seed in initGame. Match
    // config like biomeChoice — set by splash/lobby/replay header, identical
    // on every machine, NOT reset by resetMatchState.
    int civChoice[MAX_PLAYERS] = {-1, -1, -1, -1};
    int biomeChoice;      // CLIMATE: -1 = mixed climate bands, else a forced Biome (0-4)
    int layoutChoice;     // LAYOUT: -1 = random (resolved in initGame), else a Layout
    std::string mapName;  // evocative battlefield name (display only; derived from seed)
    int playerColor;      // chosen team-colour index (0=Blue); AI colours avoid it
    bool returnToMenu;    // set on game-over to break back to splash screen
    bool cursorByMouse;   // last cursor move came from the mouse: render must
                          // NOT auto-pan the view to chase it (that pan changes
                          // which tile is under a stationary pointer — feedback
                          // loop that desyncs pointer and cursor tile). Mouse
                          // scrolling is edge-scroll/minimap only.
    unsigned long long rngState; // sim RNG state — part of game state, saved/loaded
    unsigned long long simSeed;  // seed this match started from (replay header)
    // Fallen-soldier markers: written by killEntity, read only by render.
    // Transient presentation state (like projectiles): not saved, not hashed.
    struct Corpse { int x, y, tick; char glyph; };
    std::vector<Corpse> corpses;
    // "Their line broke!" status flash: counts routs per side in a short
    // window. UI-only — never saved, never hashed (doesn't feed the sim).
    int routFlashTick, routFlashOwner, routFlashCount;
    // Sacred-site domination countdown — SIM STATE (hashed + saved): who
    // currently holds the majority of claimable sites, and for how long.
    int siteHoldOwner = -1;
    int siteHoldTicks = 0;
    // Match statistics (presentation only — never saved or hashed; both
    // machines in MP derive identical numbers from the shared sim anyway).
    int statRaids[MAX_PLAYERS + 1] = {};   // successful stockyard thefts per seat
    int statEraTick[MAX_PLAYERS][ERA_COUNT] = {};   // when each seat reached each era
    struct StatSample { short army[MAX_PLAYERS]; short work[MAX_PLAYERS]; int wealth[MAX_PLAYERS]; };
    std::vector<StatSample> statSamples;    // sampled every 250 ticks for the charts
    // Save/Load overlay (M_SAVELOAD): transient UI state — never saved/hashed.
    int saveSlotSel;                                  // highlighted slot 0..NUM_SAVE_SLOTS-1
    int slMenuX, slMenuW, slMenuRowY0, slMenuRowH;    // overlay geometry for mouse hit-test
    int difficulty;       // 0 easy / 1 normal / 2 hard — AI pacing knobs (ai.cpp)
    int winterSeverity;   // rolled at each winter onset: 0 mild / 1 normal / 2 brutal
    // Which seat THIS machine plays. Single-player: 0. Multiplayer host: 0,
    // client: 1. Pure presentation/input state — never part of the sim
    // (both machines run every player's sim identically).
    int localPlayer = 0;
    // Bitmask of player slots driven by humans (bit p = slot p). The AI
    // skips these. Match config, shared by the lobby / replay header like
    // difficulty — identical on every machine, so it can't desync.
    int humanMask = 1;
    std::vector<Command> pendingCmds; // local player's queued commands; applied at tick start
    std::vector<std::string> eventLog; // rolling recent-events feed (UI only; not saved/hashed)
    // Top-bar "Idle:" readout doubles as a click target (AoE2 idle-vill
    // button). Geometry stashed by renderUI for input's mouse hit-test.
    int idleBtnX = -1, idleBtnW = 0;
    // Multiplayer chat: input line state (UI only; the sent text
    // travels as a control message, never through the sim).
    bool chatOpen = false;
    std::string chatInput;
};
extern Game g;

// ============================================================
// SIM RNG — all gameplay randomness must come from simRand(), never
// rand(): identical seed + identical command stream must replay
// identically on every machine. That property is the foundation for
// lockstep multiplayer, replays, and desync-free saves.
// (render/ui may use any randomness they like — visuals don't desync.)
// ============================================================
void seedSimRng(unsigned long long seed);
int  simRand();   // uniform 0..2^31-1, drop-in for rand()

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================

// mapgen.cpp
void generateMap();
// Evocative, AoE2-style battlefield name derived purely from the seed (and
// flavoured by layout/climate). Deterministic; never touches the sim RNG.
std::string makeMapName(unsigned long long seed, int layout, int climate);
void clearStartArea(int cx, int cy, int radius);
void placeGoldCluster(int cx, int cy, int count);

// entity.cpp — time
float       getBrightness();
Season      getSeason();
float       getSeasonProgress();
const char* getSeasonName();
const char* getTimeName();
bool        isNight();
bool        isDusk();
bool        isDawn();

// entity.cpp — helpers
int     dist(int x1,int y1,int x2,int y2);
int     mdist(int x1,int y1,int x2,int y2);
bool    inBounds(int x,int y);
bool    isPassable(int x,int y);
bool    isPassableWater(int x,int y);
bool    canStep(int fx,int fy,int tx,int ty,bool naval);
bool    isDetectedBy(int x,int y,int observerOwner);
bool    isConcealing();
void    setStatus(const std::string& msg);
Entity* findEntity(int id);
Entity* findDepot(Entity& e);
Entity* entityAt(int x,int y);
Entity* entityAtOwner(int x,int y,int owner);
int     distToBuilding(int x,int y,const Entity& b);
bool    canPlace(EntityType type,int x,int y,int owner,int ignoreId=-1);
bool    farmAnchorFor(int x,int y,int player,int ignoreId,int& ax,int& ay);
void    updateSupply(int owner);
int     spawnEntity(EntityType type,int owner,int x,int y,bool built=true);

// entity.cpp — projectiles / pathfinding
void spawnProjectile(int sx,int sy,int tx,int ty,char gl,int col);
void tickProjectiles();
std::vector<std::pair<int,int>> findPath(int sx,int sy,int tx,int ty,int maxSteps=300,bool naval=false);
inline std::vector<std::pair<int,int>> findPathFor(Entity& e, int tx, int ty) {
    return findPath(e.x, e.y, tx, ty, 300, isNaval(e.type));
}

// combat.cpp — orders / combat / garrison
Entity* findNearestEnemy(Entity& e,int range);
void orderMove(Entity& e,int tx,int ty);
void orderAttack(Entity& e,int tid);
void orderGather(Entity& e,int tx,int ty);
void orderBuild(Entity& e,EntityType bt,int bx,int by);
void orderTrain(Entity& bld,EntityType ut);
int  trainFoodCost(EntityType ut);
void orderGroupMove(const std::vector<int>& unitIds,int tx,int ty);
void orderGroupAttack(const std::vector<int>& unitIds,int tid);
void orderGroupAttackMove(const std::vector<int>& unitIds,int tx,int ty);
void orderHelp(Entity& e,int buildingId);
void orderGarrison(Entity& e,int buildingId);
bool canGarrisonIn(EntityType bt);
bool isClaimable(EntityType bt);    // neutral structures captured by garrisoning
int  garrisonCap(EntityType bt);
int  shieldBuilding(const Entity& tgt, int dmg); // wells damp damage to nearby buildings
void ejectGarrison(Entity& bld);
void killEntity(Entity& t);
int  unitAtk(const Entity& e);
int  unitRange(const Entity& e);
int  damageVs(EntityType attacker, EntityType target, int rawDmg, int targetOwner = -1);

// entity.cpp — movement / state
void moveAlongPath(Entity& e);
void resetDetectMapCache();

// entity.cpp — stockpiles. Totals (Player.gold/wood/food) stay the cheap
// check; these keep the per-depot location bookkeeping in sync with them.
bool isDepot(EntityType t);
int  depotCapGold(EntityType t);
int  depotCapWood(EntityType t);
int  depotCapFood(EntityType t);
int  depotFoodSum(const Entity& e);
void addFood(int owner, int kind, int amount, Entity* depot);
void spendPlayerFood(int owner, int amount);              // eats spoilables first
bool spendFoodKind(int owner, int kind, int amount);      // e.g. brewing pulls grain
void drainStores(int owner, int gold, int wood, int x, int y); // pay from nearest piles
void depositToNearest(int owner, int gold, int wood, int foodKind, int food, int x, int y);

// entity.cpp — tick (units)
void tickEntity(Entity& e);
void updateFog();

// world.cpp — passive ticks, seasons, weather, win
void tickTowers();
void tickGates();
void tickFarms();
void tickMarkets();
void tickChurches();
void tickAnimals();
void tickSeasons();
void tickThaw();
void tickWinter();
void tickPaving();
void tickWeather();
void tickSpoilage();
void tickTaverns();
void tickPrisoners();
void checkWin();

// Combat-feel helpers (entity.cpp / combat.cpp)
bool hasMorale(EntityType t);   // units that can break and rout

// entity.cpp — AI
int     aiCount(int owner,EntityType t);
int     aiCountAll(int owner,EntityType t);
Entity* aiIdle(int owner,EntityType t);
Entity* aiBldg(int owner,EntityType t);
void    aiGather(int owner);
void    aiBuildSpot(int owner,EntityType bt,int& ox,int& oy);
void    tickAI();

// render.cpp — world/terrain; ui.cpp — HUD, panel, minimap, menus
void initColors();
// Re-skin the per-owner colour pairs from g.playerColor: player slot 0 takes
// the chosen colour, AI slots take distinct colours that are never the player's.
void applyTeamColors();
const char* teamColorName(int idx);   // "Blue", "Red", ... for the splash
int  numTeamColors();
void renderMap();
void renderUI();
void render();

// input.cpp
void handleInput(int ch);

// commands.cpp — the command funnel
void pushCommand(const Command& c);   // queue from local input (dropped during replay playback)
void applyCommand(const Command& c);  // validate + dispatch one command (AI calls this directly)
void applyPendingCommands();          // drain g.pendingCmds at tick start; records to replay

// commands.cpp — the command codec. One binary layout everywhere: the replay
// file and the network wire both carry exactly this int32 field sequence, so
// a multiplayer session is literally a replay exchanged live.
void encodeCommand(const Command& c, std::vector<int>& out);   // appends fields
int  decodeCommand(const int* f, int avail, Command& c);       // ints consumed; 0 = need more, -1 = malformed

// commands.cpp — replays (seed + command stream)
bool replayStartRecording(int numAIs);          // uses g.simSeed/g.biomeChoice/g.humanMask; new file per match
void replayStopRecording();
bool replayLoadFile(const char* path, unsigned long long& seed, int& numAIs, int& biomeChoice, int& layoutChoice, int& difficulty, int& humanMask);
void replayInjectCommands();          // playback: queue recorded commands for the current tick
bool replayPlaying();
void replayStopPlayback();            // back to live play (splash replay browser)

// commands.cpp — desync detector
unsigned long long simStateHash();    // FNV-1a over entities + players + RNG state
void simHashTick();                   // REALM_HASH=1: append tick/hash to realm-hash.log every 100 ticks

// ============================================================
// net.cpp — deterministic-lockstep multiplayer (docs/networking-plan.md).
// TCP carries Commands only (the replay codec is the wire codec); UDP
// broadcast answers LAN lobby discovery. Host = slot 0, client = slot 1.
// ============================================================
inline constexpr int NET_TCP_PORT = 7521;   // lobby + match traffic
inline constexpr int NET_UDP_PORT = 7522;   // LAN discovery pings
inline constexpr int NET_CMD_DELAY = 3;     // commands run D ticks after issue (~240ms)

struct NetMatchConfig {
    unsigned long long seed = 0;
    int numAIs = 1, biome = -1, layout = -1, difficulty = 1, speed = 1;
    int humanMask = 3;
    int civ[MAX_PLAYERS] = {-1, -1, -1, -1};   // per-seat civ choice; -1 = rolled
};
struct NetLobbyInfo {                 // one discovered LAN game
    std::string addr;                 // dotted IP
    int port = NET_TCP_PORT;
    std::string host;                 // host's user name
    std::string map;                  // settings blurb ("2 AIs · Normal")
};

// Lobby — host side
bool netHostOpen();                             // listen + discovery responder
void netHostSetInfo(const NetMatchConfig& cfg); // advertised + sent to client on change
bool netHostPoll();                             // accept/handshake; true = client seated
bool netHostClientPresent();
std::string netHostClientName();
bool netHostStart();                            // sends START; match may begin
// Lobby — client side
bool netJoinConnect(const char* addr, int port, std::string& err);
int  netClientPoll(NetMatchConfig& cfg);        // 0 idle, 1 config updated, 2 START, -1 lost
// LAN discovery — client side
void netDiscoverStart();
void netDiscoverPoll(std::vector<NetLobbyInfo>& out);
void netDiscoverStop();
// Either side
void netClose();                                // tear everything down
bool netActive();                               // in a live network match
std::vector<std::string> netLocalAddresses();   // this machine's LAN IPv4s

// Match — the lockstep scheduler. (The per-match reset happens inside the
// net layer at START time — the peer's first bundles can ride the same TCP
// segment as START, so resetting any later would lose them.)
void netQueueLocal(const Command& c);           // local input during a net match
bool netTickReady();                            // pump socket; true = both bundles for the next tick arrived (they're injected)
void netAfterTick();                            // send our bundle / 100-tick hash
void netPump();                                 // drain socket between ticks (keepalive, pause msgs)
bool netConnectionLost();
bool netVersionMismatch();              // the BYE said our builds differ
bool netDesynced();
int  netDesyncTick();
bool netPeerPaused();
bool netWaitingForPeer();               // stalled on the opponent's bundle
std::string netPeerName();
void netSendPause(bool paused);
void netSendChat(const std::string& text);   // shows on both sides' event logs
void netSendCivPick(int civ);                // client tells host its civilisation
void netSendBye();

// main.cpp — browser build flushes the IndexedDB filesystem after file
// writes (saves/replays/config); native builds no-op.
void platformPersistFiles();
int  netProtoVersion();   // net.cpp — NET_PROTO_VERSION for display

// save.cpp
inline constexpr int NUM_SAVE_SLOTS = 4;
// Lightweight summary of a save slot for the visual Save/Load menu (peekSave
// reads only the header — never the full map/entity payload).
struct SaveSlotInfo { bool used=false; long long saveTime=0; int year=1; int season=0; };
bool saveGame(const char* path);
bool loadGame(const char* path);
bool peekSave(const char* path, SaveSlotInfo& out);
void saveSlotPath(int slot, char* buf, int n);   // slot is 1-based

// menu.cpp — the splash and every screen around it. What the splash
// resolved to: a skirmish, a saved game, a replay to watch, or a connected
// network match (host seat 0 / client seat 1, config agreed in the lobby).
struct SplashResult {
    int numAIs = 1;
    unsigned long long seed = 0;
    int loadSlot = 0;
    bool netPlay = false;
    NetMatchConfig netCfg;
    int netSlot = 0;
    std::string replayPath;   // non-empty: watch this recording
};
void showSplash(SplashResult& r);
void loadMenuConfig();   // remembered preferences (realm-config.txt)

// main.cpp
void initGame(int numAIs, unsigned long long seed = 0); // seed 0 = derive from clock
void simTick();   // one deterministic sim step — shared by game loop, replay, --verify
