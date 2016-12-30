#pragma once

#include <d3d9.h>
#include <d3dx9.h>
#include <D3Dcompiler.h>

#include <VertexFormats.h>

//--------------------------------------------
//TODO: move to common header
#include <stdint.h>
#include <algorithm>
#include "strsafe.h"

#undef min
#undef max
//--------------------------------------------

#define USE_PIX_MARKERS

namespace gfx
{
    extern LPDIRECT3D9 D3D;

    bool init();
    void fini();

    int BeginScope(D3DCOLOR col, LPCWSTR wszName);
    int EndScope(void);

    struct Device
    {
        enum
        {
            MAX_CONSTS = 224,
        };

        struct Stats
        {
            uint32_t numTriangles;
            uint32_t numDraws;
        } stats;

        LPDIRECT3DDEVICE9 device = nullptr;

        bool init(UINT adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params);
        void fini();

        bool reset(D3DPRESENT_PARAMETERS& params);

        HRESULT TestCooperativeLevel();
        HRESULT EvictManagedResources();
        HRESULT GetDeviceCaps(D3DCAPS9* pCaps);
        HRESULT GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode);
        HRESULT GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer);
        void    SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp);
        void    GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp);
        HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle);
        HRESULT CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle);
        HRESULT CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle);
        HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle);
        HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle);
        HRESULT CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);
        HRESULT CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);
        HRESULT UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint);
        HRESULT UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture);
        HRESULT GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface);
        HRESULT GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface);
        HRESULT StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter);
        HRESULT CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle);
        HRESULT SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget);
        HRESULT GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget);
        HRESULT SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil);
        HRESULT GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface);
        HRESULT BeginScene();
        HRESULT EndScene();
        HRESULT Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
        HRESULT SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix);
        HRESULT SetViewport(CONST D3DVIEWPORT9* pViewport);
        HRESULT SetMaterial(CONST D3DMATERIAL9* pMaterial);
        HRESULT LightEnable(DWORD Index, BOOL Enable);
        HRESULT SetClipPlane(DWORD Index, CONST float* pPlane);
        HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
        HRESULT CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB);
        HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture);
        HRESULT SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
        HRESULT SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value);
        HRESULT SetScissorRect(CONST RECT* pRect);
        HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
        HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT StartIndex, UINT PrimitiveCount);
        HRESULT CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl);
        HRESULT SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl);
        HRESULT CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader);
        HRESULT SetVertexShader(IDirect3DVertexShader9* pShader);
        HRESULT SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount);
        HRESULT SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount);
        HRESULT SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount);
        HRESULT SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride);
        HRESULT SetStreamSourceFreq(UINT StreamNumber, UINT Setting);
        HRESULT SetIndices(IDirect3DIndexBuffer9* pIndexData);
        HRESULT CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader);
        HRESULT SetPixelShader(IDirect3DPixelShader9* pShader);
        HRESULT SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount);
        HRESULT SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount);
        HRESULT SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount);
        HRESULT CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery);

        HRESULT Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);

    private:
        void clearStats();
        void addDrawStats(D3DPRIMITIVETYPE primitiveType, uint32_t primitiveCount);
        void resetCache();
        void applyShaderConsts();

    private:
        static bool cached;

        uint32_t  rs_valid[8];
        DWORD     rs[256];

        uint32_t  tex_valid;
        LPDIRECT3DBASETEXTURE9 textures[16];

        enum
        {
            RES_STATE_VS,
            RES_STATE_PS,
            RES_STATE_VF,
            RES_STATE_IB,
            RES_STATE_VB0,
            RES_STATE_VB1,
            RES_STATE_VB2,
            RES_STATE_VB3,
            RES_STATE_COUNT
        };

        uint32_t res_states;

        LPDIRECT3DVERTEXSHADER9 vs;
        LPDIRECT3DPIXELSHADER9  ps;

        LPDIRECT3DVERTEXDECLARATION9 vf;

        struct
        {
            LPDIRECT3DVERTEXBUFFER9 vb;
            UINT                    offset;
            UINT                    stride;
        } vb_state[4];

        LPDIRECT3DINDEXBUFFER9  ib;

        D3DXVECTOR4 vertex_consts[MAX_CONSTS];
        D3DXVECTOR4 pixel_consts[MAX_CONSTS];

        UINT vconsts_range_min;
        UINT vconsts_range_max;

        UINT pconsts_range_min;
        UINT pconsts_range_max;
    };
}

#ifdef USE_PIX_MARKERS

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)

#define GFX_TRACE_SCOPE(id) GFX_TRACE_SCOPE_INTERNAL(id, __LINE__)
#define GFX_TRACE_SCOPE_INTERNAL(id, idx) GfxTraceScope sp##idx ( WIDEN(id), idx )

class GfxTraceScope
{
public:
    GfxTraceScope(WCHAR *Name, int Line)
    {
        WCHAR wc[MAX_PATH];
        StringCchPrintfW(wc, MAX_PATH, L"%s @ Line %d.\0", Name, Line);
        gfx::BeginScope(D3DCOLOR_XRGB(0, 0, 0), wc);
    }
    ~GfxTraceScope()
    {
        gfx::EndScope();
    }

    GfxTraceScope() = delete;
};

#else

#define GFX_TRACE_SCOPE(id)

#endif