# CLAUDE.md

Guidance for an assistant (Claude Code or another) working in this repository, or reading it from a project that
uses the library.

## If you are working on a project that USES G2AlikeMaths

1. Read `Docs/status.md` first: it says what is built. The header and `Docs/api.md` describe the full intended API,
   which may be ahead of what works.
2. `include/g2alikemaths.h` and `Docs/contract.md` are the contract. Code against those, not against anything in
   `engine/` - that is internal and changes without notice.
3. Questions, requests, problems and decisions go in `Docs/exchange.md`, as a new dated entry at the END, signed with
   your project's name. Never edit an earlier entry; answer in a new one. Keep each entry short and self-contained -
   the reader may have no other context.
4. If something in the contract is unclear, ask there rather than guessing.

## If you are working on G2AlikeMaths itself

- **Public repo.** Describe what the engine does and how it was checked against the hardware. G2Alike's rules apply
  here unchanged.
- `engine/` is a copy of G2Alike's engine, made by `./sync-from-g2alike`: fix the engine in G2Alike and sync, never
  edit `engine/` by hand.
- The API changes only with an entry in `Docs/exchange.md`, and an incompatible change bumps `G2AM_API_VERSION`.
- After a change: update `Docs/status.md`, and `Docs/api.md` if usage changed.
- Commits only when the maintainer asks.
