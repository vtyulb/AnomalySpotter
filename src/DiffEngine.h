#pragma once

#include <QImage>
#include <QList>
#include <QRect>

namespace DiffEngine {

struct Analysis {
    double percent = 0.0;
    QImage mask;
    QList<QRect> regions;
};

double diffPercent(const QImage &a, const QImage &b, int threshold,
                   const QRect &exclude = QRect());
Analysis analyze(const QImage &current, const QImage &reference, int threshold,
                 const QRect &exclude = QRect());

}
