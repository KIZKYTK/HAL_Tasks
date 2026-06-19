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
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { (x)->Release(); (x) = NULL; }
#endif

//----- 構造体
typedef struct {
    int pos_x0, pos_y0;     // 端点の座標0
    int pos_x1, pos_y1;     // 端点の座標1
} LINE;

typedef struct {
    int pos_x, pos_y;       // 円の中心座標
    int radius;             // 円の半径
} CIRCLE;

struct VERTEX
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

//----- 定数定義
#define SCREEN_WIDTH            800
#define SCREEN_HEIGHT           600
#define AXIS_X_OFFSET           (SCREEN_WIDTH / 2)
#define AXIS_Y_OFFSET           (SCREEN_HEIGHT / 2)

#define MAX_VERTICES            4096

//----- グローバル変数
const TCHAR szClassName[] = _T("2DLineCircle_DX11");
const TCHAR szAppName[] = _T("[ 2D_LINE_CIRCLE ] DirectX11");

LINE        g_Line;
CIRCLE      g_Circle;
bool        g_bOnHit;
int         g_Vx, g_Vy;

HWND        g_hWndApp;
bool        g_bOnInfo;

// DirectX11
ID3D11Device* g_pDevice = NULL;
ID3D11DeviceContext* g_pImmediateContext = NULL;
IDXGISwapChain* g_pSwapChain = NULL;
ID3D11RenderTargetView* g_pRenderTargetView = NULL;
ID3D11VertexShader* g_pVertexShader = NULL;
ID3D11PixelShader* g_pPixelShader = NULL;
ID3D11InputLayout* g_pInputLayout = NULL;
ID3D11Buffer* g_pVertexBuffer = NULL;

// 描画用一時頂点
VERTEX                      g_Vertices[MAX_VERTICES];
int                         g_VertexCount = 0;

//----- プロトタイプ宣言
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

bool InitializeGraphics(HWND hWnd);
void CleanupGraphics();
bool CreateRenderTarget();
bool CreateShaders();
void RenderGraphics();

void MainModule();
bool HitCheck();

void BeginDraw();
void EndDraw();
void AddLine(int x0, int y0, int x1, int y1, float r, float g, float b, float a);
void AddCircleLine(int cx, int cy, int radius, float r, float g, float b, float a);
DirectX::XMFLOAT3 ToScreenPosition(int x, int y);

void DrawGrid();
void DrawCircle();
void DrawLine();
void DrawHitCheckLine();
void DispInfoGDI();

//---------------------------------------------------------------------------------------
// シェーダーコンパイル
//---------------------------------------------------------------------------------------
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

//---------------------------------------------------------------------------------------
// メイン
//---------------------------------------------------------------------------------------
int WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPTSTR args, int cmdShow)
{
    MSG msg;

    WNDCLASS wndClass = {
        0, WndProc, 0, 0, hInst,
        LoadIcon(hInst, IDI_APPLICATION), LoadCursor(NULL, IDC_ARROW),
        0, NULL, szClassName
    };

    if (RegisterClass(&wndClass) == 0)
    {
        return FALSE;
    }

    g_hWndApp = CreateWindow(
        szClassName,
        szAppName,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        SCREEN_WIDTH + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
        SCREEN_HEIGHT + GetSystemMetrics(SM_CYFIXEDFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
        NULL,
        NULL,
        hInst,
        NULL);

    if (!g_hWndApp)
    {
        return FALSE;
    }

    ShowWindow(g_hWndApp, cmdShow);
    UpdateWindow(g_hWndApp);

    if (!InitializeGraphics(g_hWndApp))
    {
        CleanupGraphics();
        return FALSE;
    }

    // 変数初期化
    g_bOnInfo = true;

    g_Line.pos_x0 = -300;
    g_Line.pos_y0 = -200;
    g_Line.pos_x1 = 100;
    g_Line.pos_y1 = 200;

    g_Circle.pos_x = 100;
    g_Circle.pos_y = 0;
    g_Circle.radius = 100;

    g_Vx = 0;
    g_Vy = 0;

    g_bOnHit = HitCheck();

    ZeroMemory(&msg, sizeof(msg));

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            RenderGraphics();
            Sleep(0);
        }
    }

    CleanupGraphics();
    return (int)msg.wParam;
}

//-----------------------------------------------------------------------------
// メッセージ処理
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static bool MouseButtonFlg = false;

    switch (uMsg)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            DestroyWindow(hWnd);
            return 0;

        case 'I':
            g_bOnInfo = !g_bOnInfo;
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        if (MouseButtonFlg)
        {
            g_Circle.pos_x = LOWORD(lParam) - AXIS_X_OFFSET;
            g_Circle.pos_y = (HIWORD(lParam) - AXIS_Y_OFFSET) * -1;
            g_bOnHit = HitCheck();
            return 0;
        }
        break;

    case WM_LBUTTONDOWN:
        g_Circle.pos_x = LOWORD(lParam) - AXIS_X_OFFSET;
        g_Circle.pos_y = (HIWORD(lParam) - AXIS_Y_OFFSET) * -1;
        g_bOnHit = HitCheck();
        MouseButtonFlg = true;
        return 0;

    case WM_LBUTTONUP:
        MouseButtonFlg = false;
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//---------------------------------------------------------------------------------------
// DirectX11 初期化
//---------------------------------------------------------------------------------------
bool InitializeGraphics(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));

    sd.BufferCount = 1;
    sd.BufferDesc.Width = SCREEN_WIDTH;
    sd.BufferDesc.Height = SCREEN_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#if defined(_DEBUG)
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
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

    if (FAILED(hr))
    {
        MessageBox(hWnd, _T("Direct3D11 デバイスの作成に失敗しました"), _T("error"), MB_OK | MB_ICONERROR);
        return false;
    }

    if (!CreateRenderTarget())
    {
        return false;
    }

    if (!CreateShaders())
    {
        return false;
    }

    D3D11_BUFFER_DESC bd;
    ZeroMemory(&bd, sizeof(bd));
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX) * MAX_VERTICES;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = g_pDevice->CreateBuffer(&bd, NULL, &g_pVertexBuffer);
    if (FAILED(hr))
    {
        MessageBox(hWnd, _T("頂点バッファの作成に失敗しました"), _T("error"), MB_OK | MB_ICONERROR);
        return false;
    }

    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)SCREEN_WIDTH;
    vp.Height = (FLOAT)SCREEN_HEIGHT;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    g_pImmediateContext->RSSetViewports(1, &vp);

    return true;
}

//---------------------------------------------------------------------------------------
// レンダーターゲット作成
//---------------------------------------------------------------------------------------
bool CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer = NULL;

    HRESULT hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
    if (FAILED(hr))
    {
        return false;
    }

    hr = g_pDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
    SAFE_RELEASE(pBackBuffer);

    if (FAILED(hr))
    {
        return false;
    }

    g_pImmediateContext->OMSetRenderTargets(1, &g_pRenderTargetView, NULL);
    return true;
}

//---------------------------------------------------------------------------------------
// シェーダー作成
//---------------------------------------------------------------------------------------
bool CreateShaders()
{
    const char* shaderSource =
        "struct VS_IN                                      \n"
        "{                                                 \n"
        "    float3 pos   : POSITION;                      \n"
        "    float4 color : COLOR0;                        \n"
        "};                                                \n"
        "                                                  \n"
        "struct VS_OUT                                     \n"
        "{                                                 \n"
        "    float4 pos   : SV_POSITION;                   \n"
        "    float4 color : COLOR0;                        \n"
        "};                                                \n"
        "                                                  \n"
        "VS_OUT VSMain(VS_IN input)                        \n"
        "{                                                 \n"
        "    VS_OUT output;                                \n"
        "    output.pos = float4(input.pos, 1.0f);          \n"
        "    output.color = input.color;                   \n"
        "    return output;                                \n"
        "}                                                 \n"
        "                                                  \n"
        "float4 PSMain(VS_OUT input) : SV_Target           \n"
        "{                                                 \n"
        "    return input.color;                           \n"
        "}                                                 \n";

    ID3DBlob* pVSBlob = NULL;
    ID3DBlob* pPSBlob = NULL;

    HRESULT hr = CompileShaderFromMemory(shaderSource, "VSMain", "vs_4_0", &pVSBlob);
    if (FAILED(hr))
    {
        return false;
    }

    hr = CompileShaderFromMemory(shaderSource, "PSMain", "ps_4_0", &pPSBlob);
    if (FAILED(hr))
    {
        SAFE_RELEASE(pVSBlob);
        return false;
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
        return false;
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
        return false;
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

    return SUCCEEDED(hr);
}

//---------------------------------------------------------------------------------------
// DirectX11 解放
//---------------------------------------------------------------------------------------
void CleanupGraphics()
{
    if (g_pImmediateContext != NULL)
    {
        g_pImmediateContext->ClearState();
    }

    SAFE_RELEASE(g_pVertexBuffer);
    SAFE_RELEASE(g_pInputLayout);
    SAFE_RELEASE(g_pPixelShader);
    SAFE_RELEASE(g_pVertexShader);
    SAFE_RELEASE(g_pRenderTargetView);
    SAFE_RELEASE(g_pSwapChain);
    SAFE_RELEASE(g_pImmediateContext);
    SAFE_RELEASE(g_pDevice);
}

//---------------------------------------------------------------------------------------
// 描画
//---------------------------------------------------------------------------------------
void RenderGraphics()
{
    float clearColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    g_pImmediateContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

    BeginDraw();

    DrawGrid();
    DrawCircle();
    DrawLine();
    DrawHitCheckLine();

    EndDraw();

    g_pSwapChain->Present(1, 0);

    if (g_bOnInfo)
    {
        DispInfoGDI();
    }
}

//---------------------------------------------------------------------------------------
// 衝突判定チェックモジュール　あたり＝true
//---------------------------------------------------------------------------------------
bool HitCheck()
{
    // 演習用：
    // ここに線分と円の当たり判定を実装する。
    //
    // いったんビルド確認を優先するため false を返す。
    return false;
}

//---------------------------------------------------------------------------------------
// 描画開始
//---------------------------------------------------------------------------------------
void BeginDraw()
{
    g_VertexCount = 0;
}

//---------------------------------------------------------------------------------------
// 描画終了
//---------------------------------------------------------------------------------------
void EndDraw()
{
    if (g_VertexCount <= 0)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_pImmediateContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr))
    {
        return;
    }

    memcpy(mappedResource.pData, g_Vertices, sizeof(VERTEX) * g_VertexCount);
    g_pImmediateContext->Unmap(g_pVertexBuffer, 0);

    UINT stride = sizeof(VERTEX);
    UINT offset = 0;

    g_pImmediateContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pImmediateContext->IASetInputLayout(g_pInputLayout);
    g_pImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    g_pImmediateContext->VSSetShader(g_pVertexShader, NULL, 0);
    g_pImmediateContext->PSSetShader(g_pPixelShader, NULL, 0);

    g_pImmediateContext->Draw(g_VertexCount, 0);
}

//---------------------------------------------------------------------------------------
// 座標変換
// 元のサンプル座標:
//   原点は画面中央
//   Xは右がプラス
//   Yは上がプラス
//
// DirectX11のNDC:
//   Xは左 -1、右 +1
//   Yは上 +1、下 -1
//---------------------------------------------------------------------------------------
DirectX::XMFLOAT3 ToScreenPosition(int x, int y)
{
    float screenX = (float)(AXIS_X_OFFSET + x);
    float screenY = (float)(AXIS_Y_OFFSET - y);

    float ndcX = (screenX / (float)SCREEN_WIDTH) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenY / (float)SCREEN_HEIGHT) * 2.0f;

    return DirectX::XMFLOAT3(ndcX, ndcY, 0.0f);
}

//---------------------------------------------------------------------------------------
// ライン追加
//---------------------------------------------------------------------------------------
void AddLine(int x0, int y0, int x1, int y1, float r, float g, float b, float a)
{
    if (g_VertexCount + 2 >= MAX_VERTICES)
    {
        return;
    }

    g_Vertices[g_VertexCount].pos = ToScreenPosition(x0, y0);
    g_Vertices[g_VertexCount].color = DirectX::XMFLOAT4(r, g, b, a);
    ++g_VertexCount;

    g_Vertices[g_VertexCount].pos = ToScreenPosition(x1, y1);
    g_Vertices[g_VertexCount].color = DirectX::XMFLOAT4(r, g, b, a);
    ++g_VertexCount;
}

//---------------------------------------------------------------------------------------
// 円周ライン追加
//---------------------------------------------------------------------------------------
void AddCircleLine(int cx, int cy, int radius, float red, float green, float blue, float alpha)
{
    const int CIRCLE_DIVISION = 96;
    const float PI_VALUE = 3.14159265358979323846f;
    const float TWO_PI_VALUE = PI_VALUE * 2.0f;

    int i;
    float angle0;
    float angle1;
    int x0;
    int y0;
    int x1;
    int y1;

    for (i = 0; i < CIRCLE_DIVISION; ++i)
    {
        angle0 = ((float)i * TWO_PI_VALUE) / (float)CIRCLE_DIVISION;
        angle1 = ((float)(i + 1) * TWO_PI_VALUE) / (float)CIRCLE_DIVISION;

        x0 = cx + (int)(cosf(angle0) * (float)radius);
        y0 = cy + (int)(sinf(angle0) * (float)radius);

        x1 = cx + (int)(cosf(angle1) * (float)radius);
        y1 = cy + (int)(sinf(angle1) * (float)radius);

        AddLine(x0, y0, x1, y1, red, green, blue, alpha);
    }
}

//---------------------------------------------------------------------------------------
// グリッド表示モジュール
//---------------------------------------------------------------------------------------
void DrawGrid()
{
    int x, y;

    // グリッド
    for (x = -AXIS_X_OFFSET; x <= AXIS_X_OFFSET; x += 20)
    {
        AddLine(x, -AXIS_Y_OFFSET, x, AXIS_Y_OFFSET, 0.0f, 0.8f, 0.8f, 1.0f);
    }

    for (y = -AXIS_Y_OFFSET; y <= AXIS_Y_OFFSET; y += 20)
    {
        AddLine(-AXIS_X_OFFSET, y, AXIS_X_OFFSET, y, 0.0f, 0.8f, 0.8f, 1.0f);
    }

    // X軸・Y軸
    AddLine(-AXIS_X_OFFSET, 0, AXIS_X_OFFSET, 0, 1.0f, 0.0f, 0.0f, 1.0f);
    AddLine(0, -AXIS_Y_OFFSET, 0, AXIS_Y_OFFSET, 1.0f, 0.0f, 0.0f, 1.0f);
}

//---------------------------------------------------------------------------------------
// サークル描画モジュール
//---------------------------------------------------------------------------------------
void DrawCircle()
{
    if (g_bOnHit)
    {
        // ヒット時は赤系
        AddCircleLine(g_Circle.pos_x, g_Circle.pos_y, g_Circle.radius, 1.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        // 通常時は青系
        AddCircleLine(g_Circle.pos_x, g_Circle.pos_y, g_Circle.radius, 0.0f, 0.0f, 1.0f, 1.0f);
    }
}

//---------------------------------------------------------------------------------------
// ライン描画モジュール
//---------------------------------------------------------------------------------------
void DrawLine()
{
    AddLine(
        g_Line.pos_x0,
        g_Line.pos_y0,
        g_Line.pos_x1,
        g_Line.pos_y1,
        0.0f,
        1.0f,
        0.0f,
        1.0f);
}

//---------------------------------------------------------------------------------------
// 衝突判定ライン描画モジュール
//---------------------------------------------------------------------------------------
void DrawHitCheckLine()
{
    // 線分の端から円の中心へ
    AddLine(g_Line.pos_x0, g_Line.pos_y0, g_Circle.pos_x, g_Circle.pos_y, 1.0f, 0.0f, 1.0f, 1.0f);
    AddLine(g_Line.pos_x1, g_Line.pos_y1, g_Circle.pos_x, g_Circle.pos_y, 1.0f, 0.0f, 1.0f, 1.0f);

    // 線分と円との最短距離
    AddLine(g_Vx, g_Vy, g_Circle.pos_x, g_Circle.pos_y, 1.0f, 0.0f, 1.0f, 1.0f);
}

//---------------------------------------------------------------------------------------
// 情報表示モジュール
// DirectX11の図形描画後、簡易的にGDIで文字だけ表示する。
//---------------------------------------------------------------------------------------
void DispInfoGDI()
{
    HDC hdc = GetDC(g_hWndApp);
    if (hdc == NULL)
    {
        return;
    }

    TCHAR str[256];
    int len;

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(0, 0, 0));
    SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));

    len = _stprintf(str, _T("LINE   (%4d,%4d)-(%4d,%4d)"),
        g_Line.pos_x0, g_Line.pos_y0, g_Line.pos_x1, g_Line.pos_y1);
    TextOut(hdc, 0, 0, str, len);

    len = _stprintf(str, _T("CIRCLE (%4d,%4d) r = %4d"),
        g_Circle.pos_x, g_Circle.pos_y, g_Circle.radius);
    TextOut(hdc, 0, 20, str, len);

    if (g_bOnHit)
    {
        len = _stprintf(str, _T("HIT !!"));
        TextOut(hdc, 0, 40, str, len);
    }

    ReleaseDC(g_hWndApp, hdc);
}
