#include "PortalHotkeys.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>

namespace {

struct PortalShortcut {
    QString id;
    QVariantMap properties;
};

QDBusArgument &operator<<(QDBusArgument &argument, const PortalShortcut &shortcut) {
    argument.beginStructure();
    argument << shortcut.id << shortcut.properties;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalShortcut &shortcut) {
    argument.beginStructure();
    argument >> shortcut.id >> shortcut.properties;
    argument.endStructure();
    return argument;
}

const char *kPortalService = "org.freedesktop.portal.Desktop";
const char *kPortalPath = "/org/freedesktop/portal/desktop";
const char *kShortcutsInterface = "org.freedesktop.portal.GlobalShortcuts";
const char *kRequestInterface = "org.freedesktop.portal.Request";
const char *kRegistryInterface = "org.freedesktop.host.portal.Registry";
const char *kAppId = "anomaly-spotter";
constexpr QLatin1StringView kSnapshotShortcutId("save-snapshot");
constexpr QLatin1StringView kOverlayShortcutId("toggle-overlay");
constexpr QLatin1StringView kDeleteLastShortcutId("delete-last");

void extractTriggers(const QList<PortalShortcut> &shortcuts, QString *snapshotTrigger,
                     QString *overlayTrigger, QString *deleteLastTrigger) {
    for (const PortalShortcut &shortcut : shortcuts) {
        const QString trigger =
            shortcut.properties.value(QStringLiteral("trigger_description")).toString();
        if (shortcut.id == kSnapshotShortcutId)
            *snapshotTrigger = trigger;
        else if (shortcut.id == kOverlayShortcutId)
            *overlayTrigger = trigger;
        else if (shortcut.id == kDeleteLastShortcutId)
            *deleteLastTrigger = trigger;
    }
}

}

Q_DECLARE_METATYPE(PortalShortcut)

PortalHotkeys::PortalHotkeys(QObject *parent)
    : GlobalHotkeys(parent),
      bus_(QDBusConnection::connectToBus(QDBusConnection::SessionBus,
                                         QStringLiteral("anomaly-spotter-portal"))) {
    qDBusRegisterMetaType<PortalShortcut>();
    qDBusRegisterMetaType<QList<PortalShortcut>>();
}

QString PortalHotkeys::listenForResponse(const char *slot) {
    const QString token = QStringLiteral("anomalyspotter%1").arg(++tokenCounter_);
    QString sender = bus_.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString requestPath =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    bus_.connect(kPortalService, requestPath, kRequestInterface,
                 QStringLiteral("Response"), this, slot);
    return token;
}

void PortalHotkeys::registerAppId() {
    QDBusMessage message = QDBusMessage::createMethodCall(
        kPortalService, kPortalPath, kRegistryInterface, QStringLiteral("Register"));
    message << QString::fromLatin1(kAppId) << QVariantMap{};
    bus_.asyncCall(message);
}

void PortalHotkeys::start() {
    registerAppId();
    const QString token = listenForResponse(SLOT(onCreateSessionResponse(uint,QVariantMap)));
    QDBusMessage message = QDBusMessage::createMethodCall(
        kPortalService, kPortalPath, kShortcutsInterface, QStringLiteral("CreateSession"));
    message << QVariantMap{
        {QStringLiteral("handle_token"), token},
        {QStringLiteral("session_handle_token"), QStringLiteral("anomalyspotter")}};
    bus_.asyncCall(message);
    emit statusChanged(QStringLiteral("Registering global F%1/F%2 via the portal...")
                           .arg(snapshotFKey_)
                           .arg(overlayFKey_));
}

void PortalHotkeys::onCreateSessionResponse(uint code, const QVariantMap &results) {
    if (code != 0) {
        emit statusChanged(QStringLiteral(
            "Portal rejected global hotkeys, F5/F6 work only inside the trainer window"));
        return;
    }
    sessionHandle_ =
        QDBusObjectPath(results.value(QStringLiteral("session_handle")).toString());
    bus_.connect(
        kPortalService, kPortalPath, kShortcutsInterface, QStringLiteral("Activated"), this,
        SLOT(onActivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    bus_.connect(
        kPortalService, kPortalPath, kShortcutsInterface, QStringLiteral("Deactivated"), this,
        SLOT(onDeactivated(QDBusObjectPath,QString,qulonglong,QVariantMap)));
    bus_.connect(
        kPortalService, kPortalPath, kShortcutsInterface, QStringLiteral("ShortcutsChanged"),
        this, SLOT(onShortcutsChanged(QDBusMessage)));
    bindShortcuts();
}

void PortalHotkeys::bindShortcuts() {
    const QString token = listenForResponse(SLOT(onBindResponse(uint,QVariantMap)));
    QList<PortalShortcut> shortcuts;
    shortcuts.append(
        {QString(kSnapshotShortcutId),
         {{QStringLiteral("description"), QStringLiteral("Save room snapshot")},
          {QStringLiteral("preferred_trigger"), QStringLiteral("F%1").arg(snapshotFKey_)}}});
    shortcuts.append(
        {QString(kOverlayShortcutId),
         {{QStringLiteral("description"), QStringLiteral("Show/hide differences")},
          {QStringLiteral("preferred_trigger"), QStringLiteral("F%1").arg(overlayFKey_)}}});
    QVariantMap deleteLastProperties{
        {QStringLiteral("description"), QStringLiteral("Delete last snapshot")}};
    if (deleteLastFKey_ > 0)
        deleteLastProperties.insert(QStringLiteral("preferred_trigger"),
                                    QStringLiteral("F%1").arg(deleteLastFKey_));
    shortcuts.append({QString(kDeleteLastShortcutId), deleteLastProperties});

    QDBusMessage message = QDBusMessage::createMethodCall(
        kPortalService, kPortalPath, kShortcutsInterface, QStringLiteral("BindShortcuts"));
    message << QVariant::fromValue(sessionHandle_) << QVariant::fromValue(shortcuts)
            << QString() << QVariantMap{{QStringLiteral("handle_token"), token}};
    bus_.asyncCall(message);
}

void PortalHotkeys::onBindResponse(uint code, const QVariantMap &results) {
    if (code != 0) {
        emit statusChanged(QStringLiteral(
            "Failed to bind global hotkeys, they work only inside the trainer window"));
        return;
    }
    emit bound();

    QString snapshotTrigger, overlayTrigger, deleteLastTrigger;
    const QVariant raw = results.value(QStringLiteral("shortcuts"));
    if (raw.canConvert<QDBusArgument>())
        extractTriggers(qdbus_cast<QList<PortalShortcut>>(raw.value<QDBusArgument>()),
                        &snapshotTrigger, &overlayTrigger, &deleteLastTrigger);
    if (!snapshotTrigger.isEmpty() || !overlayTrigger.isEmpty())
        emit triggersChanged(snapshotTrigger, overlayTrigger, deleteLastTrigger);
    emit statusChanged(QStringLiteral("Global hotkeys active"));
}

void PortalHotkeys::onShortcutsChanged(const QDBusMessage &message) {
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2
        || arguments.at(0).value<QDBusObjectPath>() != sessionHandle_)
        return;
    QString snapshotTrigger, overlayTrigger, deleteLastTrigger;
    const QVariant raw = arguments.at(1);
    if (raw.canConvert<QDBusArgument>())
        extractTriggers(qdbus_cast<QList<PortalShortcut>>(raw.value<QDBusArgument>()),
                        &snapshotTrigger, &overlayTrigger, &deleteLastTrigger);
    if (!snapshotTrigger.isEmpty() || !overlayTrigger.isEmpty())
        emit triggersChanged(snapshotTrigger, overlayTrigger, deleteLastTrigger);
}

void PortalHotkeys::onActivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId,
                                qulonglong, const QVariantMap &) {
    if (sessionHandle != sessionHandle_)
        return;
    if (shortcutId == kSnapshotShortcutId)
        emit snapshotRequested();
    else if (shortcutId == kOverlayShortcutId)
        emit overlayPressed();
    else if (shortcutId == kDeleteLastShortcutId)
        emit deleteLastRequested();
}

void PortalHotkeys::onDeactivated(const QDBusObjectPath &sessionHandle, const QString &shortcutId,
                                  qulonglong, const QVariantMap &) {
    if (sessionHandle != sessionHandle_)
        return;
    if (shortcutId == kOverlayShortcutId)
        emit overlayReleased();
}
