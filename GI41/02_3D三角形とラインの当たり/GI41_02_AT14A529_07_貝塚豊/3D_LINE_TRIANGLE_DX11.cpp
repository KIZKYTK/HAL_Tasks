#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS
#define STRICT

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <math.h>
#include <mmsystem.h>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "winmm.lib")

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { (x)->Release(); (x) = NULL; }
#endif

#define CLASS_NAME      _T("3D_LINE_TRIANGLE")
#define CAPTION_NAME    _T("AT14 DirectX11")

#define SCREEN_WIDTH    640
#define SCREEN_HEIGHT   480
#define FRAME_RATE      1000.0f / 60.0f
#define ASPECT_RATIO    ((float)SCREEN_WIDTH / (float)SCREEN_HEIGHT)
#define NEAR_CLIP       100.0f
#define FAR_CLIP        30000.0f
#define LINE_MOVE_SPEED 1.0f

// Set to 1 only when you need debug text.
// GDI text drawing after Present can cause stutter/flicker with DXGI.
#define ENABLE_GDI_DEBUG_TEXT 0

struct VERTEX
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

struct CB_TRANSFORM
{
    DirectX::XMMATRIX matWVP;
};

ID3D11Device* g_pDevice = NULL;
ID3D11DeviceContext* g_pImmediateContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11Texture2D* g_pDepthStencil = NULL;
ID3D11DepthStencilView* g_pDepthStencilView = NULL;
ID3D11VertexShader* g_pVertexShader = NULL;
ID3D11PixelShader* g_pPixelShader = NULL;
ID3D11InputLayout* g_pInputLayout = NULL;
ID3D11Buffer* g_pVertexBuffer = NULL;
ID3D11Buffer* g_pConstantBuffer = NULL;
ID3D11RasterizerState* g_pRasterizerState = NULL;

HWND                        g_hWnd = NULL;
TCHAR                       g_szDebug[1024];

int                         g_MousePosX, g_MousePosY;
int                         g_MouseOldPosX, g_MouseOldPosY;
int                         g_MouseSpeedX, g_MouseSpeedY;
bool                        g_MouseButtonL;
float                       g_CameraDeg;

DirectX::XMMATRIX           g_MatView;
DirectX::XMMATRIX           g_MatProj;
DirectX::XMMATRIX           g_MatWorld;

VERTEX                      g_VertexTriangle[3];
VERTEX                      g_VertexLine[2];

bool                        g_bOnHit;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HRESULT InitializeGraphics(HWND hWnd, bool bWindow);
HRESULT CleanupGraphics();
HRESULT RenderGraphics();
HRESULT CreateRenderTargets(UINT width, UINT height);
HRESULT CreateShadersAndStates();
void ExecMain();
void DrawMain();
bool HitCheck();
void DrawDebugStringGDI();

static HRESULT CompileShaderFromMemory(
    const char* source,
    const char* entryPoint,
    const char* shaderModel,
    ID3DBlob** ppBlob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG;
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrorBlob = NULL;
    HRESULT hr = D3DCompile(
        source,
        strlen(source),
        NULL,
        NULL,
        NULL,
        entryPoint,
        shaderModel,
        flags,
        0,
        ppBlob,
        &pErrorBlob);

    if (FAILED(hr))
    {
        if (pErrorBlob != NULL)
        {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
        }
        SAFE_RELEASE(pErrorBlob);
        return hr;
    }

    SAFE_RELEASE(pErrorBlob);
    return S_OK;
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInst, LPTSTR lpCmdLine, int iCmdShow)
{
    MSG msg;
    DWORD dwExecLastTime;
    DWORD dwCurrentTime;

    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WindowProc,
        0, 0, hInstance, LoadIcon(hInstance, IDI_APPLICATION), LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)GetStockObject(WHITE_BRUSH), NULL, CLASS_NAME, NULL };

    if (RegisterClassEx(&wc) == 0)
    {
        return FALSE;
    }

    g_hWnd = CreateWindow(
        CLASS_NAME,
        CAPTION_NAME,
        WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        SCREEN_WIDTH + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
        SCREEN_HEIGHT + GetSystemMetrics(SM_CYFIXEDFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
        NULL,
        NULL,
        hInstance,
        NULL);

    if (g_hWnd == NULL)
    {
        return FALSE;
    }

    ShowWindow(g_hWnd, iCmdShow);
    UpdateWindow(g_hWnd);

    bool bWindow = false;
    if (IDYES == MessageBox(g_hWnd, _T("ウィンドウモードで実行しますか？"), _T("画面モード"), MB_YESNO))
    {
        bWindow = true;
    }

    if (FAILED(InitializeGraphics(g_hWnd, bWindow)))
    {
        CleanupGraphics();
        return FALSE;
    }

    timeBeginPeriod(1);
    dwExecLastTime = timeGetTime();
    g_MouseButtonL = false;
    g_CameraDeg = 0.0f;

    msg.message = WM_NULL;
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            dwCurrentTime = timeGetTime();
            if (dwCurrentTime - dwExecLastTime > FRAME_RATE)
            {
                dwExecLastTime = dwCurrentTime;
                ExecMain();
                RenderGraphics();
            }
        }
        Sleep(0);
    }

    CleanupGraphics();
    timeEndPeriod(1);

    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hWnd);
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        if (g_MouseButtonL)
        {
            g_MousePosX = LOWORD(lParam);
            g_MousePosY = HIWORD(lParam);
        }
        return 0;

    case WM_LBUTTONDOWN:
        g_MouseButtonL = true;
        g_MouseOldPosX = g_MousePosX = LOWORD(lParam);
        g_MouseOldPosY = g_MousePosY = HIWORD(lParam);
        return 0;

    case WM_LBUTTONUP:
        g_MouseButtonL = false;
        return 0;

    case WM_SIZE:
        if (g_pSwapChain != NULL && g_pImmediateContext != NULL)
        {
            UINT width = LOWORD(lParam);
            UINT height = HIWORD(lParam);
            if (width > 0 && height > 0)
            {
                g_pImmediateContext->OMSetRenderTargets(0, NULL, NULL);
                SAFE_RELEASE(g_pRenderTargetView);
                SAFE_RELEASE(g_pDepthStencilView);
                SAFE_RELEASE(g_pDepthStencil);

                HRESULT hr = g_pSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
                if (SUCCEEDED(hr))
                {
                    CreateRenderTargets(width, height);
                    g_MatProj = DirectX::XMMatrixPerspectiveFovLH(
                        DirectX::XMConvertToRadians(30.0f),
                        (float)width / (float)height,
                        NEAR_CLIP,
                        FAR_CLIP);
                }
            }
        }
        return 0;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

HRESULT CreateRenderTargets(UINT width, UINT height)
{
    ID3D11Texture2D* pBackBuffer = NULL;
    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    SAFE_RELEASE(pBackBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory(&descDepth, sizeof(descDepth));
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = g_pDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory(&descDSV, sizeof(descDSV));
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;

    hr = g_pDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
    if (FAILED(hr))
    {
        return hr;
    }

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports(1, &vp);

    return S_OK;
}

HRESULT CreateShadersAndStates()
{
    const char* shaderSource =
        "cbuffer CBTransform : register(b0)                    \n"
        "{                                                    \n"
        "    matrix g_matWVP;                                 \n"
        "};                                                   \n"
        "                                                     \n"
        "struct VS_IN                                         \n"
        "{                                                    \n"
        "    float3 pos   : POSITION;                         \n"
        "    float4 color : COLOR0;                           \n"
        "};                                                   \n"
        "                                                     \n"
        "struct VS_OUT                                        \n"
        "{                                                    \n"
        "    float4 pos   : SV_POSITION;                      \n"
        "    float4 color : COLOR0;                           \n"
        "};                                                   \n"
        "                                                     \n"
        "VS_OUT VSMain(VS_IN input)                           \n"
        "{                                                    \n"
        "    VS_OUT output;                                   \n"
        "    output.pos = mul(float4(input.pos, 1.0f), g_matWVP); \n"
        "    output.color = input.color;                      \n"
        "    return output;                                   \n"
        "}                                                    \n"
        "                                                     \n"
        "float4 PSMain(VS_OUT input) : SV_Target              \n"
        "{                                                    \n"
        "    return input.color;                              \n"
        "}                                                    \n";

    ID3DBlob* pVSBlob = NULL;
    ID3DBlob* pPSBlob = NULL;
    HRESULT hr = CompileShaderFromMemory(shaderSource, "VSMain", "vs_4_0", &pVSBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CompileShaderFromMemory(shaderSource, "PSMain", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        SAFE_RELEASE(pVSBlob);
        return hr;
    }

    hr = g_pDevice->CreateVertexShader(
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        NULL,
        &g_pVertexShader);
    if (FAILED(hr))
    {
        SAFE_RELEASE(pVSBlob);
        SAFE_RELEASE(pPSBlob);
        return hr;
    }

    hr = g_pDevice->CreatePixelShader(
        pPSBlob->GetBufferPointer(),
        pPSBlob->GetBufferSize(),
        NULL,
        &g_pPixelShader);
    if (FAILED(hr))
    {
        SAFE_RELEASE(pVSBlob);
        SAFE_RELEASE(pPSBlob);
        return hr;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof(VERTEX, pos),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(VERTEX, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = g_pDevice->CreateInputLayout(
        layout,
        2,
        pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(),
        &g_pInputLayout);
    SAFE_RELEASE(pVSBlob);
    SAFE_RELEASE(pPSBlob);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX) * 3;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_BUFFER_DESC cbDesc;
    ZeroMemory(&cbDesc, sizeof(cbDesc));
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(CB_TRANSFORM);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    hr = g_pDevice->CreateBuffer(&cbDesc, NULL, &g_pConstantBuffer);
    if (FAILED(hr))
    {
        return hr;
    }

    D3D11_RASTERIZER_DESC rsDesc;
    ZeroMemory(&rsDesc, sizeof(rsDesc));
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.DepthClipEnable = TRUE;

    hr = g_pDevice->CreateRasterizerState(&rsDesc, &g_pRasterizerState);
    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

HRESULT InitializeGraphics(HWND hWnd, bool bWindow)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 1;
    sd.BufferDesc.Width = SCREEN_WIDTH;
    sd.BufferDesc.Height = SCREEN_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = bWindow ? 0 : 60;
    sd.BufferDesc.RefreshRate.Denominator = bWindow ? 0 : 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = bWindow ? TRUE : FALSE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = E_FAIL;
    D3D_FEATURE_LEVEL featureLevel;
    for (int i = 0; i < 3; ++i)
    {
        hr = D3D11CreateDeviceAndSwapChain(
            NULL,
            driverTypes[i],
            NULL,
            createDeviceFlags,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pDevice,
            &featureLevel,
            &g_pImmediateContext);

        if (SUCCEEDED(hr))
        {
            break;
        }
    }

    if (FAILED(hr))
    {
        MessageBox(hWnd, _T("Direct3D11 デバイスの作成に失敗しました"), _T("error"), MB_OK | MB_ICONERROR);
        return hr;
    }

    hr = CreateRenderTargets(SCREEN_WIDTH, SCREEN_HEIGHT);
    if (FAILED(hr))
    {
        MessageBox(hWnd, _T("レンダーターゲットの作成に失敗しました"), _T("error"), MB_OK | MB_ICONERROR);
        return hr;
    }

    hr = CreateShadersAndStates();
    if (FAILED(hr))
    {
        MessageBox(hWnd, _T("シェーダーまたは各種ステートの作成に失敗しました"), _T("error"), MB_OK | MB_ICONERROR);
        return hr;
    }

    g_MatProj = DirectX::XMMatrixPerspectiveFovLH(
        DirectX::XMConvertToRadians(30.0f),
        ASPECT_RATIO,
        NEAR_CLIP,
        FAR_CLIP);
    g_MatWorld = DirectX::XMMatrixIdentity();

    g_VertexTriangle[0].pos = DirectX::XMFLOAT3(-100.0f, 100.0f, 100.0f);
    g_VertexTriangle[1].pos = DirectX::XMFLOAT3(100.0f, 100.0f, 50.0f);
    g_VertexTriangle[2].pos = DirectX::XMFLOAT3(-100.0f, -100.0f, 0.0f);

    g_VertexLine[0].pos = DirectX::XMFLOAT3(-200.0f, 100.0f, -200.0f);
    g_VertexLine[1].pos = DirectX::XMFLOAT3(-200.0f, -100.0f, 0.0f);
    g_VertexLine[0].color = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    g_VertexLine[1].color = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    return S_OK;
}

HRESULT CleanupGraphics()
{
    if (g_pImmediateContext != NULL)
    {
        g_pImmediateContext->ClearState();
    }

    SAFE_RELEASE(g_pRasterizerState);
    SAFE_RELEASE(g_pConstantBuffer);
    SAFE_RELEASE(g_pVertexBuffer);
    SAFE_RELEASE(g_pInputLayout);
    SAFE_RELEASE(g_pPixelShader);
    SAFE_RELEASE(g_pVertexShader);
    SAFE_RELEASE(g_pDepthStencilView);
    SAFE_RELEASE(g_pDepthStencil);
    SAFE_RELEASE(g_pRenderTargetView);
    SAFE_RELEASE(g_pSwapChain);
    SAFE_RELEASE(g_pImmediateContext);
    SAFE_RELEASE(g_pDevice);

    return S_OK;
}

static void DrawPrimitive(VERTEX* pVertices, UINT vertexCount, D3D11_PRIMITIVE_TOPOLOGY topology)
{
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_pImmediateContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        return;
    }

    memcpy(mappedResource.pData, pVertices, sizeof(VERTEX) * vertexCount);
    g_pImmediateContext->Unmap(g_pVertexBuffer, 0);

    UINT stride = sizeof(VERTEX);
    UINT offset = 0;
    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    g_pImmediateContext->IASetPrimitiveTopology(topology);

    g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);
    g_pImmediateContext->RSSetState(g_pRasterizerState);

    CB_TRANSFORM cb;
    cb.matWVP = DirectX::XMMatrixTranspose(g_MatWorld * g_MatView * g_MatProj);
    g_pImmediateContext->UpdateSubresource(g_pConstantBuffer, 0, NULL, &cb, 0, 0);
    g_pImmediateContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);

    g_pImmediateContext->Draw(vertexCount, 0);
}

HRESULT RenderGraphics()
{
    float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);
    g_pImmediateContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    DrawMain();

    // Present(1, 0) waits for vertical sync and gives smoother frame pacing.
    HRESULT hr = g_pSwapChain->Present(1, 0);

#if ENABLE_GDI_DEBUG_TEXT
    if (SUCCEEDED(hr))
    {
        DrawDebugStringGDI();
    }
#endif

    return hr;
}

void ExecMain()
{
    TCHAR str[256];

    g_szDebug[0] = _T('\0');

    _stprintf(str, _T("マウスの左右でカメラ回転\n"));
    lstrcat(g_szDebug, str);
    _stprintf(str, _T("カーソルキーでラインの移動（ＸＺ方向）\n"));
    lstrcat(g_szDebug, str);

    if (GetAsyncKeyState(VK_LEFT) & 0x8000)
    {
        g_VertexLine[0].pos.x -= LINE_MOVE_SPEED;
        g_VertexLine[1].pos.x -= LINE_MOVE_SPEED;
    }
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
    {
        g_VertexLine[0].pos.x += LINE_MOVE_SPEED;
        g_VertexLine[1].pos.x += LINE_MOVE_SPEED;
    }
    if (GetAsyncKeyState(VK_UP) & 0x8000)
    {
        g_VertexLine[0].pos.z += LINE_MOVE_SPEED;
        g_VertexLine[1].pos.z += LINE_MOVE_SPEED;
    }
    if (GetAsyncKeyState(VK_DOWN) & 0x8000)
    {
        g_VertexLine[0].pos.z -= LINE_MOVE_SPEED;
        g_VertexLine[1].pos.z -= LINE_MOVE_SPEED;
    }

    g_MouseSpeedX = g_MousePosX - g_MouseOldPosX;
    g_MouseSpeedY = g_MousePosY - g_MouseOldPosY;
    g_MouseOldPosX = g_MousePosX;
    g_MouseOldPosY = g_MousePosY;
    g_CameraDeg -= (float)g_MouseSpeedX * 0.01f;

    g_bOnHit = HitCheck();
}

void DrawMain()
{
    float px = sinf(g_CameraDeg) * 1000.0f;
    float pz = -cosf(g_CameraDeg) * 1000.0f;

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(px, 0.0f, pz, 1.0f);
    DirectX::XMVECTOR at = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    g_MatView = DirectX::XMMatrixLookAtLH(eye, at, up);

    DirectX::XMFLOAT4 color;
    if (g_bOnHit)
    {
        color = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        color = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    g_VertexTriangle[0].color = color;
    g_VertexTriangle[1].color = color;
    g_VertexTriangle[2].color = color;

    DrawPrimitive(g_VertexTriangle, 3, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    DrawPrimitive(g_VertexLine, 2, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
}

void DrawDebugStringGDI()
{
    HDC hdc = GetDC(g_hWnd);
    if (hdc == NULL)
    {
        return;
    }

    RECT rect;
    GetClientRect(g_hWnd, &rect);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawText(hdc, g_szDebug, -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);

    ReleaseDC(g_hWnd, hdc);
}

bool HitCheck()
{
    /*************************************************************************

    ここに交差判定のプログラムを追加する

    必要となるローカル変数は自由に作る


    （Phase 1)
    ・三角形の法線ベクトル（正規化する）を求める
    ・法線ベクトルに対して直線を投影した結果を求める（２点存在。内積を使う）
    ・三角形の頂点座標を投影して上記の直線を投影した範囲内かどうか調査する
    ・範囲外なら分離面が存在するとして「衝突していない」として終了

    （Phase 2)
    ・三角形のエッジを表すベクトルと線分に直交するベクトルを外積で求める（正規化する）
    ・直交ベクトルに三角形の３頂点を投影する（内積を使う）
    ・投影した３点から最小値、最大値を求めて「範囲」を求める
    ・線分の頂点（どちらか一つ）が範囲内かどうか調査する
    ・範囲外なら分離面が存在するとして「衝突していない」として終了
    　＜Phase2の処理をエッジ３つ分について繰り返す＞

    （補足）
    ・ヒットしたら true 、ヒットしなかったら false でリターンする

    *************************************************************************/

    using namespace DirectX;

    XMVECTOR tri0 = XMLoadFloat3(&g_VertexTriangle[0].pos);
    XMVECTOR tri1 = XMLoadFloat3(&g_VertexTriangle[1].pos);
    XMVECTOR tri2 = XMLoadFloat3(&g_VertexTriangle[2].pos);

    XMVECTOR line0 = XMLoadFloat3(&g_VertexLine[0].pos);
    XMVECTOR line1 = XMLoadFloat3(&g_VertexLine[1].pos);

    //=========================================================
    // phase 1
    // 三角形法線による分離軸判定
    //=========================================================

    // 三角形エッジ
    XMVECTOR edge0 = tri1 - tri0;
    XMVECTOR edge1 = tri2 - tri0;

    // 法線ベクトル
    XMVECTOR normal = XMVector3Normalize(
        XMVector3Cross(edge0, edge1));

    // 線分を法線へ投影
    float lineProj0 =
        XMVectorGetX(XMVector3Dot(line0, normal));

    float lineProj1 =
        XMVectorGetX(XMVector3Dot(line1, normal));

    // 三角形頂点を法線へ投影
    float triProj0 =
        XMVectorGetX(XMVector3Dot(tri0, normal));

    float triProj1 =
        XMVectorGetX(XMVector3Dot(tri1, normal));

    float triProj2 =
        XMVectorGetX(XMVector3Dot(tri2, normal));

    // 三角形投影範囲
    float triMin = min(triProj0, min(triProj1, triProj2));
    float triMax = max(triProj0, max(triProj1, triProj2));

    // 線分投影範囲
    float lineMin = min(lineProj0, lineProj1);
    float lineMax = max(lineProj0, lineProj1);

    // 分離判定
    if (lineMax < triMin || lineMin > triMax)
    {
        return false;
    }

    //=========================================================
    // phase 2
    // SAT によるエッジ判定
    //=========================================================

    XMVECTOR triangle[3] =
    {
        tri0,
        tri1,
        tri2
    };

    XMVECTOR lineVec = line1 - line0;

    for (int i = 0; i < 3; ++i)
    {
        // 三角形エッジ
        XMVECTOR p0 = triangle[i];
        XMVECTOR p1 = triangle[(i + 1) % 3];

        XMVECTOR triEdge = p1 - p0;

        // 線分とエッジに直交する軸
        XMVECTOR axis =
            XMVector3Cross(triEdge, lineVec);

        // 軸長0対策
        float axisLen =
            XMVectorGetX(XMVector3LengthSq(axis));

        if (axisLen <= 0.00001f)
        {
            continue;
        }

        axis = XMVector3Normalize(axis);

        // 三角形投影
        float proj0 =
            XMVectorGetX(XMVector3Dot(tri0, axis));

        float proj1 =
            XMVectorGetX(XMVector3Dot(tri1, axis));

        float proj2 =
            XMVectorGetX(XMVector3Dot(tri2, axis));

        float minTri =
            min(proj0, min(proj1, proj2));

        float maxTri =
            max(proj0, max(proj1, proj2));

        // 線分投影
        float projLine0 =
            XMVectorGetX(XMVector3Dot(line0, axis));

        float projLine1 =
            XMVectorGetX(XMVector3Dot(line1, axis));

        float minLine =
            min(projLine0, projLine1);

        float maxLine =
            max(projLine0, projLine1);

        // 分離軸が存在
        if (maxLine < minTri || minLine > maxTri)
        {
            return false;
        }
    }

    return true;
}
