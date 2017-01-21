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

//------------------------------------------------------------------
// Storm3D_Mesh::Storm3D_Mesh
//------------------------------------------------------------------
Storm3D_Mesh::Storm3D_Mesh(Storm3D *s2, Storm3D_ResourceManager &resourceManager_) :
	Storm3D2(s2),
	resourceManager(resourceManager_),
	material(NULL),
	vertex_amount(0),
	hasLods(false),
	vertexes(NULL),
	bone_weights(NULL),
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
		face_amount[i] = 0;
		faces[i] = 0;
	}
	storm3d_mesh_allocs++;
}

const Sphere &Storm3D_Mesh::getBoundingSphere() const
{
	if(!sphere_ok)
	{
		VC3 minSize(1000000.f, 1000000.f, 1000000.f);
		VC3 maxSize(-1000000.f, -1000000.f, -1000000.f);

		for(uint32_t i = 0; i < vertex_amount; ++i)
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

		for(uint32_t i = 0; i < vertex_amount; ++i)
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

	// Remove from Storm3D's list
	Storm3D2->Remove(this, 0);

	// Delete arrays
	if (vertexes) 
		delete[] vertexes;

	delete[] bone_weights;

	// Release buffers
    
	for(int i = 0; i < LOD_AMOUNT; ++i)
	{
		delete[] faces[i];
	}

    gfx::mesh_destroy(Storm3D2->renderer, mesh);

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
		for (uint32_t i = 0; i < vertex_amount; i++)
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
			for (uint32_t j = 0; j < face_amount[i]; j++)
				ret->faces[i][j] = this->faces[i][j];
		}
	}

	if(bone_weights)
	{
		ret->bone_weights = new Storm3D_Weight[vertex_amount];
		for(uint32_t i = 0; i < vertex_amount; ++i)
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
	ReBuild();
}



//------------------------------------------------------------------
// Storm3D_Mesh::PrepareMaterialForRender (v3)
//------------------------------------------------------------------
void Storm3D_Mesh::PrepareMaterialForRender(Storm3D_Scene *scene,Storm3D_Model_Object *object)
{
    gfx::Device& device = Storm3D2->GetD3DDevice();
    // Apply material (if it is not already active)
	if (material!=Storm3D2->active_material)
	{
		// Create world matrix
		float mxx[16];
		object->GetMXG().GetAsD3DCompatible4x4(mxx);

		if (material) material->Apply(scene,0,(D3DMATRIX*)mxx);
		else
		{
			// No material
			// Default: "white plastic"...

			// Set stages (color only)
			device.SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
			device.SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
			device.SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
			device.SetTexture(0,NULL);
			device.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
			device.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
			device.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
			device.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);


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
			device.SetMaterial(&mat);
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
		(fabsf(object->scale.z-1.0f)>=0.001)) device.SetRenderState(D3DRS_NORMALIZENORMALS,TRUE);
		else device.SetRenderState(D3DRS_NORMALIZENORMALS,FALSE);
}



//------------------------------------------------------------------
// Storm3D_Mesh::RenderBuffers (v3)
//------------------------------------------------------------------
void Storm3D_Mesh::RenderBuffers(Storm3D_Model_Object *object)
{
    assert(mesh);

    gfx::Renderer& renderer = Storm3D2->renderer;
    gfx::Device& device = renderer.device;

    uint16_t lod = hasLods ? object->parent_model->lodLevel : 0;

    gfx::Mesh::SubMesh& submesh = mesh_get_submesh_array(mesh)[0];
    gfx::Mesh::Subset& subset = mesh_get_subset_array(mesh)[lod];

    gfx::VertexStorage& vtxStorage = renderer.getVertexStorage();
    gfx::IndexStorage16& idxStorage = renderer.getIndexStorage16();

    renderer.setFVF(submesh.fvf);
    device.SetStreamSource(0, vtxStorage.vertices, 0, submesh.stride);
    device.SetIndices(idxStorage.indices);

    if (bone_weights)
    {
        Storm3D_ShaderManager *manager = Storm3D_ShaderManager::GetSingleton();
        manager->SetShader(device, bone_indices);
    }

    device.DrawIndexedPrimitive(
        D3DPT_TRIANGLELIST,
        submesh.base_vertex, 0, submesh.vertex_count,
        subset.base_index, subset.index_count / 3
    );
}


//------------------------------------------------------------------
// Storm3D_Mesh::Render
//------------------------------------------------------------------
void Storm3D_Mesh::Render(Storm3D_Scene *scene,bool mirrored,Storm3D_Model_Object *object)
{
	// Prepare for rendering (v3)
	PrepareForRender(scene,object);

    if (!mesh) return;

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
}



//------------------------------------------------------------------
// Storm3D_Mesh::RenderWithoutMaterial (v3)
//------------------------------------------------------------------
void Storm3D_Mesh::RenderWithoutMaterial(Storm3D_Scene *scene,bool mirrored,Storm3D_Model_Object *object)
{
	// Prepare for rendering (v3)
	PrepareForRender(scene,object);

    if (!mesh) return;

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
}


//------------------------------------------------------------------
// Storm3D_Mesh::RenderToBackground
//------------------------------------------------------------------
void Storm3D_Mesh::RenderToBackground(Storm3D_Scene *scene,Storm3D_Model_Object *object)
{
    gfx::Device& device = Storm3D2->GetD3DDevice();
    // Prepare for rendering (v3)
	PrepareForRender(scene,object);

	if (!mesh) return;

	// Prepare material for rendering (v3)
	PrepareMaterialForRender(scene,object);

	// Disable some states
	device.SetRenderState(D3DRS_SPECULARENABLE,FALSE);
	device.SetRenderState(D3DRS_ZENABLE,FALSE);
	device.SetRenderState(D3DRS_NORMALIZENORMALS,FALSE);

	// Render vertex buffers
	RenderBuffers(object);

	// Return states
	device.SetRenderState(D3DRS_ZENABLE,TRUE);
}

//------------------------------------------------------------------
// Storm3D_Mesh::ReBuild
//------------------------------------------------------------------
void Storm3D_Mesh::ReBuild()
{
    if (mesh) return;

    // Test array sizes
    if ((face_amount[0] < 1)/*&&(facestrip_length<3)*/) return;
    if (vertex_amount < 1) return;

    uint16_t lodLevels = hasLods ? LOD_AMOUNT : 1;

    std::vector<uint16_t> hw_indices;
    gfx::Mesh::Subset subsets[LOD_AMOUNT];

    uint32_t base_index_offset = 0;
    for (uint32_t i = 0; i < lodLevels; ++i)
    {
        uint16_t index_count = face_amount[i] * 3;

        subsets[i].base_index = base_index_offset;
        subsets[i].index_count = index_count;

        base_index_offset += index_count;
    }

    for (uint32_t i = 0; i < lodLevels; ++i)
    {
        // Copy data to indexbuffer
        for (uint32_t j = 0; j < face_amount[i]; j++)
        {
            hw_indices.push_back(faces[i][j].vertex_index[0]);
            hw_indices.push_back(faces[i][j].vertex_index[1]);
            hw_indices.push_back(faces[i][j].vertex_index[2]);
        }
    }

    // Build bone chunks
    if (bone_weights)
    {
        // Find all used weights
        bone_indices.clear();
        for (uint32_t i = 0; i < vertex_amount; ++i)
        {
            int bone_index = bone_weights[i].index1;
            if (bone_index >= 0)
            {
                if (std::find(bone_indices.begin(), bone_indices.end(), bone_index) == bone_indices.end())
                    bone_indices.push_back(bone_index);
            }

            bone_index = bone_weights[i].index2;
            if (bone_index >= 0)
            {
                if (std::find(bone_indices.begin(), bone_indices.end(), bone_index) == bone_indices.end())
                    bone_indices.push_back(bone_index);
            }
        }

        assert(bone_indices.size() <= Storm3D_ShaderManager::BONE_INDICES);
        std::sort(bone_indices.begin(), bone_indices.end());

        // Create vertex buffer
        std::vector<Vertex_P3NUV2BW> hw_vertices;
        for (uint32_t i = 0; i < vertex_amount; ++i)
        {
            int index1 = bone_weights[i].index1;
            if (index1 != -1)
            {
                auto it = std::find(bone_indices.begin(), bone_indices.end(), index1);
                if (it != bone_indices.end())
                    index1 = std::distance(bone_indices.begin(), it);
            }

            int index2 = bone_weights[i].index2;
            if (index2 != -1)
            {
                auto it = std::find(bone_indices.begin(), bone_indices.end(), index2);
                if (it != bone_indices.end())
                    index2 = std::distance(bone_indices.begin(), it);
            }

            float bone_i0 = (float)index1;
            float bone_w0 = (index1 >= 0) ? bone_weights[i].weight1 / 100.f : 0.0f;

            float bone_i1 = (float)index2;
            float bone_w1 = (index2 >= 0) ? bone_weights[i].weight2 / 100.f : 0.0f;

            hw_vertices.push_back({
                vertexes[i].position,
                vertexes[i].normal,
                vertexes[i].texturecoordinates,
                vertexes[i].texturecoordinates2,
                VC4(bone_i0, bone_w0, bone_i1, bone_w1)
            });
        }

        mesh = mesh_create_simple(
            Storm3D2->renderer,
            &hw_vertices[0], hw_vertices.size(),
            &hw_indices[0], hw_indices.size(),
            subsets, lodLevels
        );
    }
    else
    {
        std::vector<Vertex_P3NUV2> hw_vertices;
        for (uint32_t i = 0; i < vertex_amount; i++)
        {
            hw_vertices.push_back({
                vertexes[i].position,
                vertexes[i].normal,
                vertexes[i].texturecoordinates,
                vertexes[i].texturecoordinates2
            });
        }

        mesh = mesh_create_simple(
            Storm3D2->renderer,
            &hw_vertices[0], hw_vertices.size(),
            &hw_indices[0], hw_indices.size(),
            subsets, lodLevels
        );
    }

    // Crear the markers
    update_vx = false;
    update_vx_amount = false;
    update_fc = false;
    update_fc_amount = false;
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
	for (uint32_t vx=0;vx<vertex_amount;vx++)
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
