#include "Input.h"

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
    HRESULT hr;

    // DirectInputの初期化（DirectInputオブジェクトの生成）
    hr = DirectInput8Create(
        hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
        (void**)&directInput, nullptr);
    assert(SUCCEEDED(hr));

    // キーボードデバイスの生成
    hr = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
    assert(SUCCEEDED(hr));

    // 入力データ形式のセット
    hr = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
    assert(SUCCEEDED(hr));

    // ：排他制御レベルのセット
    hr = keyboard->SetCooperativeLevel(
        hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
}

void Input::Update() {
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