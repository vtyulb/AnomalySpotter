#include "OverlayWindow.h"

#include <QFontMetrics>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <QScreen>
#include <QWindow>

#ifdef HAVE_LAYER_SHELL
#include <LayerShellQt/window.h>
#endif

OverlayWindow::OverlayWindow()
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlag(Qt::WindowTransparentForInput);
}

void OverlayWindow::showDiff(const QImage &mask, const QList<QRect> &regions, QScreen *screen,
                             double percent, const QString &referenceName,
                             const QString &hideHint) {
    mask_ = mask;
    regions_ = regions;
    percent_ = percent;
    referenceName_ = referenceName;
    hideHint_ = hideHint;
    highlightVisible_ = true;

    winId();
    bool layerShellConfigured = false;
#ifdef HAVE_LAYER_SHELL
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        if (auto *layerWindow = LayerShellQt::Window::get(windowHandle())) {
            layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
            layerWindow->setAnchors(LayerShellQt::Window::Anchors(
                LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
            layerWindow->setExclusiveZone(-1);
            layerWindow->setKeyboardInteractivity(
                LayerShellQt::Window::KeyboardInteractivityNone);
            layerWindow->setActivateOnShow(false);
            layerWindow->setScreen(screen);
            layerShellConfigured = true;
        }
    }
#endif
    if (screen) {
        windowHandle()->setScreen(screen);
        setGeometry(screen->geometry());
    }
    if (layerShellConfigured)
        show();
    else
        showFullScreen();
    update();
}

void OverlayWindow::setHighlightVisible(bool visible) {
    if (highlightVisible_ == visible)
        return;
    highlightVisible_ = visible;
    update();
}

void OverlayWindow::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    if (mask_.isNull())
        return;

    if (highlightVisible_) {
        painter.drawImage(rect(), mask_);

        const qreal scaleX = static_cast<qreal>(width()) / mask_.width();
        const qreal scaleY = static_cast<qreal>(height()) / mask_.height();
        painter.setPen(QPen(QColor(255, 70, 70), 3));
        painter.setBrush(Qt::NoBrush);
        for (const QRect &region : regions_) {
            painter.drawRect(QRectF(region.x() * scaleX, region.y() * scaleY,
                                    region.width() * scaleX, region.height() * scaleY));
        }
    }

    const QString banner = QStringLiteral("Compared to \"%1\": difference %2%  •  %3")
                               .arg(referenceName_)
                               .arg(percent_, 0, 'f', 2)
                               .arg(hideHint_);
    QFont bannerFont = painter.font();
    bannerFont.setPointSize(14);
    bannerFont.setBold(true);
    painter.setFont(bannerFont);

    QRect textRect = QFontMetrics(bannerFont).boundingRect(banner).adjusted(-16, -10, 16, 10);
    textRect.moveCenter(QPoint(width() / 2, textRect.height()));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 180));
    painter.drawRoundedRect(textRect, 8, 8);
    painter.setPen(Qt::white);
    painter.drawText(textRect, Qt::AlignCenter, banner);
}
