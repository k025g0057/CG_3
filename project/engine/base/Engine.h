#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <fstream>   
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include "DirectXTex.h"
#include <vector>
#include <wrl.h>
#include "Sound.h"
#include "WinApp.h"
#include "DirectXManager.h"
#include "MathTypes.h"
#include "DebugCamera.h"

using Microsoft::WRL::ComPtr;

class Input;

struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};

struct TransformationMatrix {
    Matrix4x4 wvp;
    Matrix4x4 World;
};

struct Particle {
    Transform transform;
    Vector3 velocity;
    float tailPhase = 0.0f;
    float size = 0.04f;
    float life = 0.0f;
    float maxLife = 0.0f;
};

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

struct MaterialData {
    std::string textureFilePath;
};

struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    // エンジンの初期化
    void Initialize(HINSTANCE hInstance, int nCmdShow, DirectXManager* directXManager);
    
    // メインループの実行
    void Run();
    
    // エンジンの終了処理
    void Finalize();

    // ウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    DebugCamera* GetDebugCamera() { return &debugCamera_; } 

private:
    // 内部初期化ステップ
    void InitializeLog();
    void InitializePipeline();
    void InitializeResources();
    void ResetMeteorParticles();

    // ループ処理内部
    void Update();
    void Draw();

    // ヘルパー関数
    
    ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

private:
    // ウィンドウ関連
    WinApp winApp_;
    DirectXManager* directXManager_ = nullptr;

    // ログ関連
    std::ofstream logStream_;

    // Pipeline 関連
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12RootSignature> particleRootSignature_;
    ComPtr<ID3D12PipelineState> graphicsPipelineState_;
    ComPtr<ID3D12PipelineState> legacyParticlePipelineState_;

    // 描画リソース・状態
    ModelData modelData_;
    ComPtr<ID3D12Resource> vertexResourcePlane_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewPlane_{};

    ComPtr<ID3D12Resource> vertexResourceSphere_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSphere_{};
    uint32_t kNumSphereVertices_ = 0;

    ComPtr<ID3D12Resource> vertexResourceSprite_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite_{};
    ComPtr<ID3D12Resource> indexResourceSprite_;
    D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite_{};

    ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    ComPtr<ID3D12Resource> materialResourceSprite_;
    Material* materialDataSprite_ = nullptr;

    ComPtr<ID3D12Resource> wvpResource_;
    TransformationMatrix* wvpData_ = nullptr;

    ComPtr<ID3D12Resource> instancingResource_;
    TransformationMatrix* instancingData_ = nullptr;

    static constexpr uint32_t kNumInstance = 640;
    static constexpr uint32_t kNumLegacyInstance = 10;
    static constexpr uint32_t kNumHeadParticle = 32;
    static constexpr uint32_t kNumTailStrands = 6;
    Particle particles[kNumInstance]{};

    ComPtr<ID3D12Resource> transformationMatrixResourceSprite_;
    TransformationMatrix* transformationMatrixDataSprite_ = nullptr;

    ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;

    ComPtr<ID3D12Resource> textureResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_{};

    D3D12_GPU_DESCRIPTOR_HANDLE instancingSrvHandleGPU_{};

    ComPtr<ID3D12Resource> textureResource2_;
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2_{};

    // ゲーム状態・UI制御用変数
    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Transform transformSprite_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };
    Transform uvTransformSprite_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Vector3 cameraRotateDeg_ = { 0.0f, 0.0f, 0.0f };
    Vector3 modelRotateDeg_ = { 0.0f, 0.0f, 0.0f };
    Matrix4x4 projectionMatrix_{};

    bool drawMeteorHead_ = true;
    bool updateParticles_ = true;
    bool useMonsterBall_ = true;
    int renderMode_ = 1; // 0: 以前の10枚描画、1: 流れ星
    bool cometExploded_ = false;
    float effectTime_ = 0.0f;

    Sound sound;
    SoundData soundData1;

    Input* input = nullptr;

    DebugCamera debugCamera_;

    ComPtr<ID3D12PipelineState> particlePipelineState_;
};
