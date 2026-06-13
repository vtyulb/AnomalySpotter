#include "HudOverlay.h"

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QWindow>

#ifdef HAVE_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif

namespace {

constexpr double kAlertThreshold = 0.5;
constexpr double kBlinkThreshold = 2.0;
constexpr double kNoMatchThreshold = 20.0;
constexpr int kBlinkIntervalMs = 100;
constexpr int kBlinkDelayMs = 1000;
constexpr int kBaseFontPointSize = 32;
constexpr int kBaseWidth = 300;
constexpr int kBaseHeight = 96;

}

HudOverlay::HudOverlay()
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlag(Qt::WindowTransparentForInput);
    setFixedSize(kBaseWidth, kBaseHeight);

    blinkTimer_.setInterval(kBlinkIntervalMs);
    connect(&blinkTimer_, &QTimer::timeout, this, [this] {
        blinkVisible_ = !blinkVisible_;
        update();
    });

    blinkDelayTimer_.setSingleShot(true);
    blinkDelayTimer_.setInterval(kBlinkDelayMs);
    connect(&blinkDelayTimer_, &QTimer::timeout, this, [this] {
        blinkActive_ = true;
        blinkVisible_ = true;
        blinkTimer_.start();
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
    updateBlinkState();
    update();
}

void HudOverlay::setScalePercent(int percent) {
    scale_ = qBound(50, percent, 200) / 100.0;
    setFixedSize(qRound(kBaseWidth * scale_), qRound(kBaseHeight * scale_));
    if (isVisible())
        configurePlacement(targetScreen_);
    update();
}

void HudOverlay::updateBlinkState() {
    if (percent_ >= kBlinkThreshold && percent_ <= kNoMatchThreshold) {
        if (!blinkActive_ && !blinkDelayTimer_.isActive())
            blinkDelayTimer_.start();
    } else {
        blinkDelayTimer_.stop();
        blinkTimer_.stop();
        blinkActive_ = false;
        blinkVisible_ = true;
    }
}

void HudOverlay::hideEvent(QHideEvent *) {
    blinkDelayTimer_.stop();
    blinkTimer_.stop();
    blinkActive_ = false;
    blinkVisible_ = true;
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
    const bool noMatch = percent_ > kNoMatchThreshold;
    const bool alert = !noMatch && percent_ >= kAlertThreshold;
    const bool blinking = blinkActive_;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    if (noMatch)
        painter.setBrush(QColor(60, 60, 60, 180));
    else
        painter.setBrush(blinking && blinkVisible_ ? QColor(120, 0, 0, 210)
                                                   : QColor(0, 0, 0, 170));
    painter.drawRoundedRect(rect(), 10, 10);

    if (blinking && !blinkVisible_)
        return;

    QFont font = painter.font();
    font.setPointSizeF(kBaseFontPointSize * scale_);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(noMatch ? QColor(190, 190, 190) : (alert ? QColor(255, 40, 40) : Qt::white));
    painter.drawText(rect(), Qt::AlignCenter,
                     QStringLiteral("%1 %").arg(percent_, 0, 'f', 2));
}
