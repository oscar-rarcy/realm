#include "save_migration.h"

bool migrateV8ToV9(Game& game);

bool isSupportedSaveVersion(int version) {
    return version >= REALM_MIN_SUPPORTED_SAVE_VERSION && version <= REALM_SAVE_VERSION;
}

bool migrateLoadedGame(Game& game, int fromVersion) {
    if (!isSupportedSaveVersion(fromVersion)) return false;

    // Version 9 introduces the migration framework but does not change the
    // serialized payload from version 8. Future migrations should be appended
    // here in ascending version order.
    if (fromVersion <= 8) {
        if (!migrateV8ToV9(game)) return false;
    }

    return true;
}
