#include "MainWindow.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>

namespace {

void publishFrame(const QString &framePath, QRgb base, const QRect &block = QRect(),
                  QRgb blockColor = 0) {
    QImage frame(320, 180, QImage::Format_RGB32);
    frame.fill(base);
    if (!block.isNull()) {
        for (int y = block.top(); y <= block.bottom(); ++y)
            for (int x = block.left(); x <= block.right(); ++x)
                frame.setPixel(x, y, blockColor);
    }
    const QString tempPath = framePath + QStringLiteral(".tmp");
    frame.save(tempPath, "PNG");
    std::rename(tempPath.toLocal8Bit().constData(), framePath.toLocal8Bit().constData());
}

}

class TestTrainer : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void fullPipeline();

private:
    void sendCommand(const QString &command);
    QJsonObject readState() const;

    QTemporaryDir dir_;
    QString framePath_;
    QString controlPath_;
};

void TestTrainer::sendCommand(const QString &command) {
    QFile control(controlPath_);
    QVERIFY(control.open(QIODevice::Append));
    control.write(command.toUtf8() + '\n');
}

QJsonObject TestTrainer::readState() const {
    QFile state(controlPath_ + QStringLiteral(".state"));
    if (!state.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(state.readAll()).object();
}

void TestTrainer::initTestCase() {
    QVERIFY(dir_.isValid());
    qputenv("XDG_CONFIG_HOME", dir_.filePath(QStringLiteral("config")).toUtf8());
    framePath_ = dir_.filePath(QStringLiteral("frame.png"));
    controlPath_ = dir_.filePath(QStringLiteral("ctl"));
    publishFrame(framePath_, qRgb(30, 30, 40));
    QFile control(controlPath_);
    QVERIFY(control.open(QIODevice::WriteOnly));
    qputenv("AS_FAKE_CAPTURE_FILE", framePath_.toUtf8());
    qputenv("AS_CONTROL_FILE", controlPath_.toUtf8());
}

void TestTrainer::fullPipeline() {
    MainWindow window;
    window.show();

    sendCommand(QStringLiteral("capture-on"));
    QTRY_VERIFY(readState().value(QStringLiteral("captureActive")).toBool());
    QTRY_VERIFY(readState().value(QStringLiteral("hasFrame")).toBool());

    sendCommand(QStringLiteral("snapshot"));
    QTRY_COMPARE(readState().value(QStringLiteral("snapshotCount")).toInt(), 1);
    QTRY_VERIFY(readState().value(QStringLiteral("percents")).toArray().size() == 1);
    QTRY_VERIFY(readState().value(QStringLiteral("percents")).toArray().first().toDouble()
                < 0.01);
    QTRY_VERIFY(readState().value(QStringLiteral("hudVisible")).toBool());

    publishFrame(framePath_, qRgb(30, 30, 40), QRect(0, 0, 31, 17), qRgb(250, 220, 40));
    QTRY_VERIFY(readState().value(QStringLiteral("percents")).toArray().first().toDouble()
                > 0.5);

    sendCommand(QStringLiteral("snapshot"));
    QTRY_COMPARE(readState().value(QStringLiteral("snapshotCount")).toInt(), 2);
    QTRY_COMPARE(readState().value(QStringLiteral("bestIndex")).toInt(), 1);

    sendCommand(QStringLiteral("overlay"));
    QTRY_VERIFY(readState().value(QStringLiteral("overlayVisible")).toBool());

    sendCommand(QStringLiteral("overlay"));
    QTRY_VERIFY(!readState().value(QStringLiteral("overlayVisible")).toBool());

    sendCommand(QStringLiteral("delete-last"));
    QTRY_COMPARE(readState().value(QStringLiteral("snapshotCount")).toInt(), 1);

    sendCommand(QStringLiteral("delete-last"));
    QTRY_COMPARE(readState().value(QStringLiteral("snapshotCount")).toInt(), 0);

    sendCommand(QStringLiteral("delete-last"));
    QTest::qWait(300);
    QCOMPARE(readState().value(QStringLiteral("snapshotCount")).toInt(), 0);

    sendCommand(QStringLiteral("capture-off"));
    QTRY_VERIFY(!readState().value(QStringLiteral("captureActive")).toBool());
    QTRY_VERIFY(!readState().value(QStringLiteral("hudVisible")).toBool());
}

QTEST_MAIN(TestTrainer)

#include "tst_trainer.moc"
