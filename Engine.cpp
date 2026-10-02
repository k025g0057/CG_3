// ★ 1. まずマクロを定義する
#define USE_IMGUI 

// ★ 2. その後に Engine.h を読み込む
#include "Engine.h"

// ★ 3. ImGuiに必要なヘッダーファイルとWndProcハンドラを読み込む
#ifdef USE_IMGUI

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include <cmath>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxcompiler.lib")

// --- 行列・数学関数 ---

Matrix4x4 MakeIdentity4x4() {
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}



Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float nearZ, float farZ) {
    float h = 1.0f / std::tan(fovY / 2.0f);
    float w = h / aspect;
    return Matrix4x4{
        w, 0, 0, 0,
        0, h, 0, 0,
        0, 0, farZ / (farZ - nearZ), 1,
        0, 0, (-nearZ * farZ) / (farZ - nearZ), 0
    };
}

Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
    Matrix4x4 result = {};
    result.m[0][0] = 2.0f / (right - left);
    result.m[1][1] = 2.0f / (top - bottom);
    result.m[2][2] = 1.0f / (farClip - nearClip);
    result.m[3][0] = (left + right) / (left - right);
    result.m[3][1] = (top + bottom) / (bottom - top);
    result.m[3][2] = nearClip / (nearClip - farClip);
    result.m[3][3] = 1.0f;
    return result;
}

// ユーティリティ
std::wstring ConvertString(const std::string& str) {
    auto size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring strTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed);
    return strTo;
}

std::string ConvertString(const std::wstring& str) {
    auto size_needed = WideCharToMultiByte(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_ACP, 0, &str[0], (int)str.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

void Log(std::ostream& os, const std::string& message) {
    os << message << std::endl;
    OutputDebugStringA(message.c_str());
}

void Log(std::ostream& os, const std::wstring& message) {
    std::string str = ConvertString(message);
    os << str << std::endl;
    OutputDebugStringA(str.c_str());
}

static LONG WINAPI ExportDump(EXCEPTION_POINTERS* exception) {
    SYSTEMTIME time;
    GetLocalTime(&time);

    wchar_t filePath[MAX_PATH] = { 0 };
    CreateDirectory(L"./Dumps", nullptr);
    StringCchPrintfW(filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);

    HANDLE dumpFileHandle = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);

    DWORD processId = GetCurrentProcessId();
    DWORD threadId = GetCurrentThreadId();

    MINIDUMP_EXCEPTION_INFORMATION minidumpInformation{ 0 };
    minidumpInformation.ThreadId = threadId;
    minidumpInformation.ExceptionPointers = exception;
    minidumpInformation.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(), processId, dumpFileHandle, MiniDumpNormal, &minidumpInformation, nullptr, nullptr);
    return EXCEPTION_EXECUTE_HANDLER;
}

D3DResourceLeakChecker::~D3DResourceLeakChecker() {
    ComPtr<IDXGIDebug1> debug;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
        debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
        debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
    }
}

// --- Engine クラスの実装 ---

LRESULT CALLBACK Engine::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }
#endif
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Engine::Initialize(HINSTANCE hInstance, int nCmdShow, int32_t width, int32_t height) {
    kClientWidth_ = width;
    kClientHeight_ = height;

    SetUnhandledExceptionFilter(ExportDump);
    CoInitializeEx(0, COINIT_MULTITHREADED);

    InitializeLog();
    InitializeWindow(hInstance, nCmdShow);
    InitializeDirectX();
    InitializePipeline();
    InitializeResources();
    InitializeImGui();

    // サウンドの初期化と読み込み
    sound.Initialize();
    soundData1 = sound.SoundLoadWave("Resources/botan.wav");

    input.Initialize(wc_.hInstance, hwnd_);
    debugCamera_.Initialize(&input);
}

void Engine::InitializeLog() {
    std::filesystem::create_directory("logs");
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    logStream_.open(logFilePath);

    Log(logStream_, "Application Started");
}

void Engine::InitializeWindow(HINSTANCE hInstance, int nCmdShow) {
    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"CG2WindowClass";
    wc_.hInstance = hInstance;
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc_);

    RECT wrc = { 0, 0, kClientWidth_, kClientHeight_ };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    hwnd_ = CreateWindow(
        wc_.lpszClassName,
        L"CG2",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc_.hInstance,
        nullptr);
    
    ShowWindow(hwnd_, nCmdShow);
}

void Engine::InitializeDirectX() {
#ifdef _DEBUG
    ComPtr<ID3D12Debug1> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        debugController->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    assert(SUCCEEDED(hr));

    useAdapter_ = nullptr;
    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter_)) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC3 adapterDesc{};
        hr = useAdapter_->GetDesc3(&adapterDesc);
        assert(SUCCEEDED(hr));

        if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            Log(logStream_, ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
            break;
        }
        useAdapter_ = nullptr;
    }
    assert(useAdapter_ != nullptr);

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
    const char* featureLevelStrings[] = { "12.2", "12.1", "12.0" };
    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(useAdapter_.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
        if (SUCCEEDED(hr)) {
            Log(logStream_, std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
            break;
        }
    }
    assert(device_ != nullptr);
    Log(logStream_, "Complete create D3D12Device!!!\n");

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
    hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
    assert(SUCCEEDED(hr));

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
    assert(SUCCEEDED(hr));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
    swapChainDesc.Width = kClientWidth_;
    swapChainDesc.Height = kClientHeight_;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), hwnd_, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
    assert(SUCCEEDED(hr));

    rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
    dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
    srvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);

    descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    for (UINT i = 0; i < 2; ++i) {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&swapChainResources_[i]));
        assert(SUCCEEDED(hr));
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    rtvHandles_[0] = GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, 0);
    device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);

    rtvHandles_[1] = GetCPUDescriptorHandle(rtvDescriptorHeap_, descriptorSizeRTV_, 1);
    device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);

    hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent_ != nullptr);

    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));

#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
        D3D12_MESSAGE_ID denyIds[] = { D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE };
        D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;
        infoQueue->PushStorageFilter(&filter);
    }
#endif
}

void Engine::InitializePipeline() {
    D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4]{};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].Descriptor.ShaderRegister = 0;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[1].Descriptor.ShaderRegister = 1;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[3].Descriptor.ShaderRegister = 1;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        Log(logStream_, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = device_->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
    inputLayoutDesc.pInputElementDescs = inputElementDescs;
    inputLayoutDesc.NumElements = _countof(inputElementDescs);

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    ComPtr<IDxcBlob> vertexShaderBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> pixelShaderBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
    graphicsPipelineStateDesc.pRootSignature = rootSignature_.Get();
    graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
    graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    graphicsPipelineStateDesc.BlendState = blendDesc;
    graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
    graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    graphicsPipelineStateDesc.NumRenderTargets = 1;
    graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    graphicsPipelineStateDesc.SampleDesc.Count = 1;
    graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = device_->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void Engine::InitializeResources() {
    // 1. Plane モデル読み込み
    modelData_ = LoadObjFile("resources", "plane.obj");
    vertexResourcePlane_ = CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferViewPlane_.BufferLocation = vertexResourcePlane_->GetGPUVirtualAddress();
    vertexBufferViewPlane_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferViewPlane_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexDataPlane = nullptr;
    vertexResourcePlane_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataPlane));
    std::memcpy(vertexDataPlane, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());

    // 2. Sphere 頂点生成
    const uint32_t kSubdivision = 64;
    kNumSphereVertices_ = kSubdivision * kSubdivision * 6;
    vertexResourceSphere_ = CreateBufferResource(sizeof(VertexData) * kNumSphereVertices_);
    vertexBufferViewSphere_.BufferLocation = vertexResourceSphere_->GetGPUVirtualAddress();
    vertexBufferViewSphere_.SizeInBytes = sizeof(VertexData) * kNumSphereVertices_;
    vertexBufferViewSphere_.StrideInBytes = sizeof(VertexData);

    VertexData* vertexDataSphere = nullptr;
    vertexResourceSphere_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSphere));

    const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);
    const float kLonEvery = 2.0f * std::numbers::pi_v<float> / float(kSubdivision);

    for (uint32_t lat = 0; lat < kSubdivision; ++lat) {
        float latVal = -std::numbers::pi_v<float> / 2.0f + float(lat) * kLatEvery;
        for (uint32_t lon = 0; lon < kSubdivision; ++lon) {
            float lonVal = float(lon) * kLonEvery;
            uint32_t index = (lat * kSubdivision + lon) * 6;
            float r = 0.5f;

            Vector4 p0 = { r * cosf(latVal) * cosf(lonVal), r * sinf(latVal), r * cosf(latVal) * sinf(lonVal), 1.0f };
            Vector4 p1 = { r * cosf(latVal + kLatEvery) * cosf(lonVal), r * sinf(latVal + kLatEvery), r * cosf(latVal + kLatEvery) * sinf(lonVal), 1.0f };
            Vector4 p2 = { r * cosf(latVal) * cosf(lonVal + kLonEvery), r * sinf(latVal), r * cosf(latVal) * sinf(lonVal + kLonEvery), 1.0f };
            Vector4 p3 = { r * cosf(latVal + kLatEvery) * cosf(lonVal + kLonEvery), r * sinf(latVal + kLatEvery), r * cosf(latVal + kLatEvery) * sinf(lonVal + kLonEvery), 1.0f };

            float u0 = float(lon) / float(kSubdivision);
            float u1 = float(lon + 1) / float(kSubdivision);
            float v0 = 1.0f - float(lat) / float(kSubdivision);
            float v1 = 1.0f - float(lat + 1) / float(kSubdivision);

            vertexDataSphere[index + 0] = { p0, { u0, v0 }, { p0.x, p0.y, p0.z } };
            vertexDataSphere[index + 1] = { p1, { u0, v1 }, { p1.x, p1.y, p1.z } };
            vertexDataSphere[index + 2] = { p2, { u1, v0 }, { p2.x, p2.y, p2.z } };
            vertexDataSphere[index + 3] = { p1, { u0, v1 }, { p1.x, p1.y, p1.z } };
            vertexDataSphere[index + 4] = { p3, { u1, v1 }, { p3.x, p3.y, p3.z } };
            vertexDataSphere[index + 5] = { p2, { u1, v0 }, { p2.x, p2.y, p2.z } };
        }
    }

    // 3. Sprite 頂点・インデックスリソース
    vertexResourceSprite_ = CreateBufferResource(sizeof(VertexData) * 6);
    vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
    vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);

    indexResourceSprite_ = CreateBufferResource(sizeof(uint32_t) * 6);
    indexBufferViewSprite_.BufferLocation = indexResourceSprite_->GetGPUVirtualAddress();
    indexBufferViewSprite_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferViewSprite_.Format = DXGI_FORMAT_R32_UINT;

    VertexData* vertexDataSprite = nullptr;
    vertexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
    vertexDataSprite[0] = { { 0.0f, 360.0f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
    vertexDataSprite[1] = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    vertexDataSprite[2] = { { 640.0f, 360.0f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
    vertexDataSprite[3] = { { 640.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

    uint32_t* indexDataSprite = nullptr;
    indexResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
    indexDataSprite[0] = 0; indexDataSprite[1] = 1; indexDataSprite[2] = 2;
    indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;

    // 4. 定数バッファ
    materialResource_ = CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = true;
    materialData_->uvTransform = MakeIdentity4x4();

    materialResourceSprite_ = CreateBufferResource(sizeof(Material));
    materialResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite_));
    materialDataSprite_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialDataSprite_->enableLighting = false;
    materialDataSprite_->uvTransform = MakeIdentity4x4();

    wvpResource_ = CreateBufferResource(sizeof(TransformationMatrix));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->wvp = MakeIdentity4x4();
    wvpData_->World = MakeIdentity4x4();

    transformationMatrixResourceSprite_ = CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));
    transformationMatrixDataSprite_->wvp = MakeIdentity4x4();
    transformationMatrixDataSprite_->World = MakeIdentity4x4();

    directionalLightResource_ = CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 2.0f;

    // Projection
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, float(kClientWidth_) / float(kClientHeight_), 0.1f, 100.0f);

    viewport_.Width = (float)kClientWidth_;
    viewport_.Height = (float)kClientHeight_;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.right = kClientWidth_;
    scissorRect_.top = 0;
    scissorRect_.bottom = kClientHeight_;

    // 5. テクスチャ 1 転送
    DirectX::ScratchImage mipImages = LoadTexture("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    textureResource_ = CreateTextureResource(metadata);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
    uint64_t intermediateSize = GetRequiredIntermediateSize(textureResource_.Get(), 0, UINT(subresources.size()));
    ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);

    UpdateSubresources(commandList_.Get(), textureResource_.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList_->ResourceBarrier(1, &barrier);

    commandList_->Close();
    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    // 6. DepthStencil 準備
    depthStencilResource_ = CreateDepthStencilTextureResource(kClientWidth_, kClientHeight_);
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device_->CreateDepthStencilView(depthStencilResource_.Get(), &dsvDesc, dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    // SRV1 構築
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, 1);
    textureSrvHandleGPU_ = GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, 1);
    device_->CreateShaderResourceView(textureResource_.Get(), &srvDesc, textureSrvHandleCPU);

    // 7. テクスチャ 2 転送
    DirectX::ScratchImage mipImages2 = LoadTexture(modelData_.material.textureFilePath);
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    textureResource2_ = CreateTextureResource(metadata2);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources2;
    DirectX::PrepareUpload(device_.Get(), mipImages2.GetImages(), mipImages2.GetImageCount(), mipImages2.GetMetadata(), subresources2);
    uint64_t intermediateSize2 = GetRequiredIntermediateSize(textureResource2_.Get(), 0, UINT(subresources2.size()));
    ComPtr<ID3D12Resource> intermediateResource2 = CreateBufferResource(intermediateSize2);

    UpdateSubresources(commandList_.Get(), textureResource2_.Get(), intermediateResource2.Get(), 0, 0, UINT(subresources2.size()), subresources2.data());

    D3D12_RESOURCE_BARRIER barrier2{};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Transition.pResource = textureResource2_.Get();
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    commandList_->ResourceBarrier(1, &barrier2);

    commandList_->Close();
    ID3D12CommandList* commandLists2[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists2);
    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);
    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
    commandAllocator_->Reset();
    commandList_->Reset(commandAllocator_.Get(), nullptr);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, 2);
    textureSrvHandleGPU2_ = GetGPUDescriptorHandle(srvDescriptorHeap_, descriptorSizeSRV_, 2);
    device_->CreateShaderResourceView(textureResource2_.Get(), &srvDesc2, textureSrvHandleCPU2);
}

void Engine::InitializeImGui() {
#ifdef USE_IMGUI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX12_Init(device_.Get(),
        2,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        srvDescriptorHeap_.Get(),
        srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart(),
        srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Build();
#endif
}

void Engine::Run() {
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            Update();
            Draw();
        }
    }
}

void Engine::Update() {
    input.Update();
    debugCamera_.Update();

    if (input.PushKey(DIK_0)) {
        OutputDebugStringA("Hit 0\n"); // 出力ウィンドウに「Hit 0」と表示されるかテスト
    }

#ifdef USE_IMGUI
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Begin("Settings");

    ImGui::Checkbox("Draw Sphere", &drawSphere_);
   

    ImGui::ColorEdit4("color", &materialData_->color.x, ImGuiColorEditFlags_Uint8);

    bool enableLightingBool = (materialData_->enableLighting != 0);
    if (ImGui::Checkbox("enableLighting", &enableLightingBool)) {
        materialData_->enableLighting = enableLightingBool ? 1 : 0;
    }

    ImGui::ColorEdit4("colorSprite", &materialDataSprite_->color.x, ImGuiColorEditFlags_Uint8);
    ImGui::DragFloat3("translateSprite", &transformSprite_.translate.x, 1.0f);
    ImGui::Checkbox("useMonsterBall", &useMonsterBall_);

    ImGui::ColorEdit3("LightColor", &directionalLightData_->color.x, ImGuiColorEditFlags_Uint8);
    if (ImGui::DragFloat3("LightDirection", &directionalLightData_->direction.x, 0.01f, -1.0f, 1.0f)) {
        float length = std::sqrt(
            directionalLightData_->direction.x * directionalLightData_->direction.x +
            directionalLightData_->direction.y * directionalLightData_->direction.y +
            directionalLightData_->direction.z * directionalLightData_->direction.z
        );
        if (length > 0.0f) {
            directionalLightData_->direction.x /= length;
            directionalLightData_->direction.y /= length;
            directionalLightData_->direction.z /= length;
        }
    }
    ImGui::DragFloat("Intensity", &directionalLightData_->intensity, 0.01f, 0.0f, 10.0f);

    ImGui::DragFloat2("UVTranslate", &uvTransformSprite_.translate.x, 0.01f, -10.0f, 10.0f);
    ImGui::DragFloat2("UVScale", &uvTransformSprite_.scale.x, 0.01f, -10.0f, 10.0f);
    ImGui::SliderAngle("UVRotate", &uvTransformSprite_.rotate.z);

    if (ImGui::Button("Play Alarm")) {
        sound.SoundPlayWave(soundData1);
    }

    ImGui::End();

    cameraTransform_.rotate.x = cameraRotateDeg_.x * (std::numbers::pi_v<float> / 180.0f);
    cameraTransform_.rotate.y = cameraRotateDeg_.y * (std::numbers::pi_v<float> / 180.0f);
    cameraTransform_.rotate.z = cameraRotateDeg_.z * (std::numbers::pi_v<float> / 180.0f);

    transform_.rotate.x = modelRotateDeg_.x * (std::numbers::pi_v<float> / 180.0f);
    transform_.rotate.y = modelRotateDeg_.y * (std::numbers::pi_v<float> / 180.0f);
    transform_.rotate.z = modelRotateDeg_.z * (std::numbers::pi_v<float> / 180.0f);
#endif

    Matrix4x4 uvTransformMatrix = MakeAffineMatrix(uvTransformSprite_.scale, uvTransformSprite_.rotate, uvTransformSprite_.translate);
    materialDataSprite_->uvTransform = uvTransformMatrix;

    Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
    Matrix4x4 viewMatrix = debugCamera_.GetViewMatrix();

    wvpData_->wvp = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix_));
    wvpData_->World = worldMatrix;

    Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite_.scale, transformSprite_.rotate, transformSprite_.translate);
    Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
    Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(kClientWidth_), float(kClientHeight_), 0.0f, 100.0f);
    
    transformationMatrixDataSprite_->wvp = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
    transformationMatrixDataSprite_->World = worldMatrixSprite;
   
}

void Engine::Draw() {
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);

    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
    commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor, 0, nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false, &dsvHandle);
    commandList_->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList_->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
    commandList_->SetGraphicsRootDescriptorTable(2, useMonsterBall_ ? textureSrvHandleGPU2_ : textureSrvHandleGPU_);
    commandList_->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

    commandList_->SetPipelineState(graphicsPipelineState_.Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList_->IASetVertexBuffers(0, 1, &vertexBufferViewSphere_);
    commandList_->DrawInstanced(kNumSphereVertices_, 1, 0, 0);

   

#ifdef USE_IMGUI
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());
#endif

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList_->ResourceBarrier(1, &barrier);

    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    ID3D12CommandList* commandLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, commandLists);

    swapChain_->Present(1, 0);

    fenceValue_++;
    commandQueue_->Signal(fence_.Get(), fenceValue_);

    if (fence_->GetCompletedValue() < fenceValue_) {
        fence_->SetEventOnCompletion(fenceValue_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}

void Engine::Finalize() {

    sound.Finalize();

    sound.SoundUnload(&soundData1);

    Log(logStream_, "Application Ended");

#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
    }

    CoUninitialize();
}

// 内部ヘルパー関数の実装
ComPtr<ID3D12DescriptorHeap> Engine::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible) {
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
    descriptorHeapDesc.Type = heapType;
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));
    return descriptorHeap;
}

ComPtr<ID3D12Resource> Engine::CreateBufferResource(size_t sizeInBytes) {
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

ComPtr<ID3D12Resource> Engine::CreateTextureResource(const DirectX::TexMetadata& metadata) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width);
    resourceDesc.Height = UINT(metadata.height);
    resourceDesc.MipLevels = UINT16(metadata.mipLevels);
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    resourceDesc.Format = metadata.format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

ComPtr<ID3D12Resource> Engine::CreateDepthStencilTextureResource(int32_t width, int32_t height) {
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE depthClearValue{};
    depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthClearValue.DepthStencil.Depth = 1.0f;
    depthClearValue.DepthStencil.Stencil = 0;

    ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device_->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE,
        &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

D3D12_CPU_DESCRIPTOR_HANDLE Engine::GetCPUDescriptorHandle(const ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSize * index);
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE Engine::GetGPUDescriptorHandle(const ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += (descriptorSize * index);
    return handleGPU;
}

ComPtr<IDxcBlob> Engine::CompileShader(const std::wstring& filePath, const wchar_t* profile) {
    Log(logStream_, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

    ComPtr<IDxcBlobEncoding> shaderSource;
    HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    DxcBuffer shaderSourceBuffer;
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    LPCWSTR arguments[] = {
        filePath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr",
    };

    ComPtr<IDxcResult> shaderResult;
    hr = dxcCompiler_->Compile(&shaderSourceBuffer, arguments, _countof(arguments), includeHandler_.Get(), IID_PPV_ARGS(&shaderResult));
    assert(SUCCEEDED(hr));

    ComPtr<IDxcBlobUtf8> shaderError;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(logStream_, shaderError->GetStringPointer());
        assert(false);
    }

    ComPtr<IDxcBlob> shaderBlob;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Log(logStream_, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
    return shaderBlob;
}

DirectX::ScratchImage Engine::LoadTexture(const std::string& filePath) {
    std::wstring filePathW = ConvertString(filePath);
    DirectX::ScratchImage image;
    HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
    assert(SUCCEEDED(hr));

    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        DirectX::TEX_FILTER_DEFAULT, 4, mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;
}

MaterialData Engine::LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename) {
    MaterialData materialData;
    std::string line;
    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "map_Kd") {
            std::string textureFilename;
            s >> textureFilename;
            materialData.textureFilePath = directoryPath + "/" + textureFilename;
        }
    }
    return materialData;
}

ModelData Engine::LoadObjFile(const std::string& directoryPath, const std::string& filename) {
    ModelData modelData;
    std::vector<Vector4> positions;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;
    std::string line;

    std::ifstream file(directoryPath + "/" + filename);
    assert(file.is_open());

    while (std::getline(file, line)) {
        std::string identifier;
        std::istringstream s(line);
        s >> identifier;

        if (identifier == "mtllib") {
            std::string materialFilename;
            s >> materialFilename;
            modelData.material = LoadMaterialTemplateFile(directoryPath, materialFilename);
        }
        else if (identifier == "v") {
            Vector4 position;
            s >> position.x >> position.y >> position.z;
            position.w = 1.0f;
            positions.push_back(position);
        }
        else if (identifier == "vt") {
            Vector2 texcoord;
            s >> texcoord.x >> texcoord.y;
            texcoord.y = 1.0f - texcoord.y;
            texcoords.push_back(texcoord);
        }
        else if (identifier == "vn") {
            Vector3 normal;
            s >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (identifier == "f") {
            VertexData triangle[3];
            for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {
                std::string vertexDefinition;
                s >> vertexDefinition;
                std::istringstream v(vertexDefinition);
                uint32_t elementIndices[3];
                for (int32_t element = 0; element < 3; ++element) {
                    std::string index;
                    std::getline(v, index, '/');
                    elementIndices[element] = std::stoi(index);
                }
                Vector4 position = positions[elementIndices[0] - 1];
                Vector2 texcoord = texcoords[elementIndices[1] - 1];
                Vector3 normal = normals[elementIndices[2] - 1];

                position.x *= -1.0f;
                normal.x *= -1.0f;

                triangle[faceVertex] = { position, texcoord, normal };
            }
            modelData.vertices.push_back(triangle[2]);
            modelData.vertices.push_back(triangle[1]);
            modelData.vertices.push_back(triangle[0]);
        }
    }
    return modelData;
}