#include "realm.h"
#include <cctype>

// ============================================================================
// BATTLEFIELD NAMING — every map gets an evocative, AoE2-style name derived
// purely from its seed (so the same seed always reads the same), lightly
// flavoured by climate. Names are deliberately decoupled from the biome: a
// "Black Fen" can be desert, a "Sunscorch Reach" can be tundra — the layout is
// what's unique, exactly like AoE2's named maps. No simRand here: naming must
// not perturb the deterministic sim/replay stream.
// ============================================================================
static unsigned long long nameMix(unsigned long long& s) {
    s += 0x9E3779B97F4A7C15ull;
    unsigned long long z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

std::string makeMapName(unsigned long long seed, int layout, int climate) {
    // Climate-flavoured adjectives blended with a generic evocative pool, so a
    // name hints at the land without ever being locked to it.
    static const char* adjGeneric[] = {
        "Shattered","Forgotten","Whispering","Hollow","Silent","Wandering",
        "Broken","Golden","Twin","Last","Old","Savage","Endless","Lost",
        "Wayward","Distant","Quiet","Crimson","Iron","Grey" };
    static const char* adjDesert[] = {
        "Scorched","Ashen","Burning","Sunbaked","Parched","Bleached","Amber","Blistering" };
    static const char* adjSnow[]   = {
        "Frozen","Bitter","Pale","Bleak","Frostbound","Glittering","White","Hoar" };
    static const char* adjSwamp[]  = {
        "Sunken","Drowned","Weeping","Misty","Rotten","Murky","Black","Fevered" };
    static const char* adjForest[] = {
        "Verdant","Emerald","Tangled","Shadowed","Wildwood","Deepgreen","Mossy","Thorny" };
    static const char* adjTemp[]   = {
        "Rolling","Sunlit","Windswept","Fair","Green","Bountiful","Gilded","Wide" };
    static const char* adjSteppe[] = {
        "Boundless","Dusty","Thundering","Golden","Restless","Wind-worn","Nomad","Burnt" };
    static const char* adjMoor[]   = {
        "Misty","Bleak","Peat-dark","Heathered","Brooding","Rainlashed","Lonely","Grey" };
    static const char* nouns[] = {
        "Vale","Reach","Hollow","Expanse","Marches","Frontier","Basin","Steppe",
        "Moor","Fen","Wold","Heath","Downs","Crossing","Pass","Plains","Coast",
        "Delta","Wastes","Gorge","Plateau","Mire","Glen","Barrens","Fields",
        "Hollows","Range","Hinterland","Flats","Run" };
    static const char* roots[] = {
        "Rav","Mor","Dun","Cael","Thar","Brae","Esk","Vorn","Wyck","Grim",
        "Ald","Hald","Kern","Dol","Bryn","Tor","Ash","Black","Fel","Gan",
        "Hel","Keb","Orm","Ryn" };
    static const char* sufx[] = {
        "moor","wick","ford","holm","gard","mere","fell","dale","march","helm",
        "stead","reach","watch","crag","wold","haven","barrow","ridge","glen","hold" };

    unsigned long long s = seed ^ (0xD1B54A32D192ED03ull * (unsigned)(layout + 1));
    auto pick = [&](const char** arr, int n) { return arr[nameMix(s) % n]; };
    auto cnt  = [](auto& a){ return (int)(sizeof(a)/sizeof(a[0])); };

    // Choose the climate-flavoured adjective pool (or generic).
    const char** adjC = adjTemp; int adjCn = cnt(adjTemp);
    switch (climate) {
        case B_DESERT: adjC = adjDesert; adjCn = cnt(adjDesert); break;
        case B_SNOW:   adjC = adjSnow;   adjCn = cnt(adjSnow);   break;
        case B_SWAMP:  adjC = adjSwamp;  adjCn = cnt(adjSwamp);  break;
        case B_FOREST: adjC = adjForest; adjCn = cnt(adjForest); break;
        case B_STEPPE: adjC = adjSteppe; adjCn = cnt(adjSteppe); break;
        case B_MOOR:   adjC = adjMoor;   adjCn = cnt(adjMoor);   break;
        default: break;   // temperate / ocean / mixed -> adjTemp
    }
    // Half the time use the flavoured pool, half the generic — keeps names
    // surprising rather than every desert map sounding the same.
    auto adj = [&]() -> const char* {
        return (nameMix(s) & 1) ? pick(adjC, adjCn)
                                : pick(adjGeneric, cnt(adjGeneric));
    };
    auto proper = [&]() {
        std::string p = roots[nameMix(s) % cnt(roots)];
        p += sufx[nameMix(s) % cnt(sufx)];
        p[0] = toupper(p[0]);
        return p;
    };

    switch (nameMix(s) % 5) {
        case 0: return std::string(adj()) + " " + pick(nouns, cnt(nouns));
        case 1: return "The " + std::string(pick(nouns, cnt(nouns))) + " of " + proper();
        case 2: return proper() + "'s " + pick(nouns, cnt(nouns));
        case 3: return proper();
        default: return std::string(adj()) + " " + proper();
    }
}

static float noiseGrid[32][32];

static void initNoise() {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            noiseGrid[y][x] = (float)(simRand() % 1000) / 1000.0f;
}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

static float sampleNoise(float fx, float fy) {
    int x0 = (int)fx % 31, y0 = (int)fy % 31, x1 = x0 + 1, y1 = y0 + 1;
    float tx = fx - (int)fx, ty = fy - (int)fy;
    return lerp(lerp(noiseGrid[y0][x0], noiseGrid[y0][x1], tx),
                lerp(noiseGrid[y1][x0], noiseGrid[y1][x1], tx), ty);
}

// Spawn-area prep called from main.cpp once spawn positions are chosen.
void clearStartArea(int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius+4; dy++) for (int dx = -radius; dx <= radius+4; dx++) {
        int x = cx+dx, y = cy+dy;
        if (inBounds(x,y) && g.map[y][x].terrain != T_GOLD) g.map[y][x].terrain = T_GRASS;
    }
    // Level a wider apron around the spawn so the starting base, its gold
    // cluster, and the first farms never end up split across a cliff.
    int baseElev = inBounds(cx,cy) ? g.map[cy][cx].elev : 0;
    int er = radius + 9;
    for (int dy = -er; dy <= er; dy++) for (int dx = -er; dx <= er; dx++) {
        int x = cx+dx, y = cy+dy;
        if (inBounds(x,y)) g.map[y][x].elev = baseElev;
    }
}

void placeGoldCluster(int cx, int cy, int count) {
    for (int i = 0; i < count; i++) {
        int gx = cx + (simRand()%7)-3, gy = cy + (simRand()%5)-2;
        if (inBounds(gx,gy) && g.map[gy][gx].terrain != T_WATER
            && g.map[gy][gx].terrain != T_MOUNTAIN && g.map[gy][gx].terrain != T_SHALLOWS)
            { g.map[gy][gx].terrain = T_GOLD; g.map[gy][gx].resources = 300 + simRand() % 300; }
    }
}

static void placeCastleRuin(int cx, int cy, int size) {
    for (int dy = 0; dy < size; dy++) for (int dx = 0; dx < size; dx++) {
        int x = cx + dx, y = cy + dy;
        if (!inBounds(x, y)) continue;
        bool isEdge   = (dx == 0 || dx == size-1 || dy == 0 || dy == size-1);
        bool isCorner = (dx == 0 || dx == size-1) && (dy == 0 || dy == size-1);
        bool isGate   = !isCorner && isEdge && (dx == size/2 || dy == size/2);
        if (isGate)       g.map[y][x].terrain = T_CASTLE_GATE;
        else if (isEdge)  g.map[y][x].terrain = (simRand() % 4 != 0) ? T_CASTLE_WALL : T_RUINS;
        else              g.map[y][x].terrain = T_CASTLE_FLOOR;
        g.map[y][x].resources = 0;
    }
    int corners[][2] = {{cx,cy},{cx+size-1,cy},{cx,cy+size-1},{cx+size-1,cy+size-1}};
    for (auto& c : corners)
        if (inBounds(c[0], c[1])) g.map[c[1]][c[0]].terrain = T_CASTLE_WALL;
    // The keep itself: a neutral, capturable shelter in the castle's heart.
    // Garrison units inside to claim it — vision and stone walls, no upkeep.
    int kx = cx + size/2 - 1, ky = cy + size/2 - 1;
    if (inBounds(kx, ky) && inBounds(kx+1, ky+1))
        spawnEntity(E_RUIN, OWNER_NATURE, kx, ky);
}

// Distance helper for Voronoi continent generation — Euclidean (round shapes).
static float edist(int x1, int y1, int x2, int y2) {
    int dx = x1-x2, dy = y1-y2;
    return std::sqrt((float)(dx*dx + dy*dy));
}

// --- Climate skinning + ecotones --------------------------------------------
// Layouts paint a neutral template (grass / tall grass / forest / water / rock);
// these passes then theme it to the match climate and soften the borders, so
// any layout reads correctly in any climate (Highlands+Snow = alpine, etc.).

// Is this biomeChoice value one of the pickable climates? (Values 5-8 are
// legacy layout ids and never valid as climates.)
static bool isClimateChoice(int b) {
    return b==B_TEMPERATE||b==B_DESERT||b==B_SNOW||b==B_SWAMP||b==B_FOREST
        || b==B_STEPPE||b==B_MOOR;
}

// The climate for one tile: a forced choice, or latitude-banded when mixed (-1).
// The bands read like real geography: tundra up top, moor on the cool wet
// flank, forest on the cool dry side, a steppe belt before the true desert.
static Biome pickClimate(int x, int y) {
    if (isClimateChoice(g.biomeChoice)) return (Biome)g.biomeChoice;
    float n1 = sampleNoise(x*0.028f, y*0.028f);
    float n2 = sampleNoise(x*0.020f+10, y*0.020f+10);
    float climate = (float)y / MAP_H * 0.55f + n1 * 0.45f;   // cold(north)..hot(south)
    if      (climate < 0.24f)               return B_SNOW;
    else if (climate > 0.76f)               return B_DESERT;
    else if (n2 > 0.72f)                     return B_SWAMP;
    else if (climate > 0.62f && n2 < 0.42f) return B_STEPPE;
    else if (climate < 0.40f && n2 > 0.55f) return B_MOOR;
    else if (climate < 0.45f && n2 < 0.40f) return B_FOREST;
    return B_TEMPERATE;
}

// Repaint the neutral template into the chosen climate. Water, rock, gold,
// roads, ruins and bridges are climate-independent and pass through untouched.
static void applyClimateSkin() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        Biome c = pickClimate(x, y);
        t.biome = c;
        switch (t.terrain) {
        case T_GRASS: case T_MEADOW: case T_FLOWERS:
            if      (c == B_DESERT) t.terrain = T_SAND;
            else if (c == B_SNOW)   t.terrain = T_SNOW;
            else if (c == B_SWAMP)  t.terrain = T_TALL_GRASS;
            else if (c == B_STEPPE) t.terrain = (simRand()%12==0) ? T_DIRT : T_GRASS;
            else if (c == B_MOOR)   t.terrain = (simRand()%10<7)  ? T_HEATH : T_TALL_GRASS;
            break;
        case T_TALL_GRASS:
            if      (c == B_DESERT) t.terrain = T_DUNES;
            else if (c == B_SNOW)   t.terrain = T_SNOW;
            else if (c == B_SWAMP)  t.terrain = T_REEDS;
            else if (c == B_MOOR)   t.terrain = (simRand()%2==0)  ? T_HEATH : T_TALL_GRASS;
            break;
        case T_FOREST: case T_PINE: case T_PALM: case T_DEAD_TREE:
            if      (c == B_DESERT) t.terrain = T_PALM;
            else if (c == B_SNOW)   t.terrain = T_PINE;
            else if (c == B_SWAMP)  t.terrain = T_DEAD_TREE;
            else if (c == B_STEPPE && simRand()%3 == 0) { t.terrain = T_DEAD_TREE; t.resources = std::max(30, t.resources/2); }
            else if (c == B_MOOR   && simRand()%2 == 0) t.terrain = T_PINE;
            else if (c == B_FOREST && t.terrain == T_FOREST && simRand()%4 == 0) t.terrain = T_PINE;
            break;
        default: break;
        }
    }
}

// Soften borders: sandy/reedy beaches where land meets water, scrubby treelines
// at forest edges, and palm-ringed greenery where desert holds water.
static void applyEcotones() {
    static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    auto isWater = [](Terrain t){ return t==T_WATER||t==T_SHALLOWS||t==T_FISH; };
    auto isOpen  = [](Terrain t){ return t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW||t==T_SAND||t==T_DUNES||t==T_SNOW||t==T_HEATH; };
    auto isTree  = [](Terrain t){ return t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE; };
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        bool nearWater = false;
        for (auto& d : d4) { int nx=x+d[0], ny=y+d[1]; if (inBounds(nx,ny) && isWater(g.map[ny][nx].terrain)) { nearWater=true; break; } }
        if (!nearWater) continue;
        if (isOpen(t.terrain) && t.biome != B_SNOW && t.resources == 0 && simRand()%100 < 60)
            t.terrain = (t.biome == B_SWAMP) ? T_REEDS : T_SAND;          // beach / reed fringe
        if (t.biome == B_DESERT && (t.terrain==T_SAND || t.terrain==T_DUNES) && simRand()%100 < 45) {
            if (simRand()%3 == 0) { t.terrain = T_PALM; t.resources = 60 + simRand()%40; }  // oasis palm
            else                  t.terrain = T_GRASS;                                       // oasis green
        }
    }
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (!isTree(g.map[y][x].terrain)) continue;
        int open = 0;
        for (auto& d : d4) { int nx=x+d[0], ny=y+d[1]; if (inBounds(nx,ny) && isOpen(g.map[ny][nx].terrain)) open++; }
        if (open >= 2 && simRand()%100 < 30) { g.map[y][x].terrain = T_TALL_GRASS; g.map[y][x].resources = 0; }
    }
}

// ============================================================================
// DESERT CHARACTER — turn flat sand into a real desert: coherent dune seas and
// stony badlands, green oases, dry wadi beds, rocky mesas, and bleached salt
// flats. Runs over any B_DESERT region (random climate OR a desert-skinned
// layout), so a Riverlands+Desert reads as a Nile, Highlands+Desert as mesas.
// Deterministic (simRand), so it's part of the seed and survives replays.
// ============================================================================
static void applyDesertFeatures() {
    auto isSand = [](Terrain t){
        return t==T_SAND||t==T_DUNES||t==T_GRAVEL||t==T_DIRT||t==T_MUD;
    };
    auto desertAt = [&](int x,int y){
        return inBounds(x,y) && g.map[y][x].biome==B_DESERT;
    };
    // Bail out cheaply if there's no desert on this map.
    int desertCount = 0;
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) if (g.map[y][x].biome==B_DESERT) desertCount++;
    if (desertCount < 200) return;

    // 1) Coherent zones. Two slow noise channels carve the open desert into
    //    rolling dune seas, broken stony badlands, and cracked salt flats —
    //    far more characterful than per-tile salt-and-pepper.
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
        Tile& t = g.map[y][x];
        if (t.biome!=B_DESERT || !isSand(t.terrain) || t.resources>0) continue;
        float dn = sampleNoise(x*0.045f+200, y*0.045f+200);   // dune field
        float rn = sampleNoise(x*0.060f+311, y*0.060f+311);   // rocky badlands
        if      (rn > 0.78f) { t.terrain = T_STONE;  t.resources = 0; }
        else if (rn > 0.68f) t.terrain = T_GRAVEL;
        else if (dn > 0.66f) t.terrain = T_DUNES;
        else if (dn < 0.26f) t.terrain = T_DIRT;              // bleached salt flat
        else                 t.terrain = T_SAND;
    }

    // 2) Oases — the lifelines. A little water, ringed by green grass, date
    //    palms (wood!) and the odd berry. Worth fighting over in a wasteland.
    int oases = 2 + simRand()%3;
    for (int k=0;k<oases;k++) {
        int cx = 18 + simRand()%(MAP_W-36), cy = 14 + simRand()%(MAP_H-28);
        if (!desertAt(cx,cy)) continue;
        int pr = 1 + simRand()%2;                              // pool radius
        int gr = pr + 2 + simRand()%2;                         // green ring radius
        for (int dy=-gr;dy<=gr;dy++) for (int dx=-gr;dx<=gr;dx++) {
            int nx=cx+dx, ny=cy+dy; if (!desertAt(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o==T_GOLD||o==T_MOUNTAIN) continue;
            int r2 = dx*dx+dy*dy;
            if (r2 <= pr*pr)            { g.map[ny][nx].terrain=T_WATER; g.map[ny][nx].resources=0; }
            else if (r2 <= (pr+1)*(pr+1)){ g.map[ny][nx].terrain=T_SHALLOWS; g.map[ny][nx].resources=0; }
            else if (r2 <= gr*gr) {
                int rr = simRand()%100;
                if      (rr<35) { g.map[ny][nx].terrain=T_PALM;  g.map[ny][nx].resources=60+simRand()%50; }
                else if (rr<48) { g.map[ny][nx].terrain=T_BERRY; g.map[ny][nx].resources=40+simRand()%40; }
                else            { g.map[ny][nx].terrain=T_GRASS; g.map[ny][nx].resources=0; }
            }
        }
    }

    // 3) Wadis — dry riverbeds of cracked earth with gravel banks and a few
    //    palms clinging to the moisture. Snaking lines across the sand.
    int wadis = 1 + simRand()%2;
    for (int k=0;k<wadis;k++) {
        int x = 10 + simRand()%(MAP_W-20), y = 10 + simRand()%(MAP_H-20);
        float ang = (simRand()%628)/100.0f;
        int len = 50 + simRand()%50;
        for (int i=0;i<len;i++) {
            ang += ((simRand()%100)-50)/180.0f;
            x += (int)roundf(cosf(ang)); y += (int)roundf(sinf(ang));
            for (int w=-1;w<=1;w++) for (int u=-1;u<=1;u++) {
                int nx=x+w, ny=y+u; if (!desertAt(nx,ny)) continue;
                Terrain o = g.map[ny][nx].terrain;
                if (o==T_GOLD||o==T_WATER||o==T_SHALLOWS) continue;
                if (w==0||u==0) { g.map[ny][nx].terrain = (simRand()%5==0)?T_MUD:T_DIRT; }
                else if (g.map[ny][nx].terrain!=T_DIRT && simRand()%2==0) g.map[ny][nx].terrain = T_GRAVEL;
            }
            if (simRand()%9==0 && desertAt(x,y) && g.map[y][x].terrain==T_DIRT)
                { g.map[y][x].terrain=T_PALM; g.map[y][x].resources=50+simRand()%40; }
        }
    }

    // 4) Mesas / buttes — abrupt rocky outcrops: an impassable stone heart with
    //    a gravel skirt, sometimes hiding a gold seam in the rock.
    int mesas = 3 + simRand()%4;
    for (int k=0;k<mesas;k++) {
        int cx = 16 + simRand()%(MAP_W-32), cy = 12 + simRand()%(MAP_H-24);
        if (!desertAt(cx,cy)) continue;
        int rad = 2 + simRand()%3;
        for (int dy=-rad-1;dy<=rad+1;dy++) for (int dx=-rad-1;dx<=rad+1;dx++) {
            int nx=cx+dx, ny=cy+dy; if (!desertAt(nx,ny)) continue;
            if (g.map[ny][nx].terrain==T_GOLD) continue;
            int r2 = dx*dx+dy*dy;
            if      (r2 <= rad*rad)         { g.map[ny][nx].terrain=T_STONE; g.map[ny][nx].resources=0; }
            else if (r2 <= (rad+1)*(rad+1)) g.map[ny][nx].terrain=T_GRAVEL;
        }
        if (simRand()%2==0) placeGoldCluster(cx, cy, 2 + simRand()%2);
    }

    // 5) Bleaching bones & wind-worn ruins scattered across the flats.
    for (int i=0;i<14;i++) {
        int x = 8 + simRand()%(MAP_W-16), y = 8 + simRand()%(MAP_H-16);
        if (!desertAt(x,y)) continue;
        Terrain o = g.map[y][x].terrain;
        if (o==T_SAND||o==T_DUNES||o==T_DIRT) {
            g.map[y][x].terrain = (simRand()%3==0) ? T_PALM : T_RUINS;
            if (g.map[y][x].terrain==T_PALM) g.map[y][x].resources = 40+simRand()%40;
        }
    }
}

// ============================================================================
// STEPPE CHARACTER — the grass-sea gets its furniture: bleached salt pans,
// kurgan barrow-mounds of the old horse lords (sometimes still holding their
// grave gold), dry gullies, and lone wind-bent trees on the skyline.
// ============================================================================
static void applySteppeFeatures() {
    auto steppeAt = [&](int x,int y){ return inBounds(x,y) && g.map[y][x].biome==B_STEPPE; };
    int n = 0;
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) if (g.map[y][x].biome==B_STEPPE) n++;
    if (n < 200) return;
    // Salt pans: bleached cracked flats in the driest hollows.
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
        Tile& t = g.map[y][x];
        if (t.biome!=B_STEPPE || t.resources>0) continue;
        if (t.terrain!=T_GRASS && t.terrain!=T_TALL_GRASS && t.terrain!=T_DIRT) continue;
        float sn = sampleNoise(x*0.05f+420, y*0.05f+420);
        if      (sn < 0.20f) t.terrain = T_DIRT;
        else if (sn > 0.86f) t.terrain = T_TALL_GRASS;   // waist-high banner-grass belts
    }
    // Kurgans: ringed barrow mounds; roughly half still hold grave gold.
    int kurgans = 3 + simRand()%3;
    for (int k=0;k<kurgans;k++) {
        int cx = 16 + simRand()%(MAP_W-32), cy = 12 + simRand()%(MAP_H-24);
        if (!steppeAt(cx,cy)) continue;
        for (int dy=-2;dy<=2;dy++) for (int dx=-2;dx<=2;dx++) {
            int nx=cx+dx, ny=cy+dy; if (!steppeAt(nx,ny)) continue;
            int r2 = dx*dx+dy*dy;
            Terrain o = g.map[ny][nx].terrain;
            if (o==T_GOLD||o==T_WATER) continue;
            if (r2 <= 1)      g.map[ny][nx].terrain = T_RUINS;
            else if (r2 <= 4 && simRand()%2==0) g.map[ny][nx].terrain = T_STONE;
        }
        g.map[cy][cx].terrain = T_MONOLITH; g.map[cy][cx].resources = 0;
        if (simRand()%2==0) placeGoldCluster(cx+2, cy+2, 2);
    }
    // Dry gullies: shallow cracked stream-beds wandering the flats.
    int gullies = 2 + simRand()%2;
    for (int k=0;k<gullies;k++) {
        int x = 10 + simRand()%(MAP_W-20), y = 10 + simRand()%(MAP_H-20);
        float ang = (simRand()%628)/100.0f;
        for (int i=0;i<40+simRand()%40;i++) {
            ang += ((simRand()%100)-50)/200.0f;
            x += (int)roundf(cosf(ang)); y += (int)roundf(sinf(ang));
            if (!steppeAt(x,y)) continue;
            Terrain o = g.map[y][x].terrain;
            if (o==T_GOLD||o==T_WATER||o==T_MONOLITH) continue;
            g.map[y][x].terrain = (simRand()%4==0) ? T_GRAVEL : T_DIRT;
        }
    }
    // Lone wind-bent trees on the horizon.
    for (int i=0;i<10;i++) {
        int x = 8 + simRand()%(MAP_W-16), y = 8 + simRand()%(MAP_H-16);
        if (!steppeAt(x,y)) continue;
        if (g.map[y][x].terrain==T_GRASS || g.map[y][x].terrain==T_TALL_GRASS)
            { g.map[y][x].terrain=T_DEAD_TREE; g.map[y][x].resources=40+simRand()%30; }
    }
}

// ============================================================================
// MOOR CHARACTER — heather uplands with real teeth: black peat bogs with
// open water eyes, granite tors, bilberry patches, and the old stones.
// ============================================================================
static void applyMoorFeatures() {
    auto moorAt = [&](int x,int y){ return inBounds(x,y) && g.map[y][x].biome==B_MOOR; };
    int n = 0;
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) if (g.map[y][x].biome==B_MOOR) n++;
    if (n < 200) return;
    // Peat bogs: coherent dark wet patches; the deepest have open water eyes.
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
        Tile& t = g.map[y][x];
        if (t.biome!=B_MOOR || t.resources>0) continue;
        if (t.terrain!=T_HEATH && t.terrain!=T_GRASS && t.terrain!=T_TALL_GRASS) continue;
        float bn = sampleNoise(x*0.055f+510, y*0.055f+510);
        if      (bn > 0.84f) { t.terrain = T_WATER; }                 // bog eye
        else if (bn > 0.76f) { t.terrain = T_MARSH; }
        else if (bn > 0.70f) { t.terrain = T_MUD;   }
    }
    // Tors: heaped granite outcrops with gravel skirts, landmarks for miles.
    int tors = 3 + simRand()%3;
    for (int k=0;k<tors;k++) {
        int cx = 14 + simRand()%(MAP_W-28), cy = 12 + simRand()%(MAP_H-24);
        if (!moorAt(cx,cy)) continue;
        int rad = 1 + simRand()%2;
        for (int dy=-rad-1;dy<=rad+1;dy++) for (int dx=-rad-1;dx<=rad+1;dx++) {
            int nx=cx+dx, ny=cy+dy; if (!moorAt(nx,ny)) continue;
            if (g.map[ny][nx].terrain==T_GOLD) continue;
            int r2 = dx*dx+dy*dy;
            if      (r2 <= rad*rad)         { g.map[ny][nx].terrain=T_STONE;  g.map[ny][nx].resources=0; }
            else if (r2 <= (rad+1)*(rad+1) && simRand()%2==0) g.map[ny][nx].terrain=T_GRAVEL;
        }
        if (simRand()%3==0) placeGoldCluster(cx, cy+rad+2, 2);
    }
    // Bilberries hide in the heather; stone circles keep their watch.
    for (int i=0;i<12;i++) {
        int x = 8 + simRand()%(MAP_W-16), y = 8 + simRand()%(MAP_H-16);
        if (moorAt(x,y) && g.map[y][x].terrain==T_HEATH)
            { g.map[y][x].terrain=T_BERRY; g.map[y][x].resources=45+simRand()%35; }
    }
    for (int i=0;i<2;i++) {
        int sx = 15 + simRand()%(MAP_W-30), sy = 15 + simRand()%(MAP_H-30);
        if (!moorAt(sx,sy) || !isPassable(sx,sy) || g.map[sy][sx].terrain==T_GOLD) continue;
        g.map[sy][sx].terrain = T_MONOLITH; g.map[sy][sx].resources = 0;
        for (int a=0;a<5;a++) {
            int rx = sx+(simRand()%5)-2, ry = sy+(simRand()%5)-2;
            if ((rx==sx&&ry==sy) || !moorAt(rx,ry)) continue;
            Terrain o = g.map[ry][rx].terrain;
            if (o==T_HEATH||o==T_GRASS||o==T_TALL_GRASS)
                g.map[ry][rx].terrain = (simRand()%2) ? T_STONE : T_RUINS;
        }
    }
}

// Coastal map: 2-3 large landmasses separated by sea channels, with a few
// small islands in between. Replaces the noise-soup archipelago.
static void generateContinentMap() {
    // Pick continent seeds, spread evenly across the map.
    int n = 2 + (simRand() % 2);                       // 2 or 3 continents
    std::vector<std::pair<int,int>> seeds;
    auto jitter = [](int v, int amt) { return v + (simRand() % (2*amt + 1)) - amt; };
    if (n == 2) {
        seeds.push_back({jitter(MAP_W*1/4, 10), jitter(MAP_H/2, MAP_H/6)});
        seeds.push_back({jitter(MAP_W*3/4, 10), jitter(MAP_H/2, MAP_H/6)});
    } else {
        seeds.push_back({jitter(MAP_W*1/4, 8), jitter(MAP_H*1/3, 6)});
        seeds.push_back({jitter(MAP_W*3/4, 8), jitter(MAP_H*1/3, 6)});
        seeds.push_back({jitter(MAP_W/2,    8), jitter(MAP_H*3/4, 6)});
    }

    // Continent radius: scales with map size. ~1/4 of the shorter dimension.
    int contR = std::min(MAP_W, MAP_H) / 4 + 5;

    // Assign every tile by (noise-perturbed) distance to nearest continent seed.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float minD = 1e9f;
        for (auto& s : seeds) {
            float d = edist(x, y, s.first, s.second);
            if (d < minD) minD = d;
        }
        // Wobble the coastline with noise so continents aren't perfect circles.
        float wobble = sampleNoise(x*0.07f, y*0.07f) * 18.0f - 4.0f; // -4..+14
        float adjD = minD + wobble;

        Biome b; Terrain t;
        if      (adjD < contR - 6) { b = B_TEMPERATE; t = T_GRASS;    }
        else if (adjD < contR - 3) { b = B_TEMPERATE; t = (simRand()%3==0) ? T_SAND : T_GRASS; }
        else if (adjD < contR)     { b = B_TEMPERATE; t = T_SAND;     }    // beach
        else if (adjD < contR + 3) { b = B_OCEAN;     t = T_SHALLOWS; }
        else                       { b = B_OCEAN;     t = T_WATER;    }
        g.map[y][x] = {t, 0, {}, {}, b, t, 0, 0, 0, 0, 0};
    }

    // Inland variety: scatter forest/meadow/tall grass on grass tiles.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (g.map[y][x].biome != B_TEMPERATE || g.map[y][x].terrain != T_GRASS) continue;
        int r = simRand() % 100;
        if      (r < 12) { g.map[y][x].terrain = T_FOREST; g.map[y][x].resources = 100 + simRand()%100; }
        else if (r < 16) g.map[y][x].terrain = T_TALL_GRASS;
        else if (r < 19) g.map[y][x].terrain = T_FLOWERS;
        else if (r < 21) g.map[y][x].terrain = T_MEADOW;
    }

    // A few small islands scattered between continents — strategic stepping stones.
    for (int i = 0; i < 9; i++) {
        int ix = 15 + simRand()%(MAP_W-30), iy = 15 + simRand()%(MAP_H-30);
        if (g.map[iy][ix].terrain != T_WATER) continue;
        int sz = 1 + simRand() % 2;
        for (int dy = -sz-1; dy <= sz+1; dy++) for (int dx = -sz-1; dx <= sz+1; dx++) {
            int nx = ix+dx, ny = iy+dy;
            if (!inBounds(nx,ny)) continue;
            int r2 = dx*dx + dy*dy;
            if      (r2 <= sz*sz) {
                Terrain isle = (simRand()%3==0) ? T_FOREST : T_GRASS;
                g.map[ny][nx].terrain = isle;
                g.map[ny][nx].biome = B_TEMPERATE;
                if (isle == T_FOREST) g.map[ny][nx].resources = 80 + simRand()%60;
            }
            else if (r2 <= (sz+1)*(sz+1)) {
                if (g.map[ny][nx].terrain == T_WATER) g.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }

    // Fish in water (slightly denser than the standard map — more naval food).
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && simRand() % 22 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + simRand() % 70;
        }
    }

    // Gold clusters on land only.
    for (int i = 0; i < 14; i++) {
        int gx = 15 + simRand()%(MAP_W-30), gy = 15 + simRand()%(MAP_H-30);
        if (g.map[gy][gx].biome == B_TEMPERATE) placeGoldCluster(gx, gy, 3 + simRand()%3);
    }

    // Berry, wheat, ruins on land.
    for (int i = 0; i < 16; i++) {
        int bx = 10 + simRand()%(MAP_W-20), by = 10 + simRand()%(MAP_H-20);
        if (g.map[by][bx].biome != B_TEMPERATE) continue;
        int sz = 1 + simRand() % 2;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && simRand()%3 != 0) {
                g.map[ny][nx].terrain = T_BERRY;
                g.map[ny][nx].resources = 50 + simRand() % 40;
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        int wx = 10 + simRand()%(MAP_W-20), wy = 10 + simRand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + simRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && simRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    for (int i = 0; i < 15; i++) {
        int rx = 10 + simRand()%(MAP_W-20), ry = 10 + simRand()%(MAP_H-20);
        if (g.map[ry][rx].biome != B_TEMPERATE) continue;
        for (int j = 0; j < 3+simRand()%4; j++) {
            int nx = rx + simRand()%5-2, ny = ry + simRand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }

    // One castle ruin per continent — a landmark on each landmass.
    for (auto& s : seeds) placeCastleRuin(s.first - 3, s.second - 3, 6);

    // Theme the islands to the match climate and soften their coasts.
    applyClimateSkin();
    applyEcotones();
    applyDesertFeatures();
    applySteppeFeatures();
    applyMoorFeatures();

    // Snapshot for winter thaw cycle.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}

// ============================================================================
// HAND-SHAPED LAYOUT MAPS — Highlands / Deep Woods / Riverlands.
// Like the Ocean generator, each owns its whole topology and dispatches from
// generateMap(). They set tile.biome to a real *climate* (so the renderer
// colours them), while g.biomeChoice carries the layout id for dispatch/AI.
// ============================================================================

// Shared resource + landmark scatter for the layout maps. Keeps them
// economically playable without duplicating the climate generator's long tail.
static void finishLayout() {
    // Theme the neutral template to the chosen climate, then soften the borders.
    applyClimateSkin();
    applyEcotones();
    applyDesertFeatures();
    applySteppeFeatures();
    applyMoorFeatures();
    for (int i = 0; i < 16; i++)
        placeGoldCluster(15 + simRand()%(MAP_W-30), 15 + simRand()%(MAP_H-30), 3 + simRand()%3);
    // Berry patches on open grass.
    for (int i = 0; i < 18; i++) {
        int bx = 10 + simRand()%(MAP_W-20), by = 10 + simRand()%(MAP_H-20), sz = 1 + simRand()%3;
        for (int dy=-sz; dy<=sz; dy++) for (int dx=-sz; dx<=sz; dx++) {
            int nx=bx+dx, ny=by+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain==T_GRASS && simRand()%3!=0) {
                g.map[ny][nx].terrain = T_BERRY; g.map[ny][nx].resources = 50 + simRand()%40;
            }
        }
    }
    // Wheat fields on open grass.
    for (int i = 0; i < 14; i++) {
        int wx = 10 + simRand()%(MAP_W-20), wy = 10 + simRand()%(MAP_H-20), sz = 2 + simRand()%3;
        for (int dy=-sz; dy<=sz; dy++) for (int dx=-sz; dx<=sz; dx++) {
            int nx=wx+dx, ny=wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain==T_GRASS && simRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    // Capturable keeps + a couple of sightline monoliths.
    placeCastleRuin(MAP_W/4,   MAP_H/4,   6);
    placeCastleRuin(3*MAP_W/4, 3*MAP_H/4, 6);
    for (int i = 0; i < 3; i++) {
        int sx = 15 + simRand()%(MAP_W-30), sy = 15 + simRand()%(MAP_H-30);
        if (isPassable(sx,sy) && g.map[sy][sx].terrain != T_GOLD) {
            g.map[sy][sx].terrain = T_MONOLITH; g.map[sy][sx].resources = 0;
        }
    }
    // Baseline snapshot for the winter->spring thaw cycle.
    for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}

// Draw one mountain range across the map with `gaps` clear passes so a region
// is never sealed off. Crest is impassable mountain; flanks are stone with a
// climbable T_HILLS ramp on the outer edge.
static void drawRidge(bool horizontal, int pos, int gaps) {
    int along = horizontal ? MAP_W : MAP_H;
    std::vector<int> gapAt;
    for (int k = 0; k < gaps; k++)
        gapAt.push_back(along*(k+1)/(gaps+1) + (simRand()%(along/6) - along/12));
    for (int i = 6; i < along-6; i++) {
        bool inGap = false;
        for (int gc : gapAt) if (std::abs(i - gc) < 4) { inGap = true; break; }
        if (inGap) continue;
        int center = pos + (int)(sampleNoise(i*0.35f, pos*0.35f + 70) * 4.0f) - 2;
        int thick  = 1 + (sampleNoise(i*0.5f, 30) > 0.5f ? 1 : 0);
        for (int d = -thick; d <= thick; d++) {
            int x = horizontal ? i : center + d;
            int y = horizontal ? center + d : i;
            if (!inBounds(x,y) || g.map[y][x].terrain == T_GOLD) continue;
            g.map[y][x].terrain   = (d == 0) ? T_MOUNTAIN
                                  : (std::abs(d) == thick ? T_HILLS : T_STONE);
            g.map[y][x].resources = 0;
        }
    }
}

// Highlands: rugged temperate plateau carved by mountain ranges into valleys
// and choke points. Gold favours the rock.
static void generateHighlandsMap() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.biome = B_TEMPERATE; t.resources = 0; t.elev = 0;
        float n = sampleNoise(x*0.18f+11, y*0.18f+11);
        int r = simRand()%24;
        if      (n > 0.82f) t.terrain = T_GRAVEL;                                  // rocky, passable
        else if (r < 3)     { t.terrain = T_PINE; t.resources = 80 + simRand()%60; }
        else if (r < 6)     t.terrain = T_HILLS;
        else if (r < 8)     t.terrain = T_TALL_GRASS;
        else                t.terrain = T_GRASS;
    }
    int ridges = 3 + simRand()%2;
    for (int i = 0; i < ridges; i++) {
        bool horiz = (simRand()%2 == 0);
        int sp = horiz ? MAP_H : MAP_W;
        int pos = sp*(i+1)/(ridges+1) + (simRand()%(sp/6) - sp/12);
        drawRidge(horiz, pos, 1 + simRand()%2);
    }
    for (int i = 0; i < 10; i++) {
        int gx = 12 + simRand()%(MAP_W-24), gy = 12 + simRand()%(MAP_H-24);
        Terrain o = g.map[gy][gx].terrain;
        if (o == T_GRAVEL || o == T_HILLS || o == T_STONE) placeGoldCluster(gx, gy, 3 + simRand()%3);
    }
    finishLayout();
}

// Deep Woods: wall-to-wall forest (passable but slow & sight-blocking, wood
// rich) with open glades to settle in, linked by cleared lanes.
static void generateDeepWoodsMap() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.biome = B_FOREST; t.elev = 0;
        if (sampleNoise(x*0.16f+21, y*0.16f+21) > 0.55f) { t.terrain = T_FOREST; t.resources = 100 + simRand()%100; }
        else                                              { t.terrain = T_PINE;   t.resources = 80  + simRand()%60;  }
    }
    int glades = 6 + simRand()%3;
    std::vector<std::pair<int,int>> centers;
    for (int i = 0; i < glades; i++) {
        int cx = 16 + simRand()%(MAP_W-32), cy = 14 + simRand()%(MAP_H-28), rad = 6 + simRand()%4;
        centers.push_back({cx,cy});
        for (int dy=-rad; dy<=rad; dy++) for (int dx=-rad; dx<=rad; dx++) {
            int nx=cx+dx, ny=cy+dy;
            if (!inBounds(nx,ny) || dx*dx+dy*dy > rad*rad) continue;
            g.map[ny][nx].terrain = T_GRASS; g.map[ny][nx].resources = 0; g.map[ny][nx].biome = B_TEMPERATE;
        }
    }
    // Cleared lanes link the glades in a loop so armies have open routes.
    auto lane = [&](int x0,int y0,int x1,int y1) {
        int x=x0, y=y0;
        while (x!=x1 || y!=y1) {
            for (int w=-1; w<=1; w++) {
                int ax=x, ay=y+w, bx=x+w, by=y;
                if (inBounds(ax,ay)) { g.map[ay][ax].terrain=T_GRASS; g.map[ay][ax].resources=0; g.map[ay][ax].biome=B_TEMPERATE; }
                if (inBounds(bx,by)) { g.map[by][bx].terrain=T_GRASS; g.map[by][bx].resources=0; g.map[by][bx].biome=B_TEMPERATE; }
            }
            if (x<x1) x++; else if (x>x1) x--;
            if (y<y1) y++; else if (y>y1) y--;
        }
    };
    for (size_t i=0; i+1<centers.size(); i++) lane(centers[i].first,centers[i].second,centers[i+1].first,centers[i+1].second);
    if (centers.size() > 2) lane(centers.back().first,centers.back().second,centers.front().first,centers.front().second);
    finishLayout();
}

// Riverlands (River/Fortress): temperate country split by a great meandering
// river with a handful of bridge crossings — a natural front line.
static void generateRiverMap() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.biome = B_TEMPERATE; t.resources = 0; t.elev = 0;
        int r = simRand()%20;
        if      (r < 4) { t.terrain = T_FOREST; t.resources = 90 + simRand()%80; }
        else if (r < 6) t.terrain = T_TALL_GRASS;
        else            t.terrain = T_GRASS;
    }
    bool vertical = (simRand()%2 == 0);
    int span   = vertical ? MAP_H : MAP_W;
    int center = (vertical ? MAP_W : MAP_H) / 2;
    auto axisAt = [&](int i){ return center + (int)(sampleNoise(i*0.12f, 7.0f)*18.0f) - 9; };
    auto halfAt = [&](int i){ return 2 + (sampleNoise(i*0.08f, 23.0f) > 0.5f ? 1 : 0); };
    for (int i = 0; i < span; i++) {
        int axis = axisAt(i), hw = halfAt(i);
        for (int w = -hw-1; w <= hw+1; w++) {
            int x = vertical ? axis+w : i, y = vertical ? i : axis+w;
            if (!inBounds(x,y)) continue;
            g.map[y][x].terrain = (std::abs(w) > hw) ? T_SHALLOWS : T_WATER;
            g.map[y][x].resources = 0;
        }
    }
    for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++)
        if (g.map[y][x].terrain==T_WATER && simRand()%22==0) { g.map[y][x].terrain=T_FISH; g.map[y][x].resources=80+simRand()%70; }
    // Bridges: 2-3 guaranteed land crossings (block boats, pass armies).
    int crossings = 2 + simRand()%2;
    for (int k = 0; k < crossings; k++) {
        int i = span*(k+1)/(crossings+1) + (simRand()%9 - 4);
        if (i < 2 || i >= span-2) continue;
        int axis = axisAt(i), hw = halfAt(i);
        for (int w = -hw-2; w <= hw+2; w++) for (int t2 = -1; t2 <= 1; t2++) {
            int ii = i + t2;
            int x = vertical ? axis+w : ii, y = vertical ? ii : axis+w;
            if (!inBounds(x,y)) continue;
            Terrain o = g.map[y][x].terrain;
            if (o==T_WATER||o==T_SHALLOWS||o==T_FISH) { g.map[y][x].terrain=T_BRIDGE; g.map[y][x].resources=0; }
        }
    }
    finishLayout();
}

// Open Plains / Steppe: a near-treeless sea of grass broken only by scattered
// straggler trees, the odd copse, a watering hole and a low stone outcrop —
// wide sightlines and nothing to break a charge, the natural home of cavalry.
// As a neutral template it skins to steppe (temperate), tundra (snow), or open
// desert; applyDesertFeatures then dunes/oases the desert variant.
static void generatePlainsMap() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.biome = B_TEMPERATE; t.resources = 0; t.elev = 0;
        float n = sampleNoise(x*0.10f+41, y*0.10f+41);
        int r = simRand()%100;
        if      (n > 0.88f) t.terrain = T_HILLS;        // occasional rolling rise
        else if (r < 6)     t.terrain = T_TALL_GRASS;
        else if (r < 9)     t.terrain = T_MEADOW;
        else if (r < 11)    t.terrain = T_FLOWERS;
        else                t.terrain = T_GRASS;
    }
    // Straggler trees dotted across the steppe — enough wood to play, far too
    // sparse to obstruct a cavalry line.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (g.map[y][x].terrain == T_GRASS && simRand()%100 < 3) {
            g.map[y][x].terrain = T_FOREST; g.map[y][x].resources = 60 + simRand()%50;
        }
    }
    // A handful of small copses for concentrated wood — ragged, never walls.
    int copses = 8 + simRand()%4;
    for (int i = 0; i < copses; i++) {
        int cx = 14 + simRand()%(MAP_W-28), cy = 12 + simRand()%(MAP_H-24);
        int rad = 2 + simRand()%2;
        for (int dy=-rad; dy<=rad; dy++) for (int dx=-rad; dx<=rad; dx++) {
            int nx=cx+dx, ny=cy+dy;
            if (!inBounds(nx,ny) || dx*dx+dy*dy > rad*rad) continue;
            if (simRand()%4 == 0) continue;             // ragged edge
            g.map[ny][nx].terrain = T_FOREST; g.map[ny][nx].resources = 90 + simRand()%70;
        }
    }
    // A couple of watering holes — light fishing/naval, never a barrier.
    int ponds = 1 + simRand()%2;
    for (int i = 0; i < ponds; i++) {
        int cx = 25 + simRand()%(MAP_W-50), cy = 20 + simRand()%(MAP_H-40);
        int sz = 3 + simRand()%3;
        for (int dy=-sz; dy<=sz; dy++) for (int dx=-sz; dx<=sz; dx++) {
            int nx=cx+dx, ny=cy+dy, r2 = dx*dx+dy*dy;
            if (!inBounds(nx,ny) || r2 > sz*sz) continue;
            g.map[ny][nx].terrain = (r2 < (sz-1)*(sz-1)) ? T_WATER : T_SHALLOWS;
            g.map[ny][nx].resources = 0;
        }
    }
    // Low stone outcrops — sparse landmarks and bits of cover.
    for (int i = 0; i < 5; i++) {
        int cx = 12 + simRand()%(MAP_W-24), cy = 12 + simRand()%(MAP_H-24);
        for (int j = 0; j < 3; j++) {
            int nx=cx+simRand()%4-2, ny=cy+simRand()%4-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain==T_GRASS) g.map[ny][nx].terrain = T_STONE;
        }
    }
    finishLayout();
}

// Delta: one great river enters from the north edge, runs as a single broad
// stem, then fans into braided distributaries that comb down to the south
// coast. Between them: rich silt islands heavy with wild wheat, reed marshes,
// and channels full of fish. Fords and bridges decide the land war; boats
// own everything else.
static void generateDeltaMap() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.biome = B_TEMPERATE; t.resources = 0; t.elev = 0;
        int r = simRand()%24;
        if      (r < 3) { t.terrain = T_FOREST; t.resources = 90 + simRand()%80; }
        else if (r < 5) t.terrain = T_TALL_GRASS;
        else if (r < 7) t.terrain = T_MEADOW;
        else            t.terrain = T_GRASS;
    }
    // Carve one channel from (x,y) walking a drifting angle until off-map.
    auto channel = [&](float x, float y, float ang, int hw, float wobble) {
        std::vector<std::pair<int,int>> path;
        while (x > 2 && x < MAP_W-2 && y < MAP_H-1) {
            ang += ((simRand()%100)-50)/wobble;
            x += cosf(ang); y += sinf(ang);
            int ix = (int)x, iy = (int)y;
            path.push_back({ix, iy});
            for (int w=-hw-1; w<=hw+1; w++) for (int u=-hw-1; u<=hw+1; u++) {
                int nx=ix+w, ny=iy+u; if (!inBounds(nx,ny)) continue;
                int d = std::max(std::abs(w), std::abs(u));
                if (g.map[ny][nx].terrain==T_GOLD) continue;
                if (d <= hw)      { g.map[ny][nx].terrain=T_WATER;    g.map[ny][nx].resources=0; }
                else if (g.map[ny][nx].terrain!=T_WATER)
                                  { g.map[ny][nx].terrain=T_SHALLOWS; g.map[ny][nx].resources=0; }
            }
        }
        return path;
    };
    // The stem: broad, from the top edge to a fork point mid-map.
    float sx = MAP_W*0.30f + simRand()%(MAP_W/3);
    auto stem = channel(sx, 1.0f, 1.5708f, 2, 260.0f);   // due south, gentle meander
    // Fork: 3-4 distributaries fanning from partway down the stem.
    if (!stem.empty()) {
        int arms = 3 + simRand()%2;
        for (int a = 0; a < arms; a++) {
            auto& f = stem[stem.size()/3 + simRand()%(stem.size()/3)];
            float ang = 1.5708f + ((a - arms/2.0f) + 0.5f) * 0.55f
                      + ((simRand()%40)-20)/100.0f;
            channel((float)f.first, (float)f.second, ang, 1, 200.0f);
        }
        // Bridges on the stem above the fork — the only dry crossings up north.
        for (int k = 0; k < 2; k++) {
            auto& b = stem[stem.size()/6 + k*stem.size()/6];
            for (int w=-4; w<=4; w++) for (int t2=0; t2<2; t2++) {
                int nx=b.first+w, ny=b.second+t2;
                if (!inBounds(nx,ny)) continue;
                Terrain o = g.map[ny][nx].terrain;
                if (o==T_WATER||o==T_SHALLOWS||o==T_FISH) { g.map[ny][nx].terrain=T_BRIDGE; g.map[ny][nx].resources=0; }
            }
        }
    }
    // Reed marsh hugs every channel; the silt between grows wild wheat.
    static const int d8[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        if (t.terrain != T_GRASS && t.terrain != T_MEADOW && t.terrain != T_TALL_GRASS) continue;
        bool nearWater = false;
        for (auto& d : d8) { int nx=x+d[0], ny=y+d[1];
            if (inBounds(nx,ny) && (g.map[ny][nx].terrain==T_SHALLOWS||g.map[ny][nx].terrain==T_WATER)) { nearWater=true; break; } }
        if (!nearWater) continue;
        int r = simRand()%100;
        if      (r < 30) t.terrain = T_REEDS;
        else if (r < 45) t.terrain = T_MARSH;
        else if (r < 70) t.terrain = T_WHEAT;      // the silt is black gold
    }
    // Channels teem with fish — the delta feeds whoever holds it.
    for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++)
        if ((g.map[y][x].terrain==T_WATER||g.map[y][x].terrain==T_SHALLOWS) && simRand()%14==0)
            { g.map[y][x].terrain=T_FISH; g.map[y][x].resources=80+simRand()%70; }
    finishLayout();
}

// Vale: a rift valley. Two cliff-edged plateaus flank a broad fertile
// corridor with a fordable stream down its spine — the breadbasket lies low
// and exposed, the gold and the high ground belong to whoever climbs for it.
static void generateValeMap() {
    bool horiz = (simRand()%2 == 0);           // corridor runs along the long axis?
    int span  = horiz ? MAP_H : MAP_W;         // across the valley
    int c0 = span*36/100, c1 = span*64/100;    // valley floor band
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        int cross = horiz ? y : x;
        int along = horiz ? x : y;
        int edge = (int)(sampleNoise(along*0.08f+33, 5.0f)*7.0f) - 3;   // wandering rims
        bool floorBand = (cross > c0+edge && cross < c1-edge);
        t.biome = B_TEMPERATE; t.resources = 0;
        t.elev = floorBand ? 0 : 1;
        int r = simRand()%100;
        if (floorBand) {
            if      (r < 6)  t.terrain = T_MEADOW;
            else if (r < 10) t.terrain = T_FLOWERS;
            else if (r < 14) { t.terrain = T_FOREST; t.resources = 90 + simRand()%80; }
            else if (r < 17) t.terrain = T_TALL_GRASS;
            else             t.terrain = T_GRASS;
        } else {
            if      (r < 10) { t.terrain = T_PINE; t.resources = 80 + simRand()%60; }
            else if (r < 16) t.terrain = T_GRAVEL;
            else if (r < 19) t.terrain = T_STONE;
            else if (r < 22) t.terrain = T_TALL_GRASS;
            else             t.terrain = T_GRASS;
        }
    }
    // The stream: down the valley's spine, narrow, with guaranteed fords.
    int mid = (c0 + c1) / 2;
    int len = horiz ? MAP_W : MAP_H;
    for (int i = 0; i < len; i++) {
        int axis = mid + (int)(sampleNoise(i*0.10f, 61.0f)*8.0f) - 4;
        for (int w = -1; w <= 1; w++) {
            int x = horiz ? i : axis+w, y = horiz ? axis+w : i;
            if (!inBounds(x,y)) continue;
            g.map[y][x].terrain = (w == 0) ? T_WATER : T_SHALLOWS;
            g.map[y][x].resources = 0; g.map[y][x].elev = 0;
        }
        if (i % (len/5) == len/10) {          // five fords, evenly spaced
            for (int w = -1; w <= 1; w++) for (int t2 = -1; t2 <= 1; t2++) {
                int x = horiz ? i+t2 : axis+w, y = horiz ? axis+w : i+t2;
                if (inBounds(x,y) && g.map[y][x].terrain==T_WATER) g.map[y][x].terrain = T_SHALLOWS;
            }
        }
    }
    // Ramps: broad causeways up each rim at the thirds, plus scattered goat
    // paths — the plateau is reachable everywhere but OWNED at the ramps.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        if (t.elev != 1) continue;
        static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        bool rim = false;
        for (auto& d : d4) { int nx=x+d[0], ny=y+d[1];
            if (inBounds(nx,ny) && g.map[ny][nx].elev==0 && g.map[ny][nx].terrain!=T_WATER) { rim=true; break; } }
        if (!rim) continue;
        int along = horiz ? x : y;
        bool causeway = false;
        for (int k = 1; k <= 3; k++) if (std::abs(along - len*k/4) < 3) causeway = true;
        if (causeway || ((unsigned)(x*7919 + y*6271) % 7) == 0) {
            if (t.terrain != T_GOLD && t.terrain != T_STONE) { t.terrain = T_HILLS; t.resources = 0; }
        }
    }
    // The heights hold the gold; the floor grows the corn (finishLayout wheat
    // lands mostly on the floor's grass anyway).
    for (int i = 0; i < 8; i++) {
        int gx = 12 + simRand()%(MAP_W-24), gy = 12 + simRand()%(MAP_H-24);
        if (g.map[gy][gx].elev == 1) placeGoldCluster(gx, gy, 3 + simRand()%3);
    }
    finishLayout();
}

// Canyons: badlands. Winding walls of bare rock cut the land into gorges and
// gulches; a handful of broad passes and spring-fed pockets decide where
// armies CAN fight, and towers on the gorge mouths decide if they dare.
static void generateCanyonsMap() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.biome = B_TEMPERATE; t.resources = 0; t.elev = 0;
        int r = simRand()%100;
        if      (r < 14) t.terrain = T_GRAVEL;
        else if (r < 22) t.terrain = T_DIRT;
        else if (r < 26) t.terrain = T_TALL_GRASS;
        else if (r < 29) { t.terrain = T_DEAD_TREE; t.resources = 40 + simRand()%30; }
        else             t.terrain = T_GRASS;
    }
    // Rock walls: long winding ridges in both directions carve the maze.
    for (int k = 0; k < 3; k++) drawRidge(true,  MAP_H*(k+1)/4 + (simRand()%12)-6, 2 + simRand()%2);
    for (int k = 0; k < 3; k++) drawRidge(false, MAP_W*(k+1)/4 + (simRand()%14)-7, 2 + simRand()%2);
    // Guaranteed broad lanes so no spawn is ever sealed in: one clear cross.
    auto lane = [&](bool horizL, int pos) {
        int along = horizL ? MAP_W : MAP_H;
        for (int i = 0; i < along; i++) {
            int centre = pos + (int)(sampleNoise(i*0.15f, 88.0f)*6.0f) - 3;
            for (int w = -2; w <= 2; w++) {
                int x = horizL ? i : centre+w, y = horizL ? centre+w : i;
                if (!inBounds(x,y)) continue;
                Terrain o = g.map[y][x].terrain;
                if (o==T_MOUNTAIN||o==T_STONE||o==T_HILLS)
                    { g.map[y][x].terrain = T_GRAVEL; g.map[y][x].resources = 0; }
            }
        }
    };
    lane(true,  MAP_H/2 + (simRand()%10)-5);
    lane(false, MAP_W/2 + (simRand()%10)-5);
    // Springs: green pockets in the rock shadow — water, grass, a few palms'
    // worth of trees. The only soft ground out here; worth walls.
    int springs = 3 + simRand()%2;
    for (int k = 0; k < springs; k++) {
        int cx = 20 + simRand()%(MAP_W-40), cy = 16 + simRand()%(MAP_H-32);
        int gr = 4 + simRand()%3;
        for (int dy=-gr; dy<=gr; dy++) for (int dx=-gr; dx<=gr; dx++) {
            int nx=cx+dx, ny=cy+dy; if (!inBounds(nx,ny)) continue;
            int r2 = dx*dx+dy*dy; if (r2 > gr*gr) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o==T_MOUNTAIN||o==T_GOLD) continue;
            if      (r2 <= 1)              { g.map[ny][nx].terrain=T_WATER;    g.map[ny][nx].resources=0; }
            else if (r2 <= 4)              { g.map[ny][nx].terrain=T_SHALLOWS; g.map[ny][nx].resources=0; }
            else if (simRand()%3 == 0)     { g.map[ny][nx].terrain=T_FOREST;   g.map[ny][nx].resources=70+simRand()%60; }
            else                           { g.map[ny][nx].terrain=T_GRASS;    g.map[ny][nx].resources=0; }
        }
    }
    // Gold seams glint where the rock was cut.
    for (int i = 0; i < 10; i++) {
        int gx = 12 + simRand()%(MAP_W-24), gy = 12 + simRand()%(MAP_H-24);
        Terrain o = g.map[gy][gx].terrain;
        if (o==T_GRAVEL||o==T_STONE||o==T_HILLS||o==T_DIRT) placeGoldCluster(gx, gy, 3 + simRand()%2);
    }
    finishLayout();
}

void generateMap() {
    initNoise();
    // Topology is chosen by the (climate-independent) Layout axis; each special
    // layout owns its own generator and themes itself via applyClimateSkin().
    int lay = (g.layoutChoice >= 0 && g.layoutChoice < LAYOUT_COUNT) ? g.layoutChoice : L_CONTINENTAL;
    switch (lay) {
        case L_ISLANDS:     generateContinentMap(); return;
        case L_HIGHLANDS:   generateHighlandsMap(); return;
        case L_DEEPWOODS:   generateDeepWoodsMap(); return;
        case L_RIVER:       generateRiverMap();     return;
        case L_PLAINS:      generatePlainsMap();    return;
        case L_DELTA:       generateDeltaMap();     return;
        case L_VALE:        generateValeMap();      return;
        case L_CANYONS:     generateCanyonsMap();   return;
        case L_CONTINENTAL: default: break;   // the inline land generator below
    }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        // Two noise channels at different scales give organic biome regions.
        // n1 is the macro climate (hot/cold), n2 is the micro feature (wet/dry).
        float n1 = sampleNoise(x*0.028f,     y*0.028f);
        float n2 = sampleNoise(x*0.020f+10,  y*0.020f+10);
        Biome b = B_TEMPERATE;
        if (isClimateChoice(g.biomeChoice)) {
            // Player picked a biome — make it dominant (~70%) but still inject
            // patches of other biomes so the map has variety.
            b = (Biome)g.biomeChoice;
            // ~30% chance to swap in a contrasting biome from a small patch.
            float patch = sampleNoise(x*0.06f+30, y*0.06f+30);
            if (patch > 0.78f) {
                // Pick a non-ocean contrast based on n2 for variety.
                if      (b == B_TEMPERATE) b = (n2 > 0.5f) ? B_FOREST : B_DESERT;
                else if (b == B_FOREST)    b = (n2 > 0.5f) ? B_TEMPERATE : B_SWAMP;
                else if (b == B_DESERT)    b = (n2 > 0.5f) ? B_TEMPERATE : B_SNOW;
                else if (b == B_SNOW)      b = (n2 > 0.5f) ? B_FOREST : B_TEMPERATE;
                else if (b == B_SWAMP)     b = (n2 > 0.5f) ? B_FOREST : B_TEMPERATE;
                else if (b == B_STEPPE)    b = (n2 > 0.5f) ? B_TEMPERATE : B_DESERT;
                else if (b == B_MOOR)      b = (n2 > 0.5f) ? B_FOREST : B_SWAMP;
                // Ocean stays ocean — its identity is total water coverage.
            }
        } else {
            // Random map: latitude-banded climate. North is cold, south is
            // hot, noise wobbles the band borders so they read as organic
            // frontiers rather than ruler lines. Swamps hug the wet noise,
            // forests sit on the cool side of temperate. The map gets real
            // geography: tundra campaigns up top, desert flanks below.
            float lat = (float)y / MAP_H;                 // 0 = north, 1 = south
            float climate = lat * 0.55f + n1 * 0.45f;     // cold..hot with wobble
            if      (climate < 0.24f)          b = B_SNOW;
            else if (climate > 0.76f)          b = B_DESERT;
            else if (n2 > 0.72f)               b = B_SWAMP;
            else if (climate > 0.62f && n2 < 0.42f) b = B_STEPPE;   // dry belt above the desert
            else if (climate < 0.40f && n2 > 0.55f) b = B_MOOR;     // cool wet uplands
            else if (climate < 0.45f && n2 < 0.40f) b = B_FOREST;
            // remainder stays B_TEMPERATE
        }
        g.map[y][x] = {T_GRASS, 0, {}, {}, b, T_GRASS, 0, 0, 0, 0, 0};
    }
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x]; int r = simRand() % 100;
        switch (t.biome) {
        case B_TEMPERATE:
            if (r<5)       t.terrain = T_TALL_GRASS;
            else if (r<8)  t.terrain = T_FLOWERS;
            else if (r<10) t.terrain = T_MEADOW;
            else if (r<14) { t.terrain = T_FOREST; t.resources = 100 + simRand() % 100; }
            else           t.terrain = T_GRASS;
            break;
        case B_DESERT:
            // Mostly plain sand here; applyDesertFeatures() carves the dune
            // seas, badlands, oases and mesas that give the desert its shape.
            if (r<84)      t.terrain = T_SAND;
            else if (r<92) t.terrain = T_DUNES;
            else if (r<96) t.terrain = T_GRAVEL;
            else           { t.terrain = T_PALM; t.resources = 50 + simRand() % 40; }
            break;
        case B_SNOW:
            if (r<60)      t.terrain = T_SNOW;
            else if (r<75) { t.terrain = T_PINE; t.resources = 80 + simRand() % 60; }
            else if (r<80) t.terrain = T_STONE;
            else           t.terrain = T_SNOW;
            break;
        case B_SWAMP:
            if (r<30)      t.terrain = T_MARSH;
            else if (r<45) t.terrain = T_REEDS;
            else if (r<55) t.terrain = T_SHALLOWS;
            else if (r<65) { t.terrain = T_DEAD_TREE; t.resources = 40 + simRand() % 30; }
            else           t.terrain = T_TALL_GRASS;
            break;
        case B_FOREST:
            if (r<40)      { t.terrain = T_FOREST; t.resources = 100 + simRand() % 100; }
            else if (r<55) { t.terrain = T_PINE;   t.resources = 80  + simRand() % 60;  }
            else if (r<60) { t.terrain = T_BERRY;  t.resources = 50  + simRand() % 40;  }
            else if (r<65) t.terrain = T_TALL_GRASS;
            else           t.terrain = T_GRASS;
            break;
        case B_STEPPE:
            if (r<2)       { t.terrain = T_DEAD_TREE; t.resources = 40 + simRand()%30; }
            else if (r<10) t.terrain = T_TALL_GRASS;
            else if (r<13) t.terrain = T_DIRT;
            else if (r<15) t.terrain = T_GRAVEL;
            else           t.terrain = T_GRASS;
            break;
        case B_MOOR:
            if (r<52)      t.terrain = T_HEATH;
            else if (r<60) t.terrain = T_TALL_GRASS;
            else if (r<66) { t.terrain = T_PINE; t.resources = 70 + simRand()%50; }
            else if (r<69) t.terrain = T_STONE;
            else if (r<73) t.terrain = T_MARSH;
            else if (r<76) { t.terrain = T_BERRY; t.resources = 45 + simRand()%35; }
            else           t.terrain = T_GRASS;
            break;
        case B_OCEAN:
            // Archipelago: mostly water with scattered island terrain.
            if (r<50)      t.terrain = T_WATER;
            else if (r<65) t.terrain = T_SHALLOWS;
            else if (r<70) t.terrain = T_SAND;
            else if (r<73) t.terrain = T_REEDS;
            else if (r<80) t.terrain = T_GRASS;
            else if (r<87) t.terrain = T_TALL_GRASS;
            else if (r<93) { t.terrain = T_FOREST; t.resources = 80 + simRand()%60; }
            else           t.terrain = T_GRASS;
            break;
        default:  // layout biomes (Highlands/Deep Woods/River) never reach here
            t.terrain = T_GRASS;
            break;
        }
    }
    // Mountains
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = sampleNoise(x*0.12f+5, y*0.12f+5);
        if (n > 0.78f) { g.map[y][x].terrain = T_MOUNTAIN; g.map[y][x].resources = 0; }
        else if (n > 0.72f && g.map[y][x].biome != B_DESERT)
            if (simRand() % 3 == 0) g.map[y][x].terrain = T_HILLS;
    }
    // Rivers — scaled to the larger map (5 instead of 4)
    for (int r = 0; r < 5; r++) {
        int rx, ry;
        if (r % 2 == 0) { rx = simRand() % MAP_W; ry = 0; }
        else             { rx = 0; ry = simRand() % MAP_H; }
        int len = 80 + simRand() % 50;
        float angle = (simRand() % 628) / 100.0f;
        // Track river path so we can guarantee at least one fordable crossing.
        std::vector<std::pair<int,int>> path;
        for (int i = 0; i < len; i++) {
            int wx = rx + (int)(cos(angle)*i), wy = ry + (int)(sin(angle)*i);
            angle += ((simRand() % 100) - 50) / 200.0f;
            path.push_back({wx, wy});
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = wx+dx, ny = wy+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                    if (dx==0 && dy==0) g.map[ny][nx].terrain = T_WATER;
                    else if (simRand() % 3 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                }
            }
        }
        // Guaranteed ford: pick one point along the river and clear a 2-tile-wide
        // strip of shallows — gives armies a crossable choke point.
        if ((int)path.size() > 8) {
            auto& p = path[path.size()/2 + (simRand() % (path.size()/4)) - (int)path.size()/8];
            for (int dy = -2; dy <= 2; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = p.first+dx, ny = p.second+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_WATER)
                    g.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }
    // Lakes — bumped to 9 for the larger map
    for (int l = 0; l < 9; l++) {
        int cx = 20 + simRand() % (MAP_W-40), cy = 20 + simRand() % (MAP_H-40), sz = 3 + simRand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            if (dx*dx + dy*dy > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                if (dx*dx + dy*dy < (sz-1)*(sz-1)) g.map[ny][nx].terrain = T_WATER;
                else if (simRand() % 2 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                else g.map[ny][nx].terrain = T_REEDS;
            }
        }
    }
    // Open inland seas — bumped to 3 for the larger map.
    for (int s = 0; s < 3; s++) {
        int cx = 30 + simRand() % (MAP_W - 60);
        int cy = 25 + simRand() % (MAP_H - 50);
        int sz = 7 + simRand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_MOUNTAIN || o == T_GOLD) continue;
            if (r2 < (sz-2)*(sz-2))      g.map[ny][nx].terrain = T_WATER;
            else if (r2 < (sz-1)*(sz-1)) g.map[ny][nx].terrain = (simRand()%4==0) ? T_SHALLOWS : T_WATER;
            else                         g.map[ny][nx].terrain = (simRand()%2==0) ? T_SHALLOWS : T_REEDS;
            g.map[ny][nx].resources = 0;
        }
    }
    // Fish shoals — sparse food deposits in open water and shallows.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && simRand() % 30 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + simRand() % 70;
        }
    }
    // Gold — scattered medium deposits. Spawn-point gold is added later
    // by main.cpp once spawn positions are chosen.
    for (int i = 0; i < 14; i++)
        placeGoldCluster(15 + simRand()%(MAP_W-30), 15 + simRand()%(MAP_H-30), 3 + simRand()%3);
    // Signature gold lode: one extra-rich deposit somewhere mid-map — a
    // strategic landmark worth contesting.
    {
        int gx = MAP_W/3 + simRand()%(MAP_W/3);
        int gy = MAP_H/3 + simRand()%(MAP_H/3);
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int nx = gx+dx, ny = gy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_WATER || o == T_MOUNTAIN || o == T_SHALLOWS) continue;
            if (dx*dx + dy*dy <= 5) {
                g.map[ny][nx].terrain = T_GOLD;
                g.map[ny][nx].resources = 500 + simRand() % 300;
            }
        }
    }
    // Stone clusters
    for (int i = 0; i < 17; i++) {
        int sx = 10 + simRand()%(MAP_W-20), sy = 10 + simRand()%(MAP_H-20);
        for (int j = 0; j < 3; j++) {
            int nx = sx + simRand()%4-2, ny = sy + simRand()%4-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_STONE;
        }
    }
    // Roads
    int midX = MAP_W/2, midY = MAP_H/2;
    auto makeRoad = [&](int sx, int sy, int ex, int ey) {
        int cx = sx, cy = sy;
        while (cx != ex || cy != ey) {
            if (inBounds(cx,cy) && g.map[cy][cx].terrain != T_WATER
                && g.map[cy][cx].terrain != T_MOUNTAIN && g.map[cy][cx].terrain != T_GOLD
                && g.map[cy][cx].terrain != T_SHALLOWS)
                g.map[cy][cx].terrain = T_ROAD;
            if (simRand()%2==0) { if(cx<ex)cx++; else if(cx>ex)cx--; }
            else              { if(cy<ey)cy++; else if(cy>ey)cy--; }
            if (simRand()%5==0) { cx += (simRand()%3)-1; cy += (simRand()%3)-1; }
            cx = std::max(0, std::min(cx, MAP_W-1));
            cy = std::max(0, std::min(cy, MAP_H-1));
        }
    };
    // Crossroads through the middle so spawn-to-spawn travel has natural paths.
    makeRoad(15,        15,         midX,    midY);
    makeRoad(MAP_W-15,  MAP_H-15,   midX,    midY);
    makeRoad(midX,      5,          midX,    MAP_H-5);
    makeRoad(5,         midY,       MAP_W-5, midY);
    // Mountain pass — a horizontal wall of mountains across one band of the map,
    // with a 3-tile gap forming a strategic choke point.
    if (simRand() % 2 == 0) {
        int passY = MAP_H/2 + (simRand() % 20) - 10;
        int gapX  = MAP_W/4 + simRand() % (MAP_W/2);
        for (int x = 5; x < MAP_W - 5; x++) {
            // Skip the gap and a small variation zone around it
            if (std::abs(x - gapX) < 3) continue;
            float n = sampleNoise(x*0.3f, passY*0.3f + 99);
            // Mountains in a 1-2 tile band (some scatter for organic shape)
            int thickness = 1 + (n > 0.5f ? 1 : 0);
            for (int dy = -thickness; dy <= thickness; dy++) {
                int ny = passY + dy + (int)(sampleNoise(x*0.4f, 88)*3) - 1;
                if (inBounds(x, ny) && g.map[ny][x].terrain != T_WATER
                    && g.map[ny][x].terrain != T_GOLD)
                    g.map[ny][x].terrain = T_MOUNTAIN;
            }
        }
    }
    // Castle ruins — three signature ones at thirds.
    placeCastleRuin(MAP_W/2-4, MAP_H/2-4, 8);
    placeCastleRuin(MAP_W/4,   MAP_H/4,   6);
    placeCastleRuin(3*MAP_W/4, 3*MAP_H/4, 6);
    // Smaller ruin clusters scattered everywhere
    for (int i = 0; i < 22; i++) {
        int rx = 10 + simRand()%(MAP_W-20), ry = 10 + simRand()%(MAP_H-20);
        for (int j = 0; j < 3+simRand()%4; j++) {
            int nx = rx + simRand()%5-2, ny = ry + simRand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }
    // Berry patches
    for (int i = 0; i < 20; i++) {
        int bx = 10 + simRand()%(MAP_W-20), by = 10 + simRand()%(MAP_H-20);
        Biome b = g.map[by][bx].biome;
        if (b == B_DESERT || b == B_SNOW || b == B_OCEAN) continue;
        int sz = 1 + simRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && simRand()%3 != 0) {
                g.map[ny][nx].terrain = T_BERRY;
                g.map[ny][nx].resources = 50 + simRand() % 40;
            }
        }
    }
    // Wheat patches
    for (int i = 0; i < 17; i++) {
        int wx = 10 + simRand()%(MAP_W-20), wy = 10 + simRand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + simRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && simRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    // Great corn meadows: a few HUGE swathes of wild wheat rolling across
    // temperate plains — natural breadbaskets. Settle near one and the
    // sow-a-farm-on-wheat mechanic turns it into your kingdom's larder;
    // they're also the most flammable thing in an enemy's economy to raid.
    for (int i = 0; i < 4; i++) {
        int mx = 20 + simRand()%(MAP_W-40), my = 15 + simRand()%(MAP_H-30);
        if (g.map[my][mx].biome != B_TEMPERATE) continue;
        int rx = 6 + simRand()%5, ry = 4 + simRand()%3;   // elliptical swathe
        for (int dy = -ry; dy <= ry; dy++) for (int dx = -rx; dx <= rx; dx++) {
            int nx = mx+dx, ny = my+dy;
            if (!inBounds(nx,ny)) continue;
            float ell = (float)(dx*dx)/(rx*rx) + (float)(dy*dy)/(ry*ry);
            if (ell > 1.0f) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o!=T_GRASS && o!=T_TALL_GRASS && o!=T_FLOWERS && o!=T_MEADOW) continue;
            // Dense heart of wheat, meadow fringe.
            g.map[ny][nx].terrain = (ell < 0.65f || simRand()%3 != 0) ? T_WHEAT : T_MEADOW;
        }
    }

    // Stone circles: a monolith ringed by old stones. A sentry standing on
    // the monolith itself watches the whole vale (+6 sight).
    for (int i = 0; i < 3; i++) {
        int sx = 15 + simRand()%(MAP_W-30), sy = 15 + simRand()%(MAP_H-30);
        if (!isPassable(sx, sy) || g.map[sy][sx].terrain == T_GOLD) continue;
        g.map[sy][sx].terrain = T_MONOLITH;
        g.map[sy][sx].resources = 0;
        for (int a = 0; a < 6; a++) {
            int rx = sx + (simRand()%5) - 2, ry = sy + (simRand()%5) - 2;
            if ((rx==sx && ry==sy) || !inBounds(rx,ry)) continue;
            Terrain o = g.map[ry][rx].terrain;
            if (o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW||o==T_DIRT)
                g.map[ry][rx].terrain = (simRand()%2) ? T_STONE : T_RUINS;
        }
    }

    // === ELEVATION: highland plateaus ===
    // A coarse noise channel raises broad swathes of land one level. The rim
    // of each plateau is a cliff — impassable, a hard wall for armies — except
    // where ramps spawn (below). Water, castle ruins and mountains stay put.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        Terrain ter = t.terrain;
        bool noLift = (ter==T_WATER||ter==T_SHALLOWS||ter==T_REEDS||ter==T_MARSH
                    || ter==T_FISH ||ter==T_ICE||ter==T_CASTLE_WALL
                    || ter==T_CASTLE_FLOOR||ter==T_CASTLE_GATE);
        float n = sampleNoise(x*0.035f+60, y*0.035f+60);
        t.elev = (!noLift && n > 0.74f) ? 1 : 0;
    }
    // Ramps: roughly a quarter of each plateau's rim becomes climbable hill
    // tiles, so every highland is reachable but defensible — armies funnel
    // through the ramps, and a tower on one owns the approach.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        if (t.elev != 1) continue;
        Terrain ter = t.terrain;
        if (ter==T_GOLD||ter==T_MOUNTAIN||ter==T_STONE) continue;
        bool rim = false;
        static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& d : d4) {
            int nx = x+d[0], ny = y+d[1];
            if (inBounds(nx,ny) && g.map[ny][nx].elev == 0
                && g.map[ny][nx].terrain != T_WATER) { rim = true; break; }
        }
        if (!rim) continue;
        if (((unsigned)(x*7919 + y*6271) % 4) == 0) {
            t.terrain = T_HILLS;
            t.resources = 0;
        }
    }

    // Continental already paints climate per tile; just soften the coastlines.
    applyEcotones();
    applyDesertFeatures();
    applySteppeFeatures();
    applyMoorFeatures();

    // Baseline snapshot used by the winter→spring thaw cycle.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}
