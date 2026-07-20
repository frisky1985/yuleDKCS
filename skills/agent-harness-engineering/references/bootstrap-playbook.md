# Bootstrap Playbook

Use this playbook when rolling the pattern into a repo for the first time.

## 1. Assess the repo before changing it

Check these first:

- Is there already an `AGENTS.md`? If yes, preserve project-specific constraints.
- Is there already a `docs/` tree? If yes, prefer `overlay` mode.
- What is the repo's native validation command? Reuse it instead of inventing a parallel command set.
- Are there existing architecture/spec/security docs? Link them from `docs/agent/index.md` instead of duplicating.

## 2. Pick the smallest rollout that helps

### `overlay` mode

Best for existing repos.

- Add a small `docs/agent/` overlay.
- Keep the current docs layout.
- Use `AGENTS.md` as a router into both the new overlay and any established docs.

### `full` mode

Best for greenfield repos or explicit documentation redesigns.

- Use the generated docs as the main durable knowledge system.
- Expand the taxonomy only when the repo actually needs it.

## 3. Customize immediately after scaffolding

Do not leave placeholders behind for long.

Update these first:

- doc `owner`
- repo check command names in `AGENTS.md`
- architecture boundary notes
- quality invariants
- reliability and security assumptions

## 4. Wire checks into the repo's existing flow

Preferred pattern:

- add `python3 scripts/agent_repo_check.py` to the repo's normal validation command
- if the repo already has a meta-check command, call the script from there
- if CI exists, run the script in CI too

## 5. Roll out garbage collection carefully

Enable GC when one or more are true:

- multiple agents contribute to the repo
- large volumes of generated code land each week
- stale docs appear often
- file size creep is already a problem

Start with reporting only. Require human review before cleanup patches are merged.
