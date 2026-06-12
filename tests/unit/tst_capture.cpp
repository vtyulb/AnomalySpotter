#include "ScreenCaptureService.h"

#include <QTemporaryDir>
#include <QtTest>

class TestScreenCaptureService : public QObject {
    Q_OBJECT

private slots:
    void fakeModeServesFramesFromFile();
    void stopClearsFrameAndDeactivates();
};

void TestScreenCaptureService::fakeModeServesFramesFromFile() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString framePath = dir.filePath(QStringLiteral("frame.png"));
    QImage frame(64, 48, QImage::Format_RGB32);
    frame.fill(qRgb(200, 30, 30));
    QVERIFY(frame.save(framePath));

    qputenv("AS_FAKE_CAPTURE_FILE", framePath.toUtf8());
    ScreenCaptureService service;
    QVERIFY(!service.isActive());
    QVERIFY(service.latestFrame().isNull());

    service.start(nullptr);
    QVERIFY(service.isActive());
    QTRY_VERIFY(!service.latestFrame().isNull());
    QCOMPARE(service.latestFrame().size(), QSize(64, 48));
    QCOMPARE(service.latestFrame().pixel(5, 5), qRgb(200, 30, 30));

    QImage updated(64, 48, QImage::Format_RGB32);
    updated.fill(qRgb(30, 200, 30));
    QVERIFY(updated.save(framePath));
    QTRY_COMPARE(service.latestFrame().pixel(5, 5), qRgb(30, 200, 30));
    qunsetenv("AS_FAKE_CAPTURE_FILE");
}

void TestScreenCaptureService::stopClearsFrameAndDeactivates() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString framePath = dir.filePath(QStringLiteral("frame.png"));
    QImage frame(32, 32, QImage::Format_RGB32);
    frame.fill(qRgb(1, 2, 3));
    QVERIFY(frame.save(framePath));

    qputenv("AS_FAKE_CAPTURE_FILE", framePath.toUtf8());
    ScreenCaptureService service;
    service.start(nullptr);
    QTRY_VERIFY(!service.latestFrame().isNull());

    service.stop();
    QVERIFY(!service.isActive());
    QVERIFY(service.latestFrame().isNull());
    qunsetenv("AS_FAKE_CAPTURE_FILE");
}

QTEST_MAIN(TestScreenCaptureService)

#include "tst_capture.moc"
