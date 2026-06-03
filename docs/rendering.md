# Rendering

Rendering is presentation-only. `RenderModel` is the renderer-neutral data boundary. SDL and ASCII renderers should consume descriptors and shared visual definitions instead of duplicating gameplay decisions.

SDL tileset code uses canonical asset keys such as terrain, entity, and effect keys. Terrain image detection checks runtime asset files/manifests rather than hardcoded stubs.

## Allowed dependencies

Renderers may read `Game`, `RenderModel`, visual definitions, and frontend state such as camera, cursor, cache, and touch UI state. Missing asset logging may use canonical keys.

## Forbidden dependencies

Renderers must not mutate gameplay resources, orders, combat, training, research, save/load state, or AI decisions. Renderer code should not invent alternate terrain/entity mappings when shared visual helpers exist.
