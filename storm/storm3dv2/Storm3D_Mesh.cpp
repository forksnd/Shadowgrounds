// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "storm3d.h"
#include "storm3d_mesh.h"
#include "storm3d_model.h"
#include "storm3d_model_object.h"
#include "storm3d_material.h"
#include "storm3d_texture.h"
#include "storm3d_scene.h"
#include "VertexFormats.h"

#include "Storm3D_Bone.h"
#include "Storm3D_ShaderManager.h"
#include <algorithm>
#include "../../util/Debug_MemoryManager.h"

int storm3d_mesh_allocs = 0;
int storm3d_dip_calls = 0;

//------------------------------------------------------------------
// Storm3D_Mesh::Storm3D_Mesh
//------------------------------------------------------------------
Storm3D_Mesh::Storm3D_Mesh(Storm3D *s2, Storm3D_ResourceManager &resourceManager_) :
	Storm3D2(s2),
	resourceManager(resourceManager_),
	material(NULL),
	vertex_amount(0),
	render_vertex_amount(0),
	hasLods(false),
	vertexes(NULL),
	bone_weights(NULL),
	vertices_id(0),
	radius(0),
	sq_radius(0),
	radius2d(0),
	rb_update_needed(true),
	update_vx(true),
	update_vx_amount(true),
	update_fc(true),
	update_fc_amount(true),
	col_rebuild_needed(true),
	sphere_ok(false),
	box_ok(false)
{
	for(int i = 0; i < LOD_AMOUNT; ++i)
	{
		render_face_amount[i] = 0;
		face_amount[i] = 0;
		faces[i] = 0;
		indices_id[i] = 0;
	}
	storm3d_mesh_allocs++;
}

const Sphere &Storm3D_Mesh::getBoundingSphere() const
{
	if(!sphere_ok)
	{
		VC3 minSize(1000000.f, 1000000.f, 1000000.f);
		VC3 maxSize(-1000000.f, -1000000.f, -1000000.f);

		for(int i = 0; i < vertex_amount; ++i)
		{
			const VC3 &v = vertexes[i].position;
			if(v.x < minSize.x)
				minSize.x = v.x;
			if(v.y < minSize.y)
				minSize.y = v.y;
			if(v.z < minSize.z)
				minSize.z = v.z;

			if(v.x > maxSize.x)
				maxSize.x = v.x;
			if(v.y > maxSize.y)
				maxSize.y = v.y;
			if(v.z > maxSize.z)
				maxSize.z = v.z;
		}

		VC3 center = (minSize + maxSize) / 2.f;
		bounding_sphere.position = center;
		bounding_sphere.radius = center.GetRangeTo(maxSize);

		sphere_ok = true;
	}

	return bounding_sphere;
}


const AABB &Storm3D_Mesh::getBoundingBox() const
{
	if(!box_ok)
	{
		VC3 minSize(1000000.f, 1000000.f, 1000000.f);
		VC3 maxSize(-1000000.f, -1000000.f, -1000000.f);

		for(int i = 0; i < vertex_amount; ++i)
		{
			const VC3 &v = vertexes[i].position;
			if(v.x < minSize.x)
				minSize.x = v.x;
			if(v.y < minSize.y)
				minSize.y = v.y;
			if(v.z < minSize.z)
				minSize.z = v.z;

			if(v.x > maxSize.x)
				maxSize.x = v.x;
			if(v.y > maxSize.y)
				maxSize.y = v.y;
			if(v.z > maxSize.z)
				maxSize.z = v.z;
		}

		bounding_box.mmin = minSize;
		bounding_box.mmax = maxSize;
		box_ok = true;
	}

	return bounding_box;
}

//------------------------------------------------------------------
// Storm3D_Mesh::~Storm3D_Mesh
//------------------------------------------------------------------
Storm3D_Mesh::~Storm3D_Mesh()
{
	storm3d_mesh_allocs--;

    clearBoneChunks();

	// Remove from Storm3D's list
	Storm3D2->Remove(this, 0);

	// Delete arrays
	if (vertexes) 
		delete[] vertexes;

	delete[] bone_weights;

	// Release buffers
    
    VertexStorage&  vtxStorage = Storm3D2->getVertexStorage();
    IndexStorage16& idxStorage = Storm3D2->getIndexStorage16();

    vtxStorage.free(vertices_id);

	for(int i = 0; i < LOD_AMOUNT; ++i)
	{
		delete[] faces[i];
		
        idxStorage.free(indices_id[i]);
	}

	if(material)
		resourceManager.removeUser(material, this);
}



//------------------------------------------------------------------
// Storm3D_Mesh::CreateNewClone - makes a clone of the mesh
// -jpk
//------------------------------------------------------------------
IStorm3D_Mesh *Storm3D_Mesh::CreateNewClone()
{
	Storm3D_Mesh *ret = (Storm3D_Mesh *)Storm3D2->CreateNewMesh();

	if (material)
	{
		//ret->material = (Storm3D_Material *)material->CreateNewClone();
		ret->UseMaterial((Storm3D_Material *)material->CreateNewClone());
		//ret->UseMaterial(material);
	}

	ret->vertex_amount = this->vertex_amount;

	if (vertexes)
	{
		ret->vertexes = new Storm3D_Vertex[vertex_amount];
		for (int i = 0; i < vertex_amount; i++)
		{
			ret->vertexes[i] = this->vertexes[i];
		}
	}

	for(int i = 0; i < LOD_AMOUNT; ++i)
	{
		ret->face_amount[i] = this->face_amount[i];

		if(faces[i])
		{
			ret->faces[i] = new Storm3D_Face[face_amount[i]];
			for (int j = 0; j < face_amount[i]; j++)
				ret->faces[i][j] = this->faces[i][j];
		}
	}

	if(bone_weights)
	{
		ret->bone_weights = new Storm3D_Weight[vertex_amount];
		for(int i = 0; i < vertex_amount; ++i)
			ret->bone_weights[i] = bone_weights[i];
	}

	return ret;
}


//------------------------------------------------------------------
// Storm3D_Mesh::PrepareForRender (v3)
//------------------------------------------------------------------
// You can set scene=NULL, object=NULL if you dont need animation
void Storm3D_Mesh::PrepareForRender(Storm3D_Scene *scene,Storm3D_Model_Object *object)
{
	// Check if material is changed in a way that forces object
	// buffers to be rebuilt.
	if(bone_weights)
	{
		if (vbuf_fvf!=FVF_P3NUV2BW)
		{
			update_vx=true;
			update_vx_amount=true;
		}
	}	
	else if (vbuf_fvf!=FVF_P3NUV2)
	{
		update_vx=true;
		update_vx_amount=true;
	}

	/*
	else if (material)
	{
		if (material->GetBumpType()==Storm3D_Material::BUMPTYPE_DOT3)	// DOT3 only
		{
			if (vbuf_fvf!=FVF_P3NUV2)
			{
				update_vx=true;
				update_vx_amount=true;
			}
		} 
		else
		if ((material->GetMultiTextureType()==Storm3D_Material::MTYPE_TEX_EMBM_REF)
			||(material->GetMultiTextureType()==Storm3D_Material::MTYPE_DUALTEX_EMBM_REF))	// TEX_EMBM_REF only
		{
			if (vbuf_fvf!=FVF_P3NUV2)
			{
				update_vx=true;
				update_vx_amount=true;
			}
		} 
		else	// Other than DOT3 or TEX_EMBM_REF
		{
			if (material->GetTextureCoordinateSetCount()==0)
			{
				if (vbuf_fvf!=FVF_P3NUV2)
				{
					update_vx=true;
					update_vx_amount=true;
				}
			}
			else
			{
				if (vbuf_fvf!=FVF_P3NUV2)
				{
					update_vx=true;
					update_vx_amount=true;
				}
			}
		}
	}
	else	// No material
	{
		// If material was removed, it usually means rebuild. (but not always)
		if (vbuf_fvf!=FVF_P3NUV2)
		{
			update_vx=true;
			update_vx_amount=true;
		}
	}
	*/

	// If rebuild is needed: do it!
	ReBuild();
}



//------------------------------------------------------------------
// Storm3D_Mesh::PrepareMaterialForRender (v3)
//------------------------------------------------------------------
void Storm3D_Mesh::PrepareMaterialForRender(Storm3D_Scene *scene,Storm3D_Model_Object *object)
{
	// Apply material (if it is not already active)
	if (material!=Storm3D2->active_material)
	{
		// Create world matrix
		float mxx[16];
		object->GetMXG().GetAsD3DCompatible4x4(mxx);

		if (material) material->Apply(scene,0,vbuf_fvf,(D3DMATRIX*)mxx);
		else
		{
			// No material
			// Default: "white plastic"...

			// Set stages (color only)
			Storm3D2->device.SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
			Storm3D2->device.SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
			Storm3D2->device.SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
			Storm3D2->device.SetTexture(0,NULL);
			Storm3D2->device.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
			Storm3D2->device.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
			Storm3D2->device.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
			Storm3D2->device.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);


			// Setup material
			D3DMATERIAL9 mat;

			// Set diffuse
			mat.Diffuse.r=mat.Ambient.r=1; 
			mat.Diffuse.g=mat.Ambient.g=1; 
			mat.Diffuse.b=mat.Ambient.b=1; 
			mat.Diffuse.a=mat.Ambient.a=0; 

			// Set self.illum
			mat.Emissive.r=0; 
			mat.Emissive.g=0; 
			mat.Emissive.b=0; 
			mat.Emissive.a=0; 

			// Set specular
			mat.Specular.r=1; 
			mat.Specular.g=1; 
			mat.Specular.b=1; 
			mat.Specular.a=0; 
			mat.Power=25; 

			// Use this material
			Storm3D2->device.SetMaterial(&mat);
/* PSD
			// Apply shader
			Storm3D2->device.SetVertexShader(vbuf_fvf);
*/
			// Set active material
			Storm3D2->active_material=NULL;
		}
	}

	// If object is scaled use normalizenormals, otherwise don't
	if ((fabsf(object->scale.x-1.0f)>=0.001)||
		(fabsf(object->scale.y-1.0f)>=0.001)||
		(fabsf(object->scale.z-1.0f)>=0.001)) Storm3D2->device.SetRenderState(D3DRS_NORMALIZENORMALS,TRUE);
		else Storm3D2->device.SetRenderState(D3DRS_NORMALIZENORMALS,FALSE);
}



//------------------------------------------------------------------
// Storm3D_Mesh::RenderBuffers (v3)
//------------------------------------------------------------------
void Storm3D_Mesh::RenderBuffers(Storm3D_Model_Object *object)
{
    int lod = object->parent_model->lodLevel;
    if (!hasLods)
        lod = 0;

    VertexStorage& vtxStorage = Storm3D2->getVertexStorage();
    IndexStorage16& idxStorage = Storm3D2->getIndexStorage16();

    Storm3D2->device.SetFVF(vbuf_fvf);
    Storm3D2->device.SetStreamSource(0, vtxStorage.vertices, 0, vbuf_vsize);
    Storm3D2->device.SetIndices(idxStorage.indices);

    if (bone_weights)
    {
        for (unsigned int i = 0; i < bone_chunks[lod].size(); ++i)
        {
            Storm3D_ShaderManager *manager = Storm3D_ShaderManager::GetSingleton();
            manager->SetShader(Storm3D2->device, bone_chunks[lod][i].bone_indices);


            Storm3D2->device.DrawIndexedPrimitive(
                D3DPT_TRIANGLELIST,
                bone_chunks[lod][i].base_vertex, 0, bone_chunks[lod][i].vertex_count,
                bone_chunks[lod][i].base_index, bone_chunks[lod][i].index_count
            );

            ++storm3d_dip_calls;
        }
    }
    else
    {
        Storm3D2->device.DrawIndexedPrimitive(
            D3DPT_TRIANGLELIST,
            base_vertex, 0, render_vertex_amount,
            base_index[lod], render_face_amount[lod]
        );

        ++storm3d_dip_calls;
    }
}


//------------------------------------------------------------------
// Storm3D_Mesh::Render
//------------------------------------------------------------------
void Storm3D_Mesh::Render(Storm3D_Scene *scene,bool mirrored,Storm3D_Model_Object *object)
{
	// Prepare for rendering (v3)
	PrepareForRender(scene,object);

	// Test
	if (vertices_id==NULL) return;
	if (indices_id[0]==NULL) return;

	// Prepare material for rendering (v3)
	PrepareMaterialForRender(scene,object);

	// Reverse culling if mirrored
	if (mirrored)
	{
		if (material)
		{
			bool ds,wf;
			material->GetSpecial(ds,wf);

			if (!ds)
			{
//				Storm3D2->device.SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
			}
		}
	}

	//if(GetKeyState('R') & 0x80)
	//	Storm3D2->device.SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);

	int lod = object->parent_model->lodLevel;
	if(!hasLods)
		lod = 0;

	// Render vertex buffers
	RenderBuffers(object);
	scene->AddPolyCounter(face_amount[lod]);
}



//------------------------------------------------------------------
// Storm3D_Mesh::RenderWithoutMaterial (v3)
//------------------------------------------------------------------
void Storm3D_Mesh::RenderWithoutMaterial(Storm3D_Scene *scene,bool mirrored,Storm3D_Model_Object *object)
{
	// Prepare for rendering (v3)
	PrepareForRender(scene,object);

	// Test
	if (vertices_id==NULL) return;
	if (indices_id[0]==NULL) return;

	// Reverse culling if mirrored
	if (mirrored)
	{
		if (material)
		{
			bool ds,wf;
			material->GetSpecial(ds,wf);

			if (!ds)
			{
//				Storm3D2->device.SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
			}
		}
	}

	// Render vertex buffers
	RenderBuffers(object);
	scene->AddPolyCounter(face_amount[0]);
}



//------------------------------------------------------------------
// Storm3D_Mesh::RenderToBackground
//------------------------------------------------------------------
void Storm3D_Mesh::RenderToBackground(Storm3D_Scene *scene,Storm3D_Model_Object *object)
{
	// Prepare for rendering (v3)
	PrepareForRender(scene,object);

	// Test
	if (vertices_id==NULL) return;
	if (indices_id[0]==NULL) return;

	// Prepare material for rendering (v3)
	PrepareMaterialForRender(scene,object);

	// Disable some states
	Storm3D2->device.SetRenderState(D3DRS_SPECULARENABLE,FALSE);
	Storm3D2->device.SetRenderState(D3DRS_ZENABLE,FALSE);
	Storm3D2->device.SetRenderState(D3DRS_NORMALIZENORMALS,FALSE);

	// Render vertex buffers
	RenderBuffers(object);
	scene->AddPolyCounter(face_amount[0]);

	// Return states
	Storm3D2->device.SetRenderState(D3DRS_ZENABLE,TRUE);
}

void Storm3D_Mesh::clearBoneChunks()
{
    int lodLevels = hasLods ? LOD_AMOUNT : 1;
    IndexStorage16& idxStorage = Storm3D2->getIndexStorage16();
    VertexStorage&  vtxStorage = Storm3D2->getVertexStorage();

    for(int lod = 0; lod < lodLevels; ++lod)
    {
        for (unsigned int i = 0; i < bone_chunks[lod].size(); ++i)
        {
            auto chunk = bone_chunks[lod][i];
            idxStorage.free(chunk.idx_id);
            vtxStorage.free(chunk.vtx_id);
        }

        bone_chunks[lod].clear();
    }
}

//------------------------------------------------------------------
// Storm3D_Mesh::ReBuild
//------------------------------------------------------------------
void Storm3D_Mesh::ReBuild()
{
    // Test if rebuilding is needed
    if ((!update_vx) && (!update_vx_amount) &&
        (!update_fc) && (!update_fc_amount)) return;

    // Test array sizes
    if ((face_amount[0] < 1)/*&&(facestrip_length<3)*/) return;
    if (vertex_amount < 1) return;

    // Select format for vertexbuffer
    if (bone_weights)
    {
        vbuf_vsize = sizeof(Vertex_P3NUV2BW);
        vbuf_fvf = FVF_P3NUV2BW;
    }
    else
    {
        vbuf_vsize = sizeof(Vertex_P3NUV2);
        vbuf_fvf = FVF_P3NUV2;
    }

    IndexStorage16& idxStorage = Storm3D2->getIndexStorage16();
    VertexStorage&  vtxStorage = Storm3D2->getVertexStorage();

    int lodLevels = hasLods ? LOD_AMOUNT : 1;

    // Build bone chunks
    if ((bone_weights) && ((update_vx) || (update_fc)))
    {
        clearBoneChunks();

        for (int lod = 0; lod < lodLevels; ++lod)
        {
            // Find used bones indices
            std::vector<int> bone_indices;
            int max_bone_index = 0;

            std::vector<std::vector<int> > chunk_faces;

            for (int v = 0; v < vertex_amount; ++v)
            {
                int index = bone_weights[v].index1;
                if (index == -1)
                    continue;

                if (index > max_bone_index)
                    max_bone_index = index;
                if (std::find(bone_indices.begin(), bone_indices.end(), index) == bone_indices.end())
                    bone_indices.push_back(index);

                index = bone_weights[v].index2;
                if (index == -1)
                    continue;

                if (index > max_bone_index)
                    max_bone_index = index;
                if (find(bone_indices.begin(), bone_indices.end(), index) == bone_indices.end())
                    bone_indices.push_back(index);
            }

            std::sort(bone_indices.begin(), bone_indices.end());

            for (int i = 0; i < face_amount[lod]; ++i)
            {
                // Find all used weights
                const Storm3D_Face &f = faces[lod][i];
                int weights[6] = { -1, -1, -1, -1, -1, -1 };
                int weight_index = 0;

                for (int k = 0; k < 3; ++k)
                {
                    int vindex = f.vertex_index[k];

                    int bone_index = bone_weights[vindex].index1;
                    if (bone_index >= 0)
                    {
                        if (std::find(&weights[0], &weights[6], bone_index) == &weights[6])
                            weights[weight_index++] = bone_index;
                    }

                    bone_index = bone_weights[vindex].index2;
                    if (bone_index >= 0)
                    {
                        if (std::find(&weights[0], &weights[6], bone_index) == &weights[6])
                            weights[weight_index++] = bone_index;
                    }
                }

                // If some chunk (index) already contains needed bones or contains enough spaces for them (could_insert_chunk)
                int index = -1;
                int could_insert_chunk = -1;

                for (unsigned int j = 0; j < bone_chunks[lod].size(); ++j)
                {
                    Storm3D_BoneChunk &chunk = bone_chunks[lod][j];

                    int bones_found = 0;
                    for (int k = 0; k < weight_index; ++k)
                    {
                        for (unsigned int l = 0; l < chunk.bone_indices.size(); ++l)
                        {
                            if (weights[k] == chunk.bone_indices[l])
                                ++bones_found;
                        }
                    }

                    if (int(chunk.bone_indices.size() + weight_index - bones_found) < Storm3D_ShaderManager::BONE_INDICES)
                        could_insert_chunk = j;

                    if (bones_found == weight_index)
                        index = j;
                }

                // Create new chunk if needed
                if (index == -1)
                {
                    index = could_insert_chunk;
                    if (index == -1)
                    {
                        index = bone_chunks[lod].size();
                        bone_chunks[lod].resize(index + 1);
                    }

                    Storm3D_BoneChunk &insert_chunk = bone_chunks[lod][index];
                    for (int k = 0; k < weight_index; ++k)
                    {
                        if (find(insert_chunk.bone_indices.begin(), insert_chunk.bone_indices.end(), weights[k]) != insert_chunk.bone_indices.end())
                            continue;

                        int new_index = weights[k];
                        insert_chunk.bone_indices.push_back(new_index);
                    }
                }

                if (int(chunk_faces.size()) <= index)
                    chunk_faces.resize(index + 1);

                chunk_faces[index].push_back(i);
            }

            // Ok, we now have chunks with bone lists and faces attached on them
            int bone_chunk_amount = bone_chunks[lod].size();
            for (int i = 0; i < bone_chunk_amount; ++i)
            {
                const std::vector<int> &chunk_face_list = chunk_faces[i];
                Storm3D_BoneChunk &chunk = bone_chunks[lod][i];

                std::vector<int> vertex_list;
                std::vector<int> vertex_convert_list(vertex_amount, -1);

                // Create index buffer
                {
                    idxStorage.free(chunk.idx_id);

                    chunk.idx_id = idxStorage.alloc(chunk_face_list.size() * 3);

                    uint16_t *ip = idxStorage.lock(chunk.idx_id);
                    if (ip)
                    {
                        for (unsigned int j = 0; j < chunk_face_list.size(); ++j)
                        {
                            const Storm3D_Face &f = faces[lod][chunk_face_list[j]];
                            for (int k = 0; k < 3; ++k)
                            {
                                int vertex_index = f.vertex_index[k];
                                if (vertex_convert_list[vertex_index] == -1)
                                {
                                    vertex_convert_list[vertex_index] = vertex_list.size();
                                    vertex_list.push_back(vertex_index);
                                }

                                *ip++ = vertex_convert_list[vertex_index];
                            }
                        }

                        idxStorage.unlock();
                        chunk.index_count = chunk_face_list.size();
                        chunk.base_index = idxStorage.baseIndex(chunk.idx_id);
                    }
                }

                // Create vertex buffer
                {
                    assert(vbuf_vsize==sizeof(Vertex_P3NUV2BW));

                    vtxStorage.free(chunk.vtx_id);

                    chunk.vtx_id = vtxStorage.alloc<Vertex_P3NUV2BW>(vertex_list.size());

                    Vertex_P3NUV2BW *v = vtxStorage.lock<Vertex_P3NUV2BW>(chunk.vtx_id);
                    float weight[4] = { 0 };

                    for (unsigned int i = 0; i < vertex_list.size(); ++i)
                    {
                        int vertex_index = vertex_list[i];
                        int index1 = bone_weights[vertex_index].index1;
                        int index2 = bone_weights[vertex_index].index2;

                        if (index1 != -1)
                        {
                            for (unsigned int j = 0; j < chunk.bone_indices.size(); ++j)
                            {
                                if (index1 == chunk.bone_indices[j])
                                {
                                    index1 = j;
                                    break;
                                }
                            }

                            assert(index1 >= 0 && index1 <= Storm3D_ShaderManager::BONE_INDICES);
                        }
                        if (index2 != -1)
                        {
                            for (unsigned int j = 0; j < chunk.bone_indices.size(); ++j)
                            {
                                if (index2 == chunk.bone_indices[j])
                                {
                                    index2 = j;
                                    break;
                                }
                            }

                            assert(index1 >= 0 && index1 <= Storm3D_ShaderManager::BONE_INDICES);
                        }

                        weight[0] = float((index1)* 3);
                        weight[0] += Storm3D_ShaderManager::BONE_INDEX_START;
                        //weight[1] = 1;
                        weight[1] = bone_weights[vertex_index].weight1 / 100.f;

                        weight[2] = float(Storm3D_ShaderManager::BONE_INDEX_START);
                        weight[3] = 0;

//weight[1] = fabsf(weight[1]);

                        if (index2 >= 0)
                        {
                            //weight[1] = bone_weights[vertex_index].weight1 / 100.f;
                            weight[2] = float((index2) * 3);
                            weight[2] += Storm3D_ShaderManager::BONE_INDEX_START;
                            weight[3] = bone_weights[vertex_index].weight2 / 100.f;
                        }

                        v[i] = {
                            vertexes[vertex_index].position,
                            vertexes[vertex_index].normal,
                            vertexes[vertex_index].texturecoordinates,
                            vertexes[vertex_index].texturecoordinates2,
                            VC4(weight[0], weight[1], weight[2], weight[3])
                        };
                    }

                    vtxStorage.unlock();
                    chunk.vertex_count = vertex_list.size();
                    chunk.base_vertex = vtxStorage.baseVertex<Vertex_P3NUV2BW>(chunk.vtx_id);
                }
            }
        }
    }

    for (int i = 0; i < lodLevels; ++i)
    {
        if (!faces[i])
            continue;

        if (update_fc_amount)
        {
            // Create new indexbuffer (and delete old)
            idxStorage.free(indices_id[i]);

            indices_id[i] = idxStorage.alloc(face_amount[i] * 3);
            base_index[i] = idxStorage.baseIndex(indices_id[i]);

            //update_fc_amount = false;
        }

        if (update_fc)
        {
            // Copy data to indexbuffer
            uint16_t *ip = idxStorage.lock(indices_id[i]);
            if (ip)
            {
                for (int j = 0; j < face_amount[i]; j++)
                {
                    *ip++ = faces[i][j].vertex_index[0];
                    *ip++ = faces[i][j].vertex_index[1];
                    *ip++ = faces[i][j].vertex_index[2];
                }
            }
            idxStorage.unlock();

            //update_fc = false;
            //update_fc_amount = false;
        }
    }

    if ((update_vx) && (!bone_weights))
    {
        assert(vbuf_vsize==sizeof(Vertex_P3NUV2));

        vtxStorage.free(vertices_id);

        vertices_id = vtxStorage.alloc<Vertex_P3NUV2>(vertex_amount);

        Vertex_P3NUV2 *v = vtxStorage.lock<Vertex_P3NUV2>(vertices_id);
        for(int i=0;i<vertex_amount;i++)
        {
            v[i] = {
                vertexes[i].position,
                vertexes[i].normal,
                vertexes[i].texturecoordinates,
                vertexes[i].texturecoordinates2
            };
        }
        vtxStorage.unlock();

        base_vertex = vtxStorage.baseVertex<Vertex_P3NUV2>(vertices_id);
    }

    // Crear the markers
    update_vx = false;
    update_vx_amount = false;
    update_fc = false;
    update_fc_amount = false;
    /*
    delete[] vertexes;
    delete[] faces[0];
    face_amount[0] = 0;
    vertexes = 0;
    */
    render_face_amount[0] = face_amount[0];
    render_vertex_amount = vertex_amount;
}



//------------------------------------------------------------------
// Storm3D_Mesh::UseMaterial
//------------------------------------------------------------------
void Storm3D_Mesh::UseMaterial(IStorm3D_Material *_material)
{
	if(material)
		resourceManager.removeUser(material, this);

	// Set the material
	material=(Storm3D_Material*)_material;

	// Some times material change means that object buffers
	// must be rebuilt. However now the testing is done when
	// object is rendered, because material may change itself.

	if(material)
		resourceManager.addUser(material, this);
}



//------------------------------------------------------------------
// Storm3D_Mesh::GetMaterial
//------------------------------------------------------------------
IStorm3D_Material *Storm3D_Mesh::GetMaterial()
{
	return material;
}



//------------------------------------------------------------------
// Storm3D_Mesh::DeleteAllFaces
//------------------------------------------------------------------
void Storm3D_Mesh::DeleteAllFaces()
{
	/*
	if (faces) delete[] faces;
	faces=NULL;
	face_amount=0;
	*/
	for(int i = 0; i < LOD_AMOUNT; ++i)
	{
		delete[] faces[i];
		faces[i] = 0;
		face_amount[i] = 0;
	}

	// Set rebuild
	update_fc=true;
	update_fc_amount=true;
	sphere_ok = false;
	box_ok = false;
}



//------------------------------------------------------------------
// Storm3D_Mesh::DeleteAllVertexes
//------------------------------------------------------------------
void Storm3D_Mesh::DeleteAllVertexes()
{
	if (vertexes) delete[] vertexes;
	vertexes=NULL;
	vertex_amount=0;

	// Set rebuild
	update_vx=true;
	update_vx_amount=true;
	rb_update_needed=true;

	hasLods = false;
	sphere_ok = false;
	box_ok = false;
}



//------------------------------------------------------------------
// Dynamic geometry edit (New in v2.2B)
//------------------------------------------------------------------
Storm3D_Face *Storm3D_Mesh::GetFaceBuffer()
{
	// Set rebuild
	update_fc=true;
	col_rebuild_needed=true;
	sphere_ok = false;
	box_ok = false;

	return faces[0];
}


Storm3D_Vertex *Storm3D_Mesh::GetVertexBuffer()
{
	// Set rebuild
	update_vx=true;
	col_rebuild_needed=true;
	rb_update_needed=true;
	sphere_ok = false;
	box_ok = false;

	return vertexes;
}


const Storm3D_Face *Storm3D_Mesh::GetFaceBufferReadOnly()
{
	return faces[0];
}

const Storm3D_Face *Storm3D_Mesh::GetFaceBufferReadOnly(int lodIndex)
{
	assert(lodIndex >= 0 && lodIndex < LOD_AMOUNT);

	if(face_amount[lodIndex] == 0)
		return faces[0];
	else
		return faces[lodIndex];
}

const Storm3D_Vertex *Storm3D_Mesh::GetVertexBufferReadOnly()
{
	return vertexes;
}

int Storm3D_Mesh::GetFaceCount()
{
	return face_amount[0];
}

int Storm3D_Mesh::GetFaceCount(int lodIndex)
{
	assert(lodIndex >= 0 && lodIndex < LOD_AMOUNT);

	if(face_amount[lodIndex] == 0)
		return face_amount[0];
	else
		return face_amount[lodIndex];
}

int Storm3D_Mesh::GetVertexCount()
{
	return vertex_amount;
}


void Storm3D_Mesh::ChangeFaceCount(int new_face_count)
{
	// Delete old
	DeleteAllFaces();

	// Set faceamount
	face_amount[0]=new_face_count;
	if (face_amount[0] < 1) 
		return;

	// Allocate memory for new ones
	faces[0]=new Storm3D_Face[face_amount[0]];

	// Set rebuild
	update_fc=true;
	col_rebuild_needed=true;
	sphere_ok = false;
	box_ok = false;
}


void Storm3D_Mesh::ChangeVertexCount(int new_vertex_count)
{
	// Delete old
	DeleteAllVertexes();

	// Set faceamount
	vertex_amount=new_vertex_count;
	if (vertex_amount<1) return;

	// Allocate memory for new ones
	vertexes=new Storm3D_Vertex[vertex_amount];

	// Set rebuild
	update_vx=true;
	col_rebuild_needed=true;
	rb_update_needed=true;
	sphere_ok = false;
	box_ok = false;
}


void Storm3D_Mesh::UpdateCollisionTable()
{
	if (col_rebuild_needed) 
		collision.ReBuild(this);
	col_rebuild_needed=false;
}


//------------------------------------------------------------------
// Storm3D_Mesh::CalculateRadiusAndBox
//-----------------------------------------------------------------
void Storm3D_Mesh::CalculateRadiusAndBox()
{
	// Test
	if (vertexes==NULL)
	{
		radius=0;
		radius2d = 0;
		sq_radius=0;
		//box.pmax=VC3(0,0,0);
		//box.pmin=VC3(0,0,0);
		rb_update_needed=false;
		return;
	}

	// Set values to init state
	sq_radius=0;
	radius2d = 0;
	//box.pmax=vertexes[0].position;
	//box.pmin=vertexes[0].position;

	// Loop through all vertexes
	for (int vx=0;vx<vertex_amount;vx++)
	{
		// Update sq_radius
		const VC3 &pos = vertexes[vx].position;
		float r = pos.GetSquareLength();
		if (r > sq_radius) 
			sq_radius = r;

		float r2 = (pos.x * pos.x) + (pos.z * pos.z);
		if(r2 > radius2d)
			radius2d = r2;


		// Update box
		/*
		if (vertexes[vx].position.x>box.pmax.x) box.pmax.x=vertexes[vx].position.x;
			else if (vertexes[vx].position.x<box.pmin.x) box.pmin.x=vertexes[vx].position.x;

		if (vertexes[vx].position.y>box.pmax.y) box.pmax.y=vertexes[vx].position.y;
			else if (vertexes[vx].position.y<box.pmin.y) box.pmin.y=vertexes[vx].position.y;
		
		if (vertexes[vx].position.z>box.pmax.z) box.pmax.z=vertexes[vx].position.z;
			else if (vertexes[vx].position.z<box.pmin.z) box.pmin.z=vertexes[vx].position.z;
		*/
	}

	// Calculate radius
	radius = sqrtf(sq_radius);
	radius2d = sqrtf(radius2d);

	// Clear marker
	rb_update_needed=false;
}



//------------------------------------------------------------------
// Storm3D_Mesh::GetRadius
//-----------------------------------------------------------------
float Storm3D_Mesh::GetRadius()
{
	if (rb_update_needed) CalculateRadiusAndBox();
	return radius;
}



//------------------------------------------------------------------
// Storm3D_Mesh::GetSquareRadius
//-----------------------------------------------------------------
float Storm3D_Mesh::GetSquareRadius()
{
	if (rb_update_needed) CalculateRadiusAndBox();
	return sq_radius;
}


//------------------------------------------------------------------
// Storm3D_Mesh::RayTrace
//------------------------------------------------------------------
bool Storm3D_Mesh::RayTrace(const VC3 &position,const VC3 &direction_normalized,float ray_length,Storm3D_CollisionInfo &rti, bool accurate)
{
	// No collision table! (do not test collision)
	if (col_rebuild_needed) return false;

	// Raytrace
	return collision.RayTrace(position,direction_normalized,ray_length,rti, accurate);
}



//------------------------------------------------------------------
// Storm3D_Mesh::SphereCollision
//------------------------------------------------------------------
bool Storm3D_Mesh::SphereCollision(const VC3 &position,float radius,Storm3D_CollisionInfo &cinf, bool accurate)
{
	// No collision table! (do not test collision)
	if (col_rebuild_needed) return false;

	// Test collision
	return collision.SphereCollision(position,radius,cinf, accurate);
}

bool Storm3D_Mesh::HasWeights() const
{
	return bone_weights != 0;
}
