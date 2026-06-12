#include "X11Hotkeys.h"

#include <QCoreApplication>
#include <QGuiApplication>

#include <xcb/xcb.h>

#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>

X11Hotkeys::X11Hotkeys(QObject *parent) : GlobalHotkeys(parent) {}

void X11Hotkeys::start() {
    auto *x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    if (!x11) {
        emit statusChanged(QStringLiteral(
            "No X11 access, F5/F6 work only inside the trainer window"));
        return;
    }

    snapshotKeycode_ = XKeysymToKeycode(x11->display(), XK_F1 + (snapshotFKey_ - 1));
    overlayKeycode_ = XKeysymToKeycode(x11->display(), XK_F1 + (overlayFKey_ - 1));
    if (deleteLastFKey_ > 0)
        deleteLastKeycode_ = XKeysymToKeycode(x11->display(), XK_F1 + (deleteLastFKey_ - 1));
    if (!snapshotKeycode_ || !overlayKeycode_) {
        emit statusChanged(QStringLiteral(
            "Could not resolve F%1/F%2 keycodes, they work only inside the trainer window")
                               .arg(snapshotFKey_)
                               .arg(overlayFKey_));
        return;
    }

    Bool autoRepeatSupported = False;
    XkbSetDetectableAutoRepeat(x11->display(), True, &autoRepeatSupported);

    xcb_connection_t *connection = x11->connection();
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(connection)).data;
    xcb_grab_key(connection, 1, screen->root, XCB_MOD_MASK_ANY, snapshotKeycode_,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_grab_key(connection, 1, screen->root, XCB_MOD_MASK_ANY, overlayKeycode_,
                 XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    if (deleteLastKeycode_)
        xcb_grab_key(connection, 1, screen->root, XCB_MOD_MASK_ANY, deleteLastKeycode_,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    xcb_flush(connection);

    QCoreApplication::instance()->installNativeEventFilter(this);
    emit bound();
    emit statusChanged(QStringLiteral("Global F%1/F%2 active")
                           .arg(snapshotFKey_)
                           .arg(overlayFKey_));
}

bool X11Hotkeys::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) {
    if (eventType != "xcb_generic_event_t")
        return false;
    auto *event = static_cast<xcb_generic_event_t *>(message);
    const uint8_t type = event->response_type & ~0x80;
    if (type == XCB_KEY_PRESS) {
        const auto *keyEvent = reinterpret_cast<xcb_key_press_event_t *>(event);
        if (keyEvent->detail == snapshotKeycode_)
            emit snapshotRequested();
        else if (keyEvent->detail == overlayKeycode_)
            emit overlayPressed();
        else if (deleteLastKeycode_ && keyEvent->detail == deleteLastKeycode_)
            emit deleteLastRequested();
    } else if (type == XCB_KEY_RELEASE) {
        const auto *keyEvent = reinterpret_cast<xcb_key_release_event_t *>(event);
        if (keyEvent->detail == overlayKeycode_)
            emit overlayReleased();
    }
    return false;
}
