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

L3 turns HOME state into versioned reality through `VersionedWorld`, monotonic canonical revisions, typed immutable `WorldDelta`s, before/after mutation payloads and deterministic `deltas_since()` catch-up.

## L4 — Topology / Zones

L4 gives canonical reality explicit world shape without moving collision or locomotion authority out of HAKUI.

- `TopologyRegistry` owns stable zone identity, hierarchy, connections and entity-to-zone placement.
- `ZoneRecord` separates semantic containment (`parent`) from traversal topology (`ZoneConnection`). A place can be inside another place without automatically being walkable to every sibling.
- Zone keys are unique within a world and zone IDs are deterministic, non-zero and monotonic.
- Parent changes reject missing parents, no-ops and hierarchy cycles.
- Directed and bidirectional zone links represent canonical routes/thresholds while preserving a traversable flag for locked or narrative barriers.
- Entity placement is canonical HOME state; HAKUI may physically move a body, but HOME records which semantic zone that entity occupies.
- Entity removal automatically clears canonical zone placement in the same world revision delta.
- `WorldChange` now journals `ZoneCreated`, `ZoneParentChanged`, `ZonesConnected` and `EntityZoneChanged` alongside entity lifecycle and transform changes.
- Deterministic queries expose zone children, neighbors, direct traversability, ordered zone snapshots and ordered connection snapshots.
- L4 acceptance tests model Mission Bay → Pacific Beach → Crystal Pier / Belmont containment and boardwalk traversal, cycle rejection, placement transitions, missing-zone handling and placement cleanup on entity removal.

L4 intentionally does not own polygon collision, navmesh generation or movement simulation. Those are HAKUI concerns. HOME owns the semantic topology that says *where places are in relation to one another and where entities canonically belong*.

## Build and test

```sh
cmake -S . -B build -DHOME_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

L1 Foundation ✅ → L2 Entity Registry ✅ → L3 World Revision + Deltas ✅ → L4 Topology/Zones ✅ → L5 Transactions/Validation → L6 Snapshot/Save/Restore → L7 World Clock → L8 Calendar/Seasons → L9 Temporal Domains → L10 Events/Festivals/Holidays → L11 Weather/Climate → L12 World Affect/Theme Anchors → L13 Player Life State → L14 Needs/Mood/Autonomy → L15 Relationships/Households → L16 Items/Inventory → L17 Skills/Tasks/Quests → L18 Subclass Theorism/Aura → L19 HAKUI Adapter → L20 INOKUI Adapter → L21 ENTROKOI Adapter → XENON integration → GUFF HOME Cartridge → Mission Bay World Package → HOME v0.1 Vertical Slice.
