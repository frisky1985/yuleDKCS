# Docs Blueprint

This skill uses a progressive-disclosure doc layout so agents can load only the context they need.

## Recommended leaf docs

- `docs/agent/index.md` — navigation hub and reading order
- `docs/agent/architecture.md` — boundaries, modules, dependencies, invariants
- `docs/agent/specs.md` — product or system behaviors that must remain true
- `docs/agent/plans.md` — current initiatives, active migrations, known next steps
- `docs/agent/quality.md` — tests, checks, linting, coding rules, merge gates
- `docs/agent/reliability.md` — runtime expectations, SLOs, debugging entry points
- `docs/agent/security.md` — trust boundaries, secrets, auth, threat assumptions
- `docs/agent/garbage-collection.md` — cleanup policy and review cadence

## Required frontmatter fields

The bundled validator checks for these keys:

- `title`
- `purpose`
- `owner`
- `last_reviewed`
- `source_of_truth`

Use ISO dates for `last_reviewed`, for example `2026-03-08`.

## Writing pattern

Each leaf doc should answer three questions quickly:

1. What is this document for?
2. When should an agent read it?
3. What must stay true?

That is more useful than long prose.

## Good routing patterns

- put short “read this when…” bullets near the top
- link outward to deeper docs if they already exist elsewhere in the repo
- prefer explicit invariants and check commands over narrative explanation
- keep one index page current so discovery stays cheap
