#pragma once

#include "GfxDevice.h"
#include "GfxMemory.h"

namespace gfx
{
    struct Renderer
    {
        Device device;

        bool init(UINT adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params);
        void fini();

        bool reset(D3DPRESENT_PARAMETERS& params);

        void beginFrame();
        void endFrame();

        void SetDynIdx16Buffer();
        bool lockDynIdx16(UINT count, uint16_t** ptr, UINT* baseIndex);
        void unlockDynIdx16();


        void SetDynVtxBuffer(UINT Stride);
        bool lockDynVtx(UINT count, UINT stride, void** ptr, UINT* baseVertex);
        void unlockDynVtx();

        template<typename T>
        void SetDynVtxBuffer()
        {
            SetDynVtxBuffer(sizeof(T));
        }

        template<typename T>
        bool lockDynVtx(UINT count, T** ptr, UINT* baseVertex)
        {
            return lockDynVtx(count, sizeof(T), (void**)ptr, baseVertex);
        }

        IndexStorage16& getIndexStorage16() { return indices; }
        VertexStorage&  getVertexStorage() { return vertices; }

        void setQuadIndices();
        uint32_t baseQuadIndex();

        void SetFVF(FVF vtxFmt);
        void DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* vertexData, UINT Stride);

    private:
        void createPersistantResources();
        void destroyPersistantResources();

        void createDynamicResources();
        void destroyDynamicResources();

    private:
        LPDIRECT3DVERTEXDECLARATION9 vertexFormats[FVF_COUNT] = { 0 };

        // frame sync
        LPDIRECT3DQUERY9 frameSyncQueries[NUM_FRAMES_DELAY] = { 0 };
        size_t           frameNumber = 0;

        LPDIRECT3DVERTEXBUFFER9 frameVB[NUM_FRAMES_DELAY] = { 0 };
        UINT                    frameVBBytesUsed = 0;

        LPDIRECT3DINDEXBUFFER9 frameIB16[NUM_FRAMES_DELAY] = { 0 };
        UINT                   frameIB16BytesUsed = 0;

        IndexStorage16  indices;
        VertexStorage   vertices;

        uint16_t quadIdxAlloc = 0;
    };
}