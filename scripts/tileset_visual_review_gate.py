#!/usr/bin/env python3
"""Build a tileset review sheet and emit a script-assisted review gate report."""

from __future__ import annotations

import argparse
import json
import sys
import subprocess
from pathlib import Path


def batch_paths(path: str) -> set[str]:
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    items = data.get("batch", data if isinstance(data, list) else [])
    paths: set[str] = set()
    for item in items:
        if not isinstance(item, dict):
            continue
        for rel in item.get("required_paths", []):
            paths.add(str(rel).replace("\\", "/"))
    return paths


def run_json(cmd: list[str]) -> dict:
    proc = subprocess.run(cmd, check=True, text=True, capture_output=True)
    text = proc.stdout.strip()
    if not text:
        return {}
    return json.loads(text)


def newest_batch_input_mtime(batch: str) -> float:
    newest = Path(batch).stat().st_mtime if Path(batch).exists() else 0.0
    for rel in batch_paths(batch):
        path = Path(rel)
        if path.exists():
            newest = max(newest, path.stat().st_mtime)
    return newest


def audit_cache_is_fresh(audit_json: str, batch: str) -> bool:
    path = Path(audit_json)
    if not path.exists():
        return False
    return path.stat().st_mtime >= newest_batch_input_mtime(batch)


def run_or_reuse_audit(args: argparse.Namespace) -> tuple[dict, bool, str]:
    if args.audit_mode in {"batch", "none"}:
        return {"issues": []}, False, "skipped"
    reused = args.audit_mode == "cache" and audit_cache_is_fresh(args.audit_json, args.batch)
    if not reused:
        subprocess.run(
            [
                sys.executable,
                "scripts/tileset_production_audit.py",
                "--json-out",
                args.audit_json,
                "--sheet-dir",
                args.audit_sheet_dir,
            ],
            check=False,
        )
    audit_path = Path(args.audit_json)
    if not audit_path.exists():
        return {"issues": []}, reused, "missing"
    return json.loads(audit_path.read_text(encoding="utf-8")), reused, "full"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch", default="build/tileset-next-batch.json")
    parser.add_argument("--out", required=True, help="Contact sheet path.")
    parser.add_argument("--label-strip", default="")
    parser.add_argument("--audit-json", default="build/tileset-production-audit.json")
    parser.add_argument("--audit-sheet-dir", default="build/tileset-production-audit")
    parser.add_argument(
        "--audit-mode",
        choices=("full", "cache", "batch", "none"),
        default="full",
        help=(
            "full runs the whole production audit; cache reuses a fresh audit JSON when possible; "
            "batch/none skip the full audit and only use batch cleanup findings."
        ),
    )
    parser.add_argument("--cleanup-json", default="build/tileset-cleanup-dry-run.json")
    parser.add_argument("--reference-image", action="append", default=[], help="Optional reference thumbnails to include in the contact sheet.")
    parser.add_argument("--report-out", required=True)
    args = parser.parse_args()

    review_cmd = [
        sys.executable,
        "scripts/tileset_review_existing_batch.py",
        "--batch",
        args.batch,
        "--out",
        args.out,
        "--label-strip",
        args.label_strip,
        "--review-note",
        "inspection only: visual review gate contact sheet",
    ]
    for ref in args.reference_image:
        review_cmd.extend(["--reference-image", ref])
    subprocess.run(review_cmd, check=True)
    cleanup = run_json(
        [
            sys.executable,
            "scripts/tileset_cleanup_artifacts.py",
            "--batch",
            args.batch,
            "--dry-run",
            "--json-out",
            args.cleanup_json,
        ]
    )
    audit, audit_reused, audit_scope = run_or_reuse_audit(args)
    current_paths = batch_paths(args.batch)
    high = [
        issue
        for issue in audit.get("issues", [])
        if issue.get("severity") in {"blocker", "high"} and str(issue.get("path", "")).replace("\\", "/") in current_paths
    ]
    verdict = "script_gate_passed_visual_review_still_required"
    if high:
        verdict = "blocked_by_production_audit"
    elif cleanup.get("findings", 0):
        verdict = "blocked_by_detected_detached_artifacts"
    report = {
        "batch": args.batch,
        "contact_sheet": args.out,
        "audit_json": args.audit_json,
        "audit_mode": args.audit_mode,
        "audit_reused": audit_reused,
        "audit_scope": audit_scope,
        "cleanup_json": args.cleanup_json,
        "reference_images": args.reference_image,
        "high_audit_issues": high,
        "detached_artifact_findings": cleanup.get("findings", 0),
        "verdict": verdict,
        "important": "This script gate is not acceptance. Visual contact-sheet review overrides a passing script audit.",
    }
    Path(args.report_out).write_text(json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=True))
    return 1 if verdict.startswith("blocked") else 0


if __name__ == "__main__":
    raise SystemExit(main())
