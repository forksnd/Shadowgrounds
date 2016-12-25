#include <GfxMemory.h>

namespace gfx
{
    void IndexStorage16::init(gfx::Device& device, uint32_t size, uint16_t max_allocs)
    {
        device.CreateIndexBuffer(size, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indices, NULL);
        allocator = etlsf_create(size, max_allocs);
        locked = 0;
    }

    void IndexStorage16::fini()
    {
        etlsf_destroy(allocator);
        indices->Release();

        locked = 0;
        indices = 0;
        allocator = 0;
    }

    uint16_t IndexStorage16::alloc(uint32_t numIndices)
    {
        return etlsf_alloc(allocator, numIndices * sizeof(uint16_t));
    }

    void IndexStorage16::free(uint16_t id)
    {
        etlsf_free(allocator, id);
    }

    uint16_t* IndexStorage16::lock(uint16_t id)
    {
        assert(locked == 0);
        assert(etlsf_is_block_valid(allocator, id));

        locked = id;

        uint32_t offset = etlsf_block_offset(allocator, id);
        uint32_t size = etlsf_block_size(allocator, id);

        void* ptr = 0;
        indices->Lock(offset, size, &ptr, 0);

        return (uint16_t*)ptr;
    }

    void IndexStorage16::unlock()
    {
        assert(locked != 0);

        indices->Unlock();

        locked = 0;
    }

    uint32_t IndexStorage16::baseIndex(uint16_t id)
    {
        return etlsf_block_offset(allocator, id) / sizeof(uint16_t);
    }



    void VertexStorage::init(gfx::Device& device, uint32_t size, uint16_t max_allocs)
    {
        device.CreateVertexBuffer(size, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vertices, NULL);
        allocator = etlsf_create(size, max_allocs);
        locked = 0;
    }

    void VertexStorage::fini()
    {
        etlsf_destroy(allocator);
        vertices->Release();

        locked = 0;
        vertices = 0;
        allocator = 0;
    }

    uint16_t VertexStorage::alloc(uint32_t numVertices, uint32_t stride)
    {
        return etlsf_alloc(allocator, numVertices * stride + stride);
    }

    void VertexStorage::free(uint16_t id)
    {
        etlsf_free(allocator, id);
    }

    void* VertexStorage::lock(uint16_t id, uint32_t stride)
    {
        assert(locked == 0);
        assert(etlsf_is_block_valid(allocator, id));

        locked = id;

        uint32_t offset = etlsf_block_offset(allocator, id);
        uint32_t size = etlsf_block_size(allocator, id);
        uint32_t rem = offset % stride;

        uint8_t* ptr = 0;
        vertices->Lock(offset, size, (void**)&ptr, 0);

        return ptr + ((rem == 0) ? 0 : stride - rem);
    }

    void VertexStorage::unlock()
    {
        assert(locked != 0);

        vertices->Unlock();

        locked = 0;
    }

    uint32_t VertexStorage::baseVertex(uint16_t id, uint32_t stride)
    {
        return offset(id, stride) / stride;
    }

    uint32_t VertexStorage::offset(uint16_t id, uint32_t stride)
    {
        uint32_t offset = etlsf_block_offset(allocator, id);

        uint32_t rem = offset % stride;
        offset += (rem == 0) ? 0 : stride - rem;

        return offset;
    }
}