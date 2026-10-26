#pragma once

#define DIRECTINPUT_VERSION 0x0800

#include <Windows.h>
#include <wrl.h>
#include <dinput.h>
#include <cassert>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

#include "WinApp.h"

class Input {
public:
    void Initialize(WinApp* winApp);
    void Update();
    bool PushKey(BYTE keyNumber);
    bool TriggerKey(BYTE keyNumber);

private:
    WinApp* winApp_ = nullptr;
    Microsoft::WRL::ComPtr<IDirectInput8> directInput;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard;

    BYTE key[256] = {};     
    BYTE keyPre[256] = {};  

   
};