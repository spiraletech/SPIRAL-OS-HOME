# L24 — Mission Bay World Package

L24 packages the deterministic HOME truth used by the v0.1 Mission Bay slice. It does not orchestrate HAKUI, INOKUI, ENTROKOI, XENON, or GUFF; that integration belongs to L25.

Canonical playable spine:

`Superbloom East Edge [LOCKED] -> Tecolote -> Mission Bay Golf -> Mike Gotch Bridge -> Campland -> Kendall-Frost Marsh -> Crown Point -> Fanuel -> Catamaran -> Pacific Beach -> Mission Boulevard -> Mission Beach -> Belmont Park -> Belmont Broken Coaster Edge [LOCKED]`

Pacific Beach branches to Garnet Avenue and Crystal Pier Hotel. Crown Point, Catamaran, and Mission Beach connect to Mission Bay Waters. Fiesta Island exists as canonical topology but its water approach is locked for v0.1.

Locked boundaries are stored as non-traversable `ZoneConnection` records with semantic tags. Superbloom and Belmont also have persistent boundary `Structure` entities so downstream engines can manifest diegetic barriers without redefining where traversal ends.

The package seeds six deterministic climate/weather states and a canonical `halloween` holiday definition carrying `calendar.halloween` and `theme.haunted` affinities. Snapshot format remains v12.
