#include "GlobalHotkeys.h"

#if defined(Q_OS_WIN)
#include "WindowsHotkeys.h"
#else
#include "PortalHotkeys.h"
#ifdef HAVE_X11_HOTKEYS
#include "X11Hotkeys.h"
#endif
#include <QGuiApplication>
#endif

GlobalHotkeys *GlobalHotkeys::create(QObject *parent) {
#if defined(Q_OS_WIN)
    return new WindowsHotkeys(parent);
#else
#ifdef HAVE_X11_HOTKEYS
    if (QGuiApplication::platformName() == QLatin1String("xcb"))
        return new X11Hotkeys(parent);
#endif
    return new PortalHotkeys(parent);
#endif
}
