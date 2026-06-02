# Agent Notes

Start with `README.md`. It is the human-facing overview and should stay beginner-friendly.

## Where Things Are

- `src/`, `include/`: C++ game code.
- `tests/`: automated tests.
- `web/`: browser shell.
- `scripts/`: build/run helpers.
- `assets/`: final game-loadable assets only.
- `art/`: art-generation workspace, prompts, references, candidates, workbench files.
- `docs/`: plans, audits, renderer notes, playtest docs.
- `build/`, `bin/`, `logs/`, `dist/`: generated output; do not commit unless explicitly asked.

## Useful Docs

- `docs/tests/agent-playtest.md`: repeatable playtest flow.
- `docs/tests/manual-test-plan.md`: broader manual checks.
- `docs/web-build.md`: web build and Netlify notes.
- `docs/gfx-renderer.md`: SDL/isometric renderer notes.
- `docs/tileset/realm_visual_asset_architecture.md`: tileset architecture.
- `.agents/skills/realm-tileset-from-images/SKILL.md`: tileset generation workflow.
- `scripts/export_image_generation_prompts.py`: canonical generator for `art/tiles/image-spec` markdown prompts from current game/entity/terrain data.

## Common Commands

Windows/MSYS2:

```sh
mingw32-make test
mingw32-make gfx
mingw32-make lab
```

Linux/macOS/WSL:

```sh
make test
make gfx
make terminal
```

Web:

```sh
bash scripts/validate.sh
bash scripts/build-web.sh
bash scripts/setup-web.sh
npm run test:web
```

`bash scripts/validate.sh` is the default validation path. It always runs the
native checks and only runs the web build automatically when Emscripten is
already available. If an agent hits missing emsdk during web work, it should
ask before running `bash scripts/setup-web.sh`.

## Rules Of Thumb

- Preserve unrelated dirty work.
- Keep `assets/tiles/` runtime-only; put generation material under `art/tiles/`.
- After frontend/web changes, run or at least document the relevant browser smoke path.
- After native/runtime changes, prefer real build/test proof over inspection only.
