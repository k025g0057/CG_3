#include "WInApp.h"
#include <cassert>

void WinApp::Initialize(HINSTANCE hInstance, int nCmdShow, int32_t clientWidth, int32_t clientHeight, WNDPROC windowProc) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    assert(SUCCEEDED(hr));

    wc_.lpfnWndProc = windowProc;
    wc_.lpszClassName = L"CG2WindowClass";
    wc_.hInstance = hInstance;
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc_);

    RECT wrc = { 0, 0, clientWidth, clientHeight };
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

void WinApp::Update() {
}