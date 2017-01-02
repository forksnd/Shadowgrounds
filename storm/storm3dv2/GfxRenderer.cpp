#include "GfxRenderer.h"

enum
{
    DYNAMIC_VB_FRAME_SIZE = 10 * (1 << 20),
    DYNAMIC_IB16_FRAME_SIZE = 1 * (1 << 20),
    INDEX_STORAGE_SIZE = 30 * (1 << 20),
    VERTEX_STORAGE_SIZE = 100 * (1 << 20),
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

D3DVERTEXELEMENT9 VertexFormatDesc_PT4DUV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 }, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
    { 0, 16, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,     0 },
    { 0, 20, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_PT4UV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 }, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
    { 0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0 },
    D3DDECL_END()
};

D3DVERTEXELEMENT9 VertexFormatDesc_P4UV[] = {
    { 0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 }, //TODO: use D3DDECLUSAGE_POSITION when port to shaders
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
    VertexFormatDesc_PT4DUV,
    VertexFormatDesc_PT4UV,
    VertexFormatDesc_P4UV,
    VertexFormatDesc_P2DUV,
    VertexFormatDesc_P2UV
};

namespace gfx
{
    bool Renderer::init(uint32_t adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params)
    {
        if (!device.init(adapter, hWnd, params))
        {
            return false;
        }

        createPersistantResources();
        createDynamicResources();

        programManager.init(device);

        return true;
    }

    void Renderer::fini()
    {
        programManager.fini();

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
        quadBaseIndex = indices.baseIndex(quadIdxAlloc);

        // Create fullscreen quad geometry
        fullscreenQuadVtxAlloc = vertices.alloc<Vertex_P4UV>(4);
        assert(fullscreenQuadVtxAlloc);
        Vertex_P4UV *v = vertices.lock<Vertex_P4UV>(fullscreenQuadVtxAlloc);
        *v++ = { { -1.0f,  1.0f, 1.f, 1.f },{ 0.f, 1.f } };
        *v++ = { { -1.0f, -1.0f, 1.f, 1.f },{ 0.f, 0.f } };
        *v++ = { { 1.0f,  1.0f, 1.f, 1.f },{ 1.f, 1.f } };
        *v++ = { { 1.0f, -1.0f, 1.f, 1.f },{ 1.f, 0.f } };
        vertices.unlock();
        fullscreenQuadBaseVertex = vertices.baseVertex<Vertex_P4UV>(fullscreenQuadVtxAlloc);
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

    void Renderer::setDynIdx16Buffer()
    {
        device.SetIndices(frameIB16[frameNumber]);
    }

    bool Renderer::lockDynIdx16(uint32_t count, uint16_t** ptr, uint32_t* baseIndex)
    {
        uint32_t sz = count * sizeof(uint16_t);

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

    void Renderer::setDynVtxBuffer(uint32_t Stride)
    {
        device.SetStreamSource(0, frameVB[frameNumber], 0, Stride);
    }

    bool Renderer::lockDynVtx(uint32_t count, uint32_t stride, void** ptr, uint32_t* baseVertex)
    {
        uint32_t sz = count*stride;
        uint32_t rem;

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

    void Renderer::setFVF(FVF vtxFmt)
    {
        device.SetVertexDeclaration(vertexFormats[vtxFmt]);
    }

    void Renderer::drawPrimitiveUP(D3DPRIMITIVETYPE primitiveType, uint32_t primitiveCount, const void* vertexData, uint32_t stride)
    {
        void* vertices = NULL;
        uint32_t  baseVertex = 0;
        uint32_t  vertexCount = 0;

        switch (primitiveType)
        {
        case D3DPT_TRIANGLESTRIP:
            vertexCount = primitiveCount + 2;
            break;
        case D3DPT_TRIANGLELIST:
            vertexCount = 3 * primitiveCount;
            break;
        default:
            assert(0);
        }

        lockDynVtx(vertexCount, stride, &vertices, &baseVertex);
        memcpy(vertices, vertexData, stride*vertexCount);
        unlockDynVtx();

        setDynVtxBuffer(stride);
        device.DrawPrimitive(primitiveType, baseVertex, primitiveCount);
    }

    void Renderer::drawQuads(uint32_t baseVertexIndex, uint32_t quadCount)
    {
        if (quadCount == 0)
            return;

        //TODO: make batches as indices are not always enough
        quadCount = std::min<uint32_t>(MAX_QUAD_COUNT, quadCount);
        device.SetIndices(indices.indices);
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVertexIndex, 0, quadCount * 4, quadBaseIndex, quadCount * 2);
    }

    void Renderer::drawFullScreenQuad()
    {
        device.SetVertexDeclaration(vertexFormats[FVF_P4UV]);
        device.SetStreamSource(0, vertices.vertices, 0, sizeof(Vertex_P4UV));
        device.SetIndices(indices.indices);
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, fullscreenQuadBaseVertex, 0, 4, quadBaseIndex, 2);
    }
}