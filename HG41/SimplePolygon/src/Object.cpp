#include "Object.h"
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <cassert>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace /*匿名*/ {

// Vertex 構造体（App.cpp と同じ）
struct Vertex
{
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT4 Color;
};

} // namespace

Object::Object()
    : m_FrameCount(0)
    , m_Width(0)
    , m_Height(0)
    , m_RotateAngle(0.0f)
{
    std::memset(&m_VBV, 0, sizeof(m_VBV));
    std::memset(&m_Viewport, 0, sizeof(m_Viewport));
    std::memset(&m_Scissor, 0, sizeof(m_Scissor));
}

Object::~Object()
{
    Terminate();
}

bool Object::Initialize(ID3D12Device* device, uint32_t width, uint32_t height, uint32_t frameCount)
{
    if (device == nullptr) return false;

    m_FrameCount = frameCount;
    m_Width = width;
    m_Height = height;

    // 頂点データ（App.cpp と同等）
    Vertex vertices[] = {
        { DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f,  1.0f, 0.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f,  1.0f, 0.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },

        { DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f,  1.0f, 0.0f), DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
        { DirectX::XMFLOAT3(1.0f, -1.0f, 0.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) },
    };

    // 頂点バッファ作成（アップロードヒープ）
    {
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(vertices);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        HRESULT hr = device->CreateCommittedResource(
            &prop,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(m_pVB.GetAddressOf()));
        if (FAILED(hr)) return false;

        void* ptr = nullptr;
        hr = m_pVB->Map(0, nullptr, &ptr);
        if (FAILED(hr)) return false;

        memcpy(ptr, vertices, sizeof(vertices));
        m_pVB->Unmap(0, nullptr);

        m_VBV.BufferLocation = m_pVB->GetGPUVirtualAddress();
        m_VBV.SizeInBytes = static_cast<UINT>(sizeof(vertices));
        m_VBV.StrideInBytes = static_cast<UINT>(sizeof(Vertex));
    }

    // 定数バッファ用ディスクリプタヒープ生成
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = 1 * m_FrameCount;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        desc.NodeMask = 0;

        HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(m_pHeapCBV.GetAddressOf()));
        if (FAILED(hr)) return false;
    }

    // 定数バッファ作成 + マッピング
    {
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = 0;
        desc.Width = sizeof(Transform);
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        auto incrementSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        m_pCB.resize(m_FrameCount);
        m_CBV.resize(m_FrameCount);

        for (uint32_t i = 0; i < m_FrameCount; ++i)
        {
            HRESULT hr = device->CreateCommittedResource(
                &prop,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(m_pCB[i].GetAddressOf()));
            if (FAILED(hr)) return false;

            auto address = m_pCB[i]->GetGPUVirtualAddress();
            auto handleCPU = m_pHeapCBV->GetCPUDescriptorHandleForHeapStart();
            auto handleGPU = m_pHeapCBV->GetGPUDescriptorHandleForHeapStart();

            handleCPU.ptr += incrementSize * i;
            handleGPU.ptr += incrementSize * i;

            m_CBV[i].HandleCPU = handleCPU;
            m_CBV[i].HandleGPU = handleGPU;
            m_CBV[i].Desc.BufferLocation = address;
            m_CBV[i].Desc.SizeInBytes = static_cast<UINT>(sizeof(Transform));

            device->CreateConstantBufferView(&m_CBV[i].Desc, handleCPU);

            hr = m_pCB[i]->Map(0, nullptr, reinterpret_cast<void**>(&m_CBV[i].pBuffer));
            if (FAILED(hr)) return false;

            auto eyePos = DirectX::XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
            auto targetPos = DirectX::XMVectorZero();
            auto upward = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

            auto fovY = DirectX::XMConvertToRadians(37.5f);
            auto aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);

            m_CBV[i].pBuffer->World = DirectX::XMMatrixIdentity();
            m_CBV[i].pBuffer->View = DirectX::XMMatrixLookAtRH(eyePos, targetPos, upward);
            m_CBV[i].pBuffer->Proj = DirectX::XMMatrixPerspectiveFovRH(fovY, aspect, 1.0f, 1000.0f);
        }
    }

    // ルートシグニチャ生成
    {
        auto flag = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
        flag |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        D3D12_ROOT_PARAMETER param = {};
        param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        param.Descriptor.ShaderRegister = 0;
        param.Descriptor.RegisterSpace = 0;
        param.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC desc = {};
        desc.NumParameters = 1;
        desc.NumStaticSamplers = 0;
        desc.pParameters = &param;
        desc.pStaticSamplers = nullptr;
        desc.Flags = flag;

        ComPtr<ID3DBlob> pBlob;
        ComPtr<ID3DBlob> pErrorBlob;

        HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0, pBlob.GetAddressOf(), pErrorBlob.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = device->CreateRootSignature(0, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(m_pRootSignature.GetAddressOf()));
        if (FAILED(hr)) return false;
    }

    // パイプラインステート生成
    {
        D3D12_INPUT_ELEMENT_DESC elements[2];
        elements[0].SemanticName = "POSITION";
        elements[0].SemanticIndex = 0;
        elements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        elements[0].InputSlot = 0;
        elements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        elements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[0].InstanceDataStepRate = 0;

        elements[1].SemanticName = "COLOR";
        elements[1].SemanticIndex = 0;
        elements[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        elements[1].InputSlot = 0;
        elements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
        elements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[1].InstanceDataStepRate = 0;

        D3D12_RASTERIZER_DESC descRS = {};
        descRS.FillMode = D3D12_FILL_MODE_SOLID;
        descRS.CullMode = D3D12_CULL_MODE_NONE;
        descRS.FrontCounterClockwise = FALSE;
        descRS.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        descRS.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        descRS.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        descRS.DepthClipEnable = FALSE;
        descRS.MultisampleEnable = FALSE;
        descRS.AntialiasedLineEnable = FALSE;
        descRS.ForcedSampleCount = 0;
        descRS.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_RENDER_TARGET_BLEND_DESC descRTBS = {
            FALSE, FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL
        };

        D3D12_BLEND_DESC descBS = {};
        descBS.AlphaToCoverageEnable = FALSE;
        descBS.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            descBS.RenderTarget[i] = descRTBS;
        }

        ComPtr<ID3DBlob> pVSBlob;
        ComPtr<ID3DBlob> pPSBlob;

        HRESULT hr = D3DReadFileToBlob(L"SimpleVS.cso", pVSBlob.GetAddressOf());
        if (FAILED(hr)) return false;

        hr = D3DReadFileToBlob(L"SimplePS.cso", pPSBlob.GetAddressOf());
        if (FAILED(hr)) return false;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psDesc = {};
        psDesc.InputLayout = { elements, _countof(elements) };
        psDesc.pRootSignature = m_pRootSignature.Get();
        psDesc.VS = { pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize() };
        psDesc.PS = { pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize() };
        psDesc.RasterizerState = descRS;
        psDesc.BlendState = descBS;
        psDesc.DepthStencilState.DepthEnable = FALSE;
        psDesc.DepthStencilState.StencilEnable = FALSE;
        psDesc.SampleMask = UINT_MAX;
        psDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psDesc.NumRenderTargets = 1;
        psDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        psDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        psDesc.SampleDesc.Count = 1;
        psDesc.SampleDesc.Quality = 0;

        hr = device->CreateGraphicsPipelineState(&psDesc, IID_PPV_ARGS(m_pPSO.GetAddressOf()));
        if (FAILED(hr)) return false;
    }

    // ビューポートとシザー矩形
    {
        m_Viewport.TopLeftX = 0;
        m_Viewport.TopLeftY = 0;
        m_Viewport.Width = static_cast<float>(m_Width);
        m_Viewport.Height = static_cast<float>(m_Height);
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;

        m_Scissor.left = 0;
        m_Scissor.right = static_cast<LONG>(m_Width);
        m_Scissor.top = 0;
        m_Scissor.bottom = static_cast<LONG>(m_Height);
    }

    return true;
}

void Object::Terminate()
{
    // マッピング解除
    for (uint32_t i = 0; i < m_pCB.size(); ++i)
    {
        if (m_pCB[i].Get() != nullptr)
        {
            m_pCB[i]->Unmap(0, nullptr);
            memset(&m_CBV[i], 0, sizeof(m_CBV[i]));
        }
        m_pCB[i].Reset();
    }

    m_pVB.Reset();
    m_pPSO.Reset();
    m_pRootSignature.Reset();
    m_pHeapCBV.Reset();
    m_CBV.clear();
    m_pCB.clear();
}

void Object::Update(uint32_t frameIndex, float rotateAngle)
{
    m_RotateAngle = rotateAngle;
    if (frameIndex < m_CBV.size() && m_CBV[frameIndex].pBuffer != nullptr)
    {
        m_CBV[frameIndex].pBuffer->World = DirectX::XMMatrixRotationY(m_RotateAngle);
    }
}

void Object::Draw(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex)
{
    assert(cmdList != nullptr);
    assert(frameIndex < m_FrameCount);

    // ルートシグニチャ / ディスクリプタヒープ / 定数バッファを設定して描画
    cmdList->SetGraphicsRootSignature(m_pRootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { m_pHeapCBV.Get() };
    cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
    cmdList->SetGraphicsRootConstantBufferView(0, m_CBV[frameIndex].Desc.BufferLocation);
    cmdList->SetPipelineState(m_pPSO.Get());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_VBV);
    cmdList->RSSetViewports(1, &m_Viewport);
    cmdList->RSSetScissorRects(1, &m_Scissor);

    cmdList->DrawInstanced(6, 1, 0, 0);
}

// 指定ワールド行列で描画する（タイル描画用）
void Object::DrawAt(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex, const DirectX::XMMATRIX& world)
{
    assert(cmdList != nullptr);
    assert(frameIndex < m_FrameCount);

    // ワールド行列を書き換えてから描画
    if (frameIndex < m_CBV.size() && m_CBV[frameIndex].pBuffer != nullptr)
    {
        m_CBV[frameIndex].pBuffer->World = world;
    }

    // 以降は通常の描画と同じ
    cmdList->SetGraphicsRootSignature(m_pRootSignature.Get());
    ID3D12DescriptorHeap* heaps[] = { m_pHeapCBV.Get() };
    cmdList->SetDescriptorHeaps(_countof(heaps), heaps);
    cmdList->SetGraphicsRootConstantBufferView(0, m_CBV[frameIndex].Desc.BufferLocation);
    cmdList->SetPipelineState(m_pPSO.Get());

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->IASetVertexBuffers(0, 1, &m_VBV);
    cmdList->RSSetViewports(1, &m_Viewport);
    cmdList->RSSetScissorRects(1, &m_Scissor);

    cmdList->DrawInstanced(6, 1, 0, 0);
}