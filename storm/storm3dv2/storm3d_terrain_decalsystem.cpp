// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <vector>
#include <memory>
#include <deque>

#include "storm3d_terrain_decalsystem.h"
#include "storm3d_terrain_utils.h"
#include "storm3d_texture.h"
#include "storm3d_material.h"
#include "Storm3D_ShaderManager.h"
#include "storm3d_spotlight.h"
#include <c2_sphere.h>
#include <c2_frustum.h>
#include <c2_qtree.h>
#include "storm3d.h"
#include "storm3d_scene.h"

#include "../../util/Debug_MemoryManager.h"

using namespace frozenbyte::storm;

struct Decal;
struct Material;
typedef std::deque<Decal> DecalList;
typedef std::vector<Material> MaterialList;

// position + normal + texcoord + color 
static const int VERTEX_SIZE = 3*4 + 3*4 + 4*4 + 1*4;
static const int MAX_DECAL_AMOUNT = 10000;

enum RenderMode
{
	Textures,
	Light
};

static D3DVERTEXELEMENT9 VertexDesc_DECAL[] = {
    {0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
    {0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
    {0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR,    0},
    {0, 28, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
    {0, 32, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
    D3DDECL_END()
};

struct Vertex_DECAL
{
	VC3      position;
	VC3      normal;
	uint32_t color;
	VC2      texcoords;
	VC2      texcoords2;
};

namespace {

	bool contains2D(const AABB &area, const VC3 &position)
	{
		if(position.x < area.mmin.x)
			return false;
		if(position.x > area.mmax.x)
			return false;

		if(position.z < area.mmin.z)
			return false;
		if(position.z > area.mmax.z)
			return false;

		return true;
	}

}
	typedef Quadtree<Decal> Tree;

	struct Decal
	{
		VC3 position;
		VC2 size;
		QUAT rotation;
		COL light;
		VC2 special;

		unsigned char alpha;
		int materialIndex;

		VC3 normal;
		VC3 vertices[4];
		mutable DWORD vertexColor;

		bool deleted;
		Tree::Entity *entity;

		int id;

		Decal()
		:	alpha(0),
			materialIndex(-1),
			vertexColor(0),
			deleted(false),
			entity(0),
			id(0)
		{
		}

		float getRadius() const
		{
			return vertices[0].GetRangeTo(vertices[2]) * .5f;
		}

		void calculateValues()
		{
			MAT tm;
			tm.CreateRotationMatrix(rotation);

			// A bit optimized version, use base vectors directly

			normal.x = tm.Get(8);
			normal.y = tm.Get(9);
			normal.z = tm.Get(10);
			VC3 biasNormal = normal;
			biasNormal *= .07f;

			VC3 right(tm.Get(0), tm.Get(1), tm.Get(2));
			VC3 up(tm.Get(4), tm.Get(5), tm.Get(6));

			right *= .5f * size.x;
			up *= .5f * size.y;

			assert(fabsf(up.GetDotWith(right)) < 0.0001f);

			vertices[0] = position;
			vertices[0] -= right;
			vertices[0] -= up;
			vertices[0] += biasNormal;

			vertices[1] = position;
			vertices[1] -= right;
			vertices[1] += up;
			vertices[1] += biasNormal;

			vertices[2] = position;
			vertices[2] += right;
			vertices[2] -= up;
			vertices[2] += biasNormal;

			vertices[3] = position;
			vertices[3] += right;
			vertices[3] += up;
			vertices[3] += biasNormal;

			COL finalColor = light;
			finalColor.Clamp();
			vertexColor = finalColor.GetAsD3DCompatibleARGB() & 0x00FFFFFF;
			vertexColor |= alpha << 24;

			if(entity)
				entity->setRadius(getRadius());
		}

		void insert(Vertex_DECAL *buffer) const
		{
			buffer->position = vertices[0];
			buffer->normal = normal;
			buffer->color = vertexColor;
			buffer->texcoords = VC2(0.f, 0.f);
			buffer->texcoords2 = special;

			++buffer;
			buffer->position = vertices[1];
			buffer->normal = normal;
			buffer->color = vertexColor;
			buffer->texcoords = VC2(0.f, 1.f);
			buffer->texcoords2 = special;

			++buffer;
			buffer->position = vertices[2];
			buffer->normal = normal;
			buffer->color = vertexColor;
			buffer->texcoords = VC2(1.f, 0.f);
			buffer->texcoords2 = special;

			++buffer;
			buffer->position = vertices[3];
			buffer->normal = normal;
			buffer->color = vertexColor;
			buffer->texcoords = VC2(1.f, 1.f);
			buffer->texcoords2 = special;
		}

		bool fits(const AABB &area) const
		{
			if(!contains2D(area, vertices[0]))
				return false;
			if(!contains2D(area, vertices[1]))
				return false;
			if(!contains2D(area, vertices[2]))
				return false;
			if(!contains2D(area, vertices[3]))
				return false;
			
			return true;
		}

		bool SphereCollision(const VC3 &pos, float radius, Storm3D_CollisionInfo &info, bool accurate)
		{
			if(pos.GetRangeTo(position) < radius + getRadius())
			{
				info.hit = true;
				return true;
			}

			return false;
		}
	};

	struct Material
	{
		COL diffuseColor;
		std::shared_ptr<Storm3D_Texture> baseTexture;

		DecalList decals;
		int materialIndex;

		Tree *tree;

		Material(Tree *tree_)
		:	materialIndex(-1),
			tree(tree_)
		{
		}

		int addDecal(IStorm3D_TerrainDecalSystem::Type type, const VC3 &position, int &id, bool forceSpawn)
		{
			int index = decals.size();
			for(unsigned int i = 0; i < decals.size(); ++i)
			{
				if(decals[i].deleted)
				{
					index = i;
					break;
				}
			}

			if(index >= int(decals.size()))
				decals.resize(index + 1);

			bool canAdd = true;
			if(!forceSpawn)
			{
				static const float CHECK_RADIUS = 0.15f;
				static const int MAX_OVERLAP = 3;

				std::vector<Decal *> list;
				tree->collectSphere(list, position, CHECK_RADIUS);

				int overlaps = list.size();
				//for(unsigned int i = 0; i < list.size(); ++i)
				//{
				//}

				if(overlaps >= MAX_OVERLAP)
					canAdd = false;
			}

			Decal &decal = decals[index];
			id = ++decal.id;
			decal.position = position;
			decal.materialIndex = materialIndex;

			if(canAdd)
			{
				decal.deleted = false;
				if(type == IStorm3D_TerrainDecalSystem::Outside)
					decal.special.x = 8.1f;
				else
					decal.special.x = 9.1f;

				decal.entity = tree->insert(&decal, position, 0.1f);
			}

			return index;
		}

		void eraseDecal(int index, int id)
		{
			assert(id);

			Decal &decal = decals[index];
			if(decal.id == id && decal.entity)
			{
				tree->erase(decal.entity);
				decal.entity = 0;
				decal.deleted = true;
			}
		}

		void updateDecal(int index)
		{
			Decal &decal = decals[index];
			decal.calculateValues();
		}

		void apply(GfxDevice &device)
		{
			if(baseTexture)
				baseTexture->Apply(1);

			D3DXVECTOR4 diffuse(diffuseColor.r, diffuseColor.g, diffuseColor.b, 1.f);
			device.SetVertexShaderConstantF(7, diffuse, 1);
		}

		void applyProjection(GfxDevice &device, const COL &spotColor)
		{
			if(baseTexture)
				baseTexture->Apply(2);

			//float factor = .55f;
			float factor = 1.f;
			D3DXVECTOR4 diffuse(diffuseColor.r * factor * spotColor.r, diffuseColor.g * factor * spotColor.g, diffuseColor.b * factor * spotColor.b, 1.f);
			device.SetVertexShaderConstantF(17, diffuse, 1);
		}

		void applyShadow(GfxDevice &device)
		{
			if(baseTexture)
				baseTexture->Apply(0);
		}
	};

	Material createMaterial(Storm3D_Material *material, Tree *tree)
	{
		Material result(tree);
		result.diffuseColor = material->GetColor();

		Storm3D_Texture *base = static_cast<Storm3D_Texture *> (material->GetBaseTexture());
		if(base)
			result.baseTexture = createSharedTexture(base);

		return result;
	}

namespace {

	bool close(const COL &a, const COL &b)
	{
		if(fabsf(a.r - b.r) > 0.01f)
			return false;
		if(fabsf(a.g - b.g) > 0.01f)
			return false;
		if(fabsf(a.b - b.b) > 0.01f)
			return false;

		return true;
	}

	bool equals(const Material &a, const Material &b)
	{
		if(a.baseTexture != b.baseTexture)
			return false;
		if(!close(a.diffuseColor, b.diffuseColor))
			return false;

		return true;
	}
}

	struct DecalSorter
	{
		bool operator() (const Decal *a, const Decal *b) const
		{
			if(a->materialIndex == b->materialIndex)
				return a < b;
			else
				return a->materialIndex < b->materialIndex;
		}
	};

	typedef std::vector<Decal *> DecalPtrList;

struct Storm3D_TerrainDecalSystem::Data
{
	Storm3D &storm;
	MaterialList materials;
	VC2I blockAmount;

	PixelShader pixelShader;
	VertexShader vertexShader;
	VertexShader pointVertexShader;
	VertexShader dirVertexShader;
	VertexShader flatVertexShader;

    UINT baseDecalVertex;
    LPDIRECT3DVERTEXDECLARATION9 VtxFmt_DECAL;

	std::unique_ptr<Tree> tree;
	DecalPtrList decals;
	DecalList shadowDecals;

	COL outFactor;
	COL inFactor;

	std::shared_ptr<Material> shadowMaterial;

	float fogEnd;
	float fogRange;

	Data(Storm3D &storm_)
	:	storm(storm_),
		pixelShader(storm.GetD3DDevice()),
		vertexShader(storm.GetD3DDevice()),
		pointVertexShader(storm.GetD3DDevice()),
		dirVertexShader(storm.GetD3DDevice()),
		flatVertexShader(storm.GetD3DDevice()),
		tree(),
		outFactor(1.f, 1.f, 1.f),
		inFactor(1.f, 1.f, 1.f),
		fogEnd(-1000000000000.f),
		fogRange(10.f)
	{
		pixelShader.createDecalPixelShader();
		vertexShader.createDecalShader();

		pointVertexShader.createDecalPointShader();
		dirVertexShader.createDecalDirShader();
		flatVertexShader.createDecalFlatShader();

        storm_.GetD3DDevice().CreateVertexDeclaration(VertexDesc_DECAL, &VtxFmt_DECAL);
	}

    ~Data()
    {
        VtxFmt_DECAL->Release();
    }

	void setSceneSize(const VC3 &size)
	{
		assert(materials.empty());
		tree.reset(new Tree(VC2(-size.x, -size.z), VC2(size.x, size.z)));
	}

	int addMaterial(IStorm3D_Material *stormMaterial)
	{
		assert(stormMaterial);
		Material material = createMaterial(static_cast<Storm3D_Material *> (stormMaterial), tree.get());

		for(unsigned int i = 0; i < materials.size(); ++i)
		{
			if(equals(materials[i], material))
				return i;
		}

		int index = materials.size();
		material.materialIndex = index;

		materials.push_back(material);
		return index;
	}

	int addDecal(int materialIndex, Type type, const VC3 &position, int &id, bool forceSpawn)
	{
		assert(materialIndex >= 0 && materialIndex < int(materials.size()));

		Material &material = materials[materialIndex];
		return material.addDecal(type, position, id, forceSpawn);
	}

	void eraseDecal(int materialIndex, int decalIndex, int id)
	{
		if(!id)
			return;

		Material &material = materials[materialIndex];
		assert(decalIndex >= 0 && decalIndex < int(material.decals.size()));

		material.eraseDecal(decalIndex, id);
	}

	void setRotation(int materialIndex, int decalIndex, int id, const QUAT &rotation)
	{
		Material &material = materials[materialIndex];
		assert(decalIndex >= 0 && decalIndex < int(material.decals.size()));
		
		Decal &decal = material.decals[decalIndex];
		if(decal.id != id)
			return;

		decal.rotation = rotation;
		material.updateDecal(decalIndex);
	}

	void setSize(int materialIndex, int decalIndex, int id, const VC2 &size)
	{
		Material &material = materials[materialIndex];
		assert(decalIndex >= 0 && decalIndex < int(material.decals.size()));
		
		Decal &decal = material.decals[decalIndex];
		if(decal.id != id)
			return;

		decal.size = size;
		material.updateDecal(decalIndex);
	}

	void setAlpha(int materialIndex, int decalIndex, int id, float alpha)
	{
		Material &material = materials[materialIndex];
		assert(decalIndex >= 0 && decalIndex < int(material.decals.size()));
		
		if(alpha < 0.f)
			alpha = 0.f;
		if(alpha > 1.f)
			alpha = 1.f;

		Decal &decal = material.decals[decalIndex];
		if(decal.id != id)
			return;

		decal.alpha = int(alpha * 255.f);
		material.updateDecal(decalIndex);
	}

	void setLighting(int materialIndex, int decalIndex, int id, const COL &color)
	{
		Material &material = materials[materialIndex];
		assert(decalIndex >= 0 && decalIndex < int(material.decals.size()));
		
		Decal &decal = material.decals[decalIndex];
		if(decal.id != id)
			return;

		decal.light = color;
		material.updateDecal(decalIndex);
	}

	void setShadowDecal(const VC3 &position, const QUAT &rotation, const VC2 &size, float alpha)
	{
		Decal decal;
		decal.position = position;
		decal.size = size;
		decal.alpha = (unsigned char)(alpha * 255.f);
		decal.rotation = rotation;
		decal.light = COL(1.f - alpha, 1.f - alpha, 1.f - alpha);

		decal.calculateValues();
		shadowDecals.push_back(decal);
	}

	void findDecals(Storm3D_Scene &scene)
	{
		decals.clear();

		// Find decals
		{
			IStorm3D_Camera *camera = scene.GetCamera();
			Storm3D_Camera *stormCamera = reinterpret_cast<Storm3D_Camera *> (camera);

			Frustum frustum = stormCamera->getFrustum();
			Tree::FrustumIterator itf(*tree, frustum);
			for(; !itf.end(); itf.next())
				decals.push_back(*itf);

			std::sort(decals.begin(), decals.end(), DecalSorter());
		}
	}

	void render(Storm3D_Scene &scene)
	{
        baseDecalVertex = 0;

		findDecals(scene);
		if(decals.empty())
            return;

		GfxDevice &device = storm.GetD3DDevice();

        Vertex_DECAL *buffer = 0;
        device.lockDynVtx<Vertex_DECAL>(decals.size() * 4, &buffer, &baseDecalVertex);
        for (unsigned int i = 0; i < decals.size(); ++i)
        {
            decals[i]->insert(buffer);
            buffer += 4;
        }
        device.unlockDynVtx();

		// Render
		if(!decals.empty())
		{
			device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

			D3DXMATRIX tm;
            D3DXMatrixIdentity(&tm);
			Storm3D_ShaderManager::GetSingleton()->SetWorldTransform(device, tm);
			pixelShader.apply();
			vertexShader.apply();

            device.SetDynVtxBuffer<Vertex_DECAL>();
            storm.setQuadIndices();

			D3DXVECTOR4 outfactor(outFactor.r, outFactor.g, outFactor.b, 1.f);
			device.SetVertexShaderConstantF(8, outfactor, 1);
			D3DXVECTOR4 infactor(inFactor.r, inFactor.g, inFactor.b, 1.f);
			device.SetVertexShaderConstantF(9, infactor, 1);

			int materialIndex = 0;
			int startIndex = 0;
			int endIndex = 0;


			for(;;)
			{
				materialIndex = decals[startIndex]->materialIndex;
				materials[materialIndex].apply(device);

				int decalAmount = decals.size();
				for(int i = startIndex + 1; i < decalAmount; ++i)
				{
					if(decals[i]->materialIndex != materialIndex)
						break;

					endIndex = i;
				}

				int renderAmount = endIndex - startIndex + 1;

                device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseDecalVertex+startIndex * 4, 0, renderAmount * 4, storm.baseQuadIndex(), renderAmount * 2);

				startIndex = ++endIndex;

				scene.AddPolyCounter(renderAmount * 2);
				if(startIndex >= decalAmount)
					break;
			}

			device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		}
	}

    void renderShadows(Storm3D_Scene &scene)
    {
        if (shadowDecals.empty() || !shadowMaterial)
            return;

        GfxDevice &device = storm.GetD3DDevice();

        int renderAmount = 0;

        IStorm3D_Camera *camera = scene.GetCamera();
        Storm3D_Camera *stormCamera = reinterpret_cast<Storm3D_Camera *> (camera);
        Frustum frustum = stormCamera->getFrustum();

        UINT baseVertex = 0;
        Vertex_DECAL *buffer = 0;;
        device.lockDynVtx<Vertex_DECAL>(shadowDecals.size() * 4, &buffer, &baseVertex);

        float inverseRange = 1.f / fogRange;
        for (unsigned int i = 0; i < shadowDecals.size() && renderAmount < MAX_DECAL_AMOUNT; ++i)
        {
            const Decal &decal = shadowDecals[i];
            Sphere sphere(decal.position, decal.getRadius());
            if (frustum.visibility(sphere))
            {
                float factor = decal.position.y - fogEnd;
                factor *= inverseRange;
                if (factor < 0.f)
                    factor = 0.f;
                if (factor > 1.f)
                    factor = 1.f;
                factor = 1.f - factor;

                COL color = decal.light;
                color.r += factor * (1.f - color.r);
                color.g += factor * (1.f - color.g);
                color.b += factor * (1.f - color.b);

                DWORD vertexColor = color.GetAsD3DCompatibleARGB() & 0x00FFFFFF;

                /*
                DWORD vertexColor = decal.vertexColor;
                int alpha = decal.alpha - int(factor * 255.f);
                if(alpha < 0)
                alpha = 0;
                vertexColor |= alpha << 24;
                DWORD oldColor = decal.vertexColor;
                */

                DWORD oldColor = decal.vertexColor;
                decal.vertexColor = vertexColor;
                decal.insert(buffer);
                decal.vertexColor = oldColor;

                buffer += 4;
                ++renderAmount;
            }
        }

        device.unlockDynVtx();

        if (renderAmount == 0)
            return;

        D3DXMATRIX tm;
        D3DXMatrixIdentity(&tm);

        //Storm3D_ShaderManager::GetSingleton()->SetWorldTransform(device, tm);

        device.SetVertexShader(0);
        device.SetPixelShader(0);
        device.SetTransform(D3DTS_WORLD, &tm);

        //vertexShader.applyDeclaration();
        device.SetVertexDeclaration(VtxFmt_DECAL);
        device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
        device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device.SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

        device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
        device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
        device.SetRenderState(D3DRS_LIGHTING, FALSE);


        shadowMaterial->applyShadow(device);

        device.SetDynVtxBuffer<Vertex_DECAL>();
        storm.setQuadIndices();
        device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVertex, 0, renderAmount * 4, storm.baseQuadIndex(), renderAmount * 2);

        scene.AddPolyCounter(renderAmount * 2);

        device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

        shadowDecals.clear();
    }

	void renderProjection(Storm3D_Scene &scene, Storm3D_Spotlight *spot)
	{
		GfxDevice &device = storm.GetD3DDevice();

		device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);

		if(!decals.empty())
		{
			device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
			device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
			device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

			//device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
			//device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

			D3DXMATRIX tm;
            D3DXMatrixIdentity(&tm);

			Storm3D_ShaderManager::GetSingleton()->SetWorldTransform(device, tm);

            device.SetDynVtxBuffer<Vertex_DECAL>();
            storm.setQuadIndices();

			int materialIndex = 0;
			int startIndex = 0;
			int endIndex = 0;

			if(spot->getType() == IStorm3D_Spotlight::Point)
				pointVertexShader.apply();
			else if(spot->getType() == IStorm3D_Spotlight::Directional)
				dirVertexShader.apply();
			else if(spot->getType() == IStorm3D_Spotlight::Flat)
				flatVertexShader.apply();

			Storm3D_ShaderManager::GetSingleton()->SetTransparencyFactor(0.75f);

			for(;;)
			{
				materialIndex = decals[startIndex]->materialIndex;
				materials[materialIndex].applyProjection(device, spot->getColorMultiplier());

				int decalAmount = decals.size();
				for(int i = startIndex + 1; i < decalAmount; ++i)
				{
					if(decals[i]->materialIndex != materialIndex)
						break;

					endIndex = i;
				}

				int renderAmount = endIndex - startIndex + 1;

                device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseDecalVertex+startIndex * 4, 0, renderAmount * 4, storm.baseQuadIndex(), renderAmount * 2);

                startIndex = ++endIndex;

				scene.AddPolyCounter(renderAmount * 2);
				if(startIndex >= decalAmount)
					break;
			}

			Storm3D_ShaderManager::GetSingleton()->SetTransparencyFactor(1.f);

			device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
			device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
			device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		}

		device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	}
};

Storm3D_TerrainDecalSystem::Storm3D_TerrainDecalSystem(Storm3D &storm)
:	data(new Data(storm))
{
}

Storm3D_TerrainDecalSystem::~Storm3D_TerrainDecalSystem()
{
    assert(data);
    delete data;
}

void Storm3D_TerrainDecalSystem::setSceneSize(const VC3 &size)
{
	data->setSceneSize(size);
}

int Storm3D_TerrainDecalSystem::addMaterial(IStorm3D_Material *material)
{
	return data->addMaterial(material);
}

int Storm3D_TerrainDecalSystem::addDecal(int materialId, Type type, const VC3 &position, int &id, bool forceSpawn)
{
	return data->addDecal(materialId, type, position, id, forceSpawn);
}

void Storm3D_TerrainDecalSystem::eraseDecal(int materialId, int decalId, int id)
{
	if(!id)
		return;

	data->eraseDecal(materialId, decalId, id);
}

void Storm3D_TerrainDecalSystem::setRotation(int materialId, int decalId, int id, const QUAT &rotation)
{
	data->setRotation(materialId, decalId, id, rotation);
}

void Storm3D_TerrainDecalSystem::setSize(int materialId, int decalId, int id, const VC2 &size)
{
	data->setSize(materialId, decalId, id, size);
}

void Storm3D_TerrainDecalSystem::setAlpha(int materialId, int decalId, int id, float alpha)
{
	data->setAlpha(materialId, decalId, id, alpha);
}

void Storm3D_TerrainDecalSystem::setLighting(int materialId, int decalId, int id, const COL &color)
{
	data->setLighting(materialId, decalId, id, color);
}

void Storm3D_TerrainDecalSystem::setLightmapFactor(const COL &factor)
{
	data->inFactor = factor;
}

void Storm3D_TerrainDecalSystem::setOutdoorLightmapFactor(const COL &factor)
{
	data->outFactor = factor;
}

void Storm3D_TerrainDecalSystem::setFog(float end, float range)
{
	data->fogEnd = end;
	data->fogRange = range;
}

void Storm3D_TerrainDecalSystem::renderTextures(Storm3D_Scene &scene)
{
    GFX_TRACE_SCOPE("Storm3D_TerrainDecalSystem::renderTextures");
	data->render(scene);
}

void Storm3D_TerrainDecalSystem::renderShadows(Storm3D_Scene &scene)
{
    GFX_TRACE_SCOPE("Storm3D_TerrainDecalSystem::renderShadows");
	data->renderShadows(scene);
}

void Storm3D_TerrainDecalSystem::renderProjection(Storm3D_Scene &scene, Storm3D_Spotlight *spot)
{
    GFX_TRACE_SCOPE("Storm3D_TerrainDecalSystem::renderProjection");
	data->renderProjection(scene, spot);
}

void Storm3D_TerrainDecalSystem::setShadowMaterial(IStorm3D_Material *material)
{
	assert(data->tree);
	Storm3D_Material *m = static_cast<Storm3D_Material *> (material);

	Material newMaterial = createMaterial(m, data->tree.get());
	data->shadowMaterial.reset(new Material(newMaterial));
}

void Storm3D_TerrainDecalSystem::setShadowDecal(const VC3 &position, const QUAT &rotation, const VC2 &size, float alpha)
{
	data->setShadowDecal(position, rotation, size, alpha);
}

void Storm3D_TerrainDecalSystem::clearShadowDecals()
{
	data->shadowDecals.clear();
}
