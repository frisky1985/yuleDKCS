#!/usr/bin/env python3
from __future__ import annotations

import argparse
import datetime as dt
import pathlib
import shutil
from typing import Dict

SKILL_DIR = pathlib.Path(__file__).resolve().parent.parent
TEMPLATE_DIR = SKILL_DIR / "assets" / "templates"
DOC_TEMPLATE_DIR = TEMPLATE_DIR / "docs-agent"
START_MARKER = "<!-- agent-harness:start -->"
END_MARKER = "<!-- agent-harness:end -->"


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8")


def render(text: str, values: Dict[str, str]) -> str:
    for key, value in values.items():
        text = text.replace(f"__{key}__", value)
    return text


def write_file(path: pathlib.Path, content: str, *, force: bool, dry_run: bool) -> None:
    if path.exists() and not force:
        print(f"SKIP {path}")
        return
    print(f"WRITE {path}")
    if dry_run:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def ensure_symlink(link_path: pathlib.Path, target: str, *, dry_run: bool) -> None:
    if link_path.exists() or link_path.is_symlink():
        print(f"SKIP {link_path}")
        return
    print(f"LINK {link_path} -> {target}")
    if dry_run:
        return
    link_path.symlink_to(target)


def build_agent_block(doc_root: str, with_gc: bool) -> str:
    template = read_text(TEMPLATE_DIR / "agents_block.md.tmpl")
    values = {
        "DOC_ROOT": doc_root,
        "DOC_INDEX": f"{doc_root}/index.md",
        "GC_BULLET": "- Garbage collection: `docs/agent/garbage-collection.md`" if with_gc else "",
        "GC_LINE": "- Review `python3 scripts/agent_gc_report.py` output regularly." if with_gc else "",
    }
    return render(template, values)


def update_agents_file(repo: pathlib.Path, doc_root: str, with_gc: bool, *, force: bool, dry_run: bool) -> None:
    agents_path = repo / "AGENTS.md"
    block = build_agent_block(doc_root, with_gc)
    if agents_path.exists():
        original = read_text(agents_path)
        if START_MARKER in original and END_MARKER in original:
            start = original.index(START_MARKER)
            end = original.index(END_MARKER) + len(END_MARKER)
            updated = original[:start].rstrip() + "\n\n" + block + "\n"
            if end < len(original):
                updated += "\n" + original[end:].lstrip("\n")
        else:
            updated = original.rstrip() + "\n\n" + block + "\n"
        write_file(agents_path, updated, force=True if not force else force, dry_run=dry_run)
        return

    template = read_text(TEMPLATE_DIR / "AGENTS.md.tmpl")
    content = render(template, {"AGENT_BLOCK": block, "DOC_ROOT": doc_root})
    write_file(agents_path, content, force=force, dry_run=dry_run)


def copy_doc_templates(repo: pathlib.Path, doc_root: str, *, with_gc: bool, force: bool, dry_run: bool) -> None:
    replacements = {
        "DATE": dt.date.today().isoformat(),
        "DOC_ROOT": doc_root,
        "GC_INDEX_APPEND": ", `docs/agent/garbage-collection.md`" if with_gc else "",
        "GC_INDEX_BULLET": "- `docs/agent/garbage-collection.md`" if with_gc else "",
    }
    for template_path in DOC_TEMPLATE_DIR.glob("*.tmpl"):
        name = template_path.name.removesuffix(".tmpl")
        if name == "garbage-collection.md" and not with_gc:
            continue
        output_path = repo / doc_root / name
        content = render(read_text(template_path), replacements)
        write_file(output_path, content, force=force, dry_run=dry_run)


def copy_script(repo: pathlib.Path, template_name: str, output_name: str, *, force: bool, dry_run: bool) -> None:
    output_path = repo / "scripts" / output_name
    content = read_text(TEMPLATE_DIR / template_name)
    write_file(output_path, content, force=force, dry_run=dry_run)
    if dry_run:
        return
    output_path.chmod(0o755)


def ensure_full_mode_notes(repo: pathlib.Path, *, dry_run: bool) -> None:
    docs_readme = repo / "docs" / "README.md"
    if docs_readme.exists():
        return
    content = "# Docs\n\nUse `docs/agent/index.md` as the agent-readable entry point.\n"
    write_file(docs_readme, content, force=False, dry_run=dry_run)


def main() -> int:
    parser = argparse.ArgumentParser(description="Bootstrap a repo for agent harness engineering.")
    parser.add_argument("--repo", required=True, help="Repository root")
    parser.add_argument("--mode", choices=["overlay", "full"], default="overlay")
    parser.add_argument("--with-gc", action="store_true")
    parser.add_argument("--force", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--no-claude-link", action="store_true")
    args = parser.parse_args()

    repo = pathlib.Path(args.repo).expanduser().resolve()
    if not repo.exists():
        raise SystemExit(f"Repo does not exist: {repo}")

    doc_root = "docs/agent"
    update_agents_file(repo, doc_root, args.with_gc, force=args.force, dry_run=args.dry_run)
    if not args.no_claude_link:
        ensure_symlink(repo / "CLAUDE.md", "AGENTS.md", dry_run=args.dry_run)
    copy_doc_templates(repo, doc_root, with_gc=args.with_gc, force=args.force, dry_run=args.dry_run)
    copy_script(repo, "agent_repo_check.py.tmpl", "agent_repo_check.py", force=args.force, dry_run=args.dry_run)
    if args.with_gc:
        copy_script(repo, "agent_gc_report.py.tmpl", "agent_gc_report.py", force=args.force, dry_run=args.dry_run)
    if args.mode == "full":
        ensure_full_mode_notes(repo, dry_run=args.dry_run)
    print("DONE")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
