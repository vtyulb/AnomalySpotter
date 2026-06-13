#include "MainWindow.h"

#include "DiffEngine.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFont>
#include <QSettings>
#include <QGuiApplication>
#include <QtMath>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QSaveFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QStandardPaths>
#include <QShortcut>
#include <QSlider>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const QSize kCompareSize(320, 180);
constexpr int kCompareIntervalMs = 200;
constexpr int kOverlaySettleMs = 350;
constexpr int kBlinkIntervalMs = 100;

void applyColorSwatch(QPushButton *button, const QColor &color) {
    const QString textColor = color.lightness() > 127 ? QStringLiteral("#000000")
                                                       : QStringLiteral("#ffffff");
    button->setText(color.name());
    button->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;").arg(color.name(), textColor));
}

}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    buildUi();

    QSettings settings;
    thresholdSlider_->setValue(settings.value(QStringLiteral("threshold"), 10).toInt());
    hudCheck_->setChecked(settings.value(QStringLiteral("hudEnabled"), true).toBool());
    hudSizeSlider_->setValue(qBound(50, settings.value(QStringLiteral("hudSize"), 100).toInt(), 200));
    hud_.setScalePercent(hudSizeSlider_->value());
    ignoreTopSlider_->setValue(qBound(0, settings.value(QStringLiteral("ignoreTop"), 35).toInt(), 100));
    ignoreBottomSlider_->setValue(
        qBound(0, settings.value(QStringLiteral("ignoreBottom"), 0).toInt(), 100));
    highlightColor_ = QColor(settings.value(QStringLiteral("highlightColor"),
                                            QStringLiteral("#ff2828")).toString());
    if (!highlightColor_.isValid())
        highlightColor_ = QColor(255, 40, 40);
    applyColorSwatch(colorButton_, highlightColor_);
    const int storedMode = qBound(0, settings.value(QStringLiteral("overlayMode"), 0).toInt(),
                                  overlayModeBox_->count() - 1);
    overlayModeBox_->setCurrentIndex(storedMode);
    overlayMode_ = static_cast<OverlayMode>(storedMode);
    const int storedScreen = settings.value(QStringLiteral("screenIndex"), 0).toInt();
    if (storedScreen >= 0 && storedScreen < screenBox_->count())
        screenBox_->setCurrentIndex(storedScreen);
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());

    snapshotFKey_ = qBound(1, settings.value(QStringLiteral("snapshotKey"), 5).toInt(), 12);
    overlayFKey_ = qBound(1, settings.value(QStringLiteral("overlayKey"), 6).toInt(), 12);
    deleteLastFKey_ = qBound(0, settings.value(QStringLiteral("deleteLastKey"), 0).toInt(), 12);
    overlayTriggerText_ = QStringLiteral("F%1").arg(overlayFKey_);
    snapshotButton_->setText(QStringLiteral("Snapshot (F%1)").arg(snapshotFKey_));
    overlayButton_->setText(QStringLiteral("Differences (%1)").arg(overlayTriggerText_));
    if (deleteLastFKey_ > 0)
        deleteLastButton_->setText(QStringLiteral("Delete last (F%1)").arg(deleteLastFKey_));

    compareTimer_.setInterval(kCompareIntervalMs);
    connect(&compareTimer_, &QTimer::timeout, this, &MainWindow::updateComparisons);

    overlayBlinkTimer_.setInterval(kBlinkIntervalMs);
    connect(&overlayBlinkTimer_, &QTimer::timeout, this, [this] {
        overlay_.setHighlightVisible(!overlay_.highlightVisible());
    });

    connect(&captureService_, &ScreenCaptureService::activeChanged, this, [this](bool active) {
        captureButton_->setText(active ? QStringLiteral("Stop capture")
                                       : QStringLiteral("Start capture"));
        if (active) {
            compareTimer_.start();
            statusBar()->showMessage(QStringLiteral("Capture running"), 3000);
        } else {
            compareTimer_.stop();
            hud_.hide();
        }
    });
    connect(&captureService_, &ScreenCaptureService::errorOccurred, this,
            [this](const QString &message) {
                statusBar()->showMessage(QStringLiteral("Capture error: ") + message);
            });

    hotkeys_ = GlobalHotkeys::create(this);
    connect(hotkeys_, &GlobalHotkeys::snapshotRequested, this, &MainWindow::saveSnapshot);
    connect(hotkeys_, &GlobalHotkeys::overlayPressed, this, &MainWindow::onOverlayPressed);
    connect(hotkeys_, &GlobalHotkeys::overlayReleased, this, &MainWindow::onOverlayReleased);
    connect(hotkeys_, &GlobalHotkeys::deleteLastRequested, this,
            &MainWindow::removeLastSnapshot);
    connect(hotkeys_, &GlobalHotkeys::bound, this, &MainWindow::onGlobalShortcutsBound);
    connect(hotkeys_, &GlobalHotkeys::triggersChanged, this,
            [this](const QString &snapshotTrigger, const QString &overlayTrigger,
                   const QString &deleteLastTrigger) {
                if (!snapshotTrigger.isEmpty())
                    snapshotButton_->setText(
                        QStringLiteral("Snapshot (%1)").arg(snapshotTrigger));
                if (!overlayTrigger.isEmpty()) {
                    overlayTriggerText_ = overlayTrigger;
                    overlayButton_->setText(
                        QStringLiteral("Differences (%1)").arg(overlayTrigger));
                }
                deleteLastButton_->setText(
                    deleteLastTrigger.isEmpty()
                        ? QStringLiteral("Delete last")
                        : QStringLiteral("Delete last (%1)").arg(deleteLastTrigger));
            });
    connect(hotkeys_, &GlobalHotkeys::statusChanged, this, [this](const QString &status) {
        statusBar()->showMessage(status, 5000);
    });

    hotkeys_->setFunctionKeys(snapshotFKey_, overlayFKey_, deleteLastFKey_);

    automationControlPath_ = qEnvironmentVariable("AS_CONTROL_FILE");
    localSnapshotShortcut_ = new QShortcut(
        QKeySequence(static_cast<Qt::Key>(Qt::Key_F1 + snapshotFKey_ - 1)), this);
    connect(localSnapshotShortcut_, &QShortcut::activated, this, &MainWindow::saveSnapshot);

    if (automationControlPath_.isEmpty()) {
        hotkeys_->start();
    } else {
        automationStatePath_ = automationControlPath_ + QStringLiteral(".state");
        auto *automationTimer = new QTimer(this);
        automationTimer->setInterval(100);
        connect(automationTimer, &QTimer::timeout, this, &MainWindow::pollAutomation);
        automationTimer->start();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if (!globalHotkeysBound_ && event->key() == Qt::Key_F1 + overlayFKey_ - 1
        && !event->isAutoRepeat()) {
        onOverlayPressed();
        return;
    }
    if (!globalHotkeysBound_ && deleteLastFKey_ > 0
        && event->key() == Qt::Key_F1 + deleteLastFKey_ - 1 && !event->isAutoRepeat()) {
        removeLastSnapshot();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if (!globalHotkeysBound_ && event->key() == Qt::Key_F1 + overlayFKey_ - 1
        && !event->isAutoRepeat()) {
        onOverlayReleased();
        return;
    }
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::buildUi() {
    setWindowTitle(QStringLiteral("AnomalySpotter"));

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    auto *screenRow = new QHBoxLayout;
    screenRow->addWidget(new QLabel(QStringLiteral("Screen:"), central));
    screenBox_ = new QComboBox(central);
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        screenBox_->addItem(QStringLiteral("%1 (%2×%3)")
                                .arg(screen->name())
                                .arg(screen->size().width())
                                .arg(screen->size().height()));
    }
    screenRow->addWidget(screenBox_, 1);
    captureButton_ = new QPushButton(QStringLiteral("Start capture"), central);
    connect(captureButton_, &QPushButton::clicked, this, &MainWindow::toggleCapture);
    screenRow->addWidget(captureButton_);
    layout->addLayout(screenRow);

    previewLabel_ = new QLabel(QStringLiteral("No capture"), central);
    previewLabel_->setMinimumHeight(200);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setFrameShape(QFrame::StyledPanel);
    layout->addWidget(previewLabel_);

    auto *thresholdRow = new QHBoxLayout;
    thresholdRow->addWidget(new QLabel(QStringLiteral("Sensitivity threshold:"), central));
    thresholdSlider_ = new QSlider(Qt::Horizontal, central);
    thresholdSlider_->setRange(5, 50);
    thresholdSlider_->setValue(10);
    thresholdRow->addWidget(thresholdSlider_, 1);
    thresholdValueLabel_ = new QLabel(QStringLiteral("10"), central);
    connect(thresholdSlider_, &QSlider::valueChanged, thresholdValueLabel_,
            qOverload<int>(&QLabel::setNum));
    thresholdRow->addWidget(thresholdValueLabel_);
    layout->addLayout(thresholdRow);

    hudCheck_ = new QCheckBox(QStringLiteral("Show difference % on screen"), central);
    hudCheck_->setChecked(true);
    connect(hudCheck_, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked)
            hud_.hide();
    });
    layout->addWidget(hudCheck_);

    auto *hudSizeRow = new QHBoxLayout;
    hudSizeRow->addWidget(new QLabel(QStringLiteral("HUD size:"), central));
    hudSizeSlider_ = new QSlider(Qt::Horizontal, central);
    hudSizeSlider_->setRange(50, 200);
    hudSizeSlider_->setValue(100);
    hudSizeRow->addWidget(hudSizeSlider_, 1);
    auto *hudSizeValueLabel = new QLabel(QStringLiteral("100%"), central);
    connect(hudSizeSlider_, &QSlider::valueChanged, this, [this, hudSizeValueLabel](int value) {
        hudSizeValueLabel->setText(QStringLiteral("%1%").arg(value));
        hud_.setScalePercent(value);
    });
    hudSizeRow->addWidget(hudSizeValueLabel);
    layout->addLayout(hudSizeRow);

    auto addIgnoreBandRow = [this, central, layout](const QString &label, QSlider *&slider) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(label, central));
        slider = new QSlider(Qt::Horizontal, central);
        slider->setRange(0, 100);
        slider->setValue(0);
        row->addWidget(slider, 1);
        auto *valueLabel = new QLabel(QStringLiteral("0 px"), central);
        connect(slider, &QSlider::valueChanged, valueLabel,
                [valueLabel](int value) { valueLabel->setText(QStringLiteral("%1 px").arg(value)); });
        row->addWidget(valueLabel);
        layout->addLayout(row);
    };
    addIgnoreBandRow(QStringLiteral("Ignore top pixels:"), ignoreTopSlider_);
    addIgnoreBandRow(QStringLiteral("Ignore bottom pixels:"), ignoreBottomSlider_);

    auto *colorRow = new QHBoxLayout;
    colorRow->addWidget(new QLabel(QStringLiteral("Difference color:"), central));
    colorButton_ = new QPushButton(central);
    connect(colorButton_, &QPushButton::clicked, this, &MainWindow::chooseHighlightColor);
    colorRow->addWidget(colorButton_, 1);
    layout->addLayout(colorRow);

    auto *overlayModeRow = new QHBoxLayout;
    overlayModeRow->addWidget(new QLabel(QStringLiteral("Overlay key mode:"), central));
    overlayModeBox_ = new QComboBox(central);
    overlayModeBox_->addItem(QStringLiteral("Toggle (press to show/hide)"));
    overlayModeBox_->addItem(QStringLiteral("Blink (toggle, overlay flashes)"));
    overlayModeBox_->addItem(QStringLiteral("Hold (only while key is held)"));
    connect(overlayModeBox_, &QComboBox::currentIndexChanged, this, [this](int index) {
        deactivateOverlay();
        overlayMode_ = static_cast<OverlayMode>(index);
    });
    overlayModeRow->addWidget(overlayModeBox_, 1);
    layout->addLayout(overlayModeRow);

    auto *hotkeyHint = new QLabel(central);
    hotkeyHint->setWordWrap(true);
    hotkeyHint->setStyleSheet(QStringLiteral("color: palette(mid);"));
    auto *hotkeyButtonRow = new QHBoxLayout;
    if (QGuiApplication::platformName().startsWith(QLatin1String("wayland"))) {
        hotkeyHint->setText(QStringLiteral(
            "Global hotkeys are owned by the compositor — rebind them in your desktop's "
            "keyboard-shortcuts settings (listed as \"AnomalySpotter\")."));
        auto *shortcutsButton = new QPushButton(QStringLiteral("Open shortcut settings…"), central);
        connect(shortcutsButton, &QPushButton::clicked, this, &MainWindow::openShortcutSettings);
        hotkeyButtonRow->addWidget(shortcutsButton);
    } else {
        hotkeyHint->setText(QStringLiteral(
            "Change the hotkeys via snapshotKey/overlayKey/deleteLastKey (F-number) in the "
            "config file."));
        auto *configButton = new QPushButton(QStringLiteral("Edit config…"), central);
        connect(configButton, &QPushButton::clicked, this, &MainWindow::openConfigFile);
        hotkeyButtonRow->addWidget(configButton);
    }
    layout->addWidget(hotkeyHint);
    hotkeyButtonRow->addStretch(1);
    layout->addLayout(hotkeyButtonRow);

    snapshotTable_ = new QTableWidget(0, 2, central);
    snapshotTable_->setHorizontalHeaderLabels(
        {QStringLiteral("Snapshot"), QStringLiteral("Difference")});
    snapshotTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    snapshotTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    snapshotTable_->verticalHeader()->setVisible(false);
    snapshotTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    snapshotTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    snapshotTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    snapshotTable_->setIconSize(QSize(64, 36));
    layout->addWidget(snapshotTable_, 1);

    auto *buttonsRow = new QHBoxLayout;
    snapshotButton_ = new QPushButton(QStringLiteral("Snapshot (F5)"), central);
    connect(snapshotButton_, &QPushButton::clicked, this, &MainWindow::saveSnapshot);
    overlayButton_ = new QPushButton(QStringLiteral("Differences (F6)"), central);
    connect(overlayButton_, &QPushButton::clicked, this, &MainWindow::toggleOverlay);
    deleteLastButton_ = new QPushButton(QStringLiteral("Delete last"), central);
    connect(deleteLastButton_, &QPushButton::clicked, this, &MainWindow::removeLastSnapshot);
    auto *removeButton = new QPushButton(QStringLiteral("Delete selected"), central);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::removeSelectedSnapshot);
    buttonsRow->addWidget(snapshotButton_);
    buttonsRow->addWidget(overlayButton_);
    buttonsRow->addWidget(deleteLastButton_);
    buttonsRow->addWidget(removeButton);
    layout->addLayout(buttonsRow);

    setCentralWidget(central);
    statusBar()->showMessage(QStringLiteral("Select a screen and start capture"));
    resize(560, 760);
}

QScreen *MainWindow::selectedScreen() const {
    const auto screens = QGuiApplication::screens();
    const int index = screenBox_->currentIndex();
    if (index >= 0 && index < screens.size())
        return screens[index];
    return QGuiApplication::primaryScreen();
}

void MainWindow::toggleCapture() {
    if (captureService_.isActive()) {
        captureService_.stop();
        previewLabel_->setPixmap(QPixmap());
        previewLabel_->setText(QStringLiteral("No capture"));
        return;
    }
    captureService_.start(selectedScreen());
    statusBar()->showMessage(
        QStringLiteral("Starting capture, approve screen access if the system asks"), 7000);
}

void MainWindow::updateComparisons() {
    if (overlay_.isVisible())
        return;
    const QImage frame = captureService_.latestFrame();
    if (frame.isNull())
        return;

    previewLabel_->setPixmap(QPixmap::fromImage(
        frame.scaled(previewLabel_->contentsRect().size(), Qt::KeepAspectRatio,
                     Qt::FastTransformation)));

    if (snapshots_.isEmpty()) {
        lastPercents_.clear();
        bestIndex_ = -1;
        return;
    }

    const QImage compareScaled =
        frame.scaled(kCompareSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const QRegion exclude = excludedRegion(compareScaled.size(), frame.size());
    int bestIndex = -1;
    double bestPercent = 0.0;
    lastPercents_.clear();
    for (int i = 0; i < snapshots_.size(); ++i) {
        const double percent = DiffEngine::diffPercent(compareScaled, snapshots_[i].compareScaled,
                                                       thresholdSlider_->value(), exclude);
        lastPercents_.append(percent);
        snapshotTable_->item(i, 1)->setText(QStringLiteral("%1 %").arg(percent, 0, 'f', 2));
        if (bestIndex < 0 || percent < bestPercent) {
            bestIndex = i;
            bestPercent = percent;
        }
    }
    bestIndex_ = bestIndex;

    if (hudCheck_->isChecked() && bestIndex >= 0)
        hud_.showPercent(bestPercent, screenForFrame(frame.size()));
    else
        hud_.hide();

    QFont normalFont = snapshotTable_->font();
    QFont boldFont = normalFont;
    boldFont.setBold(true);
    for (int row = 0; row < snapshotTable_->rowCount(); ++row) {
        for (int column = 0; column < snapshotTable_->columnCount(); ++column)
            snapshotTable_->item(row, column)->setFont(row == bestIndex ? boldFont : normalFont);
    }
}

int MainWindow::bestMatchIndex(const QImage &compareScaled, const QSize &frameSize,
                               double *bestPercent) const {
    const QRegion exclude = excludedRegion(compareScaled.size(), frameSize);
    int bestIndex = -1;
    double best = 0.0;
    for (int i = 0; i < snapshots_.size(); ++i) {
        const double percent = DiffEngine::diffPercent(compareScaled, snapshots_[i].compareScaled,
                                                       thresholdSlider_->value(), exclude);
        if (bestIndex < 0 || percent < best) {
            bestIndex = i;
            best = percent;
        }
    }
    if (bestPercent)
        *bestPercent = best;
    return bestIndex;
}

QScreen *MainWindow::screenForFrame(const QSize &frameSize) const {
    auto matchesFrame = [&frameSize](QScreen *screen) {
        const QSize logical = screen->geometry().size();
        return logical == frameSize || logical * screen->devicePixelRatio() == frameSize;
    };
    QScreen *selected = selectedScreen();
    if (selected && matchesFrame(selected))
        return selected;
    const auto screens = QGuiApplication::screens();
    for (QScreen *screen : screens) {
        if (matchesFrame(screen))
            return screen;
    }
    return selected;
}

QRect MainWindow::hudExclusion(const QSize &targetSize, const QSize &frameSize) const {
    if (!hud_.isVisible())
        return {};
    QScreen *screen = screenForFrame(frameSize);
    const QSizeF screenLogical = screen ? QSizeF(screen->geometry().size()) : QSizeF(frameSize);
    if (screenLogical.width() <= 0 || screenLogical.height() <= 0)
        return {};
    constexpr int hudMargin = 16;
    constexpr int safetyPad = 6;
    const double fracW = (hud_.width() + hudMargin + safetyPad) / screenLogical.width();
    const double fracH = (hud_.height() + hudMargin + safetyPad) / screenLogical.height();
    const int width = qMin(targetSize.width(), qCeil(targetSize.width() * fracW));
    const int height = qMin(targetSize.height(), qCeil(targetSize.height() * fracH));
    return QRect(targetSize.width() - width, 0, width, height);
}

QRegion MainWindow::excludedRegion(const QSize &targetSize, const QSize &frameSize) const {
    QRegion region;
    const QRect hud = hudExclusion(targetSize, frameSize);
    if (!hud.isEmpty())
        region += hud;

    QScreen *screen = screenForFrame(frameSize);
    const double screenHeight = screen ? screen->geometry().height() : frameSize.height();
    if (screenHeight > 0) {
        const int topPixels = ignoreTopSlider_->value();
        if (topPixels > 0) {
            const int band = qMin(targetSize.height(),
                                  qCeil(targetSize.height() * topPixels / screenHeight));
            region += QRect(0, 0, targetSize.width(), band);
        }
        const int bottomPixels = ignoreBottomSlider_->value();
        if (bottomPixels > 0) {
            const int band = qMin(targetSize.height(),
                                  qCeil(targetSize.height() * bottomPixels / screenHeight));
            region += QRect(0, targetSize.height() - band, targetSize.width(), band);
        }
    }
    return region;
}

void MainWindow::saveSnapshot() {
    if (overlayActive_ || overlay_.isVisible()) {
        deactivateOverlay();
        QTimer::singleShot(kOverlaySettleMs, this, &MainWindow::saveSnapshot);
        return;
    }
    const QImage frame = captureService_.latestFrame();
    if (frame.isNull()) {
        statusBar()->showMessage(QStringLiteral("No frame — capture is not running"), 4000);
        return;
    }

    Snapshot snapshot;
    snapshot.name = QStringLiteral("Snap %1").arg(++snapshotCounter_);
    snapshot.full = frame;
    snapshot.compareScaled =
        frame.scaled(kCompareSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    snapshots_.append(snapshot);

    const int row = snapshotTable_->rowCount();
    snapshotTable_->insertRow(row);
    snapshotTable_->setItem(row, 0,
                            new QTableWidgetItem(QIcon(QPixmap::fromImage(snapshot.compareScaled)),
                                                 snapshot.name));
    snapshotTable_->setItem(row, 1, new QTableWidgetItem(QStringLiteral("—")));
    statusBar()->showMessage(snapshot.name + QStringLiteral(" saved to memory"), 3000);
}

void MainWindow::showOverlay() {
    if (overlay_.isVisible())
        return;
    const QImage frame = captureService_.latestFrame();
    if (frame.isNull() || snapshots_.isEmpty()) {
        statusBar()->showMessage(
            QStringLiteral("Need active capture and at least one snapshot"), 4000);
        return;
    }

    const QImage compareScaled =
        frame.scaled(kCompareSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const int index = bestMatchIndex(compareScaled, frame.size(), nullptr);
    QImage reference = snapshots_[index].full;
    if (reference.size() != frame.size())
        reference = reference.scaled(frame.size(), Qt::IgnoreAspectRatio,
                                     Qt::SmoothTransformation);

    const DiffEngine::Analysis analysis = DiffEngine::analyze(
        frame, reference, thresholdSlider_->value(), excludedRegion(frame.size(), frame.size()),
        highlightColor_);
    QString hideHint;
    switch (overlayMode_) {
    case OverlayMode::Toggle:
        hideHint = QStringLiteral("%1 to hide").arg(overlayTriggerText_);
        break;
    case OverlayMode::Blink:
        hideHint = QStringLiteral("%1 to stop").arg(overlayTriggerText_);
        break;
    case OverlayMode::Hold:
        hideHint = QStringLiteral("release %1 to hide").arg(overlayTriggerText_);
        break;
    }
    overlay_.showDiff(analysis.mask, analysis.regions, screenForFrame(frame.size()),
                      analysis.percent, snapshots_[index].name, hideHint, highlightColor_);
    statusBar()->showMessage(
        QStringLiteral("Overlay shown, comparison paused"), 5000);
}

void MainWindow::hideOverlay() {
    if (!overlay_.isVisible())
        return;
    overlay_.hide();
    statusBar()->showMessage(QStringLiteral("Overlay hidden, comparison resumed"), 3000);
}

void MainWindow::activateOverlay() {
    showOverlay();
    if (!overlay_.isVisible())
        return;
    overlayActive_ = true;
    if (overlayMode_ == OverlayMode::Blink)
        overlayBlinkTimer_.start();
}

void MainWindow::deactivateOverlay() {
    overlayBlinkTimer_.stop();
    overlayActive_ = false;
    hideOverlay();
}

void MainWindow::toggleOverlay() {
    if (overlayActive_ || overlay_.isVisible())
        deactivateOverlay();
    else
        activateOverlay();
}

void MainWindow::onOverlayPressed() {
    if (overlayMode_ == OverlayMode::Hold)
        activateOverlay();
    else
        toggleOverlay();
}

void MainWindow::onOverlayReleased() {
    if (overlayMode_ == OverlayMode::Hold)
        deactivateOverlay();
}

void MainWindow::removeSnapshotAt(int row) {
    if (row < 0 || row >= snapshots_.size())
        return;
    const QString name = snapshots_[row].name;
    snapshots_.removeAt(row);
    snapshotTable_->removeRow(row);
    lastPercents_.clear();
    bestIndex_ = -1;
    statusBar()->showMessage(name + QStringLiteral(" deleted"), 3000);
}

void MainWindow::removeSelectedSnapshot() {
    removeSnapshotAt(snapshotTable_->currentRow());
}

void MainWindow::removeLastSnapshot() {
    if (snapshots_.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("No snapshots to delete"), 3000);
        return;
    }
    removeSnapshotAt(snapshots_.size() - 1);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    QSettings settings;
    settings.setValue(QStringLiteral("threshold"), thresholdSlider_->value());
    settings.setValue(QStringLiteral("screenIndex"), screenBox_->currentIndex());
    settings.setValue(QStringLiteral("hudEnabled"), hudCheck_->isChecked());
    settings.setValue(QStringLiteral("hudSize"), hudSizeSlider_->value());
    settings.setValue(QStringLiteral("ignoreTop"), ignoreTopSlider_->value());
    settings.setValue(QStringLiteral("ignoreBottom"), ignoreBottomSlider_->value());
    settings.setValue(QStringLiteral("highlightColor"), highlightColor_.name());
    settings.setValue(QStringLiteral("overlayMode"), overlayModeBox_->currentIndex());
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    overlay_.close();
    hud_.close();
    QMainWindow::closeEvent(event);
}

void MainWindow::onGlobalShortcutsBound() {
    globalHotkeysBound_ = true;
    localSnapshotShortcut_->setEnabled(false);
}

void MainWindow::openShortcutSettings() {
    struct Candidate {
        QString program;
        QStringList args;
    };
    const QList<Candidate> candidates = {
        {QStringLiteral("systemsettings"), {QStringLiteral("kcm_keys")}},
        {QStringLiteral("systemsettings5"), {QStringLiteral("kcm_keys")}},
        {QStringLiteral("kcmshell6"), {QStringLiteral("kcm_keys")}},
        {QStringLiteral("kcmshell5"), {QStringLiteral("kcm_keys")}},
        {QStringLiteral("gnome-control-center"), {QStringLiteral("keyboard")}},
    };
    for (const Candidate &candidate : candidates) {
        if (!QStandardPaths::findExecutable(candidate.program).isEmpty()) {
            QProcess::startDetached(candidate.program, candidate.args);
            return;
        }
    }
    statusBar()->showMessage(
        QStringLiteral("No shortcuts-settings app found — set the keys in your compositor"), 5000);
}

void MainWindow::chooseHighlightColor() {
    const QColor chosen =
        QColorDialog::getColor(highlightColor_, this, QStringLiteral("Difference highlight color"));
    if (!chosen.isValid())
        return;
    highlightColor_ = chosen;
    applyColorSwatch(colorButton_, highlightColor_);
}

void MainWindow::openConfigFile() {
    QSettings settings;
    settings.setValue(QStringLiteral("snapshotKey"), snapshotFKey_);
    settings.setValue(QStringLiteral("overlayKey"), overlayFKey_);
    settings.setValue(QStringLiteral("deleteLastKey"), deleteLastFKey_);
    settings.sync();
    QDesktopServices::openUrl(QUrl::fromLocalFile(settings.fileName()));
    statusBar()->showMessage(QStringLiteral("Restart the app after editing the config"), 5000);
}

void MainWindow::pollAutomation() {
    QFile control(automationControlPath_);
    if (control.open(QIODevice::ReadWrite)) {
        const QStringList commands =
            QString::fromUtf8(control.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        control.resize(0);
        control.close();
        for (const QString &command : commands)
            runAutomationCommand(command.trimmed());
    }
    dumpAutomationState();
}

void MainWindow::runAutomationCommand(const QString &command) {
    if (command == QLatin1String("capture-on")) {
        if (!captureService_.isActive())
            captureService_.start(selectedScreen());
    } else if (command == QLatin1String("capture-off")) {
        if (captureService_.isActive())
            captureService_.stop();
    } else if (command == QLatin1String("snapshot")) {
        saveSnapshot();
    } else if (command == QLatin1String("overlay")) {
        toggleOverlay();
    } else if (command == QLatin1String("delete-last")) {
        removeLastSnapshot();
    }
}

void MainWindow::dumpAutomationState() {
    QJsonObject state;
    state[QStringLiteral("captureActive")] = captureService_.isActive();
    state[QStringLiteral("hasFrame")] = !captureService_.latestFrame().isNull();
    state[QStringLiteral("overlayVisible")] = overlay_.isVisible();
    state[QStringLiteral("overlayActive")] = overlayActive_;
    state[QStringLiteral("hudVisible")] = hud_.isVisible();
    state[QStringLiteral("snapshotCount")] = static_cast<int>(snapshots_.size());
    state[QStringLiteral("bestIndex")] = bestIndex_;
    QJsonArray percents;
    for (double percent : lastPercents_)
        percents.append(percent);
    state[QStringLiteral("percents")] = percents;

    QSaveFile file(automationStatePath_);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
        file.commit();
    }
}
