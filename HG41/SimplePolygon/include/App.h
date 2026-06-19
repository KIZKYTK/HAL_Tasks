//-----------------------------------------------------------------------------
// File : App.h
// Desc : Application Module.
// Copyright(c) Pocol. All right reserved.
//-----------------------------------------------------------------------------
#pragma once

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <d3dcompiler.h>    //★追加.
#include <DirectXMath.h>    //★追加.
#include "Object.h"         // ← 追加

//-----------------------------------------------------------------------------
// Linker
//-----------------------------------------------------------------------------
#pragma comment( lib, "d3d12.lib" )
#pragma comment( lib, "dxgi.lib" )
#pragma comment( lib, "d3dcompiler.lib" )   // ★追加.

//-----------------------------------------------------------------------------
// Type Alias
//-----------------------------------------------------------------------------
template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;


///////////////////////////////////////////////////////////////////////////////
// App class
///////////////////////////////////////////////////////////////////////////////
class App
{
public:
    App(uint32_t width, uint32_t height);
    ~App();
    void Run();

private:
    static const uint32_t FrameCount = 2;   // フレームバッファ数です.

    HINSTANCE   m_hInst;
    HWND        m_hWnd;
    uint32_t    m_Width;
    uint32_t    m_Height;

    ComPtr<ID3D12Device>                   m_pDevice;
    ComPtr<ID3D12CommandQueue>             m_pQueue;
    ComPtr<IDXGISwapChain3>                m_pSwapChain;
    ComPtr<ID3D12Resource>                 m_pColorBuffer[FrameCount];
    ComPtr<ID3D12CommandAllocator>         m_pCmdAllocator[FrameCount];
    ComPtr<ID3D12GraphicsCommandList>      m_pCmdList;
    ComPtr<ID3D12DescriptorHeap>           m_pHeapRTV;
    ComPtr<ID3D12Fence>                    m_pFence;

    HANDLE                          m_FenceEvent;
    uint64_t                        m_FenceCounter[FrameCount];
    uint32_t                        m_FrameIndex;
    D3D12_CPU_DESCRIPTOR_HANDLE     m_HandleRTV[FrameCount];

    // Object に移譲
    Object                          m_Object;

    //=========================================================================
    // private methods.
    //=========================================================================
    bool InitApp();
    void TermApp();
    bool InitWnd();
    void TermWnd();
    void MainLoop();
    bool InitD3D();
    void TermD3D();
    void Render();
    void WaitGpu();
    void Present(uint32_t interval);
    bool OnInit();
    void OnTerm();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);
};