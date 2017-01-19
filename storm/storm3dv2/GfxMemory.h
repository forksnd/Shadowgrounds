#pragma once

#include <etlsf.h>
#include "GfxDevice.h"

namespace gfx
{
    struct IndexStorage16
    {
        LPDIRECT3DINDEXBUFFER9 indices = nullptr;
        etlsf_t                allocator = nullptr;
        etlsf_alloc_t          locked = ETLSF_INVALID_ID;

        void init(gfx::Device& device, uint32_t size, uint16_t max_allocs = 0xFFFF);
        void fini();

        etlsf_alloc_t alloc(uint32_t numIndices);
        void free(etlsf_alloc_t id);

        uint16_t* lock(etlsf_alloc_t id);
        void      unlock();

        uint32_t baseIndex(etlsf_alloc_t id);
    };

    struct VertexStorage
    {
        LPDIRECT3DVERTEXBUFFER9 vertices = nullptr;
        etlsf_t                 allocator = nullptr;
        etlsf_alloc_t           locked = ETLSF_INVALID_ID;

        void init(gfx::Device& device, uint32_t size, uint16_t max_allocs = 0xFFFF);
        void fini();

        etlsf_alloc_t alloc(uint32_t numVertices, uint32_t stride);
        void free(etlsf_alloc_t id);

        void* lock(etlsf_alloc_t id, uint32_t stride);
        void  unlock();

        uint32_t baseVertex(etlsf_alloc_t id, uint32_t stride);
        uint32_t offset(etlsf_alloc_t id, uint32_t stride);

        template<typename T>
        etlsf_alloc_t alloc(uint32_t numVertices)
        {
            return alloc(numVertices, sizeof(T));
        }

        template<typename T>
        T* lock(etlsf_alloc_t id)
        {
            return (T*)lock(id, sizeof(T));
        }

        template<typename T>
        uint32_t baseVertex(etlsf_alloc_t id)
        {
            return baseVertex(id, sizeof(T));
        }

        template<typename T>
        uint32_t offset(etlsf_alloc_t id)
        {
            return offset(id, sizeof(T));
        }
    };
}