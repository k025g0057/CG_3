#pragma once
#include "MathTypes.h"
#include <Windows.h>

class Input;

class DebugCamera {
public:
    // 初期化
    void Initialize(Input* input);
    // 更新
    void Update();

    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }

    // 注視点や距離を外部から変更したい場合用
    void SetTarget(const Vector3& target) { target_ = target; }
    void SetDistance(float distance) { distance_ = distance; }

private:
    Input* input_ = nullptr;

    // カメラの回転角 (X: Pitch / 上下, Y: Yaw / 左右)
    Vector3 rotation_ = { 0.26f, 0.0f, 0.0f };

    // 注視点（カメラが見つめ続ける中心座標）
    Vector3 target_ = { 0.0f, 0.0f, 0.0f };

    // 注視点からの距離
    float distance_ = 10.0f;

    // カメラのワールド座標
    Vector3 translation_ = { 0.0f, 0.0f, 0.0f };

    Matrix4x4 viewMatrix_;
    POINT prevMousePos_ = { 0, 0 };
};