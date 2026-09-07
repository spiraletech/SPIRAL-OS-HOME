#include "home/versioned_world.hpp"
#include <cassert>

int main(){using namespace home;VersionedWorld w{WorldId{1}};
WorldTransaction tx{};tx.id=WorldTransactionId{1};tx.expected_revision=WorldRevision{};tx.authority="operator";
EntityCreateInfo e{};e.kind=EntityKind::Avatar;e.archetype="spiral.avatar";ZoneCreateInfo z{};z.kind=ZoneKind::District;z.key="mission-bay";z.display_name="Mission Bay";
tx.operations={TxCreateEntity{e},TxCreateZone{z}};auto r=w.execute(tx);assert(r.ok());assert(w.revision()==WorldRevision{1});assert(r.value().created_entities.size()==1);assert(r.value().created_zones.size()==1);assert(w.history().back().changes().size()==2);
const auto before_size=w.entities().size();WorldTransaction bad{};bad.id=WorldTransactionId{2};bad.expected_revision=w.revision();bad.authority="operator";EntityCreateInfo item{};item.kind=EntityKind::Item;item.archetype="item";bad.operations={TxCreateEntity{item},TxPlaceEntity{EntityId{999},ZoneId{1}}};auto fail=w.execute(bad);assert(!fail.ok());assert(w.entities().size()==before_size);assert(w.revision()==WorldRevision{1});
WorldTransaction stale{};stale.id=WorldTransactionId{3};stale.expected_revision=WorldRevision{};stale.authority="operator";stale.operations={TxCreateEntity{item}};auto conflict=w.execute(stale);assert(!conflict.ok());assert(conflict.error().code==ErrorCode::RevisionConflict);
WorldTransaction place{};place.id=WorldTransactionId{4};place.expected_revision=w.revision();place.authority="hakui";place.operations={TxPlaceEntity{EntityId{1},ZoneId{1}}};auto placed=w.execute(place);assert(placed.ok());assert(w.topology().zone_of(EntityId{1})==ZoneId{1});assert(w.revision()==WorldRevision{2});return 0;}
