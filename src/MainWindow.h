#pragma once

#include "GlobalHotkeys.h"
#include "HudOverlay.h"
#include "OverlayWindow.h"
#include "ScreenCaptureService.h"

#include <QColor>
#include <QImage>
#include <QList>
#include <QMainWindow>
#include <QRegion>
#include <QTimer>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QShortcut;
class QSlider;
class QTableWidget;

struct Snapshot {
    QString name;
    QImage full;
    QImage compareScaled;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    enum class OverlayMode { Toggle, Blink, Hold };

    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void toggleCapture();
    void saveSnapshot();
    void showOverlay();
    void hideOverlay();
    void activateOverlay();
    void deactivateOverlay();
    void toggleOverlay();
    void onOverlayPressed();
    void onOverlayReleased();
    void updateComparisons();
    void removeSelectedSnapshot();
    void removeLastSnapshot();
    void onGlobalShortcutsBound();
    void openShortcutSettings();
    void chooseHighlightColor();
    void openConfigFile();
    void pollAutomation();

private:
    void buildUi();
    QScreen *selectedScreen() const;
    int bestMatchIndex(const QImage &compareScaled, const QSize &frameSize,
                       double *bestPercent) const;
    QScreen *screenForFrame(const QSize &frameSize) const;
    QRect hudExclusion(const QSize &targetSize, const QSize &frameSize) const;
    QRegion excludedRegion(const QSize &targetSize, const QSize &frameSize) const;
    void removeSnapshotAt(int row);
    void runAutomationCommand(const QString &command);
    void dumpAutomationState();

    ScreenCaptureService captureService_;
    GlobalHotkeys *hotkeys_ = nullptr;
    OverlayWindow overlay_;
    HudOverlay hud_;
    QTimer compareTimer_;
    QList<Snapshot> snapshots_;
    int snapshotCounter_ = 0;
    QList<double> lastPercents_;
    int bestIndex_ = -1;
    bool globalHotkeysBound_ = false;
    OverlayMode overlayMode_ = OverlayMode::Toggle;
    QColor highlightColor_ = QColor(255, 40, 40);
    QTimer overlayBlinkTimer_;
    bool overlayActive_ = false;
    int snapshotFKey_ = 5;
    int overlayFKey_ = 6;
    int deleteLastFKey_ = 0;
    QString overlayTriggerText_;
    QString automationControlPath_;
    QString automationStatePath_;

    QCheckBox *hudCheck_ = nullptr;
    QComboBox *screenBox_ = nullptr;
    QComboBox *overlayModeBox_ = nullptr;
    QPushButton *captureButton_ = nullptr;
    QPushButton *snapshotButton_ = nullptr;
    QPushButton *overlayButton_ = nullptr;
    QPushButton *deleteLastButton_ = nullptr;
    QLabel *previewLabel_ = nullptr;
    QSlider *thresholdSlider_ = nullptr;
    QLabel *thresholdValueLabel_ = nullptr;
    QSlider *hudSizeSlider_ = nullptr;
    QSlider *ignoreTopSlider_ = nullptr;
    QSlider *ignoreBottomSlider_ = nullptr;
    QPushButton *colorButton_ = nullptr;
    QTableWidget *snapshotTable_ = nullptr;
    QShortcut *localSnapshotShortcut_ = nullptr;
};
