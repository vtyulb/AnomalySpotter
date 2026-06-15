#pragma once

#include <QObject>

class GlobalHotkeys : public QObject {
    Q_OBJECT

public:
    static GlobalHotkeys *create(QObject *parent);

    using QObject::QObject;
    virtual void start() = 0;

    void setFunctionKeys(int snapshotFKey, int overlayFKey, int deleteLastFKey,
                         int resetTimerFKey) {
        snapshotFKey_ = snapshotFKey;
        overlayFKey_ = overlayFKey;
        deleteLastFKey_ = deleteLastFKey;
        resetTimerFKey_ = resetTimerFKey;
    }

signals:
    void snapshotRequested();
    void overlayPressed();
    void overlayReleased();
    void deleteLastRequested();
    void resetTimerRequested();
    void bound();
    void triggersChanged(const QString &snapshotTrigger, const QString &overlayTrigger,
                         const QString &deleteLastTrigger);
    void statusChanged(const QString &status);

protected:
    int snapshotFKey_ = 5;
    int overlayFKey_ = 6;
    int deleteLastFKey_ = 0;
    int resetTimerFKey_ = 0;
};
