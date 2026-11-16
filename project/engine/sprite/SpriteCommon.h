#pragma once

#include <d3d12.h>
#include <wrl.h>

class DirectXManager;

// スプライト共通部
class SpriteCommon {
public:
	// 初期化
	void Initialize(DirectXManager* directXManager);

	// Spriteごとの描画前に共通設定を行う
	void CommonDrawSetting();

	// SpriteからDirectXManagerを参照するためのgetter
	DirectXManager* GetDirectXManager() const { return directXManager_; }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィックスパイプラインの生成
	void CreateGraphicsPipelineState();

	// WinMainが所有するDirectXManagerを借りて使う（new/deleteしない）
	DirectXManager* directXManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_;
};
