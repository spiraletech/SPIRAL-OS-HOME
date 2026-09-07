# SPIRAL OS: HOME

HOME is the canonical life + world-state authority for Spiral OS.

## Authority doctrine

- **HOME** — what exists / what is true / what persists.
- **HAKUI** — what bodies physically do.
- **INOKUI** — how canonical state manifests visually.
- **ENTROKOI** — how canonical state is perceived by an observer.
- **XENON** — how canonical state manifests acoustically and through signals.
- **GOLF-GUFF** — governed reasoning, routing, policy and authority around operations.

HOME does not render, simulate live body physics, own cameras, synthesize audio, or embed GUFF. Other engines consume HOME state and submit typed consequences for HOME to validate and commit.

## L1 — Foundation

L1 establishes the stable C++20 substrate required by every later HOME layer: strongly typed IDs, monotonic world revisions, deterministic integer canonical transforms, protocol/version primitives, common result/error contracts, the `HOME::Core` target and acceptance tests.

## L2 — Entity Registry

L2 gives HOME authoritative entity identity and lifecycle storage through deterministic create/find/remove/restore/query behavior, stable IDs, cross-world validation and ordered snapshots.

## L3 — World Revision + Immutable Deltas

L3 turns HOME state into versioned reality.

- `VersionedWorld` owns the canonical `EntityRegistry`, current `WorldRevision`, and ordered delta history.
- Every successful canonical mutation advances the revision exactly once.
- Failed and no-op mutations do not advance revision or append history.
- `WorldDelta` is immutable after construction and records exact `from_revision` → `to_revision` boundaries.
- `WorldChange` currently covers entity create, remove and transform update with before/after data suitable for fanout, audit and later replay.
- Create/remove/restore operations roll back registry state if revision commit fails.
- `deltas_since(revision)` provides deterministic catch-up for future HAKUI/INOKUI/ENTROKOI/XENON adapters and rejects requests ahead of canonical reality.
- L3 acceptance tests cover revision sequencing, delta payloads, transform before/after capture, no-op behavior, historical queries, restore, removal and revision-conflict handling.

L3 is intentionally **not** the transaction layer. It establishes the change journal and revision law; L5 will group multiple validated mutations into one atomic transaction rather than overloading this layer prematurely.

## Build and test

```sh
cmake -S . -B build -DHOME_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

L1 Foundation ✅ → L2 Entity Registry ✅ → L3 World Revision + Deltas ✅ → L4 Topology/Zones → L5 Transactions/Validation → L6 Snapshot/Save/Restore → L7 World Clock → L8 Calendar/Seasons → L9 Temporal Domains → L10 Events/Festivals/Holidays → L11 Weather/Climate → L12 World Affect/Theme Anchors → L13 Player Life State → L14 Needs/Mood/Autonomy → L15 Relationships/Households → L16 Items/Inventory → L17 Skills/Tasks/Quests → L18 Subclass Theorism/Aura → L19 HAKUI Adapter → L20 INOKUI Adapter → L21 ENTROKOI Adapter → XENON integration → GUFF HOME Cartridge → Mission Bay World Package → HOME v0.1 Vertical Slice.
