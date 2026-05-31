# Web implementation note

This pass keeps Realm's game rules, map generation, simulation, entities, AI,
commands, and SDL renderer in the existing shared C++ code.

The web build adds a separate Emscripten entry point instead of reusing the
native GUI `main()`. The native GUI flow owns a blocking splash/menu loop, which
is not browser-safe. The web entry point initializes the same game state and SDL
renderer, then hands a single-frame callback to Emscripten's browser main loop.

Branch scope for this pass is `edward` only. Stable and main branch Netlify
branch-deploy mappings remain documented follow-up work until those branches are
ready for the GUI build.
