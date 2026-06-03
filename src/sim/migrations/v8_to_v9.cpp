#include "realm.h"

bool migrateV8ToV9(Game& game) {
    // Version 9 introduced typed command/save migration infrastructure without
    // changing the serialized payload. Keep this migration explicit so future
    // schema changes have a clear version-by-version location.
    (void)game;
    return true;
}

