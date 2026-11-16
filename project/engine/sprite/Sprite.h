#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>
#include "../base/MathTypes.h"

class SpriteCommon;

// スプライト
class Sprite {
    // 頂点データ
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // マテリアルデータ
    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

public:
	// 初期化
	void Initialize(SpriteCommon* spriteCommon);
	// Spriteの頂点データと座標変換行列を更新する
	void Update();
	// Spriteを描画する
	void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU, D3D12_GPU_VIRTUAL_ADDRESS directionalLightAddress);

	// 既存UIからSpriteのマテリアルを操作する
	Vector4& GetColor() { return materialData_->color; }
	void SetUVTransform(const Matrix4x4& uvTransform) { materialData_->uvTransform = uvTransform; }
	Transform& GetTransform() { return transform_; }

private:
	// Spriteの頂点・インデックスデータを作成する
	void CreateVertexData();
	// Spriteのマテリアルを作成する
	void CreateMaterial();
	// Spriteの座標変換行列を作成する
	void CreateTransformationMatrix();

	SpriteCommon* spriteCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
};
