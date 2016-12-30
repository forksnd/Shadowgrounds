#pragma once

#include "GfxDevice.h"
#include "GfxMemory.h"
#include "GfxProgramManager.h"

namespace gfx
{
    struct Renderer
    {
        Device device;
        ProgramManager programManager;

        bool init(uint32_t adapter, HWND hWnd, D3DPRESENT_PARAMETERS& params);
        void fini();

        bool reset(D3DPRESENT_PARAMETERS& params);

        void beginFrame();
        void endFrame();

        void setDynIdx16Buffer();
        bool lockDynIdx16(uint32_t count, uint16_t** ptr, uint32_t* baseIndex);
        void unlockDynIdx16();


        void setDynVtxBuffer(uint32_t Stride);
        bool lockDynVtx(uint32_t count, uint32_t stride, void** ptr, uint32_t* baseVertex);
        void unlockDynVtx();

        template<typename T>
        void setDynVtxBuffer()
        {
            setDynVtxBuffer(sizeof(T));
        }

        template<typename T>
        bool lockDynVtx(uint32_t count, T** ptr, uint32_t* baseVertex)
        {
            return lockDynVtx(count, sizeof(T), (void**)ptr, baseVertex);
        }

        IndexStorage16& getIndexStorage16() { return indices; }
        VertexStorage&  getVertexStorage() { return vertices; }

        void setFVF(FVF vtxFmt);

        void drawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, uint32_t PrimitiveCount, const void* vertexData, uint32_t Stride);
        void drawQuads(uint32_t baseVertexIndex, uint32_t QuadCount);

    private:
        void createPersistantResources();
        void destroyPersistantResources();

        void createDynamicResources();
        void destroyDynamicResources();

    private:
        LPDIRECT3DVERTEXDECLARATION9 vertexFormats[FVF_COUNT] = { 0 };

        // frame sync
        LPDIRECT3DQUERY9 frameSyncQueries[NUM_FRAMES_DELAY] = { 0 };
        uint32_t         frameNumber = 0;

        LPDIRECT3DVERTEXBUFFER9 frameVB[NUM_FRAMES_DELAY] = { 0 };
        uint32_t                frameVBBytesUsed = 0;

        LPDIRECT3DINDEXBUFFER9 frameIB16[NUM_FRAMES_DELAY] = { 0 };
        uint32_t               frameIB16BytesUsed = 0;

        IndexStorage16  indices;
        VertexStorage   vertices;

        uint16_t quadIdxAlloc = 0;
        uint32_t quadBaseIndex = 0;
    };
}