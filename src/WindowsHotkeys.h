#pragma once

#include "GlobalHotkeys.h"

#include <windows.h>

class WindowsHotkeys : public GlobalHotkeys {
    Q_OBJECT

public:
    explicit WindowsHotkeys(QObject *parent = nullptr);
    ~WindowsHotkeys() override;
    void start() override;

    void handleKey(unsigned int virtualKey, bool down);
    int snapshotVk() const { return VK_F1 + (snapshotFKey_ - 1); }
    int overlayVk() const { return VK_F1 + (overlayFKey_ - 1); }
    int deleteLastVk() const { return deleteLastFKey_ > 0 ? VK_F1 + (deleteLastFKey_ - 1) : 0; }

private:
    HHOOK hook_ = nullptr;
    bool snapshotDown_ = false;
    bool overlayDown_ = false;
    bool deleteLastDown_ = false;
};
