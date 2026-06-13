#include "DiffEngine.h"

#include <QtTest>

class TestDiffEngine : public QObject {
    Q_OBJECT

private slots:
    void identicalImagesGiveZero();
    void changedBlockGivesExactPercent();
    void sizeMismatchGivesHundred();
    void thresholdSuppressesSmallDeltas();
    void regionExcludesBandsFromDiff();
    void analyzeMarksChangedPixelsInMask();
    void analyzeBuildsRegionAroundChange();
    void analyzeSkipsSparseNoiseRegions();

private:
    QImage solidImage(const QSize &size, QRgb color) const;
};

QImage TestDiffEngine::solidImage(const QSize &size, QRgb color) const {
    QImage image(size, QImage::Format_RGB32);
    image.fill(color);
    return image;
}

void TestDiffEngine::identicalImagesGiveZero() {
    const QImage image = solidImage({100, 100}, qRgb(50, 60, 70));
    QCOMPARE(DiffEngine::diffPercent(image, image, 25), 0.0);
}

void TestDiffEngine::changedBlockGivesExactPercent() {
    const QImage base = solidImage({100, 100}, qRgb(50, 60, 70));
    QImage changed = base;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 10; ++x)
            changed.setPixel(x, y, qRgb(250, 60, 70));
    QCOMPARE(DiffEngine::diffPercent(base, changed, 25), 1.0);
}

void TestDiffEngine::sizeMismatchGivesHundred() {
    const QImage small = solidImage({10, 10}, qRgb(0, 0, 0));
    const QImage large = solidImage({20, 20}, qRgb(0, 0, 0));
    QCOMPARE(DiffEngine::diffPercent(small, large, 25), 100.0);
}

void TestDiffEngine::thresholdSuppressesSmallDeltas() {
    const QImage base = solidImage({50, 50}, qRgb(100, 100, 100));
    const QImage shifted = solidImage({50, 50}, qRgb(120, 100, 100));
    QCOMPARE(DiffEngine::diffPercent(base, shifted, 25), 0.0);
    QCOMPARE(DiffEngine::diffPercent(base, shifted, 10), 100.0);
}

void TestDiffEngine::regionExcludesBandsFromDiff() {
    const QImage base = solidImage({100, 100}, qRgb(50, 60, 70));
    QImage changed = base;
    for (int y = 0; y < 10; ++y)
        for (int x = 0; x < 100; ++x)
            changed.setPixel(x, y, qRgb(250, 60, 70));
    for (int y = 90; y < 100; ++y)
        for (int x = 0; x < 100; ++x)
            changed.setPixel(x, y, qRgb(250, 60, 70));

    QCOMPARE(DiffEngine::diffPercent(base, changed, 25), 20.0);

    QRegion bothBands;
    bothBands += QRect(0, 0, 100, 10);
    bothBands += QRect(0, 90, 100, 10);
    QCOMPARE(DiffEngine::diffPercent(base, changed, 25, bothBands), 0.0);

    QCOMPARE(DiffEngine::diffPercent(base, changed, 25, QRegion(QRect(0, 0, 100, 10))),
             100.0 * 1000 / 9000);
}

void TestDiffEngine::analyzeMarksChangedPixelsInMask() {
    const QImage base = solidImage({64, 64}, qRgb(10, 10, 10));
    QImage changed = base;
    changed.setPixel(40, 20, qRgb(255, 255, 255));
    const DiffEngine::Analysis analysis = DiffEngine::analyze(changed, base, 25);
    QVERIFY(qAlpha(analysis.mask.pixel(40, 20)) > 0);
    QCOMPARE(qAlpha(analysis.mask.pixel(0, 0)), 0);
    QVERIFY(analysis.percent > 0.0);
}

void TestDiffEngine::analyzeBuildsRegionAroundChange() {
    const QImage base = solidImage({256, 256}, qRgb(10, 10, 10));
    QImage changed = base;
    for (int y = 100; y < 140; ++y)
        for (int x = 60; x < 100; ++x)
            changed.setPixel(x, y, qRgb(255, 40, 40));
    const DiffEngine::Analysis analysis = DiffEngine::analyze(changed, base, 25);
    QCOMPARE(analysis.regions.size(), 1);
    QVERIFY(analysis.regions.first().contains(QRect(60, 100, 40, 40)));
}

void TestDiffEngine::analyzeSkipsSparseNoiseRegions() {
    const QImage base = solidImage({256, 256}, qRgb(10, 10, 10));
    QImage changed = base;
    changed.setPixel(50, 50, qRgb(255, 255, 255));
    const DiffEngine::Analysis analysis = DiffEngine::analyze(changed, base, 25);
    QVERIFY(analysis.regions.isEmpty());
    QVERIFY(analysis.percent > 0.0);
}

QTEST_APPLESS_MAIN(TestDiffEngine)

#include "tst_diffengine.moc"
