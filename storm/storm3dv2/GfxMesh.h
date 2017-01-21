#pragma once

#include "GfxRenderer.h"

namespace gfx
{
    struct Mesh
    {
        etlsf_alloc_t vertex_hw_alloc = ETLSF_INVALID_ID;
        etlsf_alloc_t index_hw_alloc = ETLSF_INVALID_ID;

        uint16_t submesh_count;
        uint16_t subset_count;

        //LOD, multiple meshes with LODs, etc
        struct SubMesh
        {
            FVF fvf;
            uint16_t stride; //calculate from fvf
            uint32_t base_vertex;
            uint32_t base_index;
            uint32_t vertex_count;
            uint32_t index_count;
            uint16_t subset_offset, subset_count;
        };

        //Per material(uniform set) call, maybe lod of single material
        struct Subset
        {
            uint32_t base_index;
            uint32_t index_count;
        };
    };

    inline Mesh::SubMesh* mesh_get_submesh_array(Mesh* mesh)
    {
        return (Mesh::SubMesh*)(&mesh[1]);
    }

    inline Mesh::Subset* mesh_get_subset_array(Mesh* mesh)
    {
        return (Mesh::Subset*)(&mesh_get_submesh_array(mesh)[mesh->submesh_count]);
    }

    template<typename VertexType>
    inline Mesh* mesh_create_simple(
        Renderer& renderer,
        VertexType* vertices, uint32_t vertex_count,
        uint16_t* indices, uint32_t index_count,
        Mesh::Subset* subsets, uint16_t subset_count
    )
    {
        size_t submesh_size = sizeof(Mesh::SubMesh);
        size_t subset_size = subset_count * sizeof(Mesh::Subset);
        size_t data_size = sizeof(Mesh) + submesh_size + subset_size;
        size_t vertex_size = vertex_count * sizeof(VertexType);
        size_t index_size = index_count * sizeof(uint16_t);

        Mesh* mesh = (Mesh*)malloc(data_size);

        mesh->submesh_count = 1;
        mesh->subset_count = subset_count;

        gfx::VertexStorage&  vtxStorage = renderer.getVertexStorage();
        mesh->vertex_hw_alloc = vtxStorage.alloc<VertexType>(vertex_count);
        VertexType* vertex_ptr = vtxStorage.lock<VertexType>(mesh->vertex_hw_alloc);
        memcpy(vertex_ptr, vertices, vertex_size);
        vtxStorage.unlock();
        uint32_t base_vertex = vtxStorage.baseVertex<VertexType>(mesh->vertex_hw_alloc);

        gfx::IndexStorage16& idxStorage = renderer.getIndexStorage16();
        mesh->index_hw_alloc = idxStorage.alloc(index_count);
        uint16_t* index_ptr = idxStorage.lock(mesh->index_hw_alloc);
        memcpy(index_ptr, indices, index_size);
        idxStorage.unlock();
        uint32_t base_index = idxStorage.baseIndex(mesh->index_hw_alloc);

        Mesh::SubMesh& submesh = mesh_get_submesh_array(mesh)[0];
        Mesh::Subset* mesh_subset = mesh_get_subset_array(mesh);

        submesh = {
            VertexType::fvf, sizeof(VertexType),
            base_vertex, base_index,
            vertex_count, index_count,
            0, subset_count
        };

        memcpy(mesh_subset, subsets, subset_size);

        for (size_t i = 0; i < subset_count; ++i)
        {
            mesh_subset[i].base_index += base_index;
        }

        return mesh;
    }

    inline void mesh_destroy(Renderer& renderer, Mesh* mesh)
    {
        if (!mesh) return;

        gfx::VertexStorage&  vtxStorage = renderer.getVertexStorage();
        gfx::IndexStorage16& idxStorage = renderer.getIndexStorage16();

        vtxStorage.free(mesh->vertex_hw_alloc);
        idxStorage.free(mesh->index_hw_alloc);

        free(mesh);
    }
};