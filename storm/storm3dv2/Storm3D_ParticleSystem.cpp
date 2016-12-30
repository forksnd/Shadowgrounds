// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "storm3d.h"

#include <vector>

#include "storm3d_scene.h"
#include "storm3d_particle.h"
#include "Storm3D_ShaderManager.h"
#include "VertexFormats.h"
#include "storm3d_terrain_utils.h"
#include "../../util/Debug_MemoryManager.h"

using namespace frozenbyte::storm;


IDirect3DVertexBuffer9* Storm3D_ParticleSystem::PointArray::m_vb = NULL;
int Storm3D_ParticleSystem::PointArray::m_totalParticles = 0;
int Storm3D_ParticleSystem::PointArray::m_maxParticles = 0;

IDirect3DVertexBuffer9* Storm3D_ParticleSystem::QuadArray::m_vb = NULL;
IDirect3DIndexBuffer9* Storm3D_ParticleSystem::QuadArray::m_ib = NULL;
int Storm3D_ParticleSystem::QuadArray::m_totalParticles = 0;
int Storm3D_ParticleSystem::QuadArray::m_maxParticles = 0;

IDirect3DVertexBuffer9* Storm3D_ParticleSystem::LineArray::m_vb = NULL;
IDirect3DIndexBuffer9* Storm3D_ParticleSystem::LineArray::m_ib = NULL;
int Storm3D_ParticleSystem::LineArray::m_totalParticles = 0;
int Storm3D_ParticleSystem::LineArray::m_maxParticles = 0;

namespace {

	frozenbyte::storm::VertexBuffer vertexBuffer[2];
	char *temporaryPointer[2] = { 0, 0};
	//int allocateIndexAmount = 4096 * 6; // 4096 particles in one call
	int allocateIndexAmount = 22000 * 6; // 4096 particles in one call
	int allocateVertexAmount[2] = { 0, 0 };
	int vertexBufferOffset[2] = { 0, 0 };

	void initBuffer(gfx::Device &device, int particleBufferSize, int index)
	{
		int vertexAmount = particleBufferSize * 4;
		if(vertexAmount < 2048)
			vertexAmount = 2048;

		if(vertexAmount > allocateVertexAmount[index])
		{
			vertexBufferOffset[index] = 0;
			vertexBuffer[index].create(device, vertexAmount, sizeof(Vertex_P3DUV2), true);
			allocateVertexAmount[index] = vertexAmount;

			delete[] temporaryPointer[index];
			temporaryPointer[index] = new char[vertexAmount * sizeof(Vertex_P3DUV2)];
		}
	}

} // unnamed

void releaseDynamicBuffers()
{
	allocateVertexAmount[0] = 0;
	vertexBufferOffset[0] = 0;
	vertexBuffer[0].release();
	allocateVertexAmount[1] = 0;
	vertexBufferOffset[1] = 0;
	vertexBuffer[1].release();
}

void recreateDynamicBuffers()
{
}

void releaseBuffers()
{
	releaseDynamicBuffers();

	delete[] temporaryPointer[0];
	delete[] temporaryPointer[1];
	temporaryPointer[0] = 0;
	temporaryPointer[1] = 0;
}

void Storm3D_ParticleSystem::Render(Storm3D_Scene* scene, bool distortion) 
{
	RenderImp(scene, distortion);
	Clear();
}

void Storm3D_ParticleSystem::RenderImp(Storm3D_Scene *scene, bool distortion) 
{
    gfx::Renderer& renderer = Storm3D2->renderer;
    gfx::Device& device = renderer.device;
    gfx::ProgramManager& programManager = renderer.programManager;

    GFX_TRACE_SCOPE("Storm3D_ParticleSystem::RenderImp");
	//if(distortion && !offsetShader.hasShader())
	//	offsetShader.createOffsetBlendShader();

	//device.SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
	frozenbyte::storm::setCulling(device, D3DCULL_NONE);
	device.SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
	device.SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
	device.SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
	device.SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
	device.SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
	device.SetRenderState(D3DRS_ALPHAREF, 0x1);

	int maxParticles = 0;
	int index = (distortion) ? 1 : 0;

	// Get particle amount
	{
		std::vector<IParticleArray *>::iterator it = m_particleArrays.begin();
		for(; it != m_particleArrays.end(); ++it)
		{
			if((*it)->isDistorted() == distortion)
				maxParticles += (*it)->getMaxParticleAmount();
		}
	}

	initBuffer(device, 2 * maxParticles, index);
	int oldVertexBufferOffset = 0;

	// Update buffer
	{
		Vertex_P3DUV2 *updatePointer = reinterpret_cast<Vertex_P3DUV2 *> (temporaryPointer[index]);
		int particleOffset = 0;

		if(updatePointer)
		{
			std::vector<IParticleArray *>::iterator it = m_particleArrays.begin();
			for(; it != m_particleArrays.end(); ++it) 
			{
				if((*it)->isDistorted() == distortion)
				{
					particleOffset += (*it)->lock(updatePointer, particleOffset, scene);
					assert(particleOffset <= maxParticles);
				}
			}

			Vertex_P3DUV2 *vertexPointer = 0;
			int neededVertexAmount = particleOffset * 4;

            if(neededVertexAmount)
			{
				// WTF we need this for -- DISCARD/NOOVERWRITE shoud do the trick already?
				// Bugs only with certain explosion effect - DX issue?
				if(vertexBufferOffset + neededVertexAmount >= allocateVertexAmount)
				{
					vertexBufferOffset[index] = 0;
					vertexPointer = static_cast<Vertex_P3DUV2 *> (vertexBuffer[index].lock());
					vertexBuffer[index].unlock();
				}

				if(vertexBufferOffset + neededVertexAmount < allocateVertexAmount)
				{
					vertexPointer = static_cast<Vertex_P3DUV2 *> (vertexBuffer[index].unsafeLock(vertexBufferOffset[index], neededVertexAmount));
					oldVertexBufferOffset = vertexBufferOffset[index];
					vertexBufferOffset[index] += neededVertexAmount;
				}
				else
				{
					vertexBufferOffset[index] = 0;
					vertexPointer = static_cast<Vertex_P3DUV2 *> (vertexBuffer[index].lock());
				}

				memcpy(vertexPointer, updatePointer, neededVertexAmount * sizeof(Vertex_P3DUV2));
				vertexBuffer[index].unlock();
			}
		}
	}

	vertexBuffer[index].apply(device, 0);

    programManager.setStdProgram(device, gfx::ProgramManager::SSF_COLOR|gfx::ProgramManager::SSF_TEXTURE);
    programManager.setProjectionMatrix(scene->camera.GetProjectionMatrix());
    renderer.setFVF(FVF_P3DUV2);

	// Render arrays
	{
		std::vector<IParticleArray *>::iterator it = m_particleArrays.begin();
		for(; it != m_particleArrays.end(); ++it) 
		{
			if((*it)->isDistorted() == distortion)
			{
				int vertexOffset = 0;
				int particleAmount = 0;

                programManager.setViewMatrix(scene->camera.GetViewMatrix());
				(*it)->setRender(renderer, vertexOffset, particleAmount);

				if(particleAmount > 0)
				{
					// Do we have to something more intelligent - no need for this large amounts?
					if(particleAmount >= allocateIndexAmount / 6)
						particleAmount = allocateIndexAmount / 6 - 1;

                    renderer.drawQuads(vertexOffset + oldVertexBufferOffset, particleAmount);
				}
			}
		}
	}
}

void Storm3D_ParticleSystem::Clear() 
{
	std::vector<IParticleArray *>::iterator it = m_particleArrays.begin();
	for(; it != m_particleArrays.end(); ++it) 
			delete (*it);

	m_particleArrays.clear();
}

