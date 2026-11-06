#ifdef _DEBUG
#define USE_IMGUI
#endif

#include "Engine.h"
#include "Input.h"
#include "Logger.h"
#include "StringUtility.h"
#include <cassert>
#include <chrono>
#include <dbghelp.h>
#include <filesystem>
#include <format>
#include <numbers>
#include <sstream>
#include <strsafe.h>
#include "externals/DirectXTex/d3dx12.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#endif

// ★ 3. ImGuiに必要なヘッダーファイルとWndProcハンドラを読み込む
#ifdef USE_IMGUI

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#include <cmath>

#pragma comment(lib, "Dbghelp.lib")

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

void Engine::Initialize(HINSTANCE hInstance, int nCmdShow, DirectXManager* directXManager) {
    directXManager_ = directXManager;
    assert(directXManager_ != nullptr);
    SetUnhandledExceptionFilter(ExportDump);

    InitializeLog();
    winApp_.Initialize(hInstance, nCmdShow, WindowProc);
    directXManager_->Initialize(&winApp_, logStream_);
    InitializePipeline();
    InitializeResources();

    // ParticleのTransformと速度を初期化する
    for (uint32_t index = 0; index < kNumInstance; ++index) {
        particles[index].transform.scale = { 1.0f, 1.0f, 1.0f };
        particles[index].transform.rotate = { 0.0f, 0.0f, 0.0f };
        particles[index].transform.translate = {
            index * 0.15f,
            index * -0.1f,
            index * 0.5f
        };
        particles[index].velocity = { 0.0f, 1.0f, 0.0f };
    }


    // サウンドの初期化と読み込み
    sound.Initialize();
    soundData1 = sound.SoundLoadWave("Resources/botan.wav");

    input = new Input();
    input->Initialize(&winApp_);
    debugCamera_.Initialize(input);
}

void Engine::InitializeLog() {
    std::filesystem::create_directory("logs");
    auto now = std::chrono::system_clock::now();
    auto nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
    std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSeconds };
    std::string dateString = std::format("{:%Y%m%d_%H%M%S}", localTime);
    std::string logFilePath = std::string("logs/") + dateString + ".log";
    logStream_.open(logFilePath);

    Logger::Log(logStream_, "Application Started");
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
        Logger::Log(logStream_, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = directXManager_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));

    // -------------------------------------------------------------------
    // Particle (インスタンシング) 用ルートシグネチャの構築
    // -------------------------------------------------------------------
    D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1]{};
    descriptorRangeForInstancing[0].BaseShaderRegister = 0;
    descriptorRangeForInstancing[0].NumDescriptors = 1;
    descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // ★重複を削除し1回のみに修正
    descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER particleRootParameters[4]{};

    // [0] マテリアル (Pixel Shader - b0)
    particleRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    particleRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParameters[0].Descriptor.ShaderRegister = 0;

    // [1] インスタンシング用 StructuredBuffer SRV (Vertex Shader - t0)
    particleRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    particleRootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
    particleRootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

    // [2] テクスチャ SRV (Pixel Shader - t0)
    particleRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    particleRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
    particleRootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

    // [3] 平行光源 (Pixel Shader - b1)
    particleRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    particleRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    particleRootParameters[3].Descriptor.ShaderRegister = 1;

    descriptionRootSignature.pParameters = particleRootParameters;
    descriptionRootSignature.NumParameters = _countof(particleRootParameters);

    hr = D3D12SerializeRootSignature(
        &descriptionRootSignature,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );
    if (FAILED(hr)) {
        Logger::Log(logStream_, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
        assert(false);
    }

    hr = directXManager_->GetDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&particleRootSignature_)
    );
    assert(SUCCEEDED(hr));


    // -------------------------------------------------------------------
    // 通常オブジェクト(Object3D)用 PSO の作成
    // -------------------------------------------------------------------
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
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    ComPtr<IDxcBlob> vertexShaderBlob = directXManager_->CompileShader(L"Object3D.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> pixelShaderBlob = directXManager_->CompileShader(L"Object3D.PS.hlsl", L"ps_6_0");

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

    hr = directXManager_->GetDevice()->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));


    // -------------------------------------------------------------------
    // ★ 追加：Particle (インスタンシング) 用 PSO の作成
    // -------------------------------------------------------------------
    ComPtr<IDxcBlob> particleVSBlob = directXManager_->CompileShader(L"Particle.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> particlePSBlob = directXManager_->CompileShader(L"Particle.PS.hlsl", L"ps_6_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC particlePsoDesc = graphicsPipelineStateDesc;
    particlePsoDesc.pRootSignature = particleRootSignature_.Get(); // インスタンシング用のルートシグネチャをセット
    particlePsoDesc.VS = { particleVSBlob->GetBufferPointer(), particleVSBlob->GetBufferSize() };
    particlePsoDesc.PS = { particlePSBlob->GetBufferPointer(), particlePSBlob->GetBufferSize() };

    hr = directXManager_->GetDevice()->CreateGraphicsPipelineState(&particlePsoDesc, IID_PPV_ARGS(&particlePipelineState_));
    assert(SUCCEEDED(hr));
}

void Engine::InitializeResources() {
    // 1. Plane モデル読み込み
    modelData_ = LoadObjFile("resources", "plane.obj"); //modelData_ = LoadObjFile("resources/fence", "fence.obj");フェンスだったらこっちに変える
    vertexResourcePlane_ = directXManager_->CreateBufferResource(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferViewPlane_.BufferLocation = vertexResourcePlane_->GetGPUVirtualAddress();
    vertexBufferViewPlane_.SizeInBytes = UINT(sizeof(VertexData) * modelData_.vertices.size());
    vertexBufferViewPlane_.StrideInBytes = sizeof(VertexData);

    for (VertexData& vertex : modelData_.vertices) {
        vertex.position.x *= -1.0f;
        vertex.position.z *= -1.0f;
        vertex.normal.x *= -1.0f;
        vertex.normal.z *= -1.0f;
    }

    VertexData* vertexDataPlane = nullptr;
    vertexResourcePlane_->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataPlane));
    std::memcpy(vertexDataPlane, modelData_.vertices.data(), sizeof(VertexData) * modelData_.vertices.size());

    // 2. Sphere 頂点生成
    const uint32_t kSubdivision = 64;
    kNumSphereVertices_ = kSubdivision * kSubdivision * 6;
    vertexResourceSphere_ = directXManager_->CreateBufferResource(sizeof(VertexData) * kNumSphereVertices_);
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
    vertexResourceSprite_ = directXManager_->CreateBufferResource(sizeof(VertexData) * 6);
    vertexBufferViewSprite_.BufferLocation = vertexResourceSprite_->GetGPUVirtualAddress();
    vertexBufferViewSprite_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferViewSprite_.StrideInBytes = sizeof(VertexData);

    indexResourceSprite_ = directXManager_->CreateBufferResource(sizeof(uint32_t) * 6);
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
    materialResource_ = directXManager_->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = true;
    materialData_->uvTransform = MakeIdentity4x4();

    materialResourceSprite_ = directXManager_->CreateBufferResource(sizeof(Material));
    materialResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&materialDataSprite_));
    materialDataSprite_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialDataSprite_->enableLighting = false;
    materialDataSprite_->uvTransform = MakeIdentity4x4();

    wvpResource_ = directXManager_->CreateBufferResource(sizeof(TransformationMatrix));
    wvpResource_->Map(0, nullptr, reinterpret_cast<void**>(&wvpData_));
    wvpData_->wvp = MakeIdentity4x4();
    wvpData_->World = MakeIdentity4x4();

    instancingResource_ =
        directXManager_->CreateBufferResource(sizeof(TransformationMatrix) * kNumInstance);

    instancingResource_->Map(
        0, nullptr, reinterpret_cast<void**>(&instancingData_));

    for (uint32_t index = 0; index < kNumInstance; ++index) {
        instancingData_[index].wvp = MakeIdentity4x4();
        instancingData_[index].World = MakeIdentity4x4();
    }

    transformationMatrixResourceSprite_ = directXManager_->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResourceSprite_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite_));
    transformationMatrixDataSprite_->wvp = MakeIdentity4x4();
    transformationMatrixDataSprite_->World = MakeIdentity4x4();

    directionalLightResource_ = directXManager_->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 2.0f;

    // Projection
    projectionMatrix_ = MakePerspectiveFovMatrix(0.45f, float(WinApp::kClientWidth) / float(WinApp::kClientHeight), 0.1f, 100.0f);


    // 5. テクスチャ 1 転送
    DirectX::ScratchImage mipImages = DirectXManager::LoadTexture("resources/uvChecker.png");
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    textureResource_ = directXManager_->CreateTextureResource(metadata);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    DirectX::PrepareUpload(directXManager_->GetDevice(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
    uint64_t intermediateSize = GetRequiredIntermediateSize(textureResource_.Get(), 0, UINT(subresources.size()));
    ComPtr<ID3D12Resource> intermediateResource = directXManager_->CreateBufferResource(intermediateSize);

    UpdateSubresources(directXManager_->GetCommandList(), textureResource_.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    directXManager_->GetCommandList()->ResourceBarrier(1, &barrier);

    directXManager_->GetCommandList()->Close();
    ID3D12CommandList* commandLists[] = { directXManager_->GetCommandList() };
    directXManager_->GetCommandQueue()->ExecuteCommandLists(1, commandLists);
    directXManager_->GetFenceValue()++;
    directXManager_->GetCommandQueue()->Signal(directXManager_->GetFence().Get(), directXManager_->GetFenceValue());
    if (directXManager_->GetFence()->GetCompletedValue() < directXManager_->GetFenceValue()) {
        directXManager_->GetFence()->SetEventOnCompletion(directXManager_->GetFenceValue(), directXManager_->GetFenceEvent());
        WaitForSingleObject(directXManager_->GetFenceEvent(), INFINITE);
    }
    directXManager_->GetCommandAllocator()->Reset();
    directXManager_->GetCommandList()->Reset(directXManager_->GetCommandAllocator().Get(), nullptr);

    // SRV1 構築
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = directXManager_->GetSRVCPUDescriptorHandle(1);
    textureSrvHandleGPU_ = directXManager_->GetSRVGPUDescriptorHandle(1);
    directXManager_->GetDevice()->CreateShaderResourceView(textureResource_.Get(), &srvDesc, textureSrvHandleCPU);

    // 7. テクスチャ 2 転送
    DirectX::ScratchImage mipImages2 = DirectXManager::LoadTexture(modelData_.material.textureFilePath);
    const DirectX::TexMetadata& metadata2 = mipImages2.GetMetadata();
    textureResource2_ = directXManager_->CreateTextureResource(metadata2);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources2;
    DirectX::PrepareUpload(directXManager_->GetDevice(), mipImages2.GetImages(), mipImages2.GetImageCount(), mipImages2.GetMetadata(), subresources2);
    uint64_t intermediateSize2 = GetRequiredIntermediateSize(textureResource2_.Get(), 0, UINT(subresources2.size()));
    ComPtr<ID3D12Resource> intermediateResource2 = directXManager_->CreateBufferResource(intermediateSize2);

    UpdateSubresources(directXManager_->GetCommandList(), textureResource2_.Get(), intermediateResource2.Get(), 0, 0, UINT(subresources2.size()), subresources2.data());

    D3D12_RESOURCE_BARRIER barrier2{};
    barrier2.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier2.Transition.pResource = textureResource2_.Get();
    barrier2.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier2.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier2.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    directXManager_->GetCommandList()->ResourceBarrier(1, &barrier2);

    directXManager_->GetCommandList()->Close();
    ID3D12CommandList* commandLists2[] = { directXManager_->GetCommandList() };
    directXManager_->GetCommandQueue()->ExecuteCommandLists(1, commandLists2);
    directXManager_->GetFenceValue()++;
    directXManager_->GetCommandQueue()->Signal(directXManager_->GetFence().Get(), directXManager_->GetFenceValue());
    if (directXManager_->GetFence()->GetCompletedValue() < directXManager_->GetFenceValue()) {
        directXManager_->GetFence()->SetEventOnCompletion(directXManager_->GetFenceValue(), directXManager_->GetFenceEvent());
        WaitForSingleObject(directXManager_->GetFenceEvent(), INFINITE);
    }
    directXManager_->GetCommandAllocator()->Reset();
    directXManager_->GetCommandList()->Reset(directXManager_->GetCommandAllocator().Get(), nullptr);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
    srvDesc2.Format = metadata2.format;
    srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc2.Texture2D.MipLevels = UINT(metadata2.mipLevels);

    D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = directXManager_->GetSRVCPUDescriptorHandle(2);
    textureSrvHandleGPU2_ = directXManager_->GetSRVGPUDescriptorHandle(2);
    directXManager_->GetDevice()->CreateShaderResourceView(textureResource2_.Get(), &srvDesc2, textureSrvHandleCPU2);

    D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};
    instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    instancingSrvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    instancingSrvDesc.Buffer.FirstElement = 0;
    instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    instancingSrvDesc.Buffer.NumElements = kNumInstance;
    instancingSrvDesc.Buffer.StructureByteStride =
        sizeof(TransformationMatrix);

    D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU =
        directXManager_->GetSRVCPUDescriptorHandle(3);
    instancingSrvHandleGPU_ =
        directXManager_->GetSRVGPUDescriptorHandle(3);

    directXManager_->GetDevice()->CreateShaderResourceView(
        instancingResource_.Get(),
        &instancingSrvDesc,
        instancingSrvHandleCPU
    );
}

void Engine::Run() {
    while (true) {
        if (winApp_.ProcessMessage()) {
            break;
        }

        Update();
        Draw();
    }
}

void Engine::Update() {
    const float kDeltaTime = 1.0f / 60.0f;
    if (updateParticles_) {
        for (uint32_t index = 0; index < kNumInstance; ++index) {
            particles[index].transform.translate.x += particles[index].velocity.x * kDeltaTime;
            particles[index].transform.translate.y += particles[index].velocity.y * kDeltaTime;
            particles[index].transform.translate.z += particles[index].velocity.z * kDeltaTime;
        }
    }

    input->Update();

    if (input->TriggerKey(DIK_0)) {
        OutputDebugStringA("Hit 0\n");
    }

#ifdef USE_IMGUI
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::ShowDemoWindow();
    ImGui::Begin("Settings");

    ImGui::Checkbox("Draw Sphere", &drawSphere_);
    ImGui::Checkbox("Update", &updateParticles_);
   

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

    debugCamera_.Update();


    Matrix4x4 uvTransformMatrix = MakeAffineMatrix(uvTransformSprite_.scale, uvTransformSprite_.rotate, uvTransformSprite_.translate);
    materialDataSprite_->uvTransform = uvTransformMatrix;

    Matrix4x4 worldMatrix = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform_.scale, cameraTransform_.rotate, cameraTransform_.translate);
    Matrix4x4 viewMatrix = debugCamera_.GetViewMatrix();

    for (uint32_t index = 0; index < kNumInstance; ++index) {
        Matrix4x4 worldMatrix = MakeAffineMatrix(
            particles[index].transform.scale,
            particles[index].transform.rotate,
            particles[index].transform.translate
        );

        Matrix4x4 worldViewProjectionMatrix =
            Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix_));

        instancingData_[index].wvp = worldViewProjectionMatrix;
        instancingData_[index].World = worldMatrix;
    }

    wvpData_->wvp = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix_));
    wvpData_->World = worldMatrix;

    Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite_.scale, transformSprite_.rotate, transformSprite_.translate);
    Matrix4x4 viewMatrixSprite = MakeIdentity4x4();
    Matrix4x4 projectionMatrixSprite = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);
    
    transformationMatrixDataSprite_->wvp = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));
    transformationMatrixDataSprite_->World = worldMatrixSprite;
   
}

void Engine::Draw() {
    directXManager_->PreDraw();

    directXManager_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
    directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
    directXManager_->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall_ ? textureSrvHandleGPU2_ : textureSrvHandleGPU_);
    directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

    directXManager_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());
    directXManager_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (drawSphere_) {
        // チェックが入っているときは球を描画（通常PSO）
        directXManager_->GetCommandList()->SetGraphicsRootSignature(rootSignature_.Get());
        directXManager_->GetCommandList()->SetPipelineState(graphicsPipelineState_.Get());

        directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
        directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());
        directXManager_->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall_ ? textureSrvHandleGPU2_ : textureSrvHandleGPU_);
        directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

        directXManager_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        directXManager_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewSphere_);
        directXManager_->GetCommandList()->DrawInstanced(kNumSphereVertices_, 1, 0, 0);
    }
    else {
        // チェックが外れているときは 10個の板をインスタンシング描画（Particle PSO）
        directXManager_->GetCommandList()->SetGraphicsRootSignature(particleRootSignature_.Get());
        directXManager_->GetCommandList()->SetPipelineState(particlePipelineState_.Get());

        directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
        directXManager_->GetCommandList()->SetGraphicsRootDescriptorTable(1, instancingSrvHandleGPU_);
        directXManager_->GetCommandList()->SetGraphicsRootDescriptorTable(2, useMonsterBall_ ? textureSrvHandleGPU2_ : textureSrvHandleGPU_);
        directXManager_->GetCommandList()->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

        directXManager_->GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        directXManager_->GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferViewPlane_);

        // 10インスタンス描画
        directXManager_->GetCommandList()->DrawInstanced(static_cast<UINT>(modelData_.vertices.size()), kNumInstance, 0, 0);
    }

#ifdef USE_IMGUI
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), directXManager_->GetCommandList());
#endif

    directXManager_->PostDraw();
}

void Engine::Finalize() {
    delete input;
    input = nullptr;
    sound.Finalize();

    sound.SoundUnload(&soundData1);

    Logger::Log(logStream_, "Application Ended");

    directXManager_->Finalize();

    winApp_.Finalize();
}

// 内部ヘルパー関数の実装
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
