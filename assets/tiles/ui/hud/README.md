# Realm Tileset HUD Assets

These are final runtime-loadable assets for the tileset HUD.

Reference material belongs in `art/reference/hud/`. Do not move reference images here unless they are actual game assets.

The current pack contains:

- `icons/`: resource, command, and system button icons.
- `materials/`: reusable fade, button, card, and portrait-frame textures.
- `manifest.json`: asset inventory and style notes.

The desktop SDL tileset HUD loads these assets by default whenever the game is in tileset visual mode. ASCII and terminal HUDs do not use this asset pack.
