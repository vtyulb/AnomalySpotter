#include "WindowsHotkeys.h"

namespace {

WindowsHotkeys *g_instance = nullptr;

LRESULT CALLBACK lowLevelKeyboardProc(int code, WPARAM wParam, LPARAM lParam) {
    if (code == HC_ACTION && g_instance) {
        const auto *event = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        if (static_cast<int>(event->vkCode) == g_instance->snapshotVk()
            || static_cast<int>(event->vkCode) == g_instance->overlayVk()
            || (g_instance->deleteLastVk()
                && static_cast<int>(event->vkCode) == g_instance->deleteLastVk())) {
            const bool down = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            const bool up = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
            if (down || up) {
                g_instance->handleKey(event->vkCode, down);
                return 1;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

}

WindowsHotkeys::WindowsHotkeys(QObject *parent) : GlobalHotkeys(parent) {}

WindowsHotkeys::~WindowsHotkeys() {
    if (hook_)
        UnhookWindowsHookEx(hook_);
    if (g_instance == this)
        g_instance = nullptr;
}

void WindowsHotkeys::start() {
    g_instance = this;
    hook_ = SetWindowsHookExW(WH_KEYBOARD_LL, &lowLevelKeyboardProc,
                              GetModuleHandleW(nullptr), 0);
    if (!hook_) {
        g_instance = nullptr;
        emit statusChanged(QStringLiteral(
            "Failed to install the keyboard hook, F5/F6 work only inside the trainer window"));
        return;
    }
    emit bound();
    emit statusChanged(QStringLiteral("Global F%1/F%2 active")
                           .arg(snapshotFKey_)
                           .arg(overlayFKey_));
}

void WindowsHotkeys::handleKey(unsigned int virtualKey, bool down) {
    if (static_cast<int>(virtualKey) == snapshotVk()) {
        if (down && !snapshotDown_) {
            snapshotDown_ = true;
            emit snapshotRequested();
        } else if (!down) {
            snapshotDown_ = false;
        }
    } else if (static_cast<int>(virtualKey) == overlayVk()) {
        if (down && !overlayDown_) {
            overlayDown_ = true;
            emit overlayPressed();
        } else if (!down) {
            overlayDown_ = false;
            emit overlayReleased();
        }
    } else if (deleteLastVk() && static_cast<int>(virtualKey) == deleteLastVk()) {
        if (down && !deleteLastDown_) {
            deleteLastDown_ = true;
            emit deleteLastRequested();
        } else if (!down) {
            deleteLastDown_ = false;
        }
    }
}
