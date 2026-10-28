#pragma once

#include <QByteArray>
#include <QSize>
#include <QtGlobal>
#include <array>
#include <vector>

// Stateful ink decisions, separate from the measured luminance. Keeping the
// wire map unchanged preserves v10 clients and diagnostics. Only this mask is
// sampled by the foreground shader: 0 = light ink, 255 = dark ink.
class BackdropInkPolicy
{
public:
    static constexpr int ConfirmMs = 110;
    static constexpr int QuietMs = 450;

    QByteArray update(const QSize &size, const QByteArray &brightness, qint64 now)
    {
        if (size.isEmpty() || size.width() > 64 || size.height() > 48
                || brightness.size() != qsizetype(size.width()) * size.height()
                || brightness.size() > 3072) {
            clear();
            return {};
        }
        const bool initialize = m_size != size;
        if (initialize) {
            clear();
            m_size = size;
            m_cells.resize(size_t(brightness.size()));
        }
        m_pending = false;
        QByteArray mask(brightness.size(), Qt::Uninitialized);
        for (qsizetype i = 0; i < brightness.size(); ++i) {
            auto &cell = m_cells[size_t(i)];
            const int value = uchar(brightness[i]);
            // Encoded brightness, not linear-light luminance. Bias the middle
            // greys towards dark ink; never mix white and black into grey ink.
            if (initialize) {
                cell.dark = value >= 117; // ~0.46
                cell.side = cell.dark ? 1 : 0;
            }
            const int side = value >= 128 ? 1 : value <= 107 ? 0 : -1;
            if (side >= 0 && side != cell.side) {
                cell.side = side;
                cell.crossings[0] = cell.crossings[1];
                cell.crossings[1] = cell.crossings[2];
                cell.crossings[2] = now;
                if (cell.crossings[0] >= 0 && now - cell.crossings[0] < 600)
                    cell.quietUntil = now + QuietMs;
            }
            if (side < 0 || (side == 1) == cell.dark) {
                cell.candidateSince = -1;
            } else {
                if (cell.candidateSince < 0) cell.candidateSince = now;
                if (now - cell.candidateSince >= ConfirmMs && now >= cell.quietUntil) {
                    cell.dark = side == 1;
                    cell.candidateSince = -1;
                } else {
                    m_pending = true;
                }
            }
            mask[i] = cell.dark ? char(255) : char(0);
        }
        return mask;
    }

    bool pending() const { return m_pending; }
    void clear() { m_size = {}; m_cells.clear(); m_pending = false; }

private:
    struct Cell {
        bool dark = false;
        int side = -1;
        qint64 candidateSince = -1;
        qint64 quietUntil = -1;
        std::array<qint64, 3> crossings{-1, -1, -1};
    };
    QSize m_size;
    std::vector<Cell> m_cells;
    bool m_pending = false;
};
