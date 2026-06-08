#!/usr/bin/env python3
"""Report Realm tileset production coverage and emit agent-ready work batches."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from dataclasses import dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any

from tileset_resolution_policy import png_dimensions, resolution_gate_failure, source_resolution_policy


ROOT = Path(__file__).resolve().parents[1]
IMAGE_JSON_ROOT = ROOT / "art" / "tiles" / "image-json"
IMAGE_SPEC_ROOT = ROOT / "art" / "tiles" / "image-spec"
LEDGER_DEFAULT = ROOT / "art" / "tiles" / "production-ledger.jsonl"
STATUS_ORDER = [
    "missing",
    "placeholder",
    "stale",
    "needs_review",
    "unsupported",
    "unreachable",
    "accepted",
]
NEXT_STATUS_ORDER = ["missing", "placeholder", "stale", "needs_review", "unsupported", "unreachable"]
DIRECT_BASE_GROUPS = {"grounds", "decals", "effects", "user_interface"}


@dataclass
class WorkItem:
    work_id: str
    group: str
    slug: str
    name: str
    asset_type: str
    prompt_path: str | None
    spec_path: str
    required_paths: list[str]
    optional_paths: list[str] = field(default_factory=list)
    manifest_path: str | None = None
    status: str = "needs_review"
    reasons: list[str] = field(default_factory=list)
    spec_hash: str = ""
    prompt_hash: str | None = None
    runtime_hashes: dict[str, str] = field(default_factory=dict)
    ledger: dict[str, Any] | None = None
    metadata: dict[str, Any] = field(default_factory=dict)

    def to_dict(self) -> dict[str, Any]:
        return {
            "work_id": self.work_id,
            "group": self.group,
            "slug": self.slug,
            "name": self.name,
            "asset_type": self.asset_type,
            "status": self.status,
            "reasons": self.reasons,
            "prompt": self.prompt_path,
            "spec": self.spec_path,
            "manifest": self.manifest_path,
            "required_paths": self.required_paths,
            "optional_paths": self.optional_paths,
            "spec_hash": self.spec_hash,
            "prompt_hash": self.prompt_hash,
            "runtime_hashes": self.runtime_hashes,
            "metadata": self.metadata,
        }


def rel(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def repo_path(path: str | Path | None) -> Path | None:
    if not path:
        return None
    raw = Path(path)
    return raw if raw.is_absolute() else ROOT / raw


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def source_policy_for_item(item: WorkItem) -> dict[str, Any]:
    spec_path = repo_path(item.spec_path)
    if not spec_path or not spec_path.exists():
        return {}
    try:
        spec = read_json(spec_path)
    except (OSError, json.JSONDecodeError):
        return {}
    canvas = spec.get("art", {}).get("source_canvas")
    if not isinstance(canvas, dict):
        canvas = spec.get("source_canvas")
    footprint = None
    if isinstance(canvas, dict) and isinstance(canvas.get("footprint"), dict):
        footprint = canvas["footprint"]
    elif isinstance(spec.get("placement"), dict) and isinstance(spec["placement"].get("footprint"), dict):
        footprint = spec["placement"]["footprint"]
    policy = source_resolution_policy(item.group, footprint=footprint)
    if not policy:
        return {}
    policy = dict(policy)
    if isinstance(canvas, dict):
        for key in (
            "width_px",
            "height_px",
            "target_kind",
            "min_width_px",
            "min_height_px",
            "min_longest_side_px",
            "profile",
            "range",
        ):
            if key in canvas:
                policy[key] = canvas[key]
    return policy


def runtime_resolution_failures(item: WorkItem) -> list[str]:
    policy = source_policy_for_item(item)
    if not policy:
        return []
    failures: list[str] = []
    for runtime_path in item.required_paths:
        path = repo_path(runtime_path)
        if not path or not path.exists() or path.suffix.lower() != ".png":
            continue
        width, height = png_dimensions(path)
        failure = resolution_gate_failure(width, height, policy)
        if failure:
            failures.append(f"{runtime_path}: {failure}")
    return failures


def stable_hash(path: Path | None) -> str | None:
    if not path or not path.exists() or not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hash_json_payload(payload: dict[str, Any]) -> str:
    data = json.dumps(payload, ensure_ascii=True, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(data).hexdigest()


def prompt_for(group: str, slug: str) -> Path | None:
    path = IMAGE_SPEC_ROOT / group / f"{slug}.md"
    return path if path.exists() else None


def load_index(root: Path) -> dict[str, Any]:
    index_path = root / "index.json"
    if not index_path.exists():
        raise SystemExit(f"missing generated spec index: {index_path}")
    return read_json(index_path)


def load_ledger(path: Path) -> dict[str, dict[str, Any]]:
    latest: dict[str, dict[str, Any]] = {}
    if not path.exists():
        return latest
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            raise SystemExit(f"{path}:{line_number}: invalid JSONL ledger row: {exc}") from exc
        work_id = record.get("work_id")
        if not work_id:
            raise SystemExit(f"{path}:{line_number}: ledger row is missing work_id")
        latest[str(work_id)] = record
    return latest


def frames_for_action(action: dict[str, Any], asset_type: str) -> int:
    for key in ("frames_recommended", "frame_count"):
        value = action.get(key)
        if isinstance(value, int) and value > 0:
            return value
    for key in ("phases", "frames"):
        value = action.get(key)
        if isinstance(value, list) and value:
            return len(value)
    if asset_type in {"unit", "animal"}:
        return 2
    return 1


def directions_for(spec: dict[str, Any]) -> list[str]:
    directions = spec.get("render", {}).get("directions")
    if isinstance(directions, list) and directions:
        return [str(item) for item in directions]
    asset_type = spec.get("asset_type")
    if asset_type == "building":
        return ["south"]
    if asset_type in {"unit", "animal"}:
        return ["front", "back"]
    return ["default"]


def actions_for(spec: dict[str, Any]) -> list[dict[str, Any]]:
    actions = spec.get("actions")
    if isinstance(actions, list) and actions:
        return [item for item in actions if isinstance(item, dict)]
    states = spec.get("states")
    if isinstance(states, list) and states:
        return [{"id": str(state), "description": str(state), "frames_recommended": 1} for state in states]
    return [{"id": "base", "description": "base", "frames_recommended": 1}]


def state_names_for(spec: dict[str, Any]) -> list[str]:
    states = spec.get("states")
    if isinstance(states, list) and states:
        return [str(item) for item in states]
    return ["default"]


def manifest_has_placeholder(path: Path | None) -> bool:
    if not path or not path.exists():
        return False
    try:
        manifest = read_json(path)
    except json.JSONDecodeError:
        return False
    if "placeholder" in str(manifest.get("schema", "")).lower():
        return True
    if manifest.get("placeholder") is True or manifest.get("temporary") is True:
        return True

    def walk(value: Any) -> bool:
        if isinstance(value, dict):
            if value.get("placeholder") is True or value.get("temporary") is True:
                return True
            return any(walk(item) for item in value.values())
        if isinstance(value, list):
            return any(walk(item) for item in value)
        return False

    return walk(manifest)


def direct_base_work(
    group: str,
    spec: dict[str, Any],
    spec_rel: str,
    prompt_path: Path | None,
) -> list[WorkItem]:
    slug = str(spec["slug"])
    paths = spec.get("paths", {})
    required = paths.get("base") or paths.get("runtime_root")
    if not required:
        required = f"assets/tiles/{group}/{slug}.png"
    manifest = paths.get("manifest")
    required_paths = [str(required)]
    optional_paths = [str(manifest)] if manifest else []
    return [
        WorkItem(
            work_id=f"{group}/{slug}/base",
            group=group,
            slug=slug,
            name=str(spec.get("name", slug)),
            asset_type=str(spec.get("asset_type", group)),
            prompt_path=rel(prompt_path) if prompt_path else None,
            spec_path=spec_rel,
            required_paths=required_paths,
            optional_paths=optional_paths,
            manifest_path=str(manifest) if manifest else None,
            metadata={"kind": "direct_base"},
        )
    ]


def feature_work(group: str, spec: dict[str, Any], spec_rel: str, prompt_path: Path | None) -> list[WorkItem]:
    slug = str(spec["slug"])
    paths = spec.get("paths", {})
    root = str(paths.get("runtime_root") or f"assets/tiles/features/{slug}")
    manifest = str(paths.get("manifest") or f"{root}/manifest.json")
    split_ready = bool(spec.get("render", {}).get("feature_layers", {}).get("split_ready"))
    layer_names = ["back", "front_occluder"] if split_ready else ["base"]
    items = []
    for state in state_names_for(spec):
        required = [f"{root}/{state}/{layer}.png" for layer in layer_names]
        items.append(
            WorkItem(
                work_id=f"{group}/{slug}/{state}",
                group=group,
                slug=slug,
                name=str(spec.get("name", slug)),
                asset_type="feature",
                prompt_path=rel(prompt_path) if prompt_path else None,
                spec_path=spec_rel,
                required_paths=required,
                manifest_path=manifest,
                optional_paths=[manifest],
                metadata={"state": state, "layers": layer_names},
            )
        )
    return items


def entity_work(group: str, spec: dict[str, Any], spec_rel: str, prompt_path: Path | None) -> list[WorkItem]:
    slug = str(spec["slug"])
    asset_type = str(spec.get("asset_type", group))
    paths = spec.get("paths", {})
    root = str(paths.get("runtime_root") or f"assets/tiles/entities/{slug}")
    manifest = str(paths.get("manifest") or f"{root}/manifest.json")
    team_required = bool(spec.get("team_color", {}).get("required"))
    items = []
    for action in actions_for(spec):
        action_id = str(action.get("id") or action.get("description") or "base")
        frame_count = frames_for_action(action, asset_type)
        for direction in directions_for(spec):
            for frame_index in range(frame_count):
                base = f"{root}/{action_id}/{direction}/frame_{frame_index:02d}_base.png"
                mask = f"{root}/{action_id}/{direction}/frame_{frame_index:02d}_teammask.png"
                required = [base, mask] if team_required else [base]
                optional = [manifest] if manifest else []
                if not team_required:
                    optional.append(mask)
                items.append(
                    WorkItem(
                        work_id=f"{group}/{slug}/{action_id}/{direction}/frame_{frame_index:02d}",
                        group=group,
                        slug=slug,
                        name=str(spec.get("name", slug)),
                        asset_type=asset_type,
                        prompt_path=rel(prompt_path) if prompt_path else None,
                        spec_path=spec_rel,
                        required_paths=required,
                        optional_paths=optional,
                        manifest_path=manifest,
                        metadata={
                            "action": action_id,
                            "direction": direction,
                            "frame": frame_index,
                            "team_color_required": team_required,
                        },
                    )
                )
    return items


def work_items_from_specs(root: Path) -> list[WorkItem]:
    index = load_index(root)
    groups = index.get("groups", {})
    items: list[WorkItem] = []
    for group, entries in groups.items():
        if not isinstance(entries, list):
            continue
        for entry in entries:
            spec_path = root / str(entry["path"])
            spec = read_json(spec_path)
            slug = str(spec.get("slug") or spec_path.stem)
            spec_rel = rel(spec_path)
            prompt_path = prompt_for(group, slug)
            asset_type = str(spec.get("asset_type", ""))
            if group in DIRECT_BASE_GROUPS:
                items.extend(direct_base_work(group, spec, spec_rel, prompt_path))
            elif asset_type == "projectile":
                items.extend(direct_base_work(group, spec, spec_rel, prompt_path))
            elif asset_type == "feature":
                items.extend(feature_work(group, spec, spec_rel, prompt_path))
            elif asset_type in {"unit", "animal", "building"}:
                items.extend(entity_work(group, spec, spec_rel, prompt_path))
            else:
                items.append(
                    WorkItem(
                        work_id=f"{group}/{slug}/base",
                        group=group,
                        slug=slug,
                        name=str(spec.get("name", slug)),
                        asset_type=asset_type or group,
                        prompt_path=rel(prompt_path) if prompt_path else None,
                        spec_path=spec_rel,
                        required_paths=[],
                        status="unsupported",
                        reasons=[f"unsupported asset_type {asset_type!r}"],
                    )
                )
    return items


def classify_items(items: list[WorkItem], ledger: dict[str, dict[str, Any]]) -> None:
    spec_cache: dict[str, str] = {}
    prompt_cache: dict[str, str | None] = {}
    manifest_placeholder_cache: dict[str, bool] = {}
    for item in items:
        spec_path = repo_path(item.spec_path)
        if item.spec_path not in spec_cache:
            spec_cache[item.spec_path] = hash_json_payload(read_json(spec_path)) if spec_path else ""
        item.spec_hash = spec_cache[item.spec_path]

        if item.prompt_path:
            if item.prompt_path not in prompt_cache:
                prompt_cache[item.prompt_path] = stable_hash(repo_path(item.prompt_path))
            item.prompt_hash = prompt_cache[item.prompt_path]

        missing = [path for path in item.required_paths if not (repo_path(path) and repo_path(path).exists())]
        item.runtime_hashes = {
            path: stable_hash(repo_path(path)) or ""
            for path in item.required_paths + [path for path in item.optional_paths if repo_path(path) and repo_path(path).exists()]
        }

        manifest_path = repo_path(item.manifest_path)
        manifest_key = str(manifest_path) if manifest_path else ""
        if manifest_key and manifest_key not in manifest_placeholder_cache:
            manifest_placeholder_cache[manifest_key] = manifest_has_placeholder(manifest_path)
        placeholder = bool(manifest_key and manifest_placeholder_cache[manifest_key])

        record = ledger.get(item.work_id)
        item.ledger = record
        item.reasons = []
        if missing:
            item.status = "missing"
            item.reasons = [f"missing required runtime path: {path}" for path in missing]
            continue
        if placeholder:
            item.status = "placeholder"
            item.reasons = [f"manifest is marked placeholder or temporary: {item.manifest_path}"]
            continue
        if item.status == "unsupported":
            continue
        resolution_failures = runtime_resolution_failures(item)
        if resolution_failures:
            item.status = "needs_review"
            item.reasons = resolution_failures
            continue
        if not record:
            item.status = "needs_review"
            item.reasons = ["runtime files exist but no accepted production ledger record was found"]
            continue
        if record.get("status") != "accepted":
            item.status = "needs_review"
            item.reasons = [f"latest ledger status is {record.get('status')!r}, not 'accepted'"]
            continue
        if record.get("spec_hash") != item.spec_hash or record.get("prompt_hash") != item.prompt_hash:
            item.status = "stale"
            item.reasons = ["source JSON or prompt hash changed since acceptance"]
            continue
        recorded_runtime = record.get("runtime_hashes", {})
        changed = [
            path
            for path, digest in item.runtime_hashes.items()
            if recorded_runtime.get(path) != digest
        ]
        if changed:
            item.status = "needs_review"
            item.reasons = [f"runtime file hash changed since acceptance: {path}" for path in changed]
            continue
        item.status = "accepted"
        item.reasons = []


def status_counts(items: list[WorkItem]) -> dict[str, int]:
    counts = {status: 0 for status in STATUS_ORDER}
    for item in items:
        counts[item.status] = counts.get(item.status, 0) + 1
    return counts


def group_counts(items: list[WorkItem]) -> dict[str, dict[str, int]]:
    result: dict[str, dict[str, int]] = {}
    for item in items:
        result.setdefault(item.group, {status: 0 for status in STATUS_ORDER})
        result[item.group][item.status] = result[item.group].get(item.status, 0) + 1
    return result


def summary_payload(items: list[WorkItem]) -> dict[str, Any]:
    return {
        "schema": "realm.tileset_coverage_report.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "total_items": len(items),
        "status_counts": status_counts(items),
        "group_counts": group_counts(items),
        "items": [item.to_dict() for item in items],
    }


def sorted_items(items: list[WorkItem]) -> list[WorkItem]:
    order = {status: index for index, status in enumerate(STATUS_ORDER)}
    return sorted(items, key=lambda item: (order.get(item.status, 99), item.group, item.slug, item.work_id))


def render_text(items: list[WorkItem], detail_limit: int) -> str:
    payload = summary_payload(items)
    lines = ["Realm tileset coverage", ""]
    lines.append(f"total items: {payload['total_items']}")
    lines.append("status counts:")
    for status in STATUS_ORDER:
        lines.append(f"  {status}: {payload['status_counts'].get(status, 0)}")
    lines.append("")
    shown = 0
    for item in sorted_items(items):
        if item.status == "accepted":
            continue
        if shown >= detail_limit:
            break
        reason = item.reasons[0] if item.reasons else ""
        lines.append(f"{item.status}: {item.work_id} ({reason})")
        shown += 1
    remaining = len([item for item in items if item.status != "accepted"]) - shown
    if remaining > 0:
        lines.append(f"... {remaining} more non-accepted item(s)")
    return "\n".join(lines) + "\n"


def render_markdown(items: list[WorkItem], detail_limit: int) -> str:
    payload = summary_payload(items)
    lines = ["# Realm Tileset Coverage", ""]
    lines.append(f"- Total work items: {payload['total_items']}")
    lines.append("")
    lines.append("## Status Counts")
    lines.append("")
    lines.append("| Status | Count |")
    lines.append("|---|---:|")
    for status in STATUS_ORDER:
        lines.append(f"| {status} | {payload['status_counts'].get(status, 0)} |")
    lines.append("")
    lines.append("## Group Counts")
    lines.append("")
    lines.append("| Group | " + " | ".join(STATUS_ORDER) + " |")
    lines.append("|---|" + "|".join("---:" for _ in STATUS_ORDER) + "|")
    for group, counts in sorted(payload["group_counts"].items()):
        lines.append("| " + group + " | " + " | ".join(str(counts.get(status, 0)) for status in STATUS_ORDER) + " |")
    lines.append("")
    lines.append("## Non-Accepted Items")
    lines.append("")
    lines.append("| Status | Work ID | Reason |")
    lines.append("|---|---|---|")
    shown = 0
    for item in sorted_items(items):
        if item.status == "accepted":
            continue
        if shown >= detail_limit:
            break
        reason = item.reasons[0] if item.reasons else ""
        lines.append(f"| {item.status} | `{item.work_id}` | {reason} |")
        shown += 1
    remaining = len([item for item in items if item.status != "accepted"]) - shown
    if remaining > 0:
        lines.append(f"| ... | ... | {remaining} more non-accepted item(s) not shown |")
    return "\n".join(lines) + "\n"


def write_or_print(text: str, out: str | None) -> None:
    if out:
        path = repo_path(out)
        assert path is not None
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        print(f"wrote {path}")
    else:
        print(text, end="")


def load_and_classify(args: argparse.Namespace) -> list[WorkItem]:
    if getattr(args, "refresh_specs", False):
        refresh_specs()
    root = repo_path(getattr(args, "root", None) or IMAGE_JSON_ROOT)
    ledger_path = repo_path(getattr(args, "ledger", None) or LEDGER_DEFAULT)
    assert root is not None and ledger_path is not None
    items = work_items_from_specs(root)
    classify_items(items, load_ledger(ledger_path))
    return items


def refresh_specs() -> None:
    commands = [
        [sys.executable, "scripts/export_tile_specs.py", "--clean"],
        [sys.executable, "scripts/export_image_generation_prompts.py", "--clean"],
    ]
    for command in commands:
        result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=False)
        if result.returncode != 0:
            details = (result.stderr or result.stdout).strip()
            raise SystemExit(f"failed to refresh generated specs with {' '.join(command)}:\n{details}")
        print((result.stdout or "").strip())


def command_report(args: argparse.Namespace) -> int:
    items = load_and_classify(args)
    if args.format == "json":
        text = json.dumps(summary_payload(items), indent=2, ensure_ascii=True) + "\n"
    elif args.format == "md":
        text = render_markdown(items, args.detail_limit)
    else:
        text = render_text(items, args.detail_limit)
    write_or_print(text, args.out)
    return 0


def batch_items(items: list[WorkItem], limit: int, statuses: list[str] | None) -> list[WorkItem]:
    status_set = set(statuses or NEXT_STATUS_ORDER)
    candidates = [item for item in sorted_items(items) if item.status in status_set]
    return candidates[:limit]


def command_next(args: argparse.Namespace) -> int:
    items = load_and_classify(args)
    batch = batch_items(items, args.limit, args.status)
    payload = {
        "schema": "realm.tileset_generation_batch.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "limit": args.limit,
        "status_filter": args.status or NEXT_STATUS_ORDER,
        "remaining_non_accepted": len([item for item in items if item.status != "accepted"]),
        "batch": [item.to_dict() for item in batch],
        "agent_guidance": [
            "Open each item's prompt and JSON spec before generating.",
            "Store generated keepers under art/tiles/candidates before promotion.",
            "Promote only reviewed runtime-ready files into the listed required paths.",
            "After promotion, append an accepted record with scripts/tileset_coverage.py accept.",
            "Rerun scripts/tileset_coverage.py verify --strict before claiming full coverage.",
        ],
    }
    text = json.dumps(payload, indent=2, ensure_ascii=True) + "\n"
    write_or_print(text, args.out)
    return 0


def command_verify(args: argparse.Namespace) -> int:
    items = load_and_classify(args)
    failures = [item for item in sorted_items(items) if item.status != "accepted"]
    if not failures:
        print(f"tileset coverage verified: {len(items)} accepted work item(s)")
        return 0
    counts = status_counts(failures)
    print("tileset coverage verification failed", file=sys.stderr)
    for status in STATUS_ORDER:
        count = counts.get(status, 0)
        if count:
            print(f"{status}: {count}", file=sys.stderr)
    for item in failures[: args.detail_limit]:
        reason = item.reasons[0] if item.reasons else ""
        print(f"- {item.status}: {item.work_id} ({reason})", file=sys.stderr)
    if len(failures) > args.detail_limit:
        print(f"... {len(failures) - args.detail_limit} more failure(s)", file=sys.stderr)
    return 1


def command_accept(args: argparse.Namespace) -> int:
    items = load_and_classify(args)
    matches = [item for item in items if item.work_id == args.work_id]
    if not matches:
        raise SystemExit(f"unknown work item: {args.work_id}")
    item = matches[0]
    if item.status in {"missing", "placeholder", "unsupported"} and not args.force:
        raise SystemExit(f"{item.work_id} is {item.status}; pass --force only after manual review if this is intentional")
    resolution_failures = runtime_resolution_failures(item)
    if resolution_failures and not args.force:
        details = "\n".join(f"- {failure}" for failure in resolution_failures)
        raise SystemExit(
            f"{item.work_id} does not meet the runtime source-resolution policy; regenerate or pass --force only after an explicit exception:\n{details}"
        )
    record = {
        "schema": "realm.tileset_production_ledger.v1",
        "work_id": item.work_id,
        "status": "accepted",
        "accepted_at": datetime.now().isoformat(timespec="seconds"),
        "review": args.review,
        "spec_hash": item.spec_hash,
        "prompt_hash": item.prompt_hash,
        "runtime_hashes": item.runtime_hashes,
        "required_paths": item.required_paths,
        "optional_paths": item.optional_paths,
    }
    ledger_path = repo_path(args.ledger or LEDGER_DEFAULT)
    assert ledger_path is not None
    ledger_path.parent.mkdir(parents=True, exist_ok=True)
    with ledger_path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, ensure_ascii=True, sort_keys=True) + "\n")
    print(f"accepted {item.work_id} in {ledger_path}")
    return 0


def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--root", default=rel(IMAGE_JSON_ROOT), help="generated image-json root")
    parser.add_argument("--ledger", default=rel(LEDGER_DEFAULT), help="production ledger JSONL path")
    parser.add_argument("--refresh-specs", action="store_true", help="regenerate image-json and image-spec before reporting")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    report = sub.add_parser("report", help="summarize tileset coverage")
    add_common_args(report)
    report.add_argument("--format", choices=["text", "md", "json"], default="text")
    report.add_argument("--out", help="write report to this path")
    report.add_argument("--detail-limit", type=int, default=200)
    report.set_defaults(func=command_report)

    next_batch = sub.add_parser("next", help="emit the next agent-ready production batch")
    add_common_args(next_batch)
    next_batch.add_argument("--limit", type=int, default=12)
    next_batch.add_argument("--status", action="append", choices=NEXT_STATUS_ORDER, help="status to include; repeatable")
    next_batch.add_argument("--out", help="write batch JSON to this path")
    next_batch.set_defaults(func=command_next)

    verify = sub.add_parser("verify", help="fail if any work item is not accepted")
    add_common_args(verify)
    verify.add_argument("--strict", action="store_true", help="document intent; verification is strict by default")
    verify.add_argument("--detail-limit", type=int, default=80)
    verify.set_defaults(func=command_verify)

    accept = sub.add_parser("accept", help="append an accepted ledger record for one reviewed work item")
    add_common_args(accept)
    accept.add_argument("--work-id", required=True)
    accept.add_argument("--review", required=True, help="short review note or artifact path")
    accept.add_argument("--force", action="store_true", help="allow accepting an item currently classified as missing/placeholder/unsupported")
    accept.set_defaults(func=command_accept)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if hasattr(args, "detail_limit") and args.detail_limit < 1:
        raise SystemExit("--detail-limit must be positive")
    if hasattr(args, "limit") and args.limit < 1:
        raise SystemExit("--limit must be positive")
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
