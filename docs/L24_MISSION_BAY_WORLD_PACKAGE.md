# L24 — Mission Bay World Package

L24 adds HOME's first deterministic packaged world without moving presentation or behavior authority into HOME.

`make_mission_bay_world(WorldId)` builds a canonical Mission Bay starter topology with stable creation order and snapshot identity:

- Mission Bay — region root
- Crown Point — district under Mission Bay
- Mission Beach — district under Mission Bay
- Belmont Park — parcel under Mission Beach
- Mission Bay Waters — water zone under Mission Bay

Canonical traversal edges connect Crown Point ↔ Mission Beach, Mission Beach ↔ Belmont Park, Crown Point ↔ Mission Bay Waters and Mission Beach ↔ Mission Bay Waters. The package owns only canonical zones and connectivity; HAKUI owns physical traversal, INOKUI owns visual manifestation, ENTROKOI owns perception, XENON owns acoustic/signal manifestation and GUFF owns governed reasoning.

The L24 acceptance test verifies invalid-world rejection, deterministic zone identity/hierarchy, traversal topology, exact revision count, snapshot restore equivalence and repeat-build snapshot determinism.
