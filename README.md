# SPIRAL-OS-HOME

SPIRAL OS: HOME is the canonical **World State host/coordinator** for Spiral engines.

HOME is not a monolithic game engine and it is not the universal simulation authority.

> Authority decides truth inside a domain. HOME validates, orders, persists, and publishes that truth.

## Core law

> Snapshot + ordered committed deltas = current reality.

The first frozen target is the **Spiral World State Contract v0.1**: semantic IDs, authority rules, snapshots, commands, deltas, events, transactions, spatial containment, versioning, and wire/schema representation.

This is intentionally **not a C++ ABI**. C++ headers in this repository are reference adapter types and may evolve behind the semantic contract.

## Initial jurisdictions

- **HOME** — world-state coordination, identity, lifecycle, topology, commit ordering, persistence, publication
- **HAKUI** — embodiment, locomotion, physical transform while embodied, pose, interaction
- **INOKUI** — manifestation, material/surface/appearance semantics
- **ENTROKOI** — observer, lens, perception semantics
- **XENON / Spiral AI** — command/intent bridge unless explicitly granted a domain authority

## Contract map

- `docs/WORLD_STATE_CONTRACT.md` — canonical semantic rules
- `docs/AUTHORITY_MODEL.md` — jurisdiction, authorship, commit authority, transfer, causality
- `docs/ENGINE_BOUNDARIES.md` — engine-private vs shared semantic state
- `schema/spiral-world-state-v0.1.schema.json` — snapshot wire schema
- `schema/spiral-world-protocol-v0.1.schema.json` — command/delta/event/transaction protocol
- `include/spiral/world/` — non-ABI C++ reference adapter types
- `tests/world_contract_smoke.cpp` — contract smoke coverage

## L0 scope

L0 defines the shared reality contract only. Terrain, weather simulation, rendering, gameplay systems, and engine-specific implementation belong to later layers and must consume this contract rather than redefine it.
