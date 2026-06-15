#include "HudOverlay.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#ifdef HAVE_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif

namespace {

constexpr double kNoMatchThreshold = 20.0;
constexpr int kBaseFontPointSize = 32;
constexpr int kBaseWidth = 300;
constexpr int kBaseHeight = 96;
constexpr int kAlarmBlinkIntervalMs = 350;

}

HudOverlay::HudOverlay()
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlag(Qt::WindowTransparentForInput);
    setFixedSize(kBaseWidth, kBaseHeight);

    alarmTimer_.setInterval(kAlarmBlinkIntervalMs);
    connect(&alarmTimer_, &QTimer::timeout, this, [this] {
        alarmVisible_ = !alarmVisible_;
        update();
    });
}

void HudOverlay::showPercent(double percent, QScreen *screen) {
    percent_ = percent;
    if (isVisible() && screen && screen != targetScreen_)
        hide();
    if (!isVisible()) {
        targetScreen_ = screen;
        winId();
        configurePlacement(screen);
        show();
    }
    update();
}

void HudOverlay::setScalePercent(int percent) {
    scale_ = qBound(50, percent, 200) / 100.0;
    setFixedSize(qRound(kBaseWidth * scale_), qRound(kBaseHeight * scale_));
    if (isVisible())
        configurePlacement(targetScreen_);
    update();
}

void HudOverlay::setRedThreshold(double threshold) {
    redThreshold_ = threshold;
    update();
}

void HudOverlay::setAlarmBlinking(bool on) {
    if (alarmBlinking_ == on)
        return;
    alarmBlinking_ = on;
    alarmVisible_ = true;
    if (on)
        alarmTimer_.start();
    else
        alarmTimer_.stop();
    update();
}

void HudOverlay::configurePlacement(QScreen *screen) {
#ifdef HAVE_LAYER_SHELL
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        if (auto *layerWindow = LayerShellQt::Window::get(windowHandle())) {
            layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
            layerWindow->setAnchors(LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorRight));
            layerWindow->setMargins(QMargins(0, 16, 16, 0));
            layerWindow->setKeyboardInteractivity(
                LayerShellQt::Window::KeyboardInteractivityNone);
            layerWindow->setActivateOnShow(false);
            layerWindow->setScreen(screen);
            return;
        }
    }
#endif
    if (screen) {
        windowHandle()->setScreen(screen);
        const QRect geometry = screen->geometry();
        move(geometry.right() - width() - 16, geometry.top() + 16);
    }
}

void HudOverlay::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont font = painter.font();
    font.setPointSizeF(kBaseFontPointSize * scale_);
    font.setBold(true);
    painter.setFont(font);

    if (alarmBlinking_) {
        if (!alarmVisible_)
            return;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(200, 0, 0, 235));
        painter.drawRoundedRect(rect(), 10, 10);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("! UP !"));
        return;
    }

    const bool noMatch = percent_ > kNoMatchThreshold;
    const bool alert = !noMatch && percent_ >= redThreshold_;
    painter.setPen(Qt::NoPen);
    painter.setBrush(noMatch ? QColor(60, 60, 60, 180) : QColor(0, 0, 0, 170));
    painter.drawRoundedRect(rect(), 10, 10);
    painter.setPen(noMatch ? QColor(190, 190, 190) : (alert ? QColor(255, 40, 40) : Qt::white));
    painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("%1 %").arg(percent_, 0, 'f', 2));
}
