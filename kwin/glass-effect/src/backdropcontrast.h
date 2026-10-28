#pragma once

#ifndef GLASS_X11
#include "../../kos-bridge/windowbuttons/titlebarreadback.h"
#endif
#include "contrastfilter.h"
#include <QByteArray>
#include <QElapsedTimer>
#include <QRect>
#include <QSize>
#include <QVector>
#include <memory>
#include <unordered_map>
#include <vector>

namespace KWin {
class GLFramebuffer;
class GLTexture;
class RenderTarget;
class RenderViewport;

// Samples only a client's declared glass, after its material and before its
// foreground. The shared readback primitive uses fenced PBOs, never GPU waits.
//
// Each sample is a coarse luminance map of the whole shape (one cell per ~8
// logical px, rows top to bottom) plus the scalar of its central half, which
// keeps the v9 whole-panel feedback unchanged.
class BackdropContrastSampler {
public:
    struct Result {
        quint64 id;
        QRectF geometry;
        qreal luminance;
        QRect repaint;
        QSize mapSize;
        QByteArray map;
    };
    // Wayland caps a message at 4096 bytes; the map event stays below it.
    static constexpr int MaxColumns = 64;
    static constexpr int MaxRows = 48;
    static constexpr int MaxCells = 3072;
    static QSize gridFor(const QSizeF &logicalSize);
    BackdropContrastSampler();
    ~BackdropContrastSampler();
    void enqueue(quint64 id, const QRectF &geometry, const QRect &sceneRect,
                 const RenderTarget &target, const RenderViewport &viewport);
    QVector<Result> poll();
    bool pending() const;
private:
#ifndef GLASS_X11
    // Pure map tests and the X11 build do not need GL readback declarations.
    struct Sample {
        KOS::TitlebarReadback readback;
        GlassContrastFilter filter;
        QRectF geometry;
        QRect sceneRect;
        QSize grid;
        QSize pendingGrid;
        std::vector<float> cells;
        qint64 cellsUpdatedAt = -1;
        bool dirty = false;
        qint64 lastSample = -1000;
        qint64 lastSeen = 0;
    };
    struct Target {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> framebuffer;
    };
    QElapsedTimer m_clock;
    std::unordered_map<quint32, Target> m_targets;
    std::unordered_map<quint64, std::unique_ptr<Sample>> m_samples;
#endif
};

// Pure helpers, shared with the logic test. `pixels` is RGBA8 of a
// (grid * 2) readback; `bottomUp` says whether its first row is the image's
// bottom (GL order). Cells are always stored top row first.
std::vector<float> backdropCellsFromReadback(const QByteArray &pixels, const QSize &grid, bool bottomUp = true);
qreal backdropInnerMean(const std::vector<float> &cells, const QSize &grid);
}
