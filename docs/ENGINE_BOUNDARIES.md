# Spiral Engine Boundaries v0.1

This document defines the initial jurisdiction boundaries between Spiral systems.

## HOME

HOME is the canonical world-state host/coordinator.

HOME owns or coordinates:

- world identity, epoch, tick, and commit ordering
- persistent entity lifecycle
- persistent placement
- topological membership and containment graph
- schema validation
- authority assignment records
- transaction commit
- persistence and snapshot publication
- replayable delta ordering

HOME does **not** own every simulation law.

## HAKUI

HAKUI is embodiment and interaction authority for assigned entities/components.

Typical HAKUI authority:

- locomotion
- embodied physical transform
- body state
- animation/pose result
- collisions and physical interaction outcomes
- gameplay interaction state when assigned

HAKUI does not define canonical world topology, persistence policy, material appearance semantics, or observer/lens semantics.

## INOKUI

INOKUI is manifestation/appearance authority for assigned components.

Typical INOKUI authority:

- material intent
- surface state
- skin/appearance state
- shading semantics
- mutation/deformation appearance semantics

GPU resources, shaders, texture handles, mesh caches, and render implementation details remain INOKUI-private and are not World State.

## ENTROKOI

ENTROKOI is perception/observer authority.

Typical ENTROKOI authority:

- observer identity
- observer transform when assigned
- lens/perception state
- view intent
- perception-layer semantics

Projection matrices, culling structures, frame history, post-processing buffers, and render targets remain ENTROKOI-private.

## XENON / Spiral AI

XENON is an intent and tool bridge, not a default world-state authority.

AI systems may:

- observe authorized world-state slices
- issue commands
- request actions
- subscribe to events/deltas

They may not directly mutate canonical World State unless explicitly assigned a domain authority through the normal authority model.

## Representation independence

A representation is not a separate reality.

HAKUI 3D, a 32-bit/demake client, a replay viewer, a server simulation, an editor, or a future accessibility client may subscribe to the same semantic world and render/interpret it differently.

Therefore:

> shared semantic reality is canonical; engine-specific representation is derived.

## No cross-engine memory ownership

World State must not contain engine-native pointers or handles.

Forbidden examples:

- raw/owned pointers into another engine
- renderer resource IDs used as persistent identity
- physics handles used as entity IDs
- scene-graph indices used as spatial identity
- compiler-specific object layouts used as wire representation

Cross-engine communication occurs through versioned semantic records, commands, events, deltas, snapshots, and adapters.
