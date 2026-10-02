#pragma once

#include <Windows.h>
#include <cstdint>
#include <string>
#include <filesystem>
#include <fstream>   
#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dbghelp.h>
#include <strsafe.h>
#include <format>
#include <dxgidebug.h>
#include <dxcapi.h>
#include "externals/DirectXTex/DirectXTex.h"
#include "externals/DirectXTex/d3dx12.h"
#include <vector>
#include <numbers>
#include <sstream>
#include <wrl.h>
#include "Sound.h"
#include "Input.h"
#include "MathTypes.h"
#include "DebugCamera.h"

using Microsoft::WRL::ComPtr;

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif


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

struct D3DResourceLeakChecker {
    ~D3DResourceLeakChecker();
};

class Engine {
public:
    Engine() = default;
    ~Engine() = default;

    // エンジンの初期化
    void Initialize(HINSTANCE hInstance, int nCmdShow, int32_t width = 1280, int32_t height = 720);
    
    // メインループの実行
    void Run();
    
    // エンジンの終了処理
    void Finalize();

    // ウィンドウプロシージャ
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    DebugCamera* GetDebugCamera() { return &debugCamera_; } 

private:
    // 内部初期化ステップ
    void InitializeWindow(HINSTANCE hInstance, int nCmdShow);
    void InitializeLog();
    void InitializeDirectX();
    void InitializePipeline();
    void InitializeResources();
    void InitializeImGui();

    // ループ処理内部
    void Update();
    void Draw();

    // ヘルパー関数
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible);
    ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);
    
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index);
    
    ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);
    DirectX::ScratchImage LoadTexture(const std::string& filePath);
    ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

private:
    // ウィンドウ関連
    HWND hwnd_ = nullptr;
    WNDCLASS wc_{};
    int32_t kClientWidth_ = 1280;
    int32_t kClientHeight_ = 720;

    // ログ関連
    std::ofstream logStream_;

    // DirectX12 関連オブジェクト
    ComPtr<IDXGIFactory7> dxgiFactory_;
    ComPtr<IDXGIAdapter4> useAdapter_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> commandQueue_;
    ComPtr<ID3D12CommandAllocator> commandAllocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<IDXGISwapChain4> swapChain_;
    
    ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;

    uint32_t descriptorSizeSRV_ = 0;
    uint32_t descriptorSizeRTV_ = 0;
    uint32_t descriptorSizeDSV_ = 0;

    ComPtr<ID3D12Resource> swapChainResources_[2];
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};

    ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // DXC / Pipeline 関連
    ComPtr<IDxcUtils> dxcUtils_;
    ComPtr<IDxcCompiler3> dxcCompiler_;
    ComPtr<IDxcIncludeHandler> includeHandler_;
    ComPtr<ID3D12RootSignature> rootSignature_;
    ComPtr<ID3D12PipelineState> graphicsPipelineState_;

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

    ComPtr<ID3D12Resource> transformationMatrixResourceSprite_;
    TransformationMatrix* transformationMatrixDataSprite_ = nullptr;

    ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;

    ComPtr<ID3D12Resource> textureResource_;
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU_{};

    ComPtr<ID3D12Resource> textureResource2_;
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2_{};

    ComPtr<ID3D12Resource> depthStencilResource_;

    // ビューポート・シザー
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    // ゲーム状態・UI制御用変数
    Transform transform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Transform transformSprite_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Transform cameraTransform_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -5.0f} };
    Transform uvTransformSprite_{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Vector3 cameraRotateDeg_ = { 0.0f, 0.0f, 0.0f };
    Vector3 modelRotateDeg_ = { 0.0f, 0.0f, 0.0f };
    Matrix4x4 projectionMatrix_{};

    bool drawSphere_ = false;
    bool useMonsterBall_ = true;

    Sound sound;
    SoundData soundData1;

    Input input;

    DebugCamera debugCamera_;
};
