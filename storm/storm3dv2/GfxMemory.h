#pragma once

#include <etlsf.h>
#include "GfxDevice.h"

namespace gfx
{
    struct IndexStorage16
    {
        LPDIRECT3DINDEXBUFFER9 indices = nullptr;
        etlsf_t                allocator = nullptr;
        uint16_t               locked = 0;

        void init(gfx::Device& device, uint32_t size, uint16_t max_allocs = 0xFFFF);
        void fini();

        uint16_t alloc(uint32_t numIndices);
        void free(uint16_t id);

        uint16_t* lock(uint16_t id);
        void      unlock();

        uint32_t baseIndex(uint16_t id);
    };

    struct VertexStorage
    {
        LPDIRECT3DVERTEXBUFFER9 vertices = nullptr;
        etlsf_t                 allocator = nullptr;
        uint16_t                locked = 0;

        void init(gfx::Device& device, uint32_t size, uint16_t max_allocs = 0xFFFF);
        void fini();

        uint16_t alloc(uint32_t numVertices, uint32_t stride);
        void free(uint16_t id);

        void* lock(uint16_t id, uint32_t stride);
        void  unlock();

        uint32_t baseVertex(uint16_t id, uint32_t stride);
        uint32_t offset(uint16_t id, uint32_t stride);

        template<typename T>
        uint16_t alloc(uint32_t numVertices)
        {
            return alloc(numVertices, sizeof(T));
        }

        template<typename T>
        T* lock(uint16_t id)
        {
            return (T*)lock(id, sizeof(T));
        }

        template<typename T>
        uint32_t baseVertex(uint16_t id)
        {
            return baseVertex(id, sizeof(T));
        }

        template<typename T>
        uint32_t offset(uint16_t id)
        {
            return offset(id, sizeof(T));
        }
    };
}