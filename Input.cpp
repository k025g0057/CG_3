#include "Input.h"
#include<cassert>
#include <cstring>

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
    HRESULT hr;

    hr = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(directInput.ReleaseAndGetAddressOf()),
        nullptr);
    assert(SUCCEEDED(hr));

    hr = directInput->CreateDevice(
        GUID_SysKeyboard,
        keyboard.ReleaseAndGetAddressOf(),
        nullptr);
    assert(SUCCEEDED(hr));

    hr = keyboard->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    hr = keyboard->SetCooperativeLevel(
        hwnd,
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
}

void Input::Update() {
    std::memcpy(keyPre, key, sizeof(key));
    keyboard->Acquire();
    keyboard->GetDeviceState(sizeof(key), key);
}

bool Input::PushKey(BYTE keyNumber) {
    // 指定されたキーが押されていれば true を返す
    if (key[keyNumber]) {
        return true;
    }
    // 押されていなければ false を返す
    return false;
}

bool Input::TriggerKey(BYTE keyNumber) {
    const bool wasPressed = (keyPre[keyNumber] & 0x80) != 0;
    const bool isPressed = (key[keyNumber] & 0x80) != 0;

    return !wasPressed && isPressed;
}