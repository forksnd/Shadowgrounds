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

        uint8_t alpha;
		int materialIndex;

		VC3 normal;
		VC3 vertices[4];

		bool deleted;
		Tree::Entity *entity;
        IStorm3D_TerrainDecalSystem::Type type;

		int id;

		Decal()
		:	alpha(0),
			materialIndex(-1),
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

			if(entity)
				entity->setRadius(getRadius());
		}

        void insert(Vertex_P3DUV *buffer, uint32_t vertexColor) const
        {
            buffer->p = vertices[0];
            buffer->d = vertexColor;
            buffer->uv = VC2(0.f, 0.f);

            ++buffer;
            buffer->p = vertices[1];
            buffer->d = vertexColor;
            buffer->uv = VC2(0.f, 1.f);

            ++buffer;
            buffer->p = vertices[2];
            buffer->d = vertexColor;
            buffer->uv = VC2(1.f, 0.f);

            ++buffer;
            buffer->p = vertices[3];
            buffer->d = vertexColor;
            buffer->uv = VC2(1.f, 1.f);
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
            decal.type = type;

			if(canAdd)
			{
				decal.deleted = false;
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

		void applyProjection(gfx::Device &device, const COL &spotColor)
		{
			if(baseTexture)
				baseTexture->Apply(2);

			//float factor = .55f;
			float factor = 1.f;
			D3DXVECTOR4 diffuse(diffuseColor.r * factor * spotColor.r, diffuseColor.g * factor * spotColor.g, diffuseColor.b * factor * spotColor.b, 1.f);
			device.SetVertexShaderConstantF(17, diffuse, 1);
		}

		void applyShadow(gfx::Device &device)
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

	VertexShader pointVertexShader;
	VertexShader dirVertexShader;
	VertexShader flatVertexShader;

    UINT baseDecalVertex;

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
		pointVertexShader(storm.GetD3DDevice()),
		dirVertexShader(storm.GetD3DDevice()),
		flatVertexShader(storm.GetD3DDevice()),
		tree(),
		outFactor(1.f, 1.f, 1.f),
		inFactor(1.f, 1.f, 1.f),
		fogEnd(-1000000000000.f),
		fogRange(10.f)
	{
		pointVertexShader.createDecalPointShader();
		dirVertexShader.createDecalDirShader();
		flatVertexShader.createDecalFlatShader();
	}

    ~Data()
    {
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
        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;
        gfx::ProgramManager& programManager = renderer.programManager;

        baseDecalVertex = 0;

        findDecals(scene);
        if (decals.empty())
            return;

        Vertex_P3DUV *buffer = 0;
        renderer.lockDynVtx<Vertex_P3DUV>(decals.size() * 4, &buffer, &baseDecalVertex);
        for (unsigned int i = 0; i < decals.size(); ++i)
        {
            Decal& decal = *decals[i];
            COL decalColor = 
                decal.light * (decal.type == Outside ? outFactor : inFactor);
            decalColor.Clamp();
            uint32_t vertexColor = decalColor.as_u32_D3D_ARGB(decal.alpha);

            decal.insert(buffer, vertexColor);
            buffer += 4;
        }
        renderer.unlockDynVtx();

        // Render
        if (!decals.empty())
        {
            device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

            D3DXMATRIX tm;
            D3DXMatrixIdentity(&tm);
            programManager.setWorldMatrix(tm);
            programManager.setProgram(gfx::ProgramManager::DECAL_LIGHTING);

            renderer.setDynVtxBuffer<Vertex_P3DUV>();
            renderer.setFVF(FVF_P3DUV);

            int materialIndex = 0;
            int startIndex = 0;
            int endIndex = 0;


            for (;;)
            {
                materialIndex = decals[startIndex]->materialIndex;
                if(materials[materialIndex].baseTexture)
                    materials[materialIndex].baseTexture->Apply(0);
                programManager.setDiffuse(materials[materialIndex].diffuseColor);
                programManager.applyState(device);

                int decalAmount = decals.size();
                for (int i = startIndex + 1; i < decalAmount; ++i)
                {
                    if (decals[i]->materialIndex != materialIndex)
                        break;

                    endIndex = i;
                }

                int renderAmount = endIndex - startIndex + 1;

                renderer.drawQuads(baseDecalVertex + startIndex * 4, renderAmount);

                startIndex = ++endIndex;

                if (startIndex >= decalAmount)
                    break;
            }

            device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        }
    }

    void renderShadows(Storm3D_Scene &scene)
    {
        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;
        gfx::ProgramManager& programManager = renderer.programManager;

        if (shadowDecals.empty() || !shadowMaterial)
            return;

        int renderAmount = 0;

        IStorm3D_Camera *camera = scene.GetCamera();
        Storm3D_Camera *stormCamera = reinterpret_cast<Storm3D_Camera *> (camera);
        Frustum frustum = stormCamera->getFrustum();

        UINT baseVertex = 0;
        Vertex_P3DUV *buffer = 0;;
        renderer.lockDynVtx<Vertex_P3DUV>(shadowDecals.size() * 4, &buffer, &baseVertex);

        float inverseRange = 1.f / fogRange;
        for (unsigned int i = 0; i < shadowDecals.size() && renderAmount < MAX_DECAL_AMOUNT; ++i)
        {
            const Decal &decal = shadowDecals[i];
            Sphere sphere(decal.position, decal.getRadius());
            if (frustum.visibility(sphere))
            {
                float factor = decal.position.y - fogEnd;
                factor *= inverseRange;
                factor = std::max(0.0f, factor);
                factor = std::min(1.0f, factor);
                factor = 1.f - factor;

                COL color = decal.light;
                color.r += factor * (1.f - color.r);
                color.g += factor * (1.f - color.g);
                color.b += factor * (1.f - color.b);

                uint32_t vertexColor = color.as_u32_D3D_ARGB(0);
                decal.insert(buffer, vertexColor);

                buffer += 4;
                ++renderAmount;
            }
        }

        renderer.unlockDynVtx();

        if (renderAmount == 0)
            return;

        D3DXMATRIX tm;
        D3DXMatrixIdentity(&tm);

        programManager.setProgram( gfx::ProgramManager::DECAL_SHADOW);
        programManager.setWorldMatrix(tm);
        programManager.applyState(device);

        device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
        device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);

        shadowMaterial->applyShadow(device);

        renderer.setFVF(FVF_P3DUV);
        renderer.setDynVtxBuffer<Vertex_P3DUV>();
        renderer.drawQuads(baseVertex, renderAmount);

        device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

        shadowDecals.clear();
    }

	void renderProjection(Storm3D_Scene &scene, Storm3D_Spotlight *spot)
	{
        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;

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

            renderer.setDynVtxBuffer<Vertex_P3DUV>();
            renderer.setFVF(FVF_P3DUV);

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

                renderer.drawQuads(baseDecalVertex+startIndex * 4, renderAmount);

                startIndex = ++endIndex;

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
