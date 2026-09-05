# Spiral World State Contract v0.1

Status: **semantic contract draft for freeze**

This document defines the canonical semantics shared by Spiral engines. It is **not a C++ ABI**. Binary layout, compiler ABI, STL container layout, padding, alignment, and in-process calling conventions are explicitly out of scope.

## Core law

> Snapshot + ordered committed deltas = current reality.

A `WorldSnapshot` is a point-in-time semantic description of reality. A `WorldDelta` is an ordered, authority-authored change committed by the HOME world-state host. Consumers reconstruct current state by applying committed deltas in commit order to a compatible snapshot.

## Roles

- **Domain authority** decides truth inside its assigned domain.
- **HOME** validates, orders, commits, persists, and publishes domain-authored truth.
- **Consumers** subscribe to snapshots, deltas, and events and interpret them through engine-specific adapters.
- **XENON / AI clients** issue commands or intents; they do not directly mutate canonical state.

HOME being the committer does **not** mean HOME authored a value.

## Canonical envelope

The wire/schema representation of a world snapshot contains:

- `schema_version`
- `world_id`
- `epoch`
- `tick`
- `time`
- `seed`
- `coordinate_frame`
- `authority_assignments`
- `spatial_graph`
- `entities`

Entities are component-native:

```text
EntityRecord
  id
  kind
  lifecycle
  components[]
```

No component is structurally mandatory. A faction, market, radio station, quest, weather system, song, account, or remote AI presence may exist without a transform.

## Component model

Each component is independently versioned and revisioned.

```text
ComponentRecord
  type
  schema_version
  revision
  payload
```

The world-state contract defines semantic component names and ownership rules, while engine-native C++ representations remain adapter-local and may evolve.

## Commands, deltas, events, transactions

Canonical causality is:

```text
Command
  -> authority validation/simulation
  -> Event(s) and/or Delta(s)
  -> Transaction
  -> HOME commit
  -> published WorldDelta / WorldEvent
```

Every committed delta carries:

- `delta_id`
- `command_id` when caused by a command
- `transaction_id`
- `authority_id`
- `base_tick`
- `commit_tick`
- `sequence`
- one or more component operations

Every event may carry the same causality identifiers when applicable.

`CommandId`, `DeltaId`, `EventId`, and `TransactionId` are stable identifiers and must survive logging, replay, networking, save/load, and audit export.

## Delta versus event

A **delta** changes canonical persistent or current state.

An **event** records that something happened and may be ephemeral.

Example: a door interaction may emit `DoorOpened` as an event and `DoorState.open = true` as a delta. The event can expire while the state remains open.

## Commit ordering

Within one world epoch, HOME publishes a total commit order using `(commit_tick, sequence)`.

A delta must identify the `base_tick` it was evaluated against. HOME may reject, rebase through domain-specific reconciliation, or defer a stale proposal; it must not silently reorder authored causality.

Transactions group related deltas/events that should be observed as one logical commit boundary.

## Authority and transforms

Transform semantics are split by domain instead of assigning all transforms to one engine:

- entity lifecycle and persistent placement: HOME/domain placement authority
- embodied kinematic/physical transform: HAKUI
- visual pose / animation result: HAKUI
- topological membership: HOME
- observer transform: ENTROKOI or explicitly assigned observer authority

HAKUI may therefore author a locomotion transform delta. HOME validates and commits that delta into canonical state without claiming authorship.

## Spatial topology

HOME uses a canonical spatial vocabulary:

`world`, `region`, `district`, `parcel`, `structure`, `room`, `volume`, `anchor`

The underlying representation is a containment/address graph, not a rigid mandatory chain. Intermediate levels may be absent.

Valid examples include:

```text
World -> Region -> District -> Parcel -> Structure -> Room -> Volume -> Anchor
World -> Region -> Volume -> Anchor
World -> Volume
World -> Region -> Structure -> Room
```

Custom node kinds may be added through schema-versioned extensions.

No consumer may manufacture fake intermediate nodes merely to satisfy a terrestrial hierarchy.

## Identity rules

Canonical world identity must never depend on:

- pointer addresses
- array indices
- render handles
- physics-body handles
- transient scene-graph node IDs

Persistent world IDs are stable across process boundaries and serialization.

## Coordinate contract

The canonical coordinate frame declares at minimum:

- linear unit
- handedness
- up axis
- origin identifier
- origin-rebase generation

No engine-native coordinate convention is silently canonical. Adapters translate engine-native transforms to and from the declared world frame.

## Non-shared implementation state

The contract does not expose renderer resources, GPU handles, physics broadphase structures, animation caches, AI hidden state, post-process buffers, inference context, persistence backends, or other engine-private implementation data.

Only semantic state crosses engine boundaries.

## Compatibility rule

Semantic compatibility is controlled through explicit schema versions. Readers must reject incompatible major versions and may tolerate additive compatible fields according to the schema versioning policy.

A future C/C++ ABI may be defined separately if required, but it must not retroactively redefine this protocol contract.
