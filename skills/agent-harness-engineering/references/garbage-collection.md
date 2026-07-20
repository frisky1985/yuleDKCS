# Garbage Collection

Garbage collection is a lightweight maintenance loop for agent-generated entropy.

## Goals

- catch drift before it becomes architecture debt
- surface cleanup candidates early
- preserve throughput without letting the repo rot

## Default GC report

The bundled report script looks for:

- stale docs based on `last_reviewed`
- docs missing required frontmatter
- docs not linked from `docs/agent/index.md`
- oversized source or doc files
- filenames that imply drift, copies, or abandoned iterations
- `TODO` and `FIXME` hotspots

## Recommended cadence

- low-change repos: run manually before major releases
- medium-change repos: run weekly
- high-change or multi-agent repos: run daily or on a recurring automation

## Review policy

Use GC to create reviewable reports or tiny cleanup patches.

- do not auto-delete files
- do not auto-merge broad refactors without review
- prefer many small cleanups over infrequent big rewrites
