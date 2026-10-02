#pragma once
#include <cmath>
#include <utility>

// 構造体群
struct Vector4 { float x, y, z, w; };
struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };

struct Transform {
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

struct Matrix4x4 { float m[4][4]; };

// --- 数学関数 ---
inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
    Matrix4x4 result = { 0 };
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += m1.m[i][k] * m2.m[k][j];
            }
        }
    }
    return result;
}

inline Matrix4x4 Inverse(const Matrix4x4& m) {
    float a[4][8];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            a[i][j] = m.m[i][j];
            a[i][j + 4] = (i == j) ? 1.0f : 0.0f;
        }
    }
    for (int i = 0; i < 4; ++i) {
        int pivot = i;
        for (int j = i + 1; j < 4; ++j) {
            if (std::abs(a[j][i]) > std::abs(a[pivot][i])) pivot = j;
        }
        std::swap(a[i], a[pivot]);
        float temp = a[i][i];
        if (std::abs(temp) < 1e-6f) return { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        for (int j = 0; j < 8; ++j) a[i][j] /= temp;
        for (int j = 0; j < 4; ++j) {
            if (i != j) {
                float factor = a[j][i];
                for (int k = 0; k < 8; ++k) a[j][k] -= factor * a[i][k];
            }
        }
    }
    Matrix4x4 result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            result.m[i][j] = a[i][j + 4];
        }
    }
    return result;
}

inline Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
    return {
        scale.x, 0.0f, 0.0f, 0.0f,
        0.0f, scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, scale.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        translate.x, translate.y, translate.z, 1.0f
    };
}

inline Matrix4x4 MakeRotateXMatrix(float radian) {
    float s = std::sin(radian);
    float c = std::cos(radian);
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, c, s, 0.0f,
        0.0f, -s, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

inline Matrix4x4 MakeRotateYMatrix(float radian) {
    float s = std::sin(radian);
    float c = std::cos(radian);
    return {
        c, 0.0f, -s, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        s, 0.0f, c, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}


inline Matrix4x4 MakeRotateZMatrix(float radian) {
    float s = std::sin(radian);
    float c = std::cos(radian);
    return {
        c, s, 0.0f, 0.0f,
        -s, c, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

inline Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
    Matrix4x4 sMatrix = MakeScaleMatrix(scale);
    Matrix4x4 rXMatrix = MakeRotateXMatrix(rotate.x);
    Matrix4x4 rYMatrix = MakeRotateYMatrix(rotate.y);
    Matrix4x4 rZMatrix = MakeRotateZMatrix(rotate.z);
    Matrix4x4 tMatrix = MakeTranslateMatrix(translate);

    Matrix4x4 rXYZMatrix = Multiply(rXMatrix, Multiply(rYMatrix, rZMatrix));
    return Multiply(sMatrix, Multiply(rXYZMatrix, tMatrix));
}
