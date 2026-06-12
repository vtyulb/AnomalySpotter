#pragma once

#include <QImage>
#include <QMediaCaptureSession>
#include <QObject>
#include <QScreenCapture>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>

class ScreenCaptureService : public QObject {
    Q_OBJECT

public:
    explicit ScreenCaptureService(QObject *parent = nullptr);

    void start(QScreen *screen);
    void stop();
    bool isActive() const;
    QImage latestFrame() const;

signals:
    void activeChanged(bool active);
    void errorOccurred(const QString &message);

private:
    bool fakeMode() const;

    QScreenCapture capture_;
    QMediaCaptureSession session_;
    QVideoSink sink_;
    QVideoFrame latest_;
    QString fakeFramePath_;
    QTimer fakeTimer_;
    QImage fakeFrame_;
    bool fakeActive_ = false;
};
