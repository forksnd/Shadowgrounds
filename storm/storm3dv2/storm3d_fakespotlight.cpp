// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <string>
#include <vector>

#include "storm3d_fakespotlight.h"
#include "storm3d_spotlight_shared.h"
#include "Storm3D_ShaderManager.h"
#include "storm3d_camera.h"
#include "storm3d_terrain_utils.h"
#include "storm3d.h"
#include <IStorm3D_Logger.h>

#include "../../util/Debug_MemoryManager.h"

// FIXME since using shared color texture
// - SetViewport
// - Scale texcoords for reading


	int BUFFER_WIDTH = 512;
	int BUFFER_HEIGHT = 512;
	static const bool SHARE_BUFFERS = true;

	static bool fakeTargetActive = false;

	struct RenderTarget
	{
		CComPtr<IDirect3DTexture9> color;
		CComPtr<IDirect3DSurface9> depth;

		gfx::Device &device;
		VC2I pos;

		bool hasInitialized() const
		{
			return color && depth;
		}

		RenderTarget(gfx::Device &device_, CComPtr<IDirect3DTexture9> sharedColor, CComPtr<IDirect3DSurface9> sharedDepth)
		:	device(device_)
		{
			color = sharedColor;
			if(!color)
			{
				HRESULT hr = device.CreateTexture(BUFFER_WIDTH * 2, BUFFER_HEIGHT * 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &color, 0);
				if(FAILED(hr))
				{
					hr = device.CreateTexture(BUFFER_WIDTH * 2, BUFFER_HEIGHT * 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &color, 0);
					if(FAILED(hr))
						return;
				}
			}

			depth = sharedDepth;
			if(!depth)
				device.CreateDepthStencilSurface(BUFFER_WIDTH * 2, BUFFER_HEIGHT * 2, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, D3DMULTISAMPLE_NONE, FALSE, &depth, 0);

			if(!hasInitialized())
			{
				color.Release();
				depth.Release();
			}
		}

		bool set()
		{
			if(!color || !depth)
				return false;

			if(!fakeTargetActive || !SHARE_BUFFERS)
			{
				fakeTargetActive = true;

				CComPtr<IDirect3DSurface9> colorSurface;
				color->GetSurfaceLevel(0, &colorSurface);

				HRESULT hr = device.SetRenderTarget(0, colorSurface);
				if(FAILED(hr))
					return false;

				hr = device.SetDepthStencilSurface(depth);
				if(FAILED(hr))
					return false;

				D3DRECT rc = { 0 };
				rc.x2 = BUFFER_WIDTH * 2;
				rc.y2 = BUFFER_HEIGHT * 2;

				// Black, no alpha test
				//hr = device.Clear(1, &rc, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFF000000, 1.0f, 0);
				// Black, alpha test
				//hr = device.Clear(1, &rc, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
				// White, no alpha test
				//hr = device.Clear(1, &rc, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFFFFFFFF, 1.0f, 0);
				// White, alpha tested (Normal)
				hr = device.Clear(1, &rc, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00FFFFFF, 1.0f, 0);
				if(FAILED(hr))
					return false;
			}

			D3DVIEWPORT9 vp;
			vp.X = pos.x * BUFFER_WIDTH;
			vp.Y = pos.y * BUFFER_HEIGHT;
			vp.Width = BUFFER_WIDTH;
			vp.Height = BUFFER_HEIGHT;
			vp.MinZ = 0.f;
			vp.MaxZ = 1.f;
			device.SetViewport(&vp);

			return true;
		}

		void applyColor(int stage)
		{
			device.SetTexture(stage, color);
		}
	};

	struct RenderTargets
	{
		enum { BUFFER_LIMIT = 4 };
		RenderTarget *targets[BUFFER_LIMIT];
		bool used[BUFFER_LIMIT];

		CComPtr<IDirect3DTexture9> sharedColor;
		CComPtr<IDirect3DSurface9> sharedDepth;

		float usedWidth;
		float usedHeight;
		float soffsetX;
		float soffsetY;
		float scaleX;
		float scaleY;

		struct BufferDeleter
		{
			RenderTargets &targets;

			BufferDeleter(RenderTargets &targets_)
			:  targets(targets_)
			{
			}

			void operator() (RenderTarget *target) const
			{
				if(target)
					targets.freeTarget(target);
			}
		};

		RenderTargets()
		{
			for(int i = 0; i < BUFFER_LIMIT; ++i)
			{
				targets[i] = 0;
				used[i] = false;
			}

			usedWidth = 0;
			usedHeight = 0;
			soffsetX = 0;
			soffsetY = 0;
			scaleX = 0;
			scaleY = 0;
		}

		~RenderTargets()
		{
		}

		bool createAll(Storm3D &storm, gfx::Device &device, int quality)
		{
			freeAll();
			if(SHARE_BUFFERS)
			{
				sharedColor = storm.getColorTarget();
				sharedDepth = storm.getDepthTarget();
			}

			for(unsigned int i = 0; i < BUFFER_LIMIT; ++i)
			{
				targets[i] = new RenderTarget(device, sharedColor, sharedDepth);
				if(SHARE_BUFFERS)
				{
					if(i == 0)
					{
						sharedColor = targets[i]->color;
						sharedDepth = targets[i]->depth;
					}
				}

				VC2I pos(0, 0);
				if(i == 1)
					pos.x = 1;
				else if(i == 2)
					pos.y = 1;
				else if(i == 3)
				{
					pos.x = 1;
					pos.y = 1;
				}

				targets[i]->pos = pos;
			}

			if(targets[0]->color)
			{
				CComPtr<IDirect3DSurface9> colorSurface;
				targets[0]->color->GetSurfaceLevel(0, &colorSurface);

				D3DSURFACE_DESC desc;
				colorSurface->GetDesc(&desc);
				usedWidth = float(BUFFER_WIDTH * 2) / (desc.Width);
				usedHeight = float(BUFFER_HEIGHT * 2) / (desc.Height);

				soffsetX = usedWidth * .5f * .5f;
				soffsetY = usedHeight * .5f * .5f;
				scaleX = soffsetX;
				scaleY = soffsetY;
			}

			if(!sharedColor)
				return false;

			return true;
		}

		void freeAll()
		{
			for(unsigned int i = 0; i < BUFFER_LIMIT; ++i)
			{
				delete targets[i];

				targets[i] = 0;
				used[i] = false;
			}

			if(sharedColor)
				sharedColor.Release();
			if(sharedDepth)
				sharedDepth.Release();
			//if(filterTexture)
			//	filterTexture.Release();
		}

		std::shared_ptr<RenderTarget> getTarget()
		{
			for(unsigned int i = 0; i < BUFFER_LIMIT; ++i)
			{
				if(!used[i] && targets[i]->hasInitialized())
				{
					used[i] = true;
					return std::shared_ptr<RenderTarget>(targets[i], BufferDeleter(*this));
				}
			}

			assert(!"Whoops");	
			return std::shared_ptr<RenderTarget>();
		}

		void freeTarget(const RenderTarget *target)
		{
			for(unsigned int i = 0; i < BUFFER_LIMIT; ++i)
			{
				if(targets[i] == target)
				{
					assert(used[i]);
					used[i] = false;
				}
			}
		}
	};

	RenderTargets renderTargets;

	struct ProjectionPlane
	{
		float height;

		VC2 min;
		VC2 max;

		ProjectionPlane()
		:	height(0)
		{
		}
	};


struct Storm3D_FakeSpotlight::Data
{
    //TODO: change to manager pattern, share this higher
	Storm3D &storm; 
	gfx::Device &device;

    //TODO: create proper rt sharing solution at higher level
	std::shared_ptr<RenderTarget> renderTarget;
	Storm3D_SpotlightShared properties;

	Storm3D_Camera camera;
	ProjectionPlane plane;

	float fadeFactor;
	float fogFactor;
	bool enabled;
	bool visible;
	bool renderObjects;

	COL renderedColor;

    //TODO: replace with dynamic and static solution
    static etlsf_alloc_t indicesAllocId;
    static uint32_t baseIndex;

	Data(Storm3D &storm_, gfx::Device &device_)
	:	storm(storm_),
		device(device_),
		properties(device),
		camera(&storm),
		fadeFactor(0),
		fogFactor(0.f),
		enabled(false),
		visible(false),
		renderObjects(true)
	{
		if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
			return;

		properties.color = COL(.5f, .5f, .5f);
	}

	void updateMatrices(const float *cameraView)
	{
		if(renderTarget)
			properties.targetPos = renderTarget->pos;

		properties.resolutionX = 2 * BUFFER_WIDTH;
		properties.resolutionY = 2 * BUFFER_HEIGHT;
		properties.soffsetX = renderTargets.soffsetX;
		properties.soffsetY = renderTargets.soffsetY;
		properties.scaleX = renderTargets.scaleX;
		properties.scaleY = renderTargets.scaleY;
		properties.updateMatricesOffCenter(cameraView, plane.min, plane.max, plane.height, camera);

		//camera.SetPosition(properties.position);
		//camera.SetTarget(properties.position + properties.direction);
		//camera.SetFieldOfView(D3DXToRadian(properties.fov));
		//camera.SetVisibilityRange(properties.range);
	}
};

//TODO: make shared
etlsf_alloc_t Storm3D_FakeSpotlight::Data::indicesAllocId = ETLSF_INVALID_ID;
uint32_t Storm3D_FakeSpotlight::Data::baseIndex = 0;

Storm3D_FakeSpotlight::Storm3D_FakeSpotlight(Storm3D &storm, gfx::Device &device)
{
	data = new Data(storm, device);
}

Storm3D_FakeSpotlight::~Storm3D_FakeSpotlight()
{
    assert(data);
    delete data;
}

void Storm3D_FakeSpotlight::testVisibility(Storm3D_Camera &camera)
{
//	data->visible = camera.TestSphereVisibility(data->properties.position, data->properties.range);
//	data->visible = true;

	// Frustum vs. AABB
	VC2 min, max;
	float planeY = data->properties.position.y - data->plane.height;
	getPlane(min, max);
	AABB clipBox ( VC3( min.x, planeY - 0.1f, min.y ), VC3( max.x, planeY + 0.1f, max.y ) );
	data->visible = camera.TestBoxVisibility ( clipBox.mmin, clipBox.mmax );

}

void Storm3D_FakeSpotlight::disableVisibility()
{
	data->visible = false;
}

void Storm3D_FakeSpotlight::enable(bool enable)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	data->enabled = enable;

	if(enable && !data->renderTarget)
		data->renderTarget = renderTargets.getTarget();
	else if(!enable)
		data->renderTarget.reset();
}

bool Storm3D_FakeSpotlight::enabled() const
{
	return data->enabled && data->visible;
}

void Storm3D_FakeSpotlight::setPosition(const VC3 &position)
{
	data->properties.position = position;
}

void Storm3D_FakeSpotlight::setDirection(const VC3 &direction)
{
	data->properties.direction = direction;
}

void Storm3D_FakeSpotlight::setFov(float fov)
{
	data->properties.fov = fov;
}

void Storm3D_FakeSpotlight::setRange(float range)
{
	data->properties.range = range;
}

void Storm3D_FakeSpotlight::setColor(const COL &color, float fadeFactor)
{
	data->properties.color = color;
	data->fadeFactor = fadeFactor;
}

void Storm3D_FakeSpotlight::setPlane(float height, const VC2 &minCorner, const VC2 &maxCorner)
{
	data->plane.height = height;
	data->plane.min = minCorner;
	data->plane.max = maxCorner;
}

void Storm3D_FakeSpotlight::renderObjectShadows(bool render)
{
	data->renderObjects = render;
}

void Storm3D_FakeSpotlight::setFogFactor(float factor)
{
	data->fogFactor = factor;
}

void Storm3D_FakeSpotlight::getPlane(VC2 &min, VC2 &max) const
{
	min = data->plane.min;
	min.x += data->properties.position.x;
	min.y += data->properties.position.z;

	max = data->plane.max;
	max.x += data->properties.position.x;
	max.y += data->properties.position.z;
}

float Storm3D_FakeSpotlight::getPlaneHeight() const
{
	return data->properties.position.y - data->plane.height;
}

bool Storm3D_FakeSpotlight::shouldRenderObjectShadows() const
{
	return data->renderObjects;
}

Storm3D_Camera &Storm3D_FakeSpotlight::getCamera()
{
	return data->camera;
}

void Storm3D_FakeSpotlight::setClipPlanes(const float *cameraView)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	data->properties.setClipPlanes(cameraView);
}

void Storm3D_FakeSpotlight::setScissorRect(Storm3D_Camera &camera, const VC2I &screenSize)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	data->properties.setScissorRect(camera, screenSize);
}

bool Storm3D_FakeSpotlight::setAsRenderTarget(const D3DXMATRIX &cameraView)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return false;

	data->updateMatrices(cameraView);
	if(data->renderTarget && data->renderTarget->set())
	{
		Storm3D_ShaderManager::GetSingleton()->setTextureTm(data->properties.shaderProjection[0]);
		Storm3D_ShaderManager::GetSingleton()->setSpot(COL(), data->properties.position, data->properties.direction, data->properties.range, .1f);
		Storm3D_ShaderManager::GetSingleton()->SetViewProjectionMatrix(data->properties.lightViewProjection[1], data->properties.lightViewProjection[1]);
	}

	COL c = data->properties.color;
	/*
	c.r += data->fogFactor * (1.f - c.r);
	c.g += data->fogFactor * (1.f - c.g);
	c.b += data->fogFactor * (1.f - c.b);
	*/

	float colorData[4] = { c.r, c.g, c.b, 0 };
	data->renderedColor = c;

    Storm3D_ShaderManager::GetSingleton()->SetShaderDiffuse(data->device, c);
	//Storm3D_ShaderManager::GetSingleton()->setFakeProperties(data->properties.position.y - data->plane.height, .1f, .3137f);

	return true;
}

void Storm3D_FakeSpotlight::applyTextures(const D3DXMATRIX &cameraView)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	data->updateMatrices(cameraView);
	if(data->renderTarget)
	{
		data->renderTarget->applyColor(0);
		data->renderTarget->applyColor(1);
		data->renderTarget->applyColor(2);
		data->renderTarget->applyColor(3);
		Storm3D_ShaderManager::GetSingleton()->setSpotTarget(data->properties.targetProjection);
	}

	Storm3D_ShaderManager::GetSingleton()->setTextureTm(data->properties.shaderProjection[0]);
	Storm3D_ShaderManager::GetSingleton()->setSpot(COL(), data->properties.position, data->properties.direction, data->properties.range, .1f);

	/*
	COL c;
	c.r = 
	float colorData[4] = { c.r, c.g, c.b, 0 };
	data->device.SetPixelShaderConstantF(2, colorData, 1);
	*/
}

void Storm3D_FakeSpotlight::renderProjection()
{
    gfx::Renderer& renderer = data->storm.renderer;

	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	if(!data->renderTarget)
		return;

	VC3 position = data->properties.position;
	position.y -= data->plane.height;

	const VC2 &min = data->plane.min;
	const VC2 &max = data->plane.max;
	float range = data->properties.range * sqrtf(2.f);

	VC3 a = position;
	a.x += min.x;
	a.z += min.y;
	float ad = a.GetRangeTo(position) / range;
	VC3 b = position;
	b.x += min.x;
	b.z += max.y;
	float bd = b.GetRangeTo(position) / range;
	VC3 c = position;
	c.x += max.x;
	c.z += min.y;
	float cd = c.GetRangeTo(position) / range;
	VC3 d = position;
	d.x += max.x;
	d.z += max.y;
	float dd = d.GetRangeTo(position) / range;
	VC3 e = position;
	float ed = 0.f;

	ad = bd = cd = dd = 1.f;

    Vertex_P3UV vertices[] =
	{
		{VC3(a.x, a.y, a.z),  VC2(ad, .5f)},
		{VC3(b.x, b.y, b.z),  VC2(bd, .5f)},
		{VC3(c.x, c.y, c.z),  VC2(cd, .5f)},
		{VC3(d.x, d.y, d.z),  VC2(dd, .5f)},
		{VC3(e.x, e.y, e.z),  VC2(ed, .5f)},
	};

    uint32_t baseVertex = 0;
    Vertex_P3UV* buffer = 0;;
    renderer.lockDynVtx<Vertex_P3UV>(5 * sizeof(Vertex_P3UV), &buffer, &baseVertex);
    memcpy(buffer, vertices, 5 * sizeof(Vertex_P3UV));
    renderer.unlockDynVtx();

	Storm3D_ShaderManager *manager = Storm3D_ShaderManager::GetSingleton();
	manager->setTextureTm(data->properties.shaderProjection[0]);
	manager->setSpot(COL(), data->properties.position, data->properties.direction, data->properties.range, .1f);

	D3DXMATRIX identity;
	D3DXMatrixIdentity(&identity);

	manager->setSpotTarget(data->properties.targetProjection);
	manager->SetWorldTransform(data->device, identity);

	data->device.SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	data->device.SetRenderState(D3DRS_ALPHAREF, 0x01);
	data->device.SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);

	{
		//float factor = data->fadeFactor + data->fogFactor * (1.f - data->fadeFactor);
		//float factor = data->fadeFactor * data->fogFactor;
		//float factor = data->fadeFactor;
		float factor = (1.f - data->fadeFactor) + data->fogFactor * (data->fadeFactor);
		float c0[4] = { factor, factor, factor, 1.f };
		data->device.SetPixelShaderConstantF(0, c0, 1);

		float fogFactor = data->fogFactor;
		float c2[4] = { fogFactor, fogFactor, fogFactor, 1.f };
		data->device.SetPixelShaderConstantF(2, c2, 1);
	}

	{
		CComPtr<IDirect3DSurface9> colorSurface;
		data->renderTarget->color->GetSurfaceLevel(0, &colorSurface);

		D3DSURFACE_DESC sourceDesc;
		colorSurface->GetDesc(&sourceDesc);
		float xd = 1.f / float(sourceDesc.Width);
		float yd = 1.f / float(sourceDesc.Height);
		float xd1 = xd * 1.5f;
		float yd1 = yd * 1.5f;
		float xd2 = xd * 2.5f;
		float yd2 = yd * 2.5f;

		/*
		float xd = 1.f / float(sourceDesc.Width);
		float yd = 1.f / float(sourceDesc.Height);
		float xd1 = xd * 0.5f;
		float yd1 = yd * 0.5f;
		float xd2 = xd * 1.5f;
		float yd2 = yd * 1.5f;
		*/

		float deltas1[4] = { -xd2, -yd2, 0, 0 };
		float deltas2[4] = { -xd1,  yd2, 0, 0 };
		float deltas3[4] = {  xd1, -yd1, 0, 0 };
		float deltas4[4] = {  xd2,  yd1, 0, 0 };

		data->device.SetVertexShaderConstantF(5, deltas1, 1);
		data->device.SetVertexShaderConstantF(6, deltas2, 1);
		data->device.SetVertexShaderConstantF(7, deltas3, 1);
		data->device.SetVertexShaderConstantF(8, deltas4, 1);
	}

    renderer.setFVF(FVF_P3UV);
    renderer.setDynVtxBuffer<Vertex_P3UV>();
    renderer.device.SetIndices(renderer.getIndexStorage16().indices);
    renderer.device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, baseVertex, 0, 5, data->baseIndex, 4);
}

void Storm3D_FakeSpotlight::debugRender()
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	float buffer[] = 
	{
		0.f, 300.f, 0.f, 1.f, 0.f, 1.f,
		0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
		300.f, 300.f, 0.f, 1.f, 1.f, 1.f,
		300.f, 0.f, 0.f, 1.f, 1.f, 0.f
	};

	data->device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	data->device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	data->device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	data->device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	data->device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	data->device.SetPixelShader(0);

	if(data->renderTarget)
		data->renderTarget->applyColor(0);

	//data->device.SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
	//data->device.SetRenderState(D3DRS_ALPHAREF, 0x50);
	//data->device.SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);

	data->device.SetPixelShader(0);
	data->device.SetVertexShader(0);
    data->storm.renderer.setFVF(FVF_PT4UV);

    data->storm.renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer, sizeof(float) *  6);

	//data->device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
}

void Storm3D_FakeSpotlight::releaseDynamicResources()
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	data->renderTarget.reset();
}

void Storm3D_FakeSpotlight::recreateDynamicResources()
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	data->renderTarget = renderTargets.getTarget();
}

void Storm3D_FakeSpotlight::querySizes(Storm3D &storm, int shadowQuality)
{
	if(shadowQuality >= 100)
	{
		BUFFER_WIDTH = 1024;
		BUFFER_HEIGHT = 1024;
	}
	else if(shadowQuality >= 75)
	{
		BUFFER_WIDTH = 768;
		BUFFER_HEIGHT = 768;
	}
	else if(shadowQuality >= 50)
	{
		BUFFER_WIDTH = 512;
		BUFFER_HEIGHT = 512;
	}
	else if(shadowQuality >= 25)
	{
		BUFFER_WIDTH = 384;
		BUFFER_HEIGHT = 384;
	}
	else if(shadowQuality >= 0)
	{
		BUFFER_WIDTH = 256;
		BUFFER_HEIGHT = 256;
	}
	else if(shadowQuality == -1)
	{
		BUFFER_WIDTH = -1;
		BUFFER_HEIGHT = -1;
	}

	if(BUFFER_WIDTH > 0 && BUFFER_HEIGHT > 0)
	{
		storm.setNeededColorTarget(VC2I(BUFFER_WIDTH * 2, BUFFER_HEIGHT * 2));
		storm.setNeededDepthTarget(VC2I(BUFFER_WIDTH * 2, BUFFER_HEIGHT * 2));
	}
}

void Storm3D_FakeSpotlight::createBuffers(Storm3D &storm, gfx::Device &device, int shadowQuality)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

	bool status = renderTargets.createAll(storm, device, shadowQuality);
	while(!status && shadowQuality >= 0)
	{
		IStorm3D_Logger *logger = storm.getLogger();
		if(logger)
			logger->warning("Failed creating fakelight's shadow rendertargets - trying again using lower resolution");

		shadowQuality -= 50;
		querySizes(storm, shadowQuality);
		status = renderTargets.createAll(storm, device, shadowQuality);
	}

	IStorm3D_Logger *logger = storm.getLogger();
	if(logger && !renderTargets.targets[0]->hasInitialized())
		logger->warning("Failed creating fakelight's shadow rendertargets - feature disabled!");

    gfx::IndexStorage16& storage = storm.renderer.getIndexStorage16();
    {
        etlsf_alloc_t allocId = storage.alloc(4 * 3);
        Storm3D_FakeSpotlight::Data::indicesAllocId = allocId;
        Storm3D_FakeSpotlight::Data::baseIndex = storage.baseIndex(allocId);

        uint16_t* indices = storage.lock(allocId);

        *indices++ = 4;
        *indices++ = 0;
        *indices++ = 1;

        *indices++ = 2;
        *indices++ = 0;
        *indices++ = 4;

        *indices++ = 4;
        *indices++ = 3;
        *indices++ = 2;

        *indices++ = 3;
        *indices++ = 4;
        *indices++ = 1;

        storage.unlock();
    }
}

void Storm3D_FakeSpotlight::freeBuffers(Storm3D &storm)
{
	if(BUFFER_WIDTH <= 0 || BUFFER_HEIGHT <= 0)
		return;

    storm.renderer.getIndexStorage16().free(Storm3D_FakeSpotlight::Data::indicesAllocId);
    Storm3D_FakeSpotlight::Data::indicesAllocId = ETLSF_INVALID_ID;
    Storm3D_FakeSpotlight::Data::baseIndex = 0;

	renderTargets.freeAll();
}

void Storm3D_FakeSpotlight::clearCache()
{
	fakeTargetActive = false;
}
