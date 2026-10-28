#pragma once

#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace KWin {
// Time-based filtering, independent of the repaint/frame rate. Static panels
// keep the low readback rate; observed motion temporarily enables 20 Hz.
struct GlassContrastFilter {
    qreal value = -1;
    qreal raw = -1;
    qint64 updatedAt = -1;
    qint64 dynamicUntil = -1;

    int interval(qint64 now) const { return now < dynamicUntil ? 50 : 150; }
    bool settling() const { return raw >= 0 && std::abs(value - raw) > 0.015; }
    qreal update(qreal sample, qint64 now) {
        sample = std::clamp(sample, 0.0, 1.0);
        if (updatedAt < 0) value = sample;
        else {
            if (std::abs(sample - raw) > 0.025) dynamicUntil = now + 400;
            const qreal dt = std::clamp<qreal>(now - updatedAt, 0, 250);
            value += (sample - value) * (1 - std::exp(-dt / 75.0));
        }
        raw = sample;
        updatedAt = now;
        if (settling()) dynamicUntil = now + 400;
        return value;
    }
};
}
