#include "save_writer.h"

#include "realm.h"
#include "sim/save_schema.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <system_error>

namespace {

void writeIntVec(std::ostream& os, const std::vector<int>& v) {
    os << v.size();
    for (int x : v) os << ' ' << x;
}

int persistedMode(GameMode mode) {
    return mode == M_PAUSED || mode == M_GAME_OVER ? (int)mode : (int)M_NORMAL;
}

} // namespace

bool writeSaveFile(const Game& game, const std::string& path) {
    std::filesystem::path finalPath(path);
    std::filesystem::path tmpPath = finalPath;
    tmpPath.replace_extension(tmpPath.extension().string() + ".tmp");
    std::ofstream os(tmpPath);
    if (!os) return false;
       os << std::setprecision(std::numeric_limits<float>::max_digits10);
       os << "REALM_SAVE " << REALM_SAVE_VERSION << "\n";
       os << "META " << game.seed << ' ' << game.startupAIs << ' ' << game.humanCorner << ' '
       << game.matchNumber << ' ' << game.biomeChoice << ' ' << game.tick << ' '
          << persistedMode(game.mode) << ' ' << -1 << ' ' << game.winner << ' ' << game.aiTimer << ' ' << game.farmTimer << ' '
          << game.animalTimer << ' '
          << game.dayPhase << ' ' << game.seasonPhase << ' ' << game.prevSeason << ' '
          << game.weather << ' ' << game.weatherTimer << ' ' << game.prevTimePhase << ' ' << game.attackNotifyCd << ' '
          << game.nextId << ' ' << game.rngState << ' '
          << (game.returnToMenu ? 1 : 0) << ' ' << 0 << ' ' << 0 << "\n";
    for (int p = 0; p <= MAX_PLAYERS; p++) {
        const Player& pl = game.players[p];
        os << "PLAYER " << p << ' ' << pl.gold << ' ' << pl.wood << ' ' << pl.food << ' '
           << pl.supply << ' ' << pl.supplyMax << ' ' << (pl.alive ? 1 : 0) << ' '
           << pl.research << ' ' << pl.aiWaveCd << "\n";
    }
    os << "SELECTED ";
    writeIntVec(os, {});
    os << "\n";
    for (int p = 0; p < MAX_PLAYERS; p++) {
        for (int i = 0; i < 9; i++) {
            os << "GROUP_OWNER " << p << ' ' << i << ' ';
            writeIntVec(os, game.controlGroupsByOwner[p][i]);
            os << "\n";
        }
    }
    os << "MAP " << MAP_W << ' ' << MAP_H << "\n";
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        const Tile& t = game.map[y][x];
        os << "TILE " << x << ' ' << y << ' ' << (int)t.terrain << ' ' << t.resources << ' '
           << (int)t.biome << ' ' << (int)t.preWinterTerrain << ' ' << t.wear;
        for (int p = 0; p < MAX_PLAYERS; p++) os << ' ' << (t.visible[p] ? 1 : 0);
        for (int p = 0; p < MAX_PLAYERS; p++) os << ' ' << (t.explored[p] ? 1 : 0);
        os << "\n";
    }
    os << "ENTITIES " << game.entities.size() << "\n";
    for (const Entity& e : game.entities) {
        os << "ENTITY " << e.id << ' ' << (int)e.type << ' ' << e.owner << ' ' << e.x << ' ' << e.y
           << ' ' << e.hp << ' ' << e.maxHp << ' ' << (int)e.state << ' ' << e.targetId
           << ' ' << e.targetX << ' ' << e.targetY << ' ' << e.pathIdx << ' ' << e.moveCd
           << ' ' << e.atkCd << ' ' << e.gatherCd << ' ' << (int)e.cargo.type << ' '
           << e.cargo.amount << ' ' << e.cargo.sourceX << ' ' << e.cargo.sourceY << ' '
           << (int)e.producing << ' ' << e.trainProgress << ' ' << e.trainTime << ' '
           << e.researchProgress << ' ' << e.researchTime << ' '
           << (e.underConstruction ? 1 : 0) << ' ' << (e.alive ? 1 : 0) << ' '
           << e.rallyX << ' ' << e.rallyY << ' ' << e.resourceX << ' ' << e.resourceY << ' '
           << e.storedFood << ' ' << e.stuckTicks << ' '
           << e.alertTicks << ' ' << e.deathTicks << ' '
           << e.carcassFoodRemaining << ' ' << e.carcassFoodMax << ' '
           << e.rallySet << ' ' << e.researching << ' '
           << e.attackMove << ' ' << e.holdPosition << ' ' << e.facingDx << ' ' << e.facingDy << ' '
           << (e.gateOpen ? 1 : 0) << ' '
           << (e.gateLocked ? 1 : 0) << ' '
           << e.convertTicks << ' ' << e.retreating << ' ' << e.packed << ' ' << e.packTicks;
        os << " PATH " << e.path.size();
        for (auto pt : e.path) os << ' ' << pt.first << ' ' << pt.second;
        os << " QUEUE ";
        writeIntVec(os, e.queue);
        os << " GARRISON ";
        writeIntVec(os, e.garrison);
        os << " WAYPOINTS " << e.waypoints.size();
        for (auto pt : e.waypoints) os << ' ' << pt.first << ' ' << pt.second;
        os << " PATROL " << (e.patrolMode ? 1 : 0);
        os << "\n";
    }
    os << "PROJECTILES " << game.projectiles.size() << "\n";
    for (const Projectile& p : game.projectiles) {
        os << "PROJECTILE " << p.x << ' ' << p.y << ' ' << p.tx << ' ' << p.ty << ' '
           << (int)p.glyph << ' ' << p.color << ' ' << p.life << ' ' << (p.alive ? 1 : 0)
           << ' ' << (int)p.type << "\n";
    }
    os << "MARKERS 0\n";
    os.flush();
    if (!os) return false;
    os.close();
    if (!os) return false;
    std::error_code ec;
    if (std::filesystem::exists(finalPath, ec)) std::filesystem::remove(finalPath, ec);
    ec = {};
    std::filesystem::rename(tmpPath, finalPath, ec);
    return !ec;
}
