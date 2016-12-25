#include "GfxRenderer.h"

enum
{
    DYNAMIC_VB_FRAME_SIZE = 10 * (1 << 20),
    DYNAMIC_IB16_FRAME_SIZE = 1 * (1 << 20),
    INDEX_STORAGE_SIZE = 20 * (1 << 20),
    VERTEX_STORAGE_SIZE = 80 * (1 << 20),
    MAX_QUAD_COUNT = 0x4000,
};

static_assert(MAX_QUAD_COUNT * 4 <= 0x10000, "Vertex count should be less then 65536(0x10000)");


D3DVERTEXELEMENT9 VertexFormatDesc_P3NUV2[] = {
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P3NUV2BW[] = {
    { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
    { 0, 40, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P3D[] = {
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P3DUV2[] = {
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P3UV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P4DUV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 }, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
    { 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,     0 },
    { 0, 20, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P4UV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 }, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
    { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P2DUV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0,  8, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0 },
    { 0, 12, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P2UV[] = {
    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    D3DDECL_END()
};

LPD3DVERTEXELEMENT9 vertexFormatDescs[FVF_COUNT] = {
    VertexFormatDesc_P3NUV2,
    VertexFormatDesc_P3NUV2BW,
    VertexFormatDesc_P3D,
    VertexFormatDesc_P3DUV2,
    VertexFormatDesc_P3UV,
    VertexFormatDesc_P4DUV,
    VertexFormatDesc_P4UV,
    VertexFormatDesc_P2DUV,
    VertexFormatDesc_P2UV
};

namespace gfx
{
    bool Renderer::init(UINT adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params)
    {
        if (!device.init(adapter, hWnd, params))
        {
            return false;
        }

        createPersistantResources();
        createDynamicResources();

        return true;
    }

    void Renderer::fini()
    {
        destroyDynamicResources();
        destroyPersistantResources();

        device.fini();
    }

    bool Renderer::reset(D3DPRESENT_PARAMETERS& params)
    {
        destroyDynamicResources();
        if (device.reset(params))
        {
            createDynamicResources();
            return true;
        }

        return false;
     }

    void Renderer::beginFrame()
    {
        assert(frameNumber >= 0);
        assert(frameNumber < NUM_FRAMES_DELAY);

        while (frameSyncQueries[frameNumber]->GetData(NULL, 0, D3DGETDATA_FLUSH) == S_FALSE);

        frameVBBytesUsed = 0;
        frameIB16BytesUsed = 0;
    }

    void Renderer::endFrame()
    {
        assert(frameNumber >= 0);
        assert(frameNumber < NUM_FRAMES_DELAY);

        frameSyncQueries[frameNumber]->Issue(D3DISSUE_END);

        frameNumber = (frameNumber + 1) % NUM_FRAMES_DELAY;

        device.Present(NULL, NULL, NULL, NULL);
    }

    void Renderer::createPersistantResources()
    {
        for (size_t i = 0; i < FVF_COUNT; ++i)
            device.CreateVertexDeclaration(vertexFormatDescs[i], &vertexFormats[i]);

        indices.init(device, INDEX_STORAGE_SIZE);
        vertices.init(device, VERTEX_STORAGE_SIZE);

        //Create quad indices
        quadIdxAlloc = indices.alloc(MAX_QUAD_COUNT * 6);
        assert(quadIdxAlloc);

        uint16_t* buffer = indices.lock(quadIdxAlloc);
        for (uint16_t i = 0; i < MAX_QUAD_COUNT; ++i)
        {
            uint16_t base = i * 4;

            *buffer++ = base + 0;
            *buffer++ = base + 2;
            *buffer++ = base + 1;
            *buffer++ = base + 1;
            *buffer++ = base + 2;
            *buffer++ = base + 3;
        }
        indices.unlock();
    }

    void Renderer::destroyPersistantResources()
    {
        indices.fini();
        vertices.fini();

        for (auto vf : vertexFormats) vf->Release();
    }

    void Renderer::createDynamicResources()
    {
        for (auto& q : frameSyncQueries)
        {
            assert(q == 0);
            HRESULT hr = device.CreateQuery(D3DQUERYTYPE_EVENT, &q);
            assert(SUCCEEDED(hr));
        }

        for (auto& vb : frameVB)
        {
            HRESULT hr = device.CreateVertexBuffer(
                DYNAMIC_VB_FRAME_SIZE,
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                0, D3DPOOL_DEFAULT,
                &vb, NULL
            );
            assert(SUCCEEDED(hr));
        }


        for (auto& ib : frameIB16)
        {
            HRESULT hr = device.CreateIndexBuffer(
                DYNAMIC_IB16_FRAME_SIZE,
                D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                D3DFMT_INDEX16, D3DPOOL_DEFAULT,
                &ib, NULL
            );
            assert(SUCCEEDED(hr));
        }

        frameNumber = 0;
        frameVBBytesUsed = 0;
        frameIB16BytesUsed = 0;
    }

    void Renderer::destroyDynamicResources()
    {
        for (auto& q : frameSyncQueries)
        {
            if (q) q->Release();
            q = 0;
        }

        for (auto& vb : frameVB)
        {
            if (vb) vb->Release();
            vb = 0;
        }

        for (auto& ib : frameIB16)
        {
            if (ib) ib->Release();
            ib = 0;
        }

        frameNumber = 0;
        frameVBBytesUsed = 0;
        frameIB16BytesUsed = 0;
    }

    void Renderer::SetDynIdx16Buffer()
    {
        device.SetIndices(frameIB16[frameNumber]);
    }

    bool Renderer::lockDynIdx16(UINT count, uint16_t** ptr, UINT* baseIndex)
    {
        UINT sz = count * sizeof(uint16_t);

        assert(frameIB16BytesUsed % sizeof(uint16_t) == 0);

        if (frameIB16BytesUsed + sz > DYNAMIC_IB16_FRAME_SIZE)
        {
            assert(0);
            return false;
        }

        HRESULT hr = frameIB16[frameNumber]->Lock(frameIB16BytesUsed, sz, (void**)ptr, D3DLOCK_NOOVERWRITE);
        assert(SUCCEEDED(hr));

        *baseIndex = frameIB16BytesUsed / sizeof(uint16_t);

        frameIB16BytesUsed += sz;

        return true;
    }

    void Renderer::unlockDynIdx16()
    {
        frameIB16[frameNumber]->Unlock();
    }

    void Renderer::SetDynVtxBuffer(UINT Stride)
    {
        device.SetStreamSource(0, frameVB[frameNumber], 0, Stride);
    }

    bool Renderer::lockDynVtx(UINT count, UINT stride, void** ptr, UINT* baseVertex)
    {
        UINT sz = count*stride;
        UINT rem;

        rem = frameVBBytesUsed % stride;
        rem = rem == 0 ? 0 : stride - rem;

        if (frameVBBytesUsed + rem + sz > DYNAMIC_VB_FRAME_SIZE)
        {
            assert(0);
            return false;
        }

        frameVBBytesUsed += rem;

        HRESULT hr = frameVB[frameNumber]->Lock(frameVBBytesUsed, sz, ptr, D3DLOCK_NOOVERWRITE);
        assert(SUCCEEDED(hr));

        *baseVertex = frameVBBytesUsed / stride;

        frameVBBytesUsed += sz;

        return true;
    }

    void Renderer::unlockDynVtx()
    {
        frameVB[frameNumber]->Unlock();
    }

    void Renderer::SetFVF(FVF vtxFmt)
    {
        device.SetVertexDeclaration(vertexFormats[vtxFmt]);
    }

    void Renderer::setQuadIndices()
    {
        device.SetIndices(indices.indices);
    }

    uint32_t Renderer::baseQuadIndex()
    {
        return indices.baseIndex(quadIdxAlloc);
    }

    void Renderer::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* vertexData, UINT Stride)
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
        device.DrawPrimitive(PrimitiveType, baseVertex, PrimitiveCount);
    }
}