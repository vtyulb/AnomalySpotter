#include "DiffEngine.h"

#include <QPoint>
#include <QVector>

namespace {

constexpr int kCellSize = 32;
constexpr double kHotCellShare = 0.02;

bool pixelDiffers(QRgb a, QRgb b, int threshold) {
    return qAbs(qRed(a) - qRed(b)) > threshold
        || qAbs(qGreen(a) - qGreen(b)) > threshold
        || qAbs(qBlue(a) - qBlue(b)) > threshold;
}

qint64 regionArea(const QRegion &region) {
    qint64 area = 0;
    for (const QRect &rect : region)
        area += static_cast<qint64>(rect.width()) * rect.height();
    return area;
}

}

namespace DiffEngine {

double diffPercent(const QImage &a, const QImage &b, int threshold, const QRegion &exclude) {
    if (a.isNull() || a.size() != b.size())
        return 100.0;
    const QRegion excluded = exclude.intersected(a.rect());
    qint64 differing = 0;
    for (int y = 0; y < a.height(); ++y) {
        const QRgb *rowA = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const QRgb *rowB = reinterpret_cast<const QRgb *>(b.constScanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            if (excluded.contains(QPoint(x, y)))
                continue;
            if (pixelDiffers(rowA[x], rowB[x], threshold))
                ++differing;
        }
    }
    const qint64 total = static_cast<qint64>(a.width()) * a.height() - regionArea(excluded);
    return total > 0 ? 100.0 * differing / total : 0.0;
}

Analysis analyze(const QImage &current, const QImage &reference, int threshold,
                 const QRegion &exclude, const QColor &highlightColor) {
    Analysis analysis;
    analysis.mask = QImage(current.size(), QImage::Format_ARGB32);
    analysis.mask.fill(Qt::transparent);
    if (current.isNull() || current.size() != reference.size()) {
        analysis.percent = 100.0;
        return analysis;
    }

    const QRegion excluded = exclude.intersected(current.rect());
    const int cellColumns = (current.width() + kCellSize - 1) / kCellSize;
    const int cellRows = (current.height() + kCellSize - 1) / kCellSize;
    QVector<int> cellCounts(cellColumns * cellRows, 0);
    const QRgb highlight =
        qRgba(highlightColor.red(), highlightColor.green(), highlightColor.blue(), 160);

    qint64 differing = 0;
    for (int y = 0; y < current.height(); ++y) {
        const QRgb *rowCurrent = reinterpret_cast<const QRgb *>(current.constScanLine(y));
        const QRgb *rowReference = reinterpret_cast<const QRgb *>(reference.constScanLine(y));
        QRgb *rowMask = reinterpret_cast<QRgb *>(analysis.mask.scanLine(y));
        for (int x = 0; x < current.width(); ++x) {
            if (excluded.contains(QPoint(x, y)))
                continue;
            if (pixelDiffers(rowCurrent[x], rowReference[x], threshold)) {
                ++differing;
                rowMask[x] = highlight;
                ++cellCounts[(y / kCellSize) * cellColumns + x / kCellSize];
            }
        }
    }
    const qint64 total =
        static_cast<qint64>(current.width()) * current.height() - regionArea(excluded);
    analysis.percent = total > 0 ? 100.0 * differing / total : 0.0;

    const int hotThreshold = static_cast<int>(kCellSize * kCellSize * kHotCellShare);
    QVector<bool> visited(cellColumns * cellRows, false);
    const QPoint neighborOffsets[] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int row = 0; row < cellRows; ++row) {
        for (int column = 0; column < cellColumns; ++column) {
            const int index = row * cellColumns + column;
            if (visited[index] || cellCounts[index] <= hotThreshold)
                continue;
            visited[index] = true;
            int minRow = row, maxRow = row, minColumn = column, maxColumn = column;
            QList<QPoint> queue{QPoint(column, row)};
            while (!queue.isEmpty()) {
                const QPoint cell = queue.takeLast();
                minRow = qMin(minRow, cell.y());
                maxRow = qMax(maxRow, cell.y());
                minColumn = qMin(minColumn, cell.x());
                maxColumn = qMax(maxColumn, cell.x());
                for (const QPoint &offset : neighborOffsets) {
                    const QPoint next = cell + offset;
                    if (next.x() < 0 || next.y() < 0 || next.x() >= cellColumns
                        || next.y() >= cellRows)
                        continue;
                    const int nextIndex = next.y() * cellColumns + next.x();
                    if (visited[nextIndex] || cellCounts[nextIndex] <= hotThreshold)
                        continue;
                    visited[nextIndex] = true;
                    queue.append(next);
                }
            }
            const QRect region(minColumn * kCellSize, minRow * kCellSize,
                               (maxColumn - minColumn + 1) * kCellSize,
                               (maxRow - minRow + 1) * kCellSize);
            analysis.regions.append(region.intersected(current.rect()));
        }
    }
    return analysis;
}

}
