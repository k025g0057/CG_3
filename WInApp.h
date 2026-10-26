#pragma once

#include <Windows.h>
#include <cstdint>

class WinApp {
public:
    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;

    void Initialize(HINSTANCE hInstance, int nCmdShow, WNDPROC windowProc);
    void Update();
    void Finalize();

    HWND GetHwnd() const { return hwnd_; }
    HINSTANCE GetHInstance() const { return wc_.hInstance; }

private:
    HWND hwnd_ = nullptr;
    WNDCLASS wc_{};
};
