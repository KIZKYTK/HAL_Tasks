#pragma once

// Object.h : 描画オブジェクト（頂点バッファ・定数バッファ・ルートシグニチャ・PSO 等）をカプセル化するクラス

#include <d3d12.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

template<typename T>
using WComPtr = Microsoft::WRL::ComPtr<T>;

struct alignas(256) Transform
{
    DirectX::XMMATRIX World;
    DirectX::XMMATRIX View;
    DirectX::XMMATRIX Proj;
};

template<typename T>
struct ConstantBufferView
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC Desc;
    D3D12_CPU_DESCRIPTOR_HANDLE     HandleCPU;
    D3D12_GPU_DESCRIPTOR_HANDLE     HandleGPU;
    T*                              pBuffer;
};

class Object
{
public:
    Object();
    ~Object();

    // 初期化（デバイスとウィンドウサイズ、フレーム数を渡す）
    bool Initialize(ID3D12Device* device, uint32_t width, uint32_t height, uint32_t frameCount);

    // 終了処理
    void Terminate();

    // フレーム毎更新（回転角を適用）
    void Update(uint32_t frameIndex, float rotateAngle);

    // 描画コマンドを記録する（コマンドリストを渡す）
    void Draw(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex);

    // 指定のワールド行列で描画する（App 側からタイル配置等を行うために追加）
    void DrawAt(ID3D12GraphicsCommandList* cmdList, uint32_t frameIndex, const DirectX::XMMATRIX& world);

private:
    uint32_t m_FrameCount;
    uint32_t m_Width;
    uint32_t m_Height;

    // リソース
    WComPtr<ID3D12DescriptorHeap>           m_pHeapCBV;
    WComPtr<ID3D12Resource>                 m_pVB;
    std::vector<WComPtr<ID3D12Resource>>    m_pCB;              // frameCount 要素
    WComPtr<ID3D12RootSignature>            m_pRootSignature;
    WComPtr<ID3D12PipelineState>            m_pPSO;

    // 描画設定
    D3D12_VERTEX_BUFFER_VIEW                m_VBV;
    D3D12_VIEWPORT                          m_Viewport;
    D3D12_RECT                              m_Scissor;

    std::vector<ConstantBufferView<Transform>> m_CBV;            // frameCount 要素
    float                                   m_RotateAngle;
};
