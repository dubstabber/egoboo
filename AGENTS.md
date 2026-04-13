# AGENTS.md

Project-level instructions for Codex sessions in this repository.

Keep this file focused on repository expectations and setup. If a specific subdirectory later needs stricter rules, add a nested `AGENTS.md` or `AGENTS.override.md` close to that work.

## Repository scope

- Active code and content live primarily in `egoboo/`, `egolib/`, `idlib/`, `idlib-game-engine/`, `data/`, and `tools/`.
- Refactor and architecture documentation belongs in `refactoring-documents/`.
- The canonical Linux build and run guide is `doc/build-linux.md`.

## Read-only and generated areas

- `backup-copy/` is a reference snapshot only. Never modify, delete, rename, or "clean up" anything in `backup-copy/`.
- `build/` is generated output. Do not treat it as source code and do not make manual edits there.

## Refactor workflow

- Before large refactors, read:
  - `refactoring-documents/README.md`
  - `refactoring-documents/04-refactoring-strategy.md`
  - `refactoring-documents/06-validator-baseline.md`
- If you analyze architecture, plan refactors, or discover important structural issues, write or update markdown documents in `refactoring-documents/`.
- Prefer small, verifiable changes over broad rewrites without checkpoints.
- Preserve current Linux/Fedora portability behavior unless you are intentionally revisiting it, and document any such change.

## Project subagents

Project-scoped custom agents live in `.codex/agents/` and share shallow delegation defaults from `.codex/config.toml`.

Available agents:

- `repo_architect`: read-only architecture and coupling mapper
- `content_auditor`: read-only content and legacy-format integrity analyst
- `validator_runner`: targeted build and validator operator
- `linux_portability`: Linux and Fedora portability troubleshooter
- `refactor_worker`: bounded implementation and refactor executor

Use them for narrow, parallelizable tasks. Keep ownership explicit when delegating implementation work.

## Build and validation

- Never run builds with more than 4 parallel jobs; higher values can destabilize this machine.
- Configure and build with CMake:
  - `cmake -S . -B build`
  - `cmake --build build -j4`
- After changing runtime code, content loading, or module/object data, run the validator at minimum on `test.mod`:
  - `./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data" --module test.mod`
- When touching module formats, object loading, VFS behavior, or shared content, run the full validator:
  - `./build/products/x64/bin/egoboo-content-validator --data-dir "$PWD/data"`
- For interactive Linux runs, prefer `./run-egoboo.sh` or follow `doc/build-linux.md`.
- In sandboxed or read-only-home environments, redirect writable user-data paths when needed:
  - `HOME=/tmp/egoboo-home XDG_DATA_HOME=/tmp/egoboo-xdg ...`

## Content realities

- Do not assume the legacy content set is internally consistent.
- The current validator baseline shows many failures caused by missing spawn-referenced objects rather than parser crashes.
- Check `refactoring-documents/06-validator-baseline.md` before treating validator failures as newly introduced regressions.

## Documentation lookup

- When the user asks about a library, framework, SDK, API, CLI tool, or cloud service, use `ctx7` to fetch current documentation.
- Workflow:
  1. `npx ctx7@latest library <name> "<full user question>"`
  2. Pick the best `/org/project` match.
  3. `npx ctx7@latest docs <libraryId> "<full user question>"`
- Use `ctx7` for API syntax, configuration, version migration, setup, CLI usage, and library-specific debugging.
- Do not use `ctx7` for refactoring plans, project-specific business logic, code review, or general programming concepts.
