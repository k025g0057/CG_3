#pragma once

#define DIRECTINPUT_VERSION 0x0800 // DirectInputのバージョン指定

#include <windows.h> // ★ Windowsの基本型（HINSTANCE, HWNDなど）を読み込む
#include <dinput.h>
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

class Input {
public:
    // 初期化関数
    void Initialize(HINSTANCE hInstance, HWND hwnd);

    void Update();

    bool PushKey(BYTE keyNumber);

private:
    IDirectInput8* directInput = nullptr;
    IDirectInputDevice8* keyboard = nullptr;

    BYTE key[256] = {};
};