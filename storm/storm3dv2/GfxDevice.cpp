#include "GfxDevice.h"
#include "storm3d_terrain_utils.h"
#include <assert.h>
#include <D3Dcompiler.h>

D3DVERTEXELEMENT9 VertexDesc_P3NUV2[] = {
    {0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
    {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P3NUV4[] = {
    {0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
    {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    {0, 40, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
    {0, 48, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P3D[] = {
    {0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P3DUV2[] = {
    {0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0},
    {0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P4DUV[] = {
    {0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0}, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
    {0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,     0},
    {0, 20, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P4UV[] = {
    {0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0}, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
    {0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P2DUV[] = {
    {0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0,  8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0},
    {0, 12, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexDesc_P2UV[] = {
    {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    D3DDECL_END()
};

LPD3DVERTEXELEMENT9 vtxFmtDescs[FVF_COUNT] = {
    VertexDesc_P3NUV2,
    VertexDesc_P3NUV4,
    VertexDesc_P3D,
    VertexDesc_P3DUV2,
    VertexDesc_P4DUV,
    VertexDesc_P4UV,
    VertexDesc_P2DUV,
    VertexDesc_P2UV
};



enum ShaderType
{
    VERTEX_SHADER,
    PIXEL_SHADER
};

ID3DBlob* compileShader(ShaderType type, const char* path, size_t path_len, D3D_SHADER_MACRO* defines, size_t defines_count)
{
    assert(path);

    const char* target = 0;

    switch (type)
    {
        case VERTEX_SHADER:
            target = "vs_3_0";
            break;
        case PIXEL_SHADER:
            target = "ps_3_0";
            break;
    }

    assert(target);

    std::string shaderString;
    frozenbyte::storm::readFile(shaderString, path);

    ID3DBlob*  code = 0;
    ID3DBlob*  msgs = 0;

    HRESULT hr = D3DCompile(
        shaderString.c_str(), shaderString.size(),
        path, defines, NULL,
        "main", target,
#ifdef _DEBUG
        D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR|D3DCOMPILE_SKIP_OPTIMIZATION|D3DCOMPILE_DEBUG,
#else
        D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR|D3DCOMPILE_OPTIMIZATION_LEVEL3,
#endif
        0, &code, &msgs
    );

    if (FAILED(hr))
    {
        char* report = (char *)msgs->GetBufferPointer();
        OutputDebugString(report);
        MessageBox(NULL, "D3DCompile failed!", "Error!!!", MB_OK);
    }

    if (msgs) msgs->Release();

    return code;
}

template<size_t path_len, size_t defines_count>
ID3DBlob* compileShader(ShaderType type, const char (&path)[path_len], D3D_SHADER_MACRO (&defines)[defines_count])
{
    return compileShader(type, path, path_len, defines, defines_count);
}



template<size_t N>
bool bitset32_is_bit_set(uint32_t (&set)[N], size_t i)
{
    size_t word = i>>5;
    size_t bit  = i&31;

    assert(word<N);

    return (set[word] & (1<<bit)) != 0;
}

template<size_t N>
void bitset32_set_bit(uint32_t (&set)[N], size_t i)
{
    size_t word = i>>5;
    size_t bit  = i&31;

    assert(word<N);

    set[word] |= 1<<bit;
}

template<size_t N>
void bitset32_clear(uint32_t (&set)[N])
{
    for (size_t i=0; i<N; ++i)
        set[i] = 0;
}

bool bitset32_is_bit_set(uint32_t set, size_t i)
{
    size_t word = i>>5;
    size_t bit  = i&31;

    assert(word==0);

    return (set & (1<<bit)) != 0;
}

void bitset32_set_bit(uint32_t& set, size_t i)
{
    size_t word = i>>5;
    size_t bit  = i&31;

    assert(word==0);

    set |= 1<<bit;
}

void bitset32_clear(uint32_t& set)
{
    set = 0;
}

#define DYNAMIC_VB_FRAME_SIZE    (10 * (1<<20))
#define DYNAMIC_IB16_FRAME_SIZE  ( 1 * (1<<20))

bool GfxDevice::cached = true;

void GfxDevice::resetCache()
{
    bitset32_clear(rs_valid);
    bitset32_clear(tex_valid);
    bitset32_clear(res_states);
}

bool GfxDevice::init(LPDIRECT3D9 d3d, UINT Adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params)
{
   	if (FAILED(d3d->CreateDevice(Adapter, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &device)))
        return false;

    for (auto& q: frame_sync) q = 0;

    createFrameResources();

    for (size_t i = 0; i < FVF_COUNT; ++i)
        device->CreateVertexDeclaration(vtxFmtDescs[i], &vtxFmtSet[i]);

    D3D_SHADER_MACRO defines[4];
    for (size_t i=0; i<STD_SHADER_COUNT; ++i)
    {
        size_t idx = 0;

        if (i&SSF_2D_POS)
        {
            defines[idx].Name = "VS_2D_POS";
            defines[idx].Definition = "";
            ++idx;
        }

        if (i & SSF_COLOR)
        {
            defines[idx].Name = "ENABLE_COLOR";
            defines[idx].Definition = "";
            ++idx;
        }

        if (i & SSF_TEXTURE)
        {
            defines[idx].Name = "ENABLE_TEXTURE";
            defines[idx].Definition = "";
            ++idx;
        }

        defines[idx].Definition = 0;
        defines[idx].Name = 0;

        ID3DBlob* vscode = compileShader(VERTEX_SHADER, "Data\\shaders\\std.vs", defines);
        ID3DBlob* pscode = compileShader(PIXEL_SHADER,  "Data\\shaders\\std.ps", defines);

        if (vscode && pscode)
        {
            CreateVertexShader((const DWORD*)vscode->GetBufferPointer(), &stdVS[i]);
            CreatePixelShader((const DWORD*)pscode->GetBufferPointer(), &stdPS[i]);
        }

        if (vscode) vscode->Release();
        if (pscode) pscode->Release();
    }

    return true;
}

void GfxDevice::fini()
{
    for (size_t i=0; i<STD_SHADER_COUNT; ++i)
    {
        if (stdVS[i]) stdVS[i]->Release();
        if (stdPS[i]) stdPS[i]->Release();
    }

    for (auto vf: vtxFmtSet) vf->Release();

    destroyFrameResources();
    device->Release();
}

bool GfxDevice::reset(D3DPRESENT_PARAMETERS& params)
{
    destroyFrameResources();

    if (SUCCEEDED(device->Reset(&params)))
    {
        createFrameResources();
        return true;
    }

    return false;
}

void GfxDevice::beginFrame()
{
    assert(frame_id >= 0);
    assert(frame_id < NUM_FRAMES_DELAY);

    while (frame_sync[frame_id]->GetData( NULL, 0, D3DGETDATA_FLUSH ) == S_FALSE);

    frame_vb_used   = 0;
    frame_ib16_used = 0;

    D3DXMatrixIdentity(&world_mat);
    D3DXMatrixIdentity(&view_mat);
    D3DXMatrixIdentity(&proj_mat);

    resetCache();
}

void GfxDevice::endFrame()
{
    assert(frame_id >= 0);
    assert(frame_id < NUM_FRAMES_DELAY);

    frame_sync[frame_id]->Issue(D3DISSUE_END);

    frame_id = (frame_id + 1) % NUM_FRAMES_DELAY;

    device->Present(NULL, NULL, NULL, NULL);
}

void GfxDevice::SetDynIdx16Buffer()
{
    SetIndices(frame_ib16[frame_id]);
}

bool GfxDevice::lockDynIdx16(UINT count, uint16_t** ptr, UINT* baseIndex)
{
    UINT sz = count*sizeof(uint16_t);

    assert(frame_ib16_used % sizeof(uint16_t) == 0);

    if (frame_ib16_used + sz > DYNAMIC_IB16_FRAME_SIZE)
    {
        assert(0);
        return false;
    }

    HRESULT hr = frame_ib16[frame_id]->Lock(frame_ib16_used, sz, (void**)ptr, D3DLOCK_NOOVERWRITE);
    assert(SUCCEEDED(hr));

    *baseIndex = frame_ib16_used / sizeof(uint16_t);

    frame_ib16_used += sz;

    return true;
}

void GfxDevice::unlockDynIdx16()
{
    frame_ib16[frame_id]->Unlock();
}

void GfxDevice::SetDynVtxBuffer(UINT Stride)
{
    SetStreamSource(0, frame_vb[frame_id], 0, Stride);
}

bool GfxDevice::lockDynVtx(UINT count, UINT stride, void** ptr, UINT* baseVertex)
{
    UINT sz = count*stride;
    UINT rem;

    rem = frame_vb_used % stride;
    rem = rem == 0 ? 0 : stride - rem;

    if (frame_vb_used + rem + sz > DYNAMIC_VB_FRAME_SIZE)
    {
        assert(0);
        return false;
    }

    frame_vb_used += rem;

    HRESULT hr = frame_vb[frame_id]->Lock(frame_vb_used, sz, ptr, D3DLOCK_NOOVERWRITE);
    assert(SUCCEEDED(hr));

    *baseVertex = frame_vb_used / stride;

    frame_vb_used += sz;

    return true;
}

void GfxDevice::unlockDynVtx()
{
    frame_vb[frame_id]->Unlock();
}

void GfxDevice::SetStdProgram(size_t id)
{
    assert(id<STD_SHADER_COUNT);

    SetVertexShader(stdVS[id]);
    SetPixelShader(stdPS[id]);
}

void GfxDevice::SetWorldMatrix(const D3DXMATRIX& world)
{
    world_mat = world;
}

void GfxDevice::SetViewMatrix(const D3DXMATRIX& view)
{
    view_mat = view;
}

void GfxDevice::SetProjectionMatrix(const D3DXMATRIX& proj)
{
    proj_mat = proj;
}

void GfxDevice::CommitConstants()
{
    D3DXMATRIX MVP;

    D3DXMatrixMultiply(&MVP, &view_mat, &proj_mat);
    D3DXMatrixMultiply(&MVP, &world_mat, &MVP);

    SetVertexShaderConstantF(0, MVP, 4);
}

void GfxDevice::SetFVF(FVF vtxFmt)
{
    SetVertexDeclaration(vtxFmtSet[vtxFmt]);
}

void GfxDevice::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* vertexData,UINT Stride)
{
    void* vertices = NULL;
    UINT  baseVertex = 0;
    UINT  VertexCount = 0;

    switch (PrimitiveType)
    {
        case D3DPT_TRIANGLESTRIP:
            VertexCount = PrimitiveCount + 2;
            break;
        case D3DPT_TRIANGLELIST:
            VertexCount = 3 * PrimitiveCount;
            break;
        default:
            assert(0);
    }

    lockDynVtx(VertexCount, Stride, &vertices, &baseVertex);
    memcpy(vertices, vertexData, Stride*VertexCount);
    unlockDynVtx();

    SetDynVtxBuffer(Stride);
    DrawPrimitive(PrimitiveType, baseVertex, PrimitiveCount);
}

void GfxDevice::createFrameResources()
{
    for (auto& q: frame_sync)
    {
        assert(q == 0);
        HRESULT hr = device->CreateQuery(D3DQUERYTYPE_EVENT, &q);
        assert(SUCCEEDED(hr));
    }

    for (auto& vb: frame_vb)
    {
        HRESULT hr = device->CreateVertexBuffer(
            DYNAMIC_VB_FRAME_SIZE,
            D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
            0, D3DPOOL_DEFAULT,
            &vb, NULL
        );
        assert(SUCCEEDED(hr));
    }


    for (auto& ib: frame_ib16)
    {
        HRESULT hr = device->CreateIndexBuffer(
            DYNAMIC_IB16_FRAME_SIZE,
            D3DUSAGE_DYNAMIC|D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX16, D3DPOOL_DEFAULT,
            &ib, NULL
        );
        assert(SUCCEEDED(hr));
    }

    frame_id = 0;
}

void GfxDevice::destroyFrameResources()
{
    for (auto q: frame_sync)
    {
        if (q) q->Release();
        q = 0;
    }

    for (auto vb: frame_vb)
    {
        if (vb) vb->Release();
        vb = 0;
    }

    for (auto ib: frame_ib16)
    {
        if (ib) ib->Release();
        ib = 0;
    }
}

HRESULT GfxDevice::TestCooperativeLevel()
{
    return device->TestCooperativeLevel();
}

HRESULT GfxDevice::EvictManagedResources()
{
    return device->EvictManagedResources();
}

HRESULT GfxDevice::GetDeviceCaps(D3DCAPS9* pCaps)
{
    return device->GetDeviceCaps(pCaps);
}

HRESULT GfxDevice::GetDisplayMode(UINT iSwapChain,D3DDISPLAYMODE* pMode)
{
    return device->GetDisplayMode(iSwapChain, pMode);
}

HRESULT GfxDevice::GetBackBuffer(UINT iSwapChain,UINT iBackBuffer,D3DBACKBUFFER_TYPE Type,IDirect3DSurface9** ppBackBuffer)
{
    return device->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
}

void GfxDevice::SetGammaRamp(UINT iSwapChain,DWORD Flags,CONST D3DGAMMARAMP* pRamp)
{
    device->SetGammaRamp(iSwapChain, Flags, pRamp);
}

void  GfxDevice::GetGammaRamp(UINT iSwapChain,D3DGAMMARAMP* pRamp)
{
    device->GetGammaRamp(iSwapChain, pRamp);
}

HRESULT GfxDevice::CreateTexture(UINT Width,UINT Height,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle)
{
    return device->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
}

HRESULT GfxDevice::CreateVolumeTexture(UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle)
{
    return device->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
}

HRESULT GfxDevice::CreateCubeTexture(UINT EdgeLength,UINT Levels,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle)
{
    return device->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
}

HRESULT GfxDevice::CreateVertexBuffer(UINT Length,DWORD Usage,DWORD FVF,D3DPOOL Pool,IDirect3DVertexBuffer9** ppVertexBuffer,HANDLE* pSharedHandle)
{
    return device->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}

HRESULT GfxDevice::CreateIndexBuffer(UINT Length,DWORD Usage,D3DFORMAT Format,D3DPOOL Pool,IDirect3DIndexBuffer9** ppIndexBuffer,HANDLE* pSharedHandle)
{
    return device->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

HRESULT GfxDevice::CreateRenderTarget(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
{
    return device->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
}

HRESULT GfxDevice::CreateDepthStencilSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
{
    return device->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
}

HRESULT GfxDevice::UpdateSurface(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestinationSurface,CONST POINT* pDestPoint)
{
    return device->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}

HRESULT GfxDevice::UpdateTexture(IDirect3DBaseTexture9* pSourceTexture,IDirect3DBaseTexture9* pDestinationTexture)
{
    return device->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT GfxDevice::GetRenderTargetData(IDirect3DSurface9* pRenderTarget,IDirect3DSurface9* pDestSurface)
{
    return device->GetRenderTargetData(pRenderTarget, pDestSurface);
}

HRESULT GfxDevice::GetFrontBufferData(UINT iSwapChain,IDirect3DSurface9* pDestSurface)
{
    return device->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT GfxDevice::StretchRect(IDirect3DSurface9* pSourceSurface,CONST RECT* pSourceRect,IDirect3DSurface9* pDestSurface,CONST RECT* pDestRect,D3DTEXTUREFILTERTYPE Filter)
{
    return device->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}

HRESULT GfxDevice::CreateOffscreenPlainSurface(UINT Width,UINT Height,D3DFORMAT Format,D3DPOOL Pool,IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
{
    return device->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

HRESULT GfxDevice::SetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9* pRenderTarget)
{
    return device->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}

HRESULT GfxDevice::GetRenderTarget(DWORD RenderTargetIndex,IDirect3DSurface9** ppRenderTarget)
{
    return device->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}

HRESULT GfxDevice::SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
{
    return device->SetDepthStencilSurface(pNewZStencil);
}

HRESULT GfxDevice::GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
{
    return device->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT GfxDevice::BeginScene()
{
    return device->BeginScene();
}

HRESULT GfxDevice::EndScene()
{
    return device->EndScene();
}

HRESULT GfxDevice::Clear(DWORD Count,CONST D3DRECT* pRects,DWORD Flags,D3DCOLOR Color,float Z,DWORD Stencil)
{
    return device->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT GfxDevice::SetTransform(D3DTRANSFORMSTATETYPE State,CONST D3DMATRIX* pMatrix)
{
    return device->SetTransform( State, pMatrix);
}

HRESULT GfxDevice::SetViewport(CONST D3DVIEWPORT9* pViewport)
{
    return device->SetViewport(pViewport);
}

HRESULT GfxDevice::SetMaterial(CONST D3DMATERIAL9* pMaterial)
{
    return device->SetMaterial(pMaterial);
}

HRESULT GfxDevice::GetMaterial(D3DMATERIAL9* pMaterial)
{
    return device->GetMaterial(pMaterial);
}

HRESULT GfxDevice::SetLight(DWORD Index,CONST D3DLIGHT9* pLight)
{
    return device->SetLight(Index, pLight);
}

HRESULT GfxDevice::LightEnable(DWORD Index,BOOL Enable)
{
    return device->LightEnable(Index, Enable);
}

HRESULT GfxDevice::SetClipPlane(DWORD Index,CONST float* pPlane)
{
    return device->SetClipPlane(Index, pPlane);
}

HRESULT GfxDevice::SetRenderState(D3DRENDERSTATETYPE State,DWORD Value)
{
    if (!cached || !bitset32_is_bit_set(rs_valid, State) || rs[State] != Value)
    {
        bitset32_set_bit(rs_valid, State);
        rs[State] = Value;
        return device->SetRenderState(State, Value);
    }

    return S_OK;
}

HRESULT GfxDevice::CreateStateBlock(D3DSTATEBLOCKTYPE Type,IDirect3DStateBlock9** ppSB)
{
    return device->CreateStateBlock(Type, ppSB);
}

HRESULT GfxDevice::BeginStateBlock()
{
    return device->BeginStateBlock();
}

HRESULT GfxDevice::EndStateBlock(IDirect3DStateBlock9** ppSB)
{
    return device->EndStateBlock(ppSB);
}

HRESULT GfxDevice::SetTexture(DWORD Stage,IDirect3DBaseTexture9* pTexture)
{
    if (!cached || !bitset32_is_bit_set(tex_valid, Stage) || textures[Stage] != pTexture)
    {
        bitset32_set_bit(tex_valid, Stage);
        textures[Stage] = pTexture;
        return device->SetTexture(Stage, pTexture);
    }

    return S_OK;
}

HRESULT GfxDevice::SetTextureStageState(DWORD Stage,D3DTEXTURESTAGESTATETYPE Type,DWORD Value)
{
    return device->SetTextureStageState(Stage, Type, Value);
}

HRESULT GfxDevice::SetSamplerState(DWORD Sampler,D3DSAMPLERSTATETYPE Type,DWORD Value)
{
    return device->SetSamplerState(Sampler, Type, Value);
}

HRESULT GfxDevice::SetScissorRect(CONST RECT* pRect)
{
    return device->SetScissorRect(pRect);
}

HRESULT GfxDevice::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
    return device->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT GfxDevice::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
{
    return device->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT GfxDevice::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
    return device->CreateVertexDeclaration(pVertexElements, ppDecl);
}

HRESULT GfxDevice::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
    if (!cached || !bitset32_is_bit_set(res_states, RES_STATE_VF) || vf != pDecl)
    {
        vf = pDecl;
        bitset32_set_bit(res_states, RES_STATE_VF);
        return device->SetVertexDeclaration(pDecl);
    }

    return S_OK;
}

HRESULT GfxDevice::CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader)
{
    return device->CreateVertexShader(pFunction, ppShader);
}

HRESULT GfxDevice::SetVertexShader(IDirect3DVertexShader9* pShader)
{
    if (!cached || !bitset32_is_bit_set(res_states, RES_STATE_VS) || vs != pShader)
    {
        vs = pShader;
        bitset32_set_bit(res_states, RES_STATE_VS);
        return device->SetVertexShader(pShader);
    }

    return S_OK;
}

HRESULT GfxDevice::SetVertexShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
    return device->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT GfxDevice::SetVertexShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
    return device->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT GfxDevice::SetVertexShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
    return device->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT GfxDevice::SetStreamSource(UINT StreamNumber,IDirect3DVertexBuffer9* pStreamData,UINT OffsetInBytes,UINT Stride)
{
    assert(StreamNumber<4);
    if (!cached || !bitset32_is_bit_set(res_states, RES_STATE_VB0+StreamNumber) ||
        vb_state[StreamNumber].vb != pStreamData ||
        vb_state[StreamNumber].offset != OffsetInBytes ||
        vb_state[StreamNumber].stride != Stride
    )
    {
        vb_state[StreamNumber].vb     = pStreamData;
        vb_state[StreamNumber].offset = OffsetInBytes;
        vb_state[StreamNumber].stride = Stride;
        bitset32_set_bit(res_states, RES_STATE_VB0+StreamNumber);
        return device->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
    }

    return S_OK;
}

HRESULT GfxDevice::SetStreamSourceFreq(UINT StreamNumber,UINT Setting)
{
    return device->SetStreamSourceFreq(StreamNumber, Setting);
}

HRESULT GfxDevice::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
    if (!cached || !bitset32_is_bit_set(res_states, RES_STATE_IB) || ib != pIndexData)
    {
        ib = pIndexData;
        bitset32_set_bit(res_states, RES_STATE_IB);
        return device->SetIndices(pIndexData);
    }

    return S_OK;
}

HRESULT GfxDevice::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader)
{
    return device->CreatePixelShader(pFunction, ppShader);
}

HRESULT GfxDevice::SetPixelShader(IDirect3DPixelShader9* pShader)
{
    if (!cached || !bitset32_is_bit_set(res_states, RES_STATE_PS) || ps != pShader)
    {
        ps = pShader;
        bitset32_set_bit(res_states, RES_STATE_PS);
        return device->SetPixelShader(pShader);
    }

    return S_OK;
}

HRESULT GfxDevice::SetPixelShaderConstantF(UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
    return device->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT GfxDevice::SetPixelShaderConstantI(UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
    return device->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT GfxDevice::SetPixelShaderConstantB(UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount)
{
    return device->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT GfxDevice::CreateQuery(D3DQUERYTYPE Type,IDirect3DQuery9** ppQuery)
{
    return device->CreateQuery(Type, ppQuery);
}
