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

L1 establishes the stable C++20 substrate required by every later HOME layer:

- strongly typed 64-bit IDs for entities, zones, worlds, transactions, time domains, players, items, quests and events;
- monotonic `WorldRevision` with overflow-safe advancement;
- deterministic canonical transforms using integer millimetres and millidegrees;
- `WorldStamp` and protocol version primitives;
- common `Result<T>` / `ErrorCode` contract;
- `HOME::Core` CMake target;
- executable L1 acceptance tests.

Canonical state deliberately avoids floating-point spatial storage. Physics and rendering adapters may project HOME values into engine-local representations without changing canonical truth.

## Build and test

```sh
cmake -S . -B build -DHOME_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

L2 Entity Registry → L3 World Revision + Deltas → L4 Topology/Zones → L5 Transactions/Validation → L6 Snapshot/Save/Restore → L7 World Clock → L8 Calendar/Seasons → L9 Temporal Domains → L10 Events/Festivals/Holidays → L11 Weather/Climate → L12 World Affect/Theme Anchors → L13 Player Life State → L14 Needs/Mood/Autonomy → L15 Relationships/Households → L16 Items/Inventory → L17 Skills/Tasks/Quests → L18 Subclass Theorism/Aura → L19 HAKUI Adapter → L20 INOKUI Adapter → L21 ENTROKOI Adapter → XENON integration → GUFF HOME Cartridge → Mission Bay World Package → HOME v0.1 Vertical Slice.
