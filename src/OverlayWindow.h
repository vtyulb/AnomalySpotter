#pragma once

#include <QImage>
#include <QList>
#include <QRect>
#include <QWidget>

class OverlayWindow : public QWidget {
    Q_OBJECT

public:
    OverlayWindow();

    void showDiff(const QImage &mask, const QList<QRect> &regions, QScreen *screen,
                  double percent, const QString &referenceName, const QString &hideHint);
    void setHighlightVisible(bool visible);
    bool highlightVisible() const { return highlightVisible_; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage mask_;
    QList<QRect> regions_;
    double percent_ = 0.0;
    QString referenceName_;
    QString hideHint_;
    bool highlightVisible_ = true;
};
