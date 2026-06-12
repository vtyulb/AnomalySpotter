#pragma once

#include "GlobalHotkeys.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QVariantMap>

class PortalHotkeys : public GlobalHotkeys {
    Q_OBJECT

public:
    explicit PortalHotkeys(QObject *parent = nullptr);
    void start() override;

private slots:
    void onCreateSessionResponse(uint code, const QVariantMap &results);
    void onBindResponse(uint code, const QVariantMap &results);
    void onShortcutsChanged(const QDBusMessage &message);
    void onActivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId,
                     qulonglong timestamp, const QVariantMap &options);
    void onDeactivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId,
                       qulonglong timestamp, const QVariantMap &options);

private:
    QString listenForResponse(const char *slot);
    void registerAppId();
    void bindShortcuts();

    QDBusConnection bus_;
    QDBusObjectPath sessionHandle_;
    int tokenCounter_ = 0;
};
