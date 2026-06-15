#pragma once

#include <QTimer>
#include <QWidget>

class HudOverlay : public QWidget {
    Q_OBJECT

public:
    HudOverlay();

    void showPercent(double percent, QScreen *screen);
    void setScalePercent(int percent);
    void setRedThreshold(double threshold);
    void setAlarmBlinking(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void configurePlacement(QScreen *screen);

    double percent_ = 0.0;
    double scale_ = 1.0;
    double redThreshold_ = 0.02;
    QScreen *targetScreen_ = nullptr;
    QTimer alarmTimer_;
    bool alarmBlinking_ = false;
    bool alarmVisible_ = true;
};
