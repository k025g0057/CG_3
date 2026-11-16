#include "Sprite.h"
#include "SpriteCommon.h"
#include "../base/DirectXManager.h"
#include "../base/WinApp.h"
#include <cassert>

namespace {
Matrix4x4 MakeSpriteOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result{};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;
	return result;
}
}

void Sprite::Initialize(SpriteCommon* spriteCommon) {
	// 引数で受け取った共通部をメンバー変数に記録する。
	this->spriteCommon_ = spriteCommon;
	assert(spriteCommon_ != nullptr);

	CreateVertexData();
	CreateMaterial();
	CreateTransformationMatrix();
}

void Sprite::CreateVertexData() {
	DirectXManager* directXManager = spriteCommon_->GetDirectXManager();
	assert(directXManager != nullptr);
	constexpr uint32_t kVertexCount = 4;
	constexpr uint32_t kIndexCount = 6;

	vertexResource_ = directXManager->CreateBufferResource(sizeof(VertexData) * kVertexCount);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * kVertexCount;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	indexResource_ = directXManager->CreateBufferResource(sizeof(uint32_t) * kIndexCount);
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * kIndexCount;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
	vertexData_[0] = { { 0.0f, 360.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
	vertexData_[1] = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	vertexData_[2] = { { 640.0f, 360.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
	vertexData_[3] = { { 640.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;
}

void Sprite::CreateMaterial() {
	DirectXManager* directXManager = spriteCommon_->GetDirectXManager();
	materialResource_ = directXManager->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = false;
	materialData_->padding[0] = 0.0f;
	materialData_->padding[1] = 0.0f;
	materialData_->padding[2] = 0.0f;

	Matrix4x4 identityMatrix{};
	for (int index = 0; index < 4; ++index) {
		identityMatrix.m[index][index] = 1.0f;
	}
	materialData_->uvTransform = identityMatrix;
}

void Sprite::CreateTransformationMatrix() {
	DirectXManager* directXManager = spriteCommon_->GetDirectXManager();
	transformationMatrixResource_ = directXManager->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	Matrix4x4 identityMatrix{};
	for (int index = 0; index < 4; ++index) {
		identityMatrix.m[index][index] = 1.0f;
	}
	transformationMatrixData_->WVP = identityMatrix;
	transformationMatrixData_->World = identityMatrix;
}

void Sprite::Update() {
	// 頂点データ（四頂点）を書き込む。
	vertexData_[0] = { { 0.0f, 360.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
	vertexData_[1] = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
	vertexData_[2] = { { 640.0f, 360.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
	vertexData_[3] = { { 640.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

	// インデックスデータ（六要素）を書き込む。
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;

	const Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	Matrix4x4 viewMatrix{};
	for (int index = 0; index < 4; ++index) {
		viewMatrix.m[index][index] = 1.0f;
	}
	const Matrix4x4 projectionMatrix = MakeSpriteOrthographicMatrix(
		0.0f, 0.0f,
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight),
		0.0f, 100.0f
	);

	transformationMatrixData_->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData_->World = worldMatrix;
}

void Sprite::Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU, D3D12_GPU_VIRTUAL_ADDRESS directionalLightAddress) {
	ID3D12GraphicsCommandList* commandList = spriteCommon_->GetDirectXManager()->GetCommandList();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
	commandList->SetGraphicsRootConstantBufferView(3, directionalLightAddress);
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
