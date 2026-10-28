#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../kwin/glass-effect/src/contrastfilter.h"
#include <cassert>
#include <cmath>
int main() {
    KWin::GlassContrastFilter filter;
    assert(filter.interval(0) == 150);
    assert(filter.update(0, 0) == 0);
    const auto first = filter.update(1, 50);
    assert(first > 0.4 && first < 0.55);
    assert(filter.interval(50) == 50);
    const auto second = filter.update(1, 100);
    assert(second > 0.7 && second < 1);
    for (int now = 150; now <= 500; now += 50) filter.update(1, now);
    assert(std::abs(filter.value - 1) < 0.01);
    assert(!filter.settling());
    assert(filter.interval(1500) == 150);
    // Equivalent elapsed time gives the same result across repaint rates.
    KWin::GlassContrastFilter highRate, lowRate;
    highRate.update(0, 0); lowRate.update(0, 0);
    for (int now = 10; now <= 100; now += 10) highRate.update(1, now);
    lowRate.update(1, 100);
    assert(std::abs(highRate.value - lowRate.value) < 0.00001);
    // Repeated same-timestamp feedback cannot jump the filter or divide by zero.
    const auto previous = highRate.value;
    assert(highRate.update(0, 100) == previous);
    assert(highRate.update(2, 100) == previous);
    return 0;
}
