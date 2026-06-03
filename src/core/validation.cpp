#include "realm.h"

#include <algorithm>
#include <cctype>
#include <cmath>

static std::string validationCodeFor(const std::string& message) {
    std::string code;
    bool underscore = false;
    for (unsigned char raw : message) {
        char ch = (char)std::tolower(raw);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            code.push_back(ch);
            underscore = false;
        } else if (!underscore && !code.empty()) {
            code.push_back('_');
            underscore = true;
        }
    }
    while (!code.empty() && code.back() == '_') code.pop_back();
    return code.empty() ? "validation_issue" : code;
}

static const Entity* findEntityIn(const Game& game, int id) {
    for (const auto& entity : game.entities)
        if (entity.alive && entity.id == id) return &entity;
    return nullptr;
}

static Entity* findEntityIn(Game& game, int id) {
    for (auto& entity : game.entities)
        if (entity.alive && entity.id == id) return &entity;
    return nullptr;
}

std::vector<ValidationIssue> validateGameStateIssues(const Game& game) {
    std::vector<ValidationIssue> issues;
    auto add = [&](ValidationSeverity severity, const std::string& msg, int entityId = -1, MapPos tile = { -1, -1 }) {
        issues.push_back({ severity, validationCodeFor(msg), msg, entityId, tile });
    };
    auto recoverable = [&](const std::string& msg, int entityId = -1, MapPos tile = { -1, -1 }) {
        add(ValidationSeverity::Recoverable, msg, entityId, tile);
    };
    auto error = [&](const std::string& msg, int entityId = -1, MapPos tile = { -1, -1 }) {
        add(ValidationSeverity::Error, msg, entityId, tile);
    };

    if (game.local.selectedId < -1) recoverable("selected id is below sentinel", game.local.selectedId);
    if (game.local.selectedId >= game.nextId) recoverable("selected id is beyond nextId", game.local.selectedId);
    if (game.local.selectedId > 0 && !findEntityIn(game, game.local.selectedId)) recoverable("selected id does not reference a live entity", game.local.selectedId);
    for (int id : game.local.selectedIds)
        if (id <= 0 || id >= game.nextId || !findEntityIn(game, id)) recoverable("selectedIds contains invalid entity id", id);
    for (const auto& group : game.controlGroupsByOwner[0])
        for (int id : group) {
            const Entity* entity = findEntityIn(game, id);
            if (id <= 0 || id >= game.nextId || !entity) recoverable("control group contains invalid entity id", id);
            else if (entity->owner != 0) recoverable("control group contains wrong owner entity id", id);
        }
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (const auto& group : game.controlGroupsByOwner[p])
            for (int id : group) {
                const Entity* entity = findEntityIn(game, id);
                if (id <= 0 || id >= game.nextId || !entity) recoverable("owner control group contains invalid entity id", id);
                else if (entity->owner != p) recoverable("owner control group contains wrong owner entity id", id);
            }
    for (const auto& e : game.entities) {
        MapPos entityTile{ e.x, e.y };
        if (e.id <= 0 || e.id >= game.nextId) error("entity id outside valid range", e.id, entityTile);
        if (e.type < E_NONE || e.type > E_BOAR) error("entity type outside valid range", e.id, entityTile);
        if (e.owner < 0 || e.owner > OWNER_NATURE) error("entity owner outside valid range", e.id, entityTile);
        if (!inBounds(e.x, e.y)) error("entity position out of bounds", e.id, entityTile);
        if (e.state < S_IDLE || e.state > S_GARRISONED) error("entity state outside valid range", e.id, entityTile);
        if (e.targetId < -1 || e.targetId >= game.nextId) recoverable("entity target id outside valid range", e.id, entityTile);
        if (e.targetId > 0 && !findEntityIn(game, e.targetId)) recoverable("entity target id does not reference a live entity", e.id, entityTile);
        if (e.targetX != -1 && e.targetY != -1 && !inBounds(e.targetX, e.targetY)) recoverable("entity target position out of bounds", e.id, { e.targetX, e.targetY });
        if (e.resourceX != -1 && e.resourceY != -1 && !inBounds(e.resourceX, e.resourceY)) error("entity resource source out of bounds", e.id, { e.resourceX, e.resourceY });
        if (e.cargo.type < CR_NONE || e.cargo.type > CR_FISH) error("cargo resource outside valid range", e.id, entityTile);
        if (e.cargo.amount < 0) error("cargo amount below zero", e.id, entityTile);
        if (e.cargo.amount > 0 && e.cargo.type == CR_NONE) error("cargo amount without resource type", e.id, entityTile);
        if (e.cargo.sourceX != -1 && e.cargo.sourceY != -1 && !inBounds(e.cargo.sourceX, e.cargo.sourceY)) error("cargo source out of bounds", e.id, { e.cargo.sourceX, e.cargo.sourceY });
        if (e.storedFood < 0) error("stored food below zero", e.id, entityTile);
        if (e.deathTicks < 0) error("death ticks below zero", e.id, entityTile);
        if (e.carcassFoodRemaining < 0 || e.carcassFoodMax < 0) error("carcass food below zero", e.id, entityTile);
        if (e.carcassFoodRemaining > e.carcassFoodMax) error("carcass food exceeds max", e.id, entityTile);
        if (e.type == E_WOLF && (e.carcassFoodRemaining != 0 || e.carcassFoodMax != 0))
            error("wolf carcass food must stay zero", e.id, entityTile);
        if (e.facingDx < -1 || e.facingDx > 1 || e.facingDy < -1 || e.facingDy > 1) error("entity facing delta outside valid range", e.id, entityTile);
        if (e.producing < E_NONE || e.producing > E_BOAR) error("producing type outside valid range", e.id, entityTile);
        if (e.trainProgress < 0 || e.trainTime < 0 || e.researchProgress < 0 || e.researchTime < 0)
            error("progress counters below zero", e.id, entityTile);
        if (e.producing == E_NONE && (e.trainProgress != 0 || e.trainTime != 0))
            error("training progress without production", e.id, entityTile);
        if (e.researching == 0 && (e.researchProgress != 0 || e.researchTime != 0))
            error("research progress without research", e.id, entityTile);
        if (e.pathIdx < 0 || e.pathIdx > (int)e.path.size()) recoverable("entity path index outside valid range", e.id, entityTile);
        for (auto pt : e.path)
            if (!inBounds(pt.first, pt.second)) recoverable("entity path point out of bounds", e.id, { pt.first, pt.second });
        for (int q : e.queue)
            if (q < E_NONE || q > E_BOAR) error("queue type outside valid range", e.id, entityTile);
        for (int gid : e.garrison)
            if (gid <= 0 || gid >= game.nextId || !findEntityIn(game, gid)) recoverable("garrison id outside valid range", e.id, entityTile);
    }
    for (const auto& p : game.projectiles) {
        if (p.life < 0) recoverable("projectile life below zero");
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.tx) || !std::isfinite(p.ty))
            recoverable("projectile coordinate is not finite");
    }
    return issues;
}

static void pruneInvalidEntityIds(Game& game, std::vector<int>& ids) {
    ids.erase(std::remove_if(ids.begin(), ids.end(),
        [&](int id){ return id <= 0 || id >= game.nextId || findEntityIn(game, id) == nullptr; }),
        ids.end());
}

static void pruneInvalidEntityIdsForOwner(Game& game, std::vector<int>& ids, int owner) {
    ids.erase(std::remove_if(ids.begin(), ids.end(),
        [&](int id){
            const Entity* entity = findEntityIn(game, id);
            return id <= 0 || id >= game.nextId || entity == nullptr || entity->owner != owner;
        }),
        ids.end());
}

RecoveryResult recoverGameState(Game& game, const std::vector<ValidationIssue>& issues) {
    RecoveryResult result;
    result.issuesProcessed = (int)issues.size();

    bool hasHardError = false;
    for (const ValidationIssue& issue : issues)
        if (issue.severity == ValidationSeverity::Error) hasHardError = true;
    if (hasHardError) {
        result.remainingIssues = issues;
        return result;
    }

    if (game.local.selectedId < 0 || game.local.selectedId >= game.nextId || findEntityIn(game, game.local.selectedId) == nullptr)
        game.local.selectedId = -1;
    pruneInvalidEntityIds(game, game.local.selectedIds);
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int i = 0; i < 9; i++)
            pruneInvalidEntityIdsForOwner(game, game.controlGroupsByOwner[p][i], p);

    for (auto& entity : game.entities) {
        if (entity.targetId < -1 || entity.targetId >= game.nextId
            || (entity.targetId > 0 && findEntityIn(game, entity.targetId) == nullptr)) {
            entity.targetId = -1;
            if (entity.state == S_ATTACKING || entity.state == S_BUILDING
                || entity.state == S_RETURNING || entity.state == S_ENTERING)
                entity.state = S_IDLE;
        }
        if (entity.targetX != -1 && entity.targetY != -1 && !inBounds(entity.targetX, entity.targetY)) {
            entity.targetX = entity.x;
            entity.targetY = entity.y;
            entity.path.clear();
            entity.pathIdx = 0;
            if (entity.state == S_MOVING || entity.state == S_GATHERING) entity.state = S_IDLE;
        }
        if (entity.pathIdx < 0 || entity.pathIdx > (int)entity.path.size()) entity.pathIdx = 0;
        entity.path.erase(std::remove_if(entity.path.begin(), entity.path.end(),
            [](const std::pair<int,int>& p){ return !inBounds(p.first, p.second); }),
            entity.path.end());
        pruneInvalidEntityIds(game, entity.garrison);
    }

    for (auto& projectile : game.projectiles) {
        if (!std::isfinite(projectile.x) || !std::isfinite(projectile.y)
            || !std::isfinite(projectile.tx) || !std::isfinite(projectile.ty)
            || projectile.life < 0) {
            projectile.x = 0.0f;
            projectile.y = 0.0f;
            projectile.tx = 0.0f;
            projectile.ty = 0.0f;
            projectile.alive = false;
            projectile.life = 0;
        }
    }

    result.remainingIssues = validateGameStateIssues(game);
    result.recovered = result.remainingIssues.empty();
    return result;
}

bool validateGameState(const Game& game, std::string* error) {
    std::vector<ValidationIssue> issues = validateGameStateIssues(game);
    if (issues.empty()) return true;
    if (error) *error = issues.front().message;
    return false;
}

bool isPassable(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    // Land passability is centralized in TerrainDefinition.
    return terrainDef(game.map[y][x].terrain).passableLand;
}

bool isPassableWater(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    // Water passability is centralized in TerrainDefinition.
    return terrainDef(game.map[y][x].terrain).passableWater;
}
