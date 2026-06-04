#!/usr/bin/env python3
"""Prepare and store generated Realm reference images."""

from __future__ import annotations

import argparse
import json
import re
import shutil
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

from PIL import Image


REPO_ROOT = Path(__file__).resolve().parents[4]
IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".webp"}


def slug_id(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9]+", "_", text.strip().lower()).strip("_")
    return value or "reference"


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def spec_path(root: Path, group: str, slug: str) -> Path:
    path = root / group / f"{slug}.json"
    if not path.exists():
        raise SystemExit(f"missing generated spec: {path}")
    return path


def prompt_path(root: Path, group: str, slug: str) -> Path | None:
    path = root / group / f"{slug}.md"
    return path if path.exists() else None


def compact_spec_summary(spec: dict[str, Any]) -> list[str]:
    entity = spec.get("entity", {})
    art = spec.get("art", {})
    gameplay = spec.get("gameplay", {})
    team = spec.get("team_color", {})
    render = spec.get("render", {})
    placement = spec.get("placement", {})
    actions = spec.get("actions", [])
    states = spec.get("states", [])

    lines = [
        f"- Asset id: `{spec.get('id') or spec.get('slug') or entity.get('slug') or 'unknown'}`.",
        f"- Asset type: `{spec.get('asset_type') or entity.get('actor_type') or 'unknown'}`.",
    ]
    name = spec.get("name") or entity.get("name")
    if name:
        lines.append(f"- Display name: {name}.")
    visual = art.get("visual_design") or gameplay.get("description") or spec.get("description")
    if visual:
        lines.append(f"- Visual design: {visual}")
    canvas = art.get("source_canvas")
    if canvas:
        lines.append(f"- Standalone source canvas from spec: {canvas.get('width_px')} by {canvas.get('height_px')} px.")
    directions = render.get("directions") or spec.get("directions")
    if directions:
        lines.append(f"- Directions in spec: {', '.join(map(str, directions))}.")
    footprint = placement.get("footprint") or spec.get("footprint")
    if isinstance(footprint, dict):
        lines.append(f"- Footprint: {footprint.get('w', '?')} by {footprint.get('h', '?')} tile(s).")
    if team.get("required"):
        slots = team.get("slots") or []
        if slots:
            lines.append(f"- Team-colour slots: {', '.join(map(str, slots))}.")
        else:
            lines.append("- Team-colour is required.")
    item_ids = []
    if isinstance(actions, list) and actions:
        item_ids = [str(action.get("id") or action.get("description") or "action") for action in actions[:16]]
    elif isinstance(states, list):
        item_ids = [str(state.get("id") if isinstance(state, dict) else state) for state in states[:16]]
    if item_ids:
        lines.append(f"- First states/actions: {', '.join(item_ids)}.")
    return lines


def token_set(*values: str) -> set[str]:
    tokens: set[str] = set()
    for value in values:
        tokens.update(token for token in re.split(r"[^a-z0-9]+", value.lower()) if len(token) >= 3)
    return tokens


def list_reference_images(root: Path, include_nested: bool) -> list[Path]:
    if not root.exists():
        return []
    iterator = root.rglob("*") if include_nested else root.glob("*")
    return sorted(path for path in iterator if path.is_file() and path.suffix.lower() in IMAGE_EXTS)


def reference_match_score(path: Path, target_tokens: set[str], slug: str) -> int:
    name = path.stem.lower()
    tokens = token_set(name)
    score = 0
    score += 100 if slug in name else 0
    score += 18 * len(tokens & target_tokens)

    close_groups = [
        {"militia", "spearman", "pikeman", "sword", "shield", "infantry"},
        {"archer", "crossbow", "bow", "quiver", "bolt"},
        {"knight", "scout", "horse", "cavalry", "spear", "lance"},
        {"peasant", "worker", "villager", "tool"},
    ]
    for group in close_groups:
        if slug in group and tokens & group:
            score += 12
    if "wip" in path.parts:
        score -= 25
    return score


def score_reference(path: Path, target_tokens: set[str], slug: str) -> tuple[int, str]:
    score = reference_match_score(path, target_tokens, slug)
    return (-score, str(path).lower())


def choose_context_images(root: Path, spec: dict[str, Any], group: str, slug: str, include_nested: bool, limit: int, min_score: int) -> list[Path]:
    name = str(spec.get("name") or spec.get("entity", {}).get("name") or slug)
    visual = str(spec.get("art", {}).get("visual_design") or spec.get("gameplay", {}).get("description") or "")
    target_tokens = token_set(slug, name, visual, group)
    images = list_reference_images(root, include_nested)
    ranked = sorted(
        (path for path in images if reference_match_score(path, target_tokens, slug) >= min_score),
        key=lambda path: score_reference(path, target_tokens, slug),
    )
    return ranked[:limit]


def make_context_grid(images: list[Path], out: Path, cols: int, rows: int, cell: int) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGB", (cols * cell, rows * cell), (236, 232, 218))
    for index, path in enumerate(images[: cols * rows]):
        try:
            image = Image.open(path).convert("RGBA")
        except OSError:
            continue
        image.thumbnail((cell - 16, cell - 16), Image.Resampling.LANCZOS)
        x = (index % cols) * cell + (cell - image.width) // 2
        y = (index // cols) * cell + (cell - image.height) // 2
        backdrop = Image.new("RGBA", image.size, (236, 232, 218, 255))
        backdrop.alpha_composite(image)
        sheet.paste(backdrop.convert("RGB"), (x, y))
    sheet.save(out)


def reference_prompt(
    spec: dict[str, Any],
    group: str,
    slug: str,
    context_grid: Path | None,
    context_images: list[Path],
    output_kind: str,
) -> str:
    name = spec.get("name") or spec.get("entity", {}).get("name") or slug.replace("_", " ")
    output_rule = (
        "Create one generated reference image for this asset."
        if output_kind == "single"
        else "Create a 4 by 4 generated reference sheet exploring readable variants for this asset."
    )
    context_lines = []
    if context_grid:
        context_lines.append(f"- Visual context grid: `{context_grid}`.")
    for path in context_images:
        context_lines.append(f"- Context source: `{path}`.")
    if not context_lines:
        context_lines.append("- No visual context images were found; rely on the generated spec only.")

    lines = [
        f"# Generated Reference Prompt: {name}",
        "",
        "## Task",
        "",
        f"{output_rule} This is reference material only, not a production sprite and not a runtime asset.",
        "",
        "Use the generated Realm spec as the authority for identity, equipment, state coverage, style direction, and output resolution.",
        "Use any supplied visual context grid only as nearby subject evidence. Do not copy its pixels, crop, lighting, finish, labels, or exact source style.",
        "",
        "## Generated Spec Summary",
        "",
        *compact_spec_summary(spec),
        "",
        "## Visual Context",
        "",
        *context_lines,
        "",
        "## Reference-Only Rules",
        "",
        "- Output may be useful for later prompts, audits, or style comparison.",
        "- Output must not be treated as accepted runtime art.",
        "- Output must not be stored under `art/reference/`; that folder is user-owned source reference material.",
        "- Store keepers under `art/generated-reference/<group>/<slug>/<version>/`.",
        "- Keep the result easy to inspect: isolated subject or organized reference sheet, no labels baked into the image, no UI chrome, no watermark.",
        "",
    ]
    return "\n".join(lines)


def command_prepare(args: argparse.Namespace) -> None:
    json_root = REPO_ROOT / args.image_json_root
    spec = read_json(spec_path(json_root, args.group, args.slug))
    context_root = REPO_ROOT / args.context_root if args.context_root else REPO_ROOT / "art" / "reference" / args.group
    out_dir = REPO_ROOT / args.out_dir / args.group / args.slug / args.version
    build_dir = REPO_ROOT / args.build_dir / args.group / args.slug / args.version

    images = choose_context_images(context_root, spec, args.group, args.slug, args.include_nested, args.cols * args.rows, args.min_context_score)
    context_grid = None
    if images:
        context_grid = build_dir / "context_grid.png"
        make_context_grid(images, context_grid, args.cols, args.rows, args.cell_px)

    prompt = reference_prompt(spec, args.group, args.slug, context_grid, images, args.output_kind)
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "prompt.md").write_text(prompt, encoding="utf-8")
    manifest = {
        "created_at": datetime.now(UTC).isoformat(timespec="seconds").replace("+00:00", "Z"),
        "group": args.group,
        "slug": args.slug,
        "version": args.version,
        "spec": str(spec_path(json_root, args.group, args.slug)),
        "prompt": str(build_dir / "prompt.md"),
        "context_grid": str(context_grid) if context_grid else None,
        "context_images": [str(path) for path in images],
        "intended_store_dir": str(out_dir),
        "rules": [
            "art/reference is read-only user source material",
            "generated references are reference-only, not runtime assets",
        ],
    }
    (build_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {build_dir / 'prompt.md'}")
    if context_grid:
        print(f"wrote {context_grid}")
    print(f"intended generated-reference output: {out_dir}")


def command_store(args: argparse.Namespace) -> None:
    source = Path(args.input)
    if not source.exists():
        raise SystemExit(f"missing input image: {source}")
    out_dir = REPO_ROOT / args.out_dir / args.group / args.slug / args.version
    out_dir.mkdir(parents=True, exist_ok=True)
    dest = out_dir / f"source{source.suffix.lower()}"
    shutil.copy2(source, dest)
    prompt_file = Path(args.prompt_file) if args.prompt_file else None
    if prompt_file and prompt_file.exists():
        shutil.copy2(prompt_file, out_dir / "prompt.md")
    manifest = {
        "created_at": datetime.now(UTC).isoformat(timespec="seconds").replace("+00:00", "Z"),
        "group": args.group,
        "slug": args.slug,
        "version": args.version,
        "source": str(dest),
        "status": "generated_reference",
        "reference_only": True,
        "runtime_asset": False,
        "do_not_store_under": "art/reference",
    }
    (out_dir / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"stored generated reference: {dest}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser("prepare", help="prepare a generated-reference prompt and optional context grid")
    prepare.add_argument("--group", required=True, help="spec group, for example units, animals, buildings")
    prepare.add_argument("--slug", required=True, help="asset slug, for example militia")
    prepare.add_argument("--version", default="v001")
    prepare.add_argument("--image-json-root", default="art/tiles/image-json")
    prepare.add_argument("--context-root", default="", help="reference image root; defaults to art/reference/<group>")
    prepare.add_argument("--out-dir", default="art/generated-reference")
    prepare.add_argument("--build-dir", default="build/generated-reference-context")
    prepare.add_argument("--cols", type=int, default=4)
    prepare.add_argument("--rows", type=int, default=4)
    prepare.add_argument("--cell-px", type=int, default=192)
    prepare.add_argument("--include-nested", action="store_true", help="include nested/WIP reference images")
    prepare.add_argument("--min-context-score", type=int, default=25, help="minimum context-image match score; avoids unrelated grids")
    prepare.add_argument("--output-kind", choices=["single", "sheet"], default="single")
    prepare.set_defaults(func=command_prepare)

    store = sub.add_parser("store", help="store an image as a generated reference")
    store.add_argument("--group", required=True)
    store.add_argument("--slug", required=True)
    store.add_argument("--version", default="v001")
    store.add_argument("--input", required=True)
    store.add_argument("--prompt-file", default="")
    store.add_argument("--out-dir", default="art/generated-reference")
    store.set_defaults(func=command_store)

    return parser


def main() -> None:
    args = build_parser().parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
