#include "GfxDevice.h"
#include <assert.h>

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

#define DYNAMIC_BUFFER_FRAME_SIZE  (10 * (1<<20))

bool GfxDevice::cached = true;

void GfxDevice::resetCache()
{
    bitset32_clear(rs_valid);
    bitset32_clear(tex_valid);
}

bool GfxDevice::init(LPDIRECT3D9 d3d, UINT Adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params)
{
    for (auto& q: frame_sync) q = 0;

   	if (FAILED(d3d->CreateDevice(Adapter, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &params, &device)))
        return false;

    createFrameResources();

    return true;
}

void GfxDevice::fini()
{
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
}

void GfxDevice::endFrame()
{
    assert(frame_id >= 0);
    assert(frame_id < NUM_FRAMES_DELAY);

    frame_sync[frame_id]->Issue(D3DISSUE_END);

    frame_id = (frame_id + 1) % NUM_FRAMES_DELAY;

    device->Present(NULL, NULL, NULL, NULL);
}

bool GfxDevice::lockDynVtx(size_t count, size_t stride, void** ptr, UINT* baseVertex)
{
    UINT    sz = count*stride;

    if (frame_vb_used + sz <= DYNAMIC_BUFFER_FRAME_SIZE)
        return false;

    HRESULT hr = frame_vb[frame_id]->Lock(frame_vb_used, sz, ptr, D3DLOCK_NOOVERWRITE);
    assert(hr);

    frame_vb_used += sz;

    return true;
}

void GfxDevice::unlockDynVtx()
{
    frame_vb[frame_id]->Unlock();
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
            DYNAMIC_BUFFER_FRAME_SIZE,
            D3DUSAGE_DYNAMIC, 0, D3DPOOL_DEFAULT,
            &vb, NULL
        );
        assert(SUCCEEDED(hr));
    }

    frame_id = 0;
}

void GfxDevice::destroyFrameResources()
{
    for (auto& q: frame_sync)
    {
        if (q) q->Release();
        q = 0;
    }

    for (auto& vb: frame_vb)
    {
        if (vb) vb->Release();
        vb = 0;
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

HRESULT GfxDevice::SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus)
{
    return device->SetClipStatus(pClipStatus);
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

HRESULT GfxDevice::SetSoftwareVertexProcessing(BOOL bSoftware)
{
    return device->SetSoftwareVertexProcessing(bSoftware);
}

HRESULT GfxDevice::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
    return device->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT GfxDevice::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
{
    return device->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT GfxDevice::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
    return device->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT GfxDevice::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
    return device->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT GfxDevice::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl)
{
    return device->CreateVertexDeclaration(pVertexElements, ppDecl);
}

HRESULT GfxDevice::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
{
    return device->SetVertexDeclaration(pDecl);
}

HRESULT GfxDevice::SetFVF(DWORD FVF)
{
    return device->SetFVF(FVF);
}

HRESULT GfxDevice::CreateVertexShader(CONST DWORD* pFunction,IDirect3DVertexShader9** ppShader)
{
    return device->CreateVertexShader(pFunction, ppShader);
}

HRESULT GfxDevice::SetVertexShader(IDirect3DVertexShader9* pShader)
{
    return device->SetVertexShader(pShader);
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
    return device->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT GfxDevice::SetStreamSourceFreq(UINT StreamNumber,UINT Setting)
{
    return device->SetStreamSourceFreq(StreamNumber, Setting);
}

HRESULT GfxDevice::SetIndices(IDirect3DIndexBuffer9* pIndexData)
{
    return device->SetIndices(pIndexData);
}

HRESULT GfxDevice::CreatePixelShader(CONST DWORD* pFunction,IDirect3DPixelShader9** ppShader)
{
    return device->CreatePixelShader(pFunction, ppShader);
}

HRESULT GfxDevice::SetPixelShader(IDirect3DPixelShader9* pShader)
{
    return device->SetPixelShader(pShader);
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
