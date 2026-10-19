#pragma once

#include <Windows.h>
#include <cstdint>

class WinApp {
public:
    void Initialize(HINSTANCE hInstance, int nCmdShow, int32_t clientWidth, int32_t clientHeight, WNDPROC windowProc);
    void Update();

    HWND GetHwnd() const { return hwnd_; }

private:
    HWND hwnd_ = nullptr;
    WNDCLASS wc_{};
};