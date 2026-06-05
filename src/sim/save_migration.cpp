#include "save_migration.h"

bool migrateV8ToV9(Game& game);

bool isSupportedSaveVersion(int version) {
    return version >= REALM_MIN_SUPPORTED_SAVE_VERSION && version <= REALM_SAVE_VERSION;
}

bool migrateLoadedGame(Game& game, int fromVersion) {
    if (!isSupportedSaveVersion(fromVersion)) return false;

    // Version 9 introduces the migration framework but does not change the
    // serialized payload from version 8. Version 10 adds optional entity
    // waypoint/patrol fields that default empty when loading older saves.
    // Version 11 stops persisting viewer-local state in saves and uses
    // owner-scoped control-group records on disk; older saves are translated
    // directly during parsing into the v11 live structures.
    // Version 12 persists ProjectileType alongside the legacy glyph/colour
    // payload; older saves infer projectile type from glyph/colour at read time.
    // Older saves also normalize legacy T_ROAD tiles with zero wear so explicit
    // roads keep their paved semantics instead of decaying immediately on load.
    if (fromVersion <= 8) {
        if (!migrateV8ToV9(game)) return false;
    }

    return true;
}
