#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../kwin/glass-effect/src/backdropcontrast.h"
#include <cassert>
#include <cmath>

// Fill a (grid * 2) RGBA readback from a top-down brightness function.
static QByteArray readback(const QSize &grid, bool bottomUp, float (*luma)(int x, int y, int w, int h))
{
    const int w = grid.width() * 2, h = grid.height() * 2;
    QByteArray pixels(w * h * 4, 0);
    for (int row = 0; row < h; ++row) {
        const int y = bottomUp ? h - 1 - row : row;
        for (int x = 0; x < w; ++x) {
            const auto v = char(std::lround(luma(x, y, w, h) * 255));
            char *p = pixels.data() + (row * w + x) * 4;
            p[0] = p[1] = p[2] = v; p[3] = char(255);
        }
    }
    return pixels;
}

int main()
{
    // Grid: ~8 logical px per cell, bounded by the protocol's cell limit.
    assert(KWin::BackdropContrastSampler::gridFor(QSizeF(296, 57)) == QSize(37, 7));
    assert(KWin::BackdropContrastSampler::gridFor(QSizeF(2, 2)) == QSize(1, 1));
    const QSize huge = KWin::BackdropContrastSampler::gridFor(QSizeF(4000, 4000));
    assert(huge.width() <= 64 && huge.height() <= 48 && huge.width() * huge.height() <= 3072);

    // Left half white, right half black; top half white, bottom half black.
    const QSize grid(8, 4);
    for (bool bottomUp : {true, false}) {
        auto cells = KWin::backdropCellsFromReadback(
            readback(grid, bottomUp, [](int x, int, int w, int) { return x < w / 2 ? 1.f : 0.f; }), grid, bottomUp);
        assert(cells.size() == 32);
        assert(cells[0] > 0.99f && cells[7] < 0.01f && cells[24] > 0.99f && cells[31] < 0.01f);
        cells = KWin::backdropCellsFromReadback(
            readback(grid, bottomUp, [](int, int y, int, int h) { return y < h / 2 ? 1.f : 0.f; }), grid, bottomUp);
        // Row 0 is the top in either readback order.
        assert(cells[0] > 0.99f && cells[31] < 0.01f);
        // The scalar is the central half, matching the v9 single sample.
        assert(std::abs(KWin::backdropInnerMean(cells, grid) - 0.5) < 0.01);
    }
    // Malformed input never indexes out of bounds.
    assert(KWin::backdropCellsFromReadback(QByteArray(10, 0), grid).empty());
    assert(KWin::backdropInnerMean({}, grid) < 0);
    return 0;
}
