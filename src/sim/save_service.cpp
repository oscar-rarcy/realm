#include "save_service.h"
#include "realm.h"
#include "core/game_events.h"
#include "sim/save_reader.h"
#include "sim/save_schema.h"

#include <iostream>

namespace {

std::string saveMessageForSlot(int slot) {
    return slot > 0 ? "Saved to slot " + std::to_string(slot) + "." : "Saved realm-save.txt";
}

std::string loadMessageForSlot(int slot) {
    return slot > 0 ? "Loaded slot " + std::to_string(slot) + "." : "Loaded realm-save.txt";
}

SaveLoadResult fail(const std::string& path, const std::string& error) {
    return { false, path, {}, error };
}

} // namespace

SaveLoadResult saveGameService(GameContext& context, const SaveRequest& request) {
    if (request.path.empty()) return fail(request.path, "Save path is empty.");
    bool ok = saveGame(context.game, request.path);
    if (!ok) {
        std::cerr << "realm: save failed " << request.path << " tick=" << context.game.tick << "\n";
        return fail(request.path, "Save failed.");
    }
    SaveLoadResult result{ true, request.path, saveMessageForSlot(request.slot), {} };
    context.events.emit({ GameEventType::SaveCompleted, request.issuer, -1, { -1, -1 }, result.message });
    std::cerr << "realm: saved " << request.path << " tick=" << context.game.tick << "\n";
    return result;
}

SaveLoadResult loadGameService(GameContext& context, const LoadRequest& request) {
    if (request.path.empty()) return fail(request.path, "Load path is empty.");
    SaveHeaderInfo header = inspectSaveHeader(request.path);
    if (!header.ok) {
        std::cerr << "realm: load failed " << request.path << ": " << header.error << "\n";
        return fail(request.path, header.error);
    }
    bool ok = loadGame(context.game, request.path);
    if (!ok) {
        std::cerr << "realm: load failed " << request.path << "\n";
        return fail(request.path, "Load failed.");
    }
    SaveLoadResult result{ true, request.path, loadMessageForSlot(request.slot), {} };
    context.events.emit({ GameEventType::LoadCompleted, request.issuer, -1, { -1, -1 }, result.message });
    std::cerr << "realm: loaded " << request.path << " tick=" << context.game.tick << "\n";
    return result;
}
