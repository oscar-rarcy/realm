# Realm Tileset Art Workspace

This folder holds tileset generation material: source sheets, references, prompt specs, JSON specs, candidates, and workbench metadata.

Final game-loadable assets stay under `assets/tiles/`. In practice, `assets/tiles/entities/<entity>/manifest.json` and its `*_base.png` / `*_teammask.png` frames are the runtime contract.

Current layout:

- `image-spec/`: Markdown image-generation prompts and planning specs.
- `image-json/`: generated JSON specs for tooling and audit.
- `reference/`: positive visual references and contact sheets.
- `source/`: generated or imported source sheets.
- `candidates/`: stored generated image candidates before processing.
- `workbench/`: split source frames, prompts, metadata, and unit specs used to assemble runtime assets.
