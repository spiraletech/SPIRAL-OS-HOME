#include "home/temporal_domain.hpp"

#include <cassert>

int main() {
    using namespace home;
    const auto slow = make_temporal_domain(TimeDomainId{1}, "dream", WorldTime{1000}, WorldTime{5000}, 1, 2, false);
    assert(slow.ok());
    const auto t = project_domain_time(slow.value(), WorldTime{5000});
    assert(t.ok());
    assert(t.value().milliseconds == 7000);

    const auto fast = make_temporal_domain(TimeDomainId{2}, "scene", WorldTime{0}, WorldTime{0}, 3, 2, false);
    assert(fast.ok());
    const auto f = project_domain_time(fast.value(), WorldTime{4000});
    assert(f.ok());
    assert(f.value().milliseconds == 6000);

    const auto paused = rebase_temporal_domain(fast.value(), WorldTime{4000}, true);
    assert(paused.ok());
    const auto frozen = project_domain_time(paused.value(), WorldTime{999999});
    assert(frozen.ok());
    assert(frozen.value().milliseconds == 6000);

    const auto resumed = rebase_temporal_domain(paused.value(), WorldTime{10000}, false);
    assert(resumed.ok());
    const auto resumed_time = project_domain_time(resumed.value(), WorldTime{12000});
    assert(resumed_time.ok());
    assert(resumed_time.value().milliseconds == 9000);

    const auto invalid = make_temporal_domain(TimeDomainId{}, "", WorldTime{}, WorldTime{}, 1, 1, false);
    assert(!invalid.ok());
    return 0;
}
