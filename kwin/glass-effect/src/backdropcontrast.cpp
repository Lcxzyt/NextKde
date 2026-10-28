#include "backdropcontrast.h"
#include <algorithm>
#include <cmath>

namespace KWin {

QSize BackdropContrastSampler::gridFor(const QSizeF &logicalSize)
{
    // About one cell per 8 logical px: a glyph stroke is not resolved, but a
    // background edge crossing a label is, and bilinear sampling on the client
    // turns the cell lattice into a continuous gradient.
    int columns = std::clamp(int(std::lround(logicalSize.width() / 8.0)), 1, MaxColumns);
    int rows = std::clamp(int(std::lround(logicalSize.height() / 8.0)), 1, MaxRows);
    while (columns * rows > MaxCells) {
        columns = std::max(1, columns * 7 / 8);
        rows = std::max(1, rows * 7 / 8);
    }
    return QSize(columns, rows);
}

std::vector<float> backdropCellsFromReadback(const QByteArray &pixels, const QSize &grid, bool bottomUp)
{
    const int width = grid.width() * 2, height = grid.height() * 2;
    std::vector<float> cells;
    if (grid.isEmpty() || pixels.size() != qsizetype(width) * height * 4) return cells;
    cells.resize(size_t(grid.width()) * grid.height());
    const auto *rgba = reinterpret_cast<const unsigned char *>(pixels.constData());
    const auto luma = [rgba, width](int x, int y) {
        const unsigned char *p = rgba + (qsizetype(y) * width + x) * 4;
        return (0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]) / 255.0;
    };
    // Each cell averages a 2x2 block of the supersampled blit.
    for (int row = 0; row < grid.height(); ++row) {
        const int y = bottomUp ? height - 2 - row * 2 : row * 2;
        for (int column = 0; column < grid.width(); ++column) {
            const int x = column * 2;
            cells[size_t(row) * grid.width() + column] = float(
                (luma(x, y) + luma(x + 1, y) + luma(x, y + 1) + luma(x + 1, y + 1)) / 4.0);
        }
    }
    return cells;
}

qreal backdropInnerMean(const std::vector<float> &cells, const QSize &grid)
{
    if (grid.isEmpty() || cells.size() != size_t(grid.width()) * grid.height()) return -1;
    // The central half, as v9 always sampled: it avoids rounded corners, rims
    // and neighbouring panels for the whole-panel decision.
    const int x0 = grid.width() / 4, x1 = std::max(x0 + 1, grid.width() - grid.width() / 4);
    const int y0 = grid.height() / 4, y1 = std::max(y0 + 1, grid.height() - grid.height() / 4);
    qreal total = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            total += cells[size_t(y) * grid.width() + x];
    return total / ((x1 - x0) * (y1 - y0));
}
}

#ifndef GLASS_X11
#include "core/rect.h"
#include "opengl/glframebuffer.h"
#include "opengl/gltexture.h"

namespace KWin {
BackdropContrastSampler::BackdropContrastSampler() { m_clock.start(); }
BackdropContrastSampler::~BackdropContrastSampler() = default;

void BackdropContrastSampler::enqueue(quint64 id, const QRectF &geometry,
    const QRect &sceneRect, const RenderTarget &target, const RenderViewport &viewport)
{
    const qint64 now = m_clock.elapsed();
    // Bounded memory even if an untrusted client continually creates shapes.
    auto it = m_samples.find(id);
    if (it == m_samples.end()) {
        std::erase_if(m_samples, [now](const auto &entry) {
            return !entry.second->readback.pending() && now - entry.second->lastSeen > 3000;
        });
        if (m_samples.size() >= 128) return;
        it = m_samples.emplace(id, std::make_unique<Sample>()).first;
    }
    auto &sample = *it->second;
    sample.lastSeen = now;
    sample.sceneRect = sceneRect;
    sample.dirty = true;
    if (sample.readback.pending() || now - sample.lastSample < sample.filter.interval(now)) return;
    if (sceneRect.isEmpty()) return;
    const QSize grid = gridFor(geometry.size());
    const QSize blitSize = grid * 2;
    // One small target per grid size, so the blit and the read both cover the
    // whole attachment and no sub-rectangle convention is involved.
    const quint32 key = quint32(blitSize.width()) << 16 | quint32(blitSize.height());
    auto target2 = m_targets.find(key);
    if (target2 == m_targets.end()) {
        if (m_targets.size() >= 16) m_targets.clear();
        Target created;
        created.texture = GLTexture::allocate(GL_RGBA8, blitSize);
        if (!created.texture) return;
        created.framebuffer = std::make_unique<GLFramebuffer>(created.texture.get());
        if (!created.framebuffer->valid()) return;
        target2 = m_targets.emplace(key, std::move(created)).first;
    }
    GLFramebuffer *framebuffer = target2->second.framebuffer.get();
    const QRect destination(QPoint(), blitSize);
    if (!framebuffer->blitFromRenderTarget(target, viewport, Rect(sceneRect), Rect(destination))) return;
    if (sample.readback.enqueue(framebuffer->handle(), blitSize, destination)) {
        sample.dirty = false;
        sample.lastSample = now;
        sample.geometry = geometry;
        sample.pendingGrid = grid;
    }
}

QVector<BackdropContrastSampler::Result> BackdropContrastSampler::poll()
{
    QVector<Result> result;
    for (auto it = m_samples.begin(); it != m_samples.end();) {
        auto &sample = *it->second;
        QByteArray pixels;
        const qint64 now = m_clock.elapsed();
        if (sample.readback.pending() && now - sample.lastSample > 500)
            sample.readback.discard();
        if (sample.readback.take(pixels)) {
            const std::vector<float> fresh = backdropCellsFromReadback(pixels, sample.pendingGrid);
            const qreal mean = backdropInnerMean(fresh, sample.pendingGrid);
            if (mean >= 0) {
                const qreal smoothed = sample.filter.update(mean, now);
                bool cellsSettling = false;
                // Cells share the scalar's time constant, so the map and the
                // whole-panel decision settle together.
                if (sample.grid != sample.pendingGrid || sample.cells.size() != fresh.size()
                        || sample.cellsUpdatedAt < 0) {
                    sample.cells = fresh;
                } else {
                    const qreal dt = std::clamp<qreal>(now - sample.cellsUpdatedAt, 0, 250);
                    const float k = float(1 - std::exp(-dt / 75.0));
                    for (size_t i = 0; i < fresh.size(); ++i) {
                        if (std::abs(fresh[i] - sample.cells[i]) > 0.025)
                            sample.filter.dynamicUntil = now + 400;
                        sample.cells[i] += (fresh[i] - sample.cells[i]) * k;
                        cellsSettling |= std::abs(fresh[i] - sample.cells[i]) > 0.015;
                    }
                }
                sample.grid = sample.pendingGrid;
                sample.cellsUpdatedAt = now;
                QByteArray map(qsizetype(sample.cells.size()), Qt::Uninitialized);
                for (size_t i = 0; i < sample.cells.size(); ++i)
                    map[qsizetype(i)] = char(std::clamp(int(std::lround(sample.cells[i] * 255.0)), 0, 255));
                result.append({it->first, sample.geometry, smoothed, {}, sample.grid, map});
                // Finish smoothing a one-off change even when the background
                // stops painting immediately after the transition.
                if (sample.filter.settling() || cellsSettling) {
                    sample.filter.dynamicUntil = now + 400;
                    sample.dirty = true;
                }
            }
        }
        if (sample.dirty && !sample.readback.pending()
                && now - sample.lastSample >= sample.filter.interval(now)) {
            result.append({0, {}, -1, sample.sceneRect, {}, {}});
            sample.dirty = false;
        }
        if (!sample.readback.pending() && now - sample.lastSeen > 3000)
            it = m_samples.erase(it);
        else ++it;
    }
    return result;
}

bool BackdropContrastSampler::pending() const
{
    return std::any_of(m_samples.begin(), m_samples.end(),
        [](const auto &entry) { return entry.second->readback.pending() || entry.second->dirty; });
}
}

#endif
