#include "MainWindow.h"

#include <QApplication>
#include <QIcon>
#include <QSettings>

#if defined(Q_OS_LINUX)
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <climits>
#include <unistd.h>

static std::string selfExecutablePath() {
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    return std::string(buf);
}

static void installDesktopFileForPortalAppId() {
    const char *sessionType = getenv("XDG_SESSION_TYPE");
    if (!sessionType || std::string(sessionType) != "wayland")
        return;
    if (getenv("AS_CONTROL_FILE"))
        return;
    const std::string self = selfExecutablePath();
    if (self.empty())
        return;
    const char *home = getenv("HOME");
    if (!home)
        return;
    std::filesystem::path path =
        std::filesystem::path(home) / ".local/share/applications/anomaly-spotter.desktop";
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path);
    if (!out)
        return;
    out << "[Desktop Entry]\n"
           "Type=Application\n"
           "Name=AnomalySpotter\n"
           "GenericName=Anomaly Trainer\n"
           "Comment=Anomaly-spotting practice tool for Observation Duty-style games\n"
           "Exec="
        << self
        << "\n"
           "Icon=anomaly-spotter\n"
           "Terminal=false\n"
           "Categories=Utility;Game;\n";
}
#endif

int main(int argc, char **argv) {
#if defined(Q_OS_LINUX)
    installDesktopFileForPortalAppId();
#endif

    QApplication app(argc, argv);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QApplication::setOrganizationName(QStringLiteral("anomaly-spotter"));
    QApplication::setApplicationName(QStringLiteral("anomaly-spotter"));
    QApplication::setApplicationDisplayName(QStringLiteral("AnomalySpotter"));
    QApplication::setDesktopFileName(QStringLiteral("anomaly-spotter"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/assets/icon.png")));

    MainWindow window;
    window.show();
    return app.exec();
}
