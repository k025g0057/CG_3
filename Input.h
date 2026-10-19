#pragma once

#define DIRECTINPUT_VERSION 0x0800

#include <Windows.h>
#include <wrl.h>
#include <dinput.h>
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Update();
    bool PushKey(BYTE keyNumber);

private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard;

    BYTE key[256] = {};
};