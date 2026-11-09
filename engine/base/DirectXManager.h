#pragma once

#include <array>
#include <chrono>
#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>
#include "DirectXTex.h"
#include "WinApp.h"

class DirectXManager {
public:
    void Initialize(WinApp* winApp, std::ofstream& logStream);
    void Finalize();
    void PreDraw();
    void PostDraw();
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(const std::wstring& filePath, const wchar_t* profile);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource(size_t sizeInBytes);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(const DirectX::TexMetadata& metadata);
    static DirectX::ScratchImage LoadTexture(const std::string& filePath);

    ID3D12Device* GetDevice() const { return device_.Get(); }
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>& GetCommandQueue() { return commandQueue_; }
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& GetCommandAllocator() { return commandAllocator_; }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    Microsoft::WRL::ComPtr<IDXGISwapChain4>& GetSwapChain() { return swapChain_; }

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& GetRtvDescriptorHeap() { return rtvDescriptorHeap_; }
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& GetDsvDescriptorHeap() { return dsvDescriptorHeap_; }
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& GetSrvDescriptorHeap() { return srvDescriptorHeap_; }

    uint32_t GetDescriptorSizeSRV() const { return descriptorSizeSRV_; }
    uint32_t GetDescriptorSizeRTV() const { return descriptorSizeRTV_; }
    uint32_t GetDescriptorSizeDSV() const { return descriptorSizeDSV_; }

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUDescriptorHandle(uint32_t index);

    Microsoft::WRL::ComPtr<ID3D12Resource>& GetSwapChainResource(uint32_t index) { return swapChainResources_[index]; }
    D3D12_CPU_DESCRIPTOR_HANDLE* GetRtvHandles() { return rtvHandles_; }

    Microsoft::WRL::ComPtr<ID3D12Fence>& GetFence() { return fence_; }
    uint64_t& GetFenceValue() { return fenceValue_; }
    HANDLE GetFenceEvent() const { return fenceEvent_; }

    Microsoft::WRL::ComPtr<ID3D12Resource>& GetDepthStencilResource() { return depthStencilResource_; }
    D3D12_VIEWPORT& GetViewport() { return viewport_; }
    D3D12_RECT& GetScissorRect() { return scissorRect_; }

private:
    void InitializeDevice(std::ofstream& logStream);
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDepthBuffer();
    void InitializeDepthStencilView();
    void InitializeDescriptorHeaps();
    void InitializeRenderTargetView();
    void InitializeFence();
    void InitializeViewport();
    void InitializeScissorRect();
    void InitializeDXC();
    void InitializeFixFPS();
    void UpdateFixFPS();
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(
        D3D12_DESCRIPTOR_HEAP_TYPE heapType,
        UINT numDescriptors,
        bool shaderVisible);
    static D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);
    static D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(
        const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap,
        uint32_t descriptorSize,
        uint32_t index);
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(int32_t width, int32_t height);
    void InitializeImGui();

    WinApp* winApp_ = nullptr;
    std::ofstream* logStream_ = nullptr;
    std::chrono::steady_clock::time_point reference_{};
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;

    uint32_t descriptorSizeSRV_ = 0;
    uint32_t descriptorSizeRTV_ = 0;
    uint32_t descriptorSizeDSV_ = 0;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> swapChainResources_{};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
};
