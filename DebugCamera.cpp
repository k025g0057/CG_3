#include "DebugCamera.h"
#include "Input.h"
#include <cmath>
#include <algorithm>

void DebugCamera::Initialize(Input* input) {
    input_ = input;
    GetCursorPos(&prevMousePos_);

    // 起動時の初期表示位置（注視点からの距離と角度で設定）
    translation_.x = target_.x - distance_ * std::sin(rotation_.y) * std::cos(rotation_.x);
    translation_.y = target_.y + distance_ * std::sin(rotation_.x);
    translation_.z = target_.z - distance_ * std::cos(rotation_.y) * std::cos(rotation_.x);
}

void DebugCamera::Update() {
    // ==================================================
    // ① マウス移動量の計算
    // ==================================================
    POINT currentPos;
    GetCursorPos(&currentPos);

    float deltaX = static_cast<float>(currentPos.x - prevMousePos_.x);
    float deltaY = static_cast<float>(currentPos.y - prevMousePos_.y);

    prevMousePos_ = currentPos;

    const float rotSpeed = 0.005f; // 回転速度

    // ==================================================
    // ②-1 【左クリックドラッグ】カメラ自身を中心に回転（自転 / その場で首振り）
    // ==================================================
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
        rotation_.y += deltaX * rotSpeed; // 左右に首を振る
        rotation_.x += deltaY * rotSpeed; // 上下に首を振る

        // 上下角度の制限
        const float kMaxPitch = 1.55f;
        if (rotation_.x > kMaxPitch) rotation_.x = kMaxPitch;
        if (rotation_.x < -kMaxPitch) rotation_.x = -kMaxPitch;

        // ★位置（translation_）は動かさないため、カメラ自身を中心にその場で回転します
    }

    // ==================================================
    // ②-2 【右クリックドラッグ】中央のオブジェクトを中心に回り込む（公転 / オービット）
    // ==================================================
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
        rotation_.y += deltaX * rotSpeed; // 左右回り込み
        rotation_.x += deltaY * rotSpeed; // 上下回り込み

        // 上下角度の制限
        const float kMaxPitch = 1.55f;
        if (rotation_.x > kMaxPitch) rotation_.x = kMaxPitch;
        if (rotation_.x < -kMaxPitch) rotation_.x = -kMaxPitch;

        // ★右クリック時のみ、中央のオブジェクトを中心とした球面座標にカメラ座標（translation_）を更新
        translation_.x = target_.x - distance_ * std::sin(rotation_.y) * std::cos(rotation_.x);
        translation_.y = target_.y + distance_ * std::sin(rotation_.x);
        translation_.z = target_.z - distance_ * std::cos(rotation_.y) * std::cos(rotation_.x);
    }

    // ==================================================
    // ③ W / S キーによるズームイン・ズームアウト（距離の変更）
    // ==================================================
    const float zoomSpeed = 0.2f;
    if (GetAsyncKeyState('W') & 0x8000) { distance_ -= zoomSpeed; } // 近づく
    if (GetAsyncKeyState('S') & 0x8000) { distance_ += zoomSpeed; } // 離れる
    if (distance_ < 1.0f) { distance_ = 1.0f; }                     // 最小距離制限

    // ==================================================
    // ④ ビュー行列の更新
    // ==================================================
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
    Matrix4x4 cameraWorldMatrix = MakeAffineMatrix(scale, rotation_, translation_);

    // ワールド行列の逆行列を計算してビュー行列にセット
    viewMatrix_ = Inverse(cameraWorldMatrix);
}