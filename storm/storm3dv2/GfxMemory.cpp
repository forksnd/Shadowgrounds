#include <GfxMemory.h>

namespace gfx
{
    void IndexStorage16::init(gfx::Device& device, uint32_t size, uint16_t max_allocs)
    {
        device.CreateIndexBuffer(size, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indices, NULL);
        allocator = etlsf_create(size, max_allocs);
        locked = ETLSF_INVALID_ID;
    }

    void IndexStorage16::fini()
    {
        etlsf_destroy(allocator);
        indices->Release();

        locked = ETLSF_INVALID_ID;
        indices = 0;
        allocator = 0;
    }

    etlsf_alloc_t IndexStorage16::alloc(uint32_t numIndices)
    {
        return etlsf_alloc_range(allocator, numIndices * sizeof(uint16_t));
    }

    void IndexStorage16::free(etlsf_alloc_t id)
    {
        etlsf_free_range(allocator, id);
    }

    uint16_t* IndexStorage16::lock(etlsf_alloc_t id)
    {
        assert(!etlsf_alloc_is_valid(allocator, locked));
        assert(etlsf_alloc_is_valid(allocator, id));

        locked = id;

        uint32_t offset = etlsf_alloc_offset(allocator, id);
        uint32_t size = etlsf_alloc_size(allocator, id);

        void* ptr = 0;
        indices->Lock(offset, size, &ptr, 0);

        return (uint16_t*)ptr;
    }

    void IndexStorage16::unlock()
    {
        assert(etlsf_alloc_is_valid(allocator, locked));

        indices->Unlock();

        locked = ETLSF_INVALID_ID;
    }

    uint32_t IndexStorage16::baseIndex(etlsf_alloc_t id)
    {
        return etlsf_alloc_offset(allocator, id) / sizeof(uint16_t);
    }



    void VertexStorage::init(gfx::Device& device, uint32_t size, uint16_t max_allocs)
    {
        device.CreateVertexBuffer(size, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vertices, NULL);
        allocator = etlsf_create(size, max_allocs);
        locked = ETLSF_INVALID_ID;
    }

    void VertexStorage::fini()
    {
        etlsf_destroy(allocator);
        vertices->Release();

        locked = ETLSF_INVALID_ID;
        vertices = 0;
        allocator = 0;
    }

    etlsf_alloc_t VertexStorage::alloc(uint32_t numVertices, uint32_t stride)
    {
        return etlsf_alloc_range(allocator, numVertices * stride + stride);
    }

    void VertexStorage::free(etlsf_alloc_t id)
    {
        etlsf_free_range(allocator, id);
    }

    void* VertexStorage::lock(etlsf_alloc_t id, uint32_t stride)
    {
        assert(!etlsf_alloc_is_valid(allocator, locked));
        assert(etlsf_alloc_is_valid(allocator, id));

        locked = id;

        uint32_t offset = etlsf_alloc_offset(allocator, id);
        uint32_t size = etlsf_alloc_size(allocator, id);
        uint32_t rem = offset % stride;

        uint8_t* ptr = 0;
        vertices->Lock(offset, size, (void**)&ptr, 0);

        return ptr + ((rem == 0) ? 0 : stride - rem);
    }

    void VertexStorage::unlock()
    {
        assert(etlsf_alloc_is_valid(allocator, locked));

        vertices->Unlock();

        locked = ETLSF_INVALID_ID;
    }

    uint32_t VertexStorage::baseVertex(etlsf_alloc_t id, uint32_t stride)
    {
        return offset(id, stride) / stride;
    }

    uint32_t VertexStorage::offset(etlsf_alloc_t id, uint32_t stride)
    {
        uint32_t offset = etlsf_alloc_offset(allocator, id);

        uint32_t rem = offset % stride;
        offset += (rem == 0) ? 0 : stride - rem;

        return offset;
    }
}