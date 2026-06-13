#pragma once

#include <QColor>
#include <QImage>
#include <QList>
#include <QRect>
#include <QRegion>

namespace DiffEngine {

struct Analysis {
    double percent = 0.0;
    QImage mask;
    QList<QRect> regions;
};

double diffPercent(const QImage &a, const QImage &b, int threshold,
                   const QRegion &exclude = QRegion());
Analysis analyze(const QImage &current, const QImage &reference, int threshold,
                 const QRegion &exclude = QRegion(),
                 const QColor &highlightColor = QColor(255, 40, 40));

}
