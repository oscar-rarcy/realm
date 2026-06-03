#include "realm.h"
#include "core/game_events.h"
#include "core/entity_query.h"
#include "core/order_service.h"
#include "core/world_index.h"

namespace {

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

} // namespace

void tickProduction(Game& game, WorldIndex& world, EventSink& events, Entity& e) {
    if (e.producing != E_NONE && !e.underConstruction) {
        int bonus = 0;
        for (auto& o : game.entities)
            if (o.alive && o.owner==e.owner && o.type==E_BLACKSMITH && !o.underConstruction) { bonus=1; break; }
        e.trainProgress += 1 + bonus;
        if (e.trainProgress >= e.trainTime) {
            auto& bs = STATS[e.type]; bool placed = false;
            bool produceNaval = isNaval(e.producing);
            int newId = -1;
            for (int r = 0; r <= 4 && !placed; r++)
                for (int dy = -r; dy <= bs.sizeH+r && !placed; dy++)
                    for (int dx = -r; dx <= bs.sizeW+r && !placed; dx++) {
                        int nx = e.x+dx, ny = e.y+dy;
                        if (!inBounds(nx,ny) || entityAt(game, world, nx, ny)) continue;
                        bool ok = produceNaval ? isPassableWater(game,nx,ny) : isPassable(game,nx,ny);
                        if (!ok) continue;
                        newId = spawnEntity(game, e.producing, e.owner, nx, ny);
                        placed = true;
                    }
            // If no spawn spot was found, keep the unit queued and retry next tick
            // instead of silently consuming it — resources were already spent.
            if (!placed) {
                e.trainProgress = e.trainTime; // stay at completion threshold
            } else {
                world = buildWorldIndex(game);
                // Send to rally point if the building has a player-set one
                EntityType completed = e.producing;
                if (e.rallySet && newId >= 0) {
                    Entity* nu = findEntity(game, world, newId);
                    if (nu) startMove(game, world, events, nu->owner, Selection{ nu->id, { nu->id } }, { e.rallyX, e.rallyY });
                }
                e.producing = E_NONE; e.trainProgress = 0; e.trainTime = 0; e.state = S_IDLE;
                emitStatus(events, e.owner, std::string(STATS[completed].name) + " is ready.", GameEventType::EntitySpawned);
                // Pop the next queued unit straight into production.
                if (!e.queue.empty()) {
                    EntityType next = (EntityType)e.queue.front();
                    e.queue.erase(e.queue.begin());
                    e.producing = next; e.trainProgress = 0;
                    e.trainTime = STATS[next].trainTime; e.state = S_TRAINING;
                }
            }
        }
    }
}

void tickResearch(Game& game, EventSink& events, Entity& e) {
    if (e.researching != 0 && !e.underConstruction) {
        e.researchProgress += 1;
        if (e.researchProgress >= e.researchTime) {
            game.players[e.owner].research |= e.researching;
            int bit = e.researching;
            e.researching = 0; e.researchProgress = 0; e.researchTime = 0;
            if (bit == R_IRON_WEAPONS) emitStatus(events, e.owner, "Iron Weapons researched - militia/knights +2 atk!", GameEventType::ResearchCompleted);
            else if (bit == R_CROSSBOWS) emitStatus(events, e.owner, "Crossbows researched - archers +2 range!", GameEventType::ResearchCompleted);
            else if (bit == R_PIKES) emitStatus(events, e.owner, "Pikes researched - spearmen +1 range!", GameEventType::ResearchCompleted);
            else if (bit == R_COUNTERWEIGHT) emitStatus(events, e.owner, "Counterweight researched - trebuchets deploy faster!", GameEventType::ResearchCompleted);
            else if (bit == R_PLATE_HELM) emitStatus(events, e.owner, "Plate Helm researched - knights take less melee damage!", GameEventType::ResearchCompleted);
        }
    }
}
