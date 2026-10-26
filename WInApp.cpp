#include "WinApp.h"
#include <cassert>

const int32_t WinApp::kClientWidth;
const int32_t WinApp::kClientHeight;

void WinApp::Initialize(HINSTANCE hInstance, int nCmdShow, WNDPROC windowProc) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    assert(SUCCEEDED(hr));

    wc_.lpfnWndProc = windowProc;
    wc_.lpszClassName = L"CG2WindowClass";
    wc_.hInstance = hInstance;
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc_);

    RECT wrc = { 0, 0, kClientWidth, kClientHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    hwnd_ = CreateWindow(
        wc_.lpszClassName,
        L"CG2",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc_.hInstance,
        nullptr);

    ShowWindow(hwnd_, nCmdShow);
}

bool WinApp::ProcessMessage() {
    MSG msg{};
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (msg.message == WM_QUIT) {
        return true;
    }

    return false;
}

void WinApp::Finalize() {
    CloseWindow(hwnd_);
    CoUninitialize();
}
