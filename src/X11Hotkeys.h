#pragma once

#include "GlobalHotkeys.h"

#include <QAbstractNativeEventFilter>

class X11Hotkeys : public GlobalHotkeys, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit X11Hotkeys(QObject *parent = nullptr);
    void start() override;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    quint32 snapshotKeycode_ = 0;
    quint32 overlayKeycode_ = 0;
    quint32 deleteLastKeycode_ = 0;
};
