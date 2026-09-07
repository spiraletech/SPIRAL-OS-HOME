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

L4 gives canonical reality explicit world shape without moving collision or locomotion authority out of HAKUI. `TopologyRegistry` owns stable zone identity, containment hierarchy, traversal connections and entity-to-zone placement. Containment and traversal are intentionally distinct concepts.

## L5 — Transactions + Validation

L5 adds staged `WorldTransaction` execution with optimistic revision checks, explicit authority tags, atomic rollback on any invalid operation, multi-operation/single-revision commits and `TransactionReceipt`s. CI validates HOME Release builds on Ubuntu and Windows while keeping acceptance assertions active in test executables.

## L6 — Snapshot / Save / Restore

L6 gives HOME exact persistence without manufacturing fake world mutations.

- `WorldSnapshot` captures world identity, canonical revision, entities, zones, topology connections and entity placements.
- Snapshot encoding is deterministic, versioned and dependency-free; quoted strings preserve display names and keys containing spaces.
- File save/load uses the same canonical codec and returns typed serialization errors.
- `VersionedWorld::from_snapshot()` hydrates registries directly, preserves the exact saved revision and begins with an empty post-load delta history.
- Zone hydration is deliberately two-pass, so parent/child relationships restore correctly even when a parent has a higher `ZoneId` than its child.
- Entity and zone allocators advance beyond restored IDs so newly created state never reuses canonical identity.
- Snapshot validation rejects foreign-world records, missing placement entities, malformed sections, unknown enum values and unsupported format versions.

## L14 — Needs / Mood / Autonomy

L14 adds deterministic bounded player-state primitives without stealing behavior authority from HAKUI or reasoning authority from GUFF. `NeedsLedger` stores five canonical need channels on a fixed 0–1000 scale, deterministic per-tick decay, bounded mood valence/arousal, autonomy mode plus initiative threshold, monotonic per-player revisioning, ordered snapshots and typed validation failures. HOME records these facts; downstream engines decide how they manifest or act on them.

## L15 — Relationships / Households

L15 adds canonical social membership facts while keeping behavior and perception downstream. `RelationshipsLedger` owns normalized unordered player-pair relationships, typed relationship kinds, bounded affinity, monotonic relationship revisions, deterministic snapshots, stable `HouseholdId` allocation, named households, sorted unique membership, one-household-per-player validation, household renaming/membership mutation/dissolution and monotonic household revisions. HOME records who is related to whom and who belongs together; HAKUI, INOKUI, ENTROKOI, XENON and GUFF decide how those facts are acted on, shown, perceived, sounded or reasoned about.

## L16 — Items / Inventory

L16 adds deterministic canonical item instances and inventory ownership without turning HOME into a rendering, equipment, or action engine. `InventoryLedger` allocates stable `ItemId`s, stores archetype/display identity, typed item kinds, bounded stack quantities, player ownership and monotonic item revisions. Ownership transfer, stack mutation, consumption, removal, per-player inventory queries, unowned-item queries and ordered snapshots all preserve canonical state while downstream engines decide how items look, equip, animate, sound or are used.

## L17 — Skills / Tasks / Quests

L17 adds canonical progression facts without moving action execution, planning or presentation into HOME. `ProgressionLedger` owns per-player keyed skills with bounded levels, deterministic experience rollover and monotonic skill revisions; stable `TaskId`/`QuestId` identity; player-owned tasks with bounded progress and terminal status rules; quests with deterministic task membership and completion validation; and ordered per-player/global snapshots. HAKUI performs actions, GUFF may reason about goals, and HOME only validates and persists the resulting progression truth.

## L18 — Subclass / Theorism / Aura

L18 adds deterministic canonical identity and metaphysical-state facts without letting HOME decide how those facts look, feel, sound or influence behavior. `TheorismLedger` stores one keyed subclass per player with bounded rank, one keyed theorism with bounded conviction, and one aura signature with bounded intensity and signed charge. Each channel owns monotonic per-player revisions, deterministic ordered snapshots, explicit clear operations and typed validation failures. HOME records the canonical facts only; HAKUI, INOKUI, ENTROKOI, XENON and GUFF remain responsible for embodiment, manifestation, perception, signal treatment and reasoning.

## L19 — HAKUI Adapter

L19 establishes the first explicit engine boundary. `HakuiAdapter` projects canonical HOME entities into a deterministic `HakuiWorldView` containing identity, archetype, canonical transform and zone placement, tagged with the HOME world/revision that produced it. HAKUI returns only typed physical consequences: transform changes, zone entry and zone exit. Every consequence carries its source HOME revision; stale consequences are rejected before mutation, unknown entities are rejected, and valid consequences flow back through `VersionedWorld` so HOME remains the only authority that commits persistent spatial truth. The adapter contains no physics, locomotion, animation or renderer logic.

## Build and test

```sh
cmake -S . -B build -DHOME_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Roadmap

L1 Foundation ✅ → L2 Entity Registry ✅ → L3 World Revision + Deltas ✅ → L4 Topology/Zones ✅ → L5 Transactions/Validation ✅ → L6 Snapshot/Save/Restore ✅ → L7 World Clock ✅ → L8 Calendar/Seasons ✅ → L9 Temporal Domains ✅ → L10 Events/Festivals/Holidays ✅ → L11 Weather/Climate ✅ → L12 World Affect/Theme Anchors ✅ → L13 Player Life State ✅ → L14 Needs/Mood/Autonomy ✅ → L15 Relationships/Households ✅ → L16 Items/Inventory ✅ → L17 Skills/Tasks/Quests ✅ → L18 Subclass Theorism/Aura ✅ → L19 HAKUI Adapter ✅ → L20 INOKUI Adapter → L21 ENTROKOI Adapter → L22 XENON Adapter → L23 GUFF HOME Cartridge → L24 Mission Bay World Package → L25 HOME v0.1 Vertical Slice.
