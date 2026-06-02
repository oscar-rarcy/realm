# Realm Tileset Art Workspace

This folder is for art-production material only. Final game-loadable assets stay under `assets/tiles/`; the game should not load files from `art/tiles/`.

The folders here are deliberately split by concern:

## Local References

- `reference/`: images added by a person for future visual reference.

Reference images are local project material, not repo source. They are ignored by git.

## Regenerable Exports

- `image-spec/`: Markdown image-generation prompts exported from the current game data and tileset docs.
- `image-json/`: JSON specs exported from the current game data and tileset docs.

These folders are generated output. Recreate them with:

```powershell
python scripts\export_image_generation_prompts.py --clean
python scripts\export_tile_specs.py --clean
```

They are ignored by git because they can be regenerated.

## Generation Workspace

- `candidates/`: generated image candidates copied into the repo while evaluating them.
- `source/`: imported or generated source sheets before they are split or processed.
- `workbench/`: split frames, prompts, metadata, and temporary assembly state used while producing runtime assets.

These folders are intermediate workspace state. They are ignored by git.

## Runtime Assets

Accepted production assets belong under `assets/tiles/`, not here. In practice, `assets/tiles/entities/<entity>/manifest.json` and its `*_base.png` / `*_teammask.png` frames are the runtime contract.
