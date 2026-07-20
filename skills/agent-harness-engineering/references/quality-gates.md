# Quality Gates

The point of this skill is not just to add docs. It is to convert repo discipline into checks that agents can follow reliably.

## Baseline checks

The bundled `scripts/agent_repo_check.py` covers stack-agnostic rules:

- required docs exist
- docs include required frontmatter
- `last_reviewed` is a valid ISO date
- the docs index references every active leaf doc
- `AGENTS.md` points to the docs index and validation script

## Recommended project-native additions

After bootstrapping, adapt the repo to enforce its own higher-value invariants.

Examples:

- architecture imports or dependency direction
- mandatory parser/validator boundaries
- forbidden folders or file naming patterns
- required tests for changed subsystems
- performance budgets or smoke checks

## Best practice

Keep the generic checker small and deterministic.

- use it for universal structural checks
- use repo-native lint/test/build commands for stack-specific rules
- surface actionable error messages so the next agent can self-correct
