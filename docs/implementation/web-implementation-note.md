# Web implementation note

This pass keeps Realm's game rules, map generation, simulation, entities, AI,
commands, and SDL renderer in the existing shared C++ code.

The web build adds a separate Emscripten entry point instead of reusing the
native GUI `main()`. The native GUI flow owns a blocking splash/menu loop, which
is not browser-safe. The web entry point uses the same splash renderer through a
non-blocking menu frame, then initializes the same game state and SDL renderer
when the player starts a match. `/embed` keeps a deterministic immediate match
startup for embed surfaces.

Branch scope for this pass is `edward` only. Stable and main branch Netlify
branch-deploy mappings remain documented follow-up work until those branches are
ready for the GUI build.
