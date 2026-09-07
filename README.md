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

L2 gives HOME authoritative entity identity and lifecycle storage.

- `EntityRecord` is the canonical minimal record: stable ID, owning world, kind, archetype, display name, transform and persistence flag.
- `EntityRegistry` allocates non-zero monotonically increasing IDs and never silently reuses removed IDs during a running registry lifetime.
- Creation validates world authority, entity kind and archetype.
- Restore accepts explicit stable IDs for persistence/replay and advances the allocator beyond restored identity.
- Cross-world restore and duplicate identity are rejected.
- Remove returns the retired record so later transaction/delta layers can journal lifecycle changes.
- Queries and snapshots are returned in deterministic ID order.
- L2 acceptance tests cover create/find/filter/snapshot/restore/remove, invalid worlds, duplicate/cross-world restore and allocator exhaustion.

L2 intentionally does **not** advance `WorldRevision`: L3 will make lifecycle operations produce immutable canonical deltas and revisions rather than coupling revision semantics prematurely to registry storage.

## Build and test

```sh
cmake -S . -B build -DHOME_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

L1 Foundation ✅ → L2 Entity Registry ✅ → L3 World Revision + Deltas → L4 Topology/Zones → L5 Transactions/Validation → L6 Snapshot/Save/Restore → L7 World Clock → L8 Calendar/Seasons → L9 Temporal Domains → L10 Events/Festivals/Holidays → L11 Weather/Climate → L12 World Affect/Theme Anchors → L13 Player Life State → L14 Needs/Mood/Autonomy → L15 Relationships/Households → L16 Items/Inventory → L17 Skills/Tasks/Quests → L18 Subclass Theorism/Aura → L19 HAKUI Adapter → L20 INOKUI Adapter → L21 ENTROKOI Adapter → XENON integration → GUFF HOME Cartridge → Mission Bay World Package → HOME v0.1 Vertical Slice.
