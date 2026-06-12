#pragma once

#include <QTimer>
#include <QWidget>

class HudOverlay : public QWidget {
    Q_OBJECT

public:
    HudOverlay();

    void showPercent(double percent, QScreen *screen);

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void configurePlacement(QScreen *screen);
    void updateBlinkState();

    double percent_ = 0.0;
    QScreen *targetScreen_ = nullptr;
    QTimer blinkTimer_;
    QTimer blinkDelayTimer_;
    bool blinkActive_ = false;
    bool blinkVisible_ = true;
};
