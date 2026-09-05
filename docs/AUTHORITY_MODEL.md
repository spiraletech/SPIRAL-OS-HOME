# Spiral Authority Model v0.1

## Prime invariant

> Authority decides truth inside a domain. HOME validates, orders, persists, and publishes that truth.

HOME is the **World State host/coordinator**. It is not the universal simulation authority.

## Authority versus commit authority

These are separate concepts:

- **Domain authority** authors the semantic value.
- **Commit authority (HOME)** validates the proposal against world invariants, assigns canonical commit ordering, persists it, and publishes it.

The canonical record therefore preserves both authorship and commit metadata.

## Initial jurisdiction map

| Domain | Initial authority | HOME role |
|---|---|---|
| World identity / epoch / tick | HOME | author + commit |
| Entity lifecycle | HOME | author + commit |
| Persistent placement / spawn placement | HOME | author + commit |
| Topological membership / containment | HOME | author + commit |
| Embodied kinematic / physical transform | HAKUI | validate + commit |
| Body state / locomotion | HAKUI | validate + commit |
| Visual pose / animation result | HAKUI | validate + commit |
| Interaction / gameplay result | HAKUI unless reassigned | validate + commit |
| Material / surface / appearance intent | INOKUI | validate + commit |
| Observer / lens state | ENTROKOI | validate + commit |
| Observer transform | ENTROKOI or designated observer authority | validate + commit |
| AI intent / requests | XENON command source, not state authority | route only |

Authority assignments are data and may change by world/session/entity/component when explicitly negotiated. They are not assumed from process ownership alone.

## Transform jurisdiction

`Transform` is not globally owned by HOME or HAKUI. Authority depends on semantic role.

Examples:

1. HOME spawns a chair at a persistent placement.
2. HAKUI simulates a player pushing it and authors a physical transform delta while the object is under HAKUI physical authority.
3. HOME commits the authored transform.
4. If the object returns to static persistence authority, HOME records its final persistent placement.

An observer is different: ENTROKOI may author observer transform without becoming authority over the observed entity's physical transform.

## Mutation rule

No engine directly mutates another engine's authoritative component.

Instead:

```text
Requester
  -> Command
  -> Domain Authority
  -> validation / simulation
  -> proposed Event(s) + Delta(s)
  -> HOME validation
  -> Transaction commit
  -> publication
```

HOME may reject proposals that violate:

- schema constraints
- authority assignment
- revision preconditions
- stale base tick policy
- entity lifecycle rules
- world identity or epoch rules
- transaction invariants

HOME must not silently invent a replacement domain value merely because a proposal failed.

## Authority transfer

Authority transfer is explicit and itself auditable. A transfer record should identify:

- domain/component scope
- previous authority
- new authority
- effective tick
- transaction ID
- cause/command ID when applicable

This allows future networking, server migration, replay, editor possession, or subsystem handoff without changing the core contract.

## Causality

Canonical causal chain:

```text
CommandId
   -> simulation / decision by AuthorityId
   -> EventId(s) / DeltaId(s)
   -> TransactionId
   -> HOME commit order
```

A committed record should permit a debugger or AI observer to answer:

- What changed?
- Who authored the change?
- Which command caused it?
- Which transaction committed it?
- At which base tick was it evaluated?
- At which canonical tick/sequence was it published?

This is required infrastructure for deterministic replay, audit, reconciliation, and explainable world observation.
