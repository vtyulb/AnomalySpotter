#include "ScreenCaptureService.h"

ScreenCaptureService::ScreenCaptureService(QObject *parent) : QObject(parent) {
    fakeFramePath_ = qEnvironmentVariable("AS_FAKE_CAPTURE_FILE");
    if (fakeMode()) {
        fakeTimer_.setInterval(50);
        connect(&fakeTimer_, &QTimer::timeout, this, [this] {
            const QImage frame(fakeFramePath_);
            if (!frame.isNull())
                fakeFrame_ = frame.convertToFormat(QImage::Format_RGB32);
        });
        return;
    }

    session_.setScreenCapture(&capture_);
    session_.setVideoSink(&sink_);

    connect(&sink_, &QVideoSink::videoFrameChanged, this,
            [this](const QVideoFrame &frame) { latest_ = frame; });
    connect(&capture_, &QScreenCapture::activeChanged, this, &ScreenCaptureService::activeChanged);
    connect(&capture_, &QScreenCapture::errorOccurred, this,
            [this](QScreenCapture::Error, const QString &message) { emit errorOccurred(message); });
}

bool ScreenCaptureService::fakeMode() const {
    return !fakeFramePath_.isEmpty();
}

void ScreenCaptureService::start(QScreen *screen) {
    if (fakeMode()) {
        fakeActive_ = true;
        fakeTimer_.start();
        emit activeChanged(true);
        return;
    }
    capture_.setScreen(screen);
    capture_.start();
}

void ScreenCaptureService::stop() {
    if (fakeMode()) {
        fakeActive_ = false;
        fakeTimer_.stop();
        fakeFrame_ = {};
        emit activeChanged(false);
        return;
    }
    capture_.stop();
    latest_ = {};
}

bool ScreenCaptureService::isActive() const {
    if (fakeMode())
        return fakeActive_;
    return capture_.isActive();
}

QImage ScreenCaptureService::latestFrame() const {
    if (fakeMode())
        return fakeFrame_;
    if (!latest_.isValid())
        return {};
    return latest_.toImage().convertToFormat(QImage::Format_RGB32);
}
