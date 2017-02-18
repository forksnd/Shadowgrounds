// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <vector>
#include <string>

#include "storm3d_spotlight.h"
#include "storm3d_spotlight_shared.h"
#include "storm3d_terrain_utils.h"
#include "storm3d_texture.h"
#include "Storm3D_ShaderManager.h"
#include "storm3d_adapter.h"
#include "storm3d_camera.h"
#include "storm3d.h"
#include <IStorm3D_Logger.h>
#include "c2_sphere.h"
#include <atlbase.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <math.h>

#include "../../util/Debug_MemoryManager.h"

int SHADOW_WIDTH = 512;
int SHADOW_HEIGHT = 512;
static const bool SHARE_BUFFERS = true;

static const int CONE_CIRCLE_VERTICES = 100;
static const int CONE_BASE_VERTICES = CONE_CIRCLE_VERTICES;
static const int CONE_VERTICES = CONE_CIRCLE_VERTICES + CONE_BASE_VERTICES + 1;
static const int CONE_FACES = (CONE_CIRCLE_VERTICES);

static bool spotTargetActive = false;

namespace {
	struct StaticBuffers;
}

	struct ShadowMap
	{
		gfx::Device &device;

		CComPtr<IDirect3DTexture9> color;
		CComPtr<IDirect3DTexture9> depthTexture;
		CComPtr<IDirect3DSurface9> depthSurface;

		VC2I pos;

		bool hasInitialized() const
		{
			return (depthTexture && color) || (depthSurface && color);
		}

	public:
		ShadowMap(gfx::Device &device_, CComPtr<IDirect3DTexture9> sharedColor, CComPtr<IDirect3DTexture9> sharedDepthTexture, CComPtr<IDirect3DSurface9> sharedDepth, const VC2I &pos_)
		:	device(device_),
			pos(pos_)
		{
			color = sharedColor;
			depthSurface = sharedDepth;
			depthTexture = sharedDepthTexture;

			device.EvictManagedResources();
			if(!color)
			{
				device.CreateTexture(SHADOW_WIDTH * 2, SHADOW_HEIGHT * 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &color, 0);
			}

			if(!depthTexture)
			{
				device.CreateTexture(SHADOW_WIDTH * 2, SHADOW_HEIGHT * 2, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24S8, D3DPOOL_DEFAULT, &depthTexture, 0);
			}

			if(!hasInitialized())
				reset();
		}

		bool set()
		{
			if(!hasInitialized())
				return false;

			if(!spotTargetActive || !SHARE_BUFFERS)
			{
				spotTargetActive = true;

				CComPtr<IDirect3DSurface9> colorSurface;
				color->GetSurfaceLevel(0, &colorSurface);

				CComPtr<IDirect3DSurface9> surface;
				depthTexture->GetSurfaceLevel(0, &surface);

				HRESULT hr = device.SetRenderTarget(0, colorSurface);
				if(FAILED(hr))
					return false;

				hr = device.SetDepthStencilSurface(surface);
				if(FAILED(hr))
					return false;

				device.Clear(0, 0, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0);
			}

			D3DVIEWPORT9 vp;
			vp.X = pos.x * SHADOW_WIDTH;
			vp.Y = pos.y * SHADOW_HEIGHT;
			vp.Width = SHADOW_WIDTH;
			vp.Height = SHADOW_HEIGHT;
			vp.MinZ = 0.f;
			vp.MaxZ = 1.f;
			device.SetViewport(&vp);

			// !!!!!
			//device.Clear(0, 0, D3DCLEAR_TARGET, 0xFFFFFFFF, 1.0f, 0);

			return true;
		}

		void reset()
		{
			color.Release();
			depthTexture.Release();
			depthSurface.Release();
		}

		void apply(int stage)
		{
    		device.SetTexture(stage, depthTexture);
		}

		void applyColor(int stage)
		{
			device.SetTexture(stage, color);
		}

		friend struct StaticBuffers;
	};

namespace {

	struct StaticBuffers
	{
		enum { SHADOW_BUFFER_LIMIT = 4 };

		CComPtr<IDirect3DTexture9> sharedColor;
		CComPtr<IDirect3DSurface9> sharedDepth;
		CComPtr<IDirect3DTexture9> sharedDepthTexture;

		ShadowMap *buffers[SHADOW_BUFFER_LIMIT];
		bool used[SHADOW_BUFFER_LIMIT];

		struct BufferDeleter
		{
			StaticBuffers &manager;

			BufferDeleter(StaticBuffers &manager_)
			:  manager(manager_)
			{
			}

			void operator() (ShadowMap *map)
			{
				if(map)
					manager.freeShadowMap(map);
			}
		};

	public:
		StaticBuffers()
		{
			for(unsigned int i = 0; i < SHADOW_BUFFER_LIMIT; ++i)
			{
				buffers[i] = 0;
				used[i] = false;
			}
		}

		~StaticBuffers()
		{
			freeAll();
		}

		bool createAll(Storm3D &storm, gfx::Device &device, int quality)
		{
			freeAll();

			for(unsigned int i = 0; i < SHADOW_BUFFER_LIMIT; ++i)
			{
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

				buffers[i] = new ShadowMap(device, sharedColor, sharedDepthTexture, sharedDepth, pos);
				if(SHARE_BUFFERS)
				{
					if(i == 0)
					{
						sharedColor = buffers[i]->color;
						sharedDepth = buffers[i]->depthSurface;
						sharedDepthTexture = buffers[i]->depthTexture;
					}
					else
					{
						assert(buffers[0]->color == buffers[i]->color);
						assert(buffers[0]->depthSurface == buffers[i]->depthSurface);
						assert(buffers[0]->depthTexture == buffers[i]->depthTexture);
					}
				}
			}

			if(!sharedColor && SHARE_BUFFERS)
				return false;

			return true;
		}

		void freeAll()
		{
			if(sharedColor)
				sharedColor.Release();
			if(sharedDepth)
				sharedDepth.Release();
			if(sharedDepthTexture)
				sharedDepthTexture.Release();

			for(unsigned int i = 0; i < SHADOW_BUFFER_LIMIT; ++i)
			{
				delete buffers[i];
				buffers[i] = 0;
				used[i] = false;
			}
		}

		std::shared_ptr<ShadowMap> getShadowMap()
		{
			for(unsigned int i = 0; i < SHADOW_BUFFER_LIMIT; ++i)
			{
				if(!used[i])// && buffers[i]->hasInitialized())
				{
					used[i] = true;
					return std::shared_ptr<ShadowMap>(buffers[i], BufferDeleter(*this));
				}
			}

			return std::shared_ptr<ShadowMap>();
		}

		void freeShadowMap(const ShadowMap *map)
		{
			for(unsigned int i = 0; i < SHADOW_BUFFER_LIMIT; ++i)
			{
				if(buffers[i] == map)
				{
					assert(used[i]);
					used[i] = false;
				}
			}
		}
	};

	StaticBuffers staticBuffers;
} // unnamed

struct Storm3D_SpotlightData
{
	Storm3D &storm;
	gfx::Device &device;

	std::shared_ptr<ShadowMap> shadowMap;
	std::shared_ptr<Storm3D_Texture> projectionTexture;
	std::shared_ptr<Storm3D_Texture> coneTexture;

    //TODO: create proper rt sharing solution at higher level
	Storm3D_SpotlightShared properties;

	float coneColorMultiplier;
	float smoothness;

	bool smoothing;
	Storm3D_Camera camera;

	bool hasFade;
	bool hasCone;
	bool hasShadows;
    bool shadowActive;
	bool enabled;
	bool visible;
	bool coneUpdated;

	frozenbyte::storm::VertexBuffer coneVertexBuffer;
	frozenbyte::storm::IndexBuffer coneIndexBuffer;
	bool updateCone;
	bool scissorRect;

	frozenbyte::storm::VertexBuffer coneStencilVertexBuffer;
	frozenbyte::storm::IndexBuffer coneStencilIndexBuffer;

	const IStorm3D_Model *noShadowModel;
	IStorm3D_Spotlight::Type light_type;

	float coneFov;
	float angle[2];
	float speed[2];

	bool spotlightAlwaysVisible;

	static frozenbyte::storm::PixelShader *nvShadowPixelShader;
	static frozenbyte::storm::PixelShader *nvSmoothShadowPixelShader;
	static frozenbyte::storm::PixelShader *nvNoShadowPixelShader;
	static frozenbyte::storm::PixelShader *coneNvPixelShader_Texture;
	static frozenbyte::storm::PixelShader *coneNvPixelShader_NoTexture;
	static frozenbyte::storm::VertexShader *coneStencilVertexShader;

	Storm3D_SpotlightData(Storm3D &storm_, gfx::Device &device_)
	:	storm(storm_),
		device(device_),
		properties(device),
		coneColorMultiplier(0.3f),
		smoothness(5.f),
		smoothing(false),
		camera(&storm),
		hasFade(false),
		hasCone(false),
		hasShadows(false),
        shadowActive(false),
		enabled(true),
		visible(false),
		coneUpdated(false),

		updateCone(true),
		scissorRect(true),
		noShadowModel(0),
		light_type(IStorm3D_Spotlight::Directional),
		coneFov(0),
		spotlightAlwaysVisible(false)
	{
		properties.direction = VC3(0, 0, 1.f);
		properties.color = COL(1.f, 1.f, 1.f);
		shadowMap = staticBuffers.getShadowMap();

		angle[0] = angle[1] = 0.f;
		speed[0] = 0.07f;
		speed[1] = -0.11f;
	}

	float getBias() const
	{
		return 0.9989f;
	}

	void updateMatrices(const float *cameraView)
	{
		float bias = getBias();
		if(shadowMap)
			properties.targetPos = shadowMap->pos;

		properties.resolutionX = 2 * SHADOW_WIDTH;
		properties.resolutionY = 2 * SHADOW_HEIGHT;
		properties.updateMatrices(cameraView, bias);

		camera.SetPosition(properties.position);
		camera.SetTarget(properties.position + properties.direction);
		camera.SetFieldOfView(D3DXToRadian(properties.fov));
		camera.SetVisibilityRange(properties.range);
	}

	bool setScissorRect(Storm3D_Camera &camera, const VC2I &screenSize, Storm3D_Scene &scene)
	{
		/*
		if(!scissorRect)
		{
			RECT rc;
			rc.left = 0;
			rc.top = 0;
			rc.right = screenSize.x;
			rc.bottom = screenSize.y;

			device.SetScissorRect(&rc);
			device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
			return true;
		}
		*/

		return properties.setScissorRect(camera, screenSize, &scene);
	}

	void setClipPlanes(const float *cameraView)
	{
		properties.setClipPlanes(cameraView);
	}

	void createCone()
	{
		if(!hasCone || !updateCone)
			return;

		// Vertices
		{
			// Uh oh. Undefined casting etc, should clean up sometime
			// -- psd
			struct VertexType
			{
				float position[3];
				float normal[3];
				unsigned int color;
				float uv[2];

				void setPosition(float x, float y, float z)
				{
					position[0] = x;
					position[1] = y;
					position[2] = z;
				}

				void setNormal(float x, float y, float z)
				{
					normal[0] = x;
					normal[1] = y;
					normal[2] = z;
				}

				void setColor(unsigned int color_)
				{
					color = color_;
				}

				void setUv(float u, float v)
				{
					uv[0] = u;
					uv[1] = v;
				}
			};

			//static const unsigned char color = 96;
			static const unsigned char color = 255;
			coneVertexBuffer.create(device, CONE_VERTICES, 3 * sizeof(float) + 3 * sizeof(float) + sizeof(unsigned int) + 2 * sizeof(float), false);
			VertexType *buffer = reinterpret_cast<VertexType *> (coneVertexBuffer.lock());

			for(int i = 0; i < CONE_BASE_VERTICES; ++i)
			{
				float angle = (float(i) / (CONE_BASE_VERTICES)) * PI * 2.f;

				float nx = sinf(angle);
				float ny = cosf(angle);
				float nz = 0;
				float u = .5f;
				float v = .5f;

				buffer->setPosition(0, 0, 0);
				buffer->setNormal(nx, ny, nz);
				buffer->setColor(D3DCOLOR_RGBA(color, color, color, color));
				buffer->setUv(u, v);
				++buffer;
			}

			float coneRange = properties.range * .5f;
			float farMul = tanf(D3DXToRadian(coneFov / 2)) * coneRange * .9f;

			// Create circle
			for(int i = 0; i < CONE_CIRCLE_VERTICES; ++i)
			{
				float angle = (float(i) / (CONE_CIRCLE_VERTICES)) * PI * 2.f;
				float x = sinf(angle);
				float y = cosf(angle);
				float z = coneRange;

				float nx = x;
				float ny = y;
				float nz = 0;

				float u = (x * .5f) + .5f;
				float v = (y * .5f) + .5f;

				buffer->setPosition(x * farMul, y * farMul, z);
				buffer->setNormal(nx, ny, nz);
				buffer->setColor(D3DCOLOR_RGBA(color, color, color, color));
				buffer->setUv(u, v);
				++buffer;
			}

			coneVertexBuffer.unlock();
		}

		// Faces
		if(!coneIndexBuffer)
		{
			coneIndexBuffer.create(device, CONE_FACES, false);
			unsigned short *indexBuffer = coneIndexBuffer.lock();

			for(int i = 0; i < CONE_CIRCLE_VERTICES; ++i)
			{
				int base = CONE_BASE_VERTICES;
				int last = i - 1;
				if(i == 0)
					last = CONE_CIRCLE_VERTICES - 1;

				*indexBuffer++ = i + base;
				*indexBuffer++ = i;
				*indexBuffer++ = last + base;
			}

			coneIndexBuffer.unlock();
		}
	}

	void createStencilCone()
	{
		// Vertices
		{
			// Uh oh. Undefined casting etc, should clean up sometime
			// -- psd
			struct VertexType
			{
				float position[3];

				void setPosition(float x, float y, float z)
				{
					position[0] = x;
					position[1] = y;
					position[2] = z;
				}
			};

			coneStencilVertexBuffer.create(device, CONE_VERTICES, 3 * sizeof(float), false);
			VertexType *buffer = reinterpret_cast<VertexType *> (coneStencilVertexBuffer.lock());

			for(int i = 0; i < CONE_BASE_VERTICES; ++i)
			{
				buffer->setPosition(0, 0, 0.1f);
				++buffer;
			}

			float coneRange = properties.range;
			float farMul = tanf(D3DXToRadian(properties.fov / 2)) * coneRange * .9f;

			// Create circle
			for(int i = 0; i < CONE_CIRCLE_VERTICES; ++i)
			{
				float angle = (float(i) / (CONE_CIRCLE_VERTICES)) * PI * 2.f;
				float x = sinf(angle);
				float y = cosf(angle);
				float z = coneRange;

				buffer->setPosition(x * farMul, y * farMul, z);
				++buffer;
			}

			coneStencilVertexBuffer.unlock();
		}

		// Faces
		{
			coneStencilIndexBuffer.create(device, CONE_FACES, false);
			unsigned short *indexBuffer = coneStencilIndexBuffer.lock();

			for(int i = 0; i < CONE_CIRCLE_VERTICES; ++i)
			{
				int base = CONE_BASE_VERTICES;
				int last = i - 1;
				if(i == 0)
					last = CONE_CIRCLE_VERTICES - 1;

				*indexBuffer++ = i + base;
				*indexBuffer++ = i;
				*indexBuffer++ = last + base;
			}

			coneStencilIndexBuffer.unlock();
		}
	}

	void updateBuffer()
	{
		if(!hasShadows || !enabled)
			shadowMap.reset();

		if(!shadowMap)
			shadowMap = staticBuffers.getShadowMap();
	}
};

frozenbyte::storm::PixelShader *Storm3D_SpotlightData::nvShadowPixelShader = 0;
frozenbyte::storm::PixelShader *Storm3D_SpotlightData::nvSmoothShadowPixelShader = 0;
frozenbyte::storm::PixelShader *Storm3D_SpotlightData::nvNoShadowPixelShader = 0;
frozenbyte::storm::PixelShader *Storm3D_SpotlightData::coneNvPixelShader_Texture = 0;
frozenbyte::storm::PixelShader *Storm3D_SpotlightData::coneNvPixelShader_NoTexture = 0;
frozenbyte::storm::VertexShader *Storm3D_SpotlightData::coneStencilVertexShader = 0;

Storm3D_Spotlight::Storm3D_Spotlight(Storm3D &storm, gfx::Device &device)
{
	data = new Storm3D_SpotlightData(storm, device);
}

Storm3D_Spotlight::~Storm3D_Spotlight()
{
    assert(data);
    delete data;
}

void Storm3D_Spotlight::testVisibility(Storm3D_Camera &camera)
{
	// Simple hack, skip visibility check if camera has orthogonal projection on (practically when mapview is enabled)
	if(!(data->spotlightAlwaysVisible || camera.GetForcedOrthogonalProjectionEnabled() ) )
	{
		data->visible = camera.TestSphereVisibility(data->properties.position, data->properties.range);
		data->coneUpdated = false;
	}
	else
	{
		data->visible = true;
		data->coneUpdated = false;
	}
}

void Storm3D_Spotlight::enable(bool enable)
{
	data->enabled = enable;
	data->updateBuffer();
}

bool Storm3D_Spotlight::enabled() const
{
	if(!data->enabled || !data->visible)
		return false;

	return true;
}

void Storm3D_Spotlight::setFov(float fov)
{
	data->properties.fov = fov;
	data->updateCone = true;
	data->coneFov = fov;
}

void Storm3D_Spotlight::setConeFov(float fov)
{
	data->coneFov = fov;
}

void Storm3D_Spotlight::setRange(float range)
{
	data->properties.range = range;
	//data->clipRange = range;
	data->updateCone = true;
}

void Storm3D_Spotlight::setClipRange(float range)
{
	//data->clipRange = range;
}

void Storm3D_Spotlight::setPosition(const VC3 &position)
{
	data->properties.position = position;
}

void Storm3D_Spotlight::setDirection(const VC3 &direction)
{
	data->properties.direction = direction;
}

void Storm3D_Spotlight::enableFeature(IStorm3D_Spotlight::Feature feature, bool enable)
{
	if(feature == IStorm3D_Spotlight::Fade)
		data->hasFade = enable;
	else if(feature == IStorm3D_Spotlight::ConeVisualization)
	{
		data->hasCone = enable;
		//data->createCone();
	}
	else if(feature == IStorm3D_Spotlight::Shadows)
	{
		data->hasShadows = enable;
		data->updateBuffer();
	}
	else if(feature == IStorm3D_Spotlight::ScissorRect)
	{
		data->scissorRect = enable;
	}
	else
	{
		assert(!"whoops");
	}
}

bool Storm3D_Spotlight::featureEnabled(Feature feature) const
{
	if(feature == IStorm3D_Spotlight::Fade)
		return data->hasFade;
	else if(feature == IStorm3D_Spotlight::ConeVisualization)
	{
		if(!featureEnabled(IStorm3D_Spotlight::Shadows))
			return false;

		return data->hasCone;
	}
	else if(feature == IStorm3D_Spotlight::Shadows)
	{
		if(!data->shadowMap || !data->shadowMap->hasInitialized())
			return false;

		return data->hasShadows;
	}
	else
	{
		assert(!"whoops");
		return false;
	}
}

void Storm3D_Spotlight::setType(Type type)
{
	data->light_type = type;
}

IStorm3D_Spotlight::Type Storm3D_Spotlight::getType() const
{
	return data->light_type;
}

void Storm3D_Spotlight::setNoShadowModel(const IStorm3D_Model *model)
{
	data->noShadowModel = model;
}

const IStorm3D_Model *Storm3D_Spotlight::getNoShadowModel() const
{
	return data->noShadowModel;
}

void Storm3D_Spotlight::setClipPlanes(const float *cameraView)
{
	data->setClipPlanes(cameraView);
}

void Storm3D_Spotlight::enableSmoothing(bool enable)
{
	data->smoothing = enable;
}

bool Storm3D_Spotlight::setScissorRect(Storm3D_Camera &camera, const VC2I &screenSize, Storm3D_Scene &scene)
{
	return data->setScissorRect(camera, screenSize, scene);
}

void Storm3D_Spotlight::setProjectionTexture(std::shared_ptr<IStorm3D_Texture> texture)
{
	data->projectionTexture = std::static_pointer_cast<Storm3D_Texture> (texture);
}

void Storm3D_Spotlight::setConeTexture(std::shared_ptr<IStorm3D_Texture> texture)
{
	data->coneTexture = std::static_pointer_cast<Storm3D_Texture> (texture);
}

bool Storm3D_Spotlight::hasConeTexture() const
{
	return (bool)data->coneTexture;
}

void Storm3D_Spotlight::setColorMultiplier(const COL &color)
{
	data->properties.color = color;
}

void Storm3D_Spotlight::setConeMultiplier(float scalar)
{
	data->coneColorMultiplier = scalar;
}

void Storm3D_Spotlight::setSmoothness(float smoothness)
{
	data->smoothness = smoothness;
}

const COL &Storm3D_Spotlight::getColorMultiplier() const
{
	return data->properties.color;
}

QUAT Storm3D_Spotlight::getOrientation() const
{
	D3DXVECTOR3 lightPosition(data->properties.position.x, data->properties.position.y, data->properties.position.z);
	D3DXVECTOR3 up(0, 1.f, 0);
	D3DXVECTOR3 lookAt = lightPosition;
	lookAt += D3DXVECTOR3(data->properties.direction.x, data->properties.direction.y, data->properties.direction.z);

	D3DXMATRIX tm;
	D3DXMatrixLookAtLH(&tm, &lightPosition, &lookAt, &up);

	float det = D3DXMatrixDeterminant(&tm);
	D3DXMatrixInverse(&tm, &det, &tm);

	MAT m;

	for(int j = 0; j < 4; ++j)
	for(int i = 0; i < 4; ++i)
		m.Set(j*4 + i, tm[j*4 + i]);

	return m.GetRotation();
}

bool Storm3D_Spotlight::setAsRenderTarget(const float *cameraView)
{
    gfx::Renderer& renderer = data->storm.renderer;
    gfx::Device& device = renderer.device;
    gfx::ProgramManager& programManager = renderer.programManager;

    data->updateMatrices(cameraView);

    if (data->shadowMap && data->shadowMap->hasInitialized())
    {
        if (data->shadowMap->set())
        {
            D3DXMATRIX dm;
            D3DXMatrixIdentity(&dm);

            //TODO: remove
            data->device.SetTransform(D3DTS_WORLD, &dm);
            data->device.SetTransform(D3DTS_VIEW, &data->properties.lightView[1]);
            data->device.SetTransform(D3DTS_PROJECTION, &data->properties.lightProjection);

            programManager.setViewMatrix(dm);
            programManager.setProjectionMatrix(data->properties.lightViewProjection[1]); //HACK: combined view projection
            
            Storm3D_ShaderManager::GetSingleton()->SetViewProjectionMatrix(data->properties.lightViewProjection[1], data->properties.lightViewProjection[1]);
            Storm3D_ShaderManager::GetSingleton()->setSpot(data->properties.color, data->properties.position, data->properties.direction, data->properties.range, .1f);

            return true;
        }

        IStorm3D_Logger *logger = data->storm.logger;
        if (logger)
        {
            logger->error("Failed to set render target");
        }
    }

    return false;
}

Storm3D_Camera &Storm3D_Spotlight::getCamera()
{
	//data->camera.SetPosition(data->properties.position);
	//data->camera.SetTarget(data->properties.position + data->properties.direction);
	//data->camera.SetFieldOfView(D3DXToRadian(data->properties.fov));
	//data->camera.SetVisibilityRange(data->properties.range);

	return data->camera;
}

void Storm3D_Spotlight::renderStencilCone(Storm3D_Camera &camera)
{
	data->createStencilCone();

	D3DXVECTOR3 lightPosition(data->properties.position.x, data->properties.position.y, data->properties.position.z);
	D3DXVECTOR3 up(0, 1.f, 0);
	D3DXVECTOR3 lookAt = lightPosition;
	lookAt += D3DXVECTOR3(data->properties.direction.x, data->properties.direction.y, data->properties.direction.z);

	D3DXMATRIX tm;
	D3DXMatrixLookAtLH(&tm, &lightPosition, &lookAt, &up);

	/*
	VC3 cameraDir = camera.GetDirection();
	cameraDir.Normalize();
	D3DXVECTOR3 direction(cameraDir.x, cameraDir.y, cameraDir.z);
	D3DXVec3TransformNormal(&direction, &direction, &tm);
	*/

	float det = D3DXMatrixDeterminant(&tm);
	D3DXMatrixInverse(&tm, &det, &tm);
	Storm3D_ShaderManager::GetSingleton()->SetWorldTransform(data->device, tm, true);

	data->coneStencilVertexShader->apply();
	data->coneStencilVertexBuffer.apply(data->device, 0);
	data->coneStencilIndexBuffer.render(data->device, CONE_FACES, CONE_VERTICES);
}


void Storm3D_Spotlight::applyTextures(const float *cameraView, const float *cameraViewProjection, Storm3D &storm, bool renderShadows)
{
    gfx::ProgramManager& programManager = data->storm.renderer.programManager;

    data->updateMatrices(cameraView);

	int projection = 0;
	int shadow = 1;

	if(data->projectionTexture)
	{
		data->projectionTexture->AnimateVideo();
		data->projectionTexture->Apply(projection);
	}

	if(renderShadows && data->shadowMap && data->shadowMap->hasInitialized())
		data->shadowMap->apply(shadow);

    data->shadowActive = data->hasShadows && renderShadows && data->shadowMap && data->shadowMap->hasInitialized();

	Storm3D_ShaderManager::GetSingleton()->setSpot(data->properties.color, data->properties.position, data->properties.direction, data->properties.range, .1f);
	if(shadow == 1)
	{
		Storm3D_ShaderManager::GetSingleton()->setTextureTm(data->properties.shaderProjection[0]);
		Storm3D_ShaderManager::GetSingleton()->setSpotTarget(data->properties.targetProjection);
	}
	else
	{
		Storm3D_ShaderManager::GetSingleton()->setTextureTm(data->properties.targetProjection);
		Storm3D_ShaderManager::GetSingleton()->setSpotTarget(data->properties.shaderProjection[0]);
	}

    D3DXMATRIX matTerrainProjectedTexture;
    D3DXMATRIX matTerrainShadowTexture;
    D3DXMatrixTranspose(&matTerrainProjectedTexture, &data->properties.shaderProjection[0]);
    D3DXMatrixTranspose(&matTerrainShadowTexture, &data->properties.targetProjection);
    programManager.setTextureMatrix(0, matTerrainProjectedTexture);
    programManager.setTextureMatrix(1, matTerrainShadowTexture);

    if (data->light_type == Directional)
    {
        programManager.setPointLight(-data->properties.direction, data->properties.range);
    }
    else if (data->light_type == Point)
    {
        programManager.setPointLight(data->properties.position, data->properties.range);
    }

	if(data->light_type == Directional)
		Storm3D_ShaderManager::GetSingleton()->setSpotType(Storm3D_ShaderManager::Directional);
	if(data->light_type == Point)
		Storm3D_ShaderManager::GetSingleton()->setSpotType(Storm3D_ShaderManager::Point);
	if(data->light_type == Flat)
		Storm3D_ShaderManager::GetSingleton()->setSpotType(Storm3D_ShaderManager::Flat);

	frozenbyte::storm::enableMinMagFiltering(data->device, shadow, shadow, true);
	frozenbyte::storm::enableMinMagFiltering(data->device, projection, projection, true);
}

void Storm3D_Spotlight::applyTerrainShader(bool renderShadows)
{
    gfx::ProgramManager& programManager = data->storm.renderer.programManager;

    bool hasShadow = data->hasShadows && renderShadows && data->shadowMap && data->shadowMap->hasInitialized();

    switch (data->light_type)
    {
        case Directional:
            programManager.setProgram(
                hasShadow
                    ? gfx::ProgramManager::TERRAIN_PROJECTION_DIRECT_SHADOW
                    : gfx::ProgramManager::TERRAIN_PROJECTION_DIRECT_NOSHADOW
            );
            break;
        case Point:
            programManager.setProgram(
                hasShadow
                ? gfx::ProgramManager::TERRAIN_PROJECTION_POINT_SHADOW
                : gfx::ProgramManager::TERRAIN_PROJECTION_POINT_NOSHADOW
            );
            break;
        case Flat:
            programManager.setProgram(
                hasShadow
                ? gfx::ProgramManager::TERRAIN_PROJECTION_FLAT_SHADOW
                : gfx::ProgramManager::TERRAIN_PROJECTION_FLAT_NOSHADOW
            );
            break;
        default:
            assert(0);
    }

    programManager.setDiffuse(data->properties.color);
}

void Storm3D_Spotlight::applySolidShader(bool renderShadows)
{
}

void Storm3D_Spotlight::applyNormalShader(bool renderShadows)
{
	if (data->shadowActive)
	{
		if (data->smoothing)
			data->nvSmoothShadowPixelShader->apply();
		else
			data->nvShadowPixelShader->apply();
	}
	else
		data->nvNoShadowPixelShader->apply();
    data->device.SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
}

void Storm3D_Spotlight::renderCone(Storm3D_Camera &camera, float timeFactor, bool renderGlows)
{
	if(!data->hasCone || !data->hasShadows || !data->shadowMap)
		return;

	bool normalPass = !data->coneUpdated;
	if(data->hasCone && data->updateCone && !data->coneUpdated)
	{
		data->createCone();
		data->coneUpdated = true;
	}

	D3DXVECTOR3 lightPosition(data->properties.position.x, data->properties.position.y, data->properties.position.z);
	D3DXVECTOR3 up(0, 1.f, 0);
	D3DXVECTOR3 lookAt = lightPosition;
	lookAt += D3DXVECTOR3(data->properties.direction.x, data->properties.direction.y, data->properties.direction.z);

	D3DXMATRIX tm;
	D3DXMatrixLookAtLH(&tm, &lightPosition, &lookAt, &up);

	VC3 cameraDir = camera.GetDirection();
	cameraDir.Normalize();
	D3DXVECTOR3 direction(cameraDir.x, cameraDir.y, cameraDir.z);
	D3DXVec3TransformNormal(&direction, &direction, &tm);

	Storm3D_ShaderManager::GetSingleton()->setSpot(data->properties.color, data->properties.position, data->properties.direction, data->properties.range, .1f);
	Storm3D_ShaderManager::GetSingleton()->setTextureTm(data->properties.shaderProjection[0]);
	Storm3D_ShaderManager::GetSingleton()->setSpotTarget(data->properties.targetProjection);

	float det = D3DXMatrixDeterminant(&tm);
	D3DXMatrixInverse(&tm, &det, &tm);
	Storm3D_ShaderManager::GetSingleton()->SetWorldTransform(data->device, tm, true);

	if(data->shadowMap && data->shadowMap->hasInitialized())
		data->shadowMap->apply(0);
	if(data->coneTexture)
	{
		data->coneTexture->AnimateVideo();
		data->coneTexture->Apply(3);

		data->coneTexture->Apply(1);
	}

	if(data->coneTexture)
		data->coneNvPixelShader_Texture->apply();
	else
		data->coneNvPixelShader_NoTexture->apply();

	float colorMul = data->coneColorMultiplier;
	float colorData[4] = { data->properties.color.r * colorMul, data->properties.color.g * colorMul, data->properties.color.b * colorMul, 1.f };
	if(renderGlows)
	{
		if(normalPass)
		{
			colorData[0] *= 0.2f;
			colorData[1] *= 0.2f;
			colorData[2] *= 0.2f;
			colorData[3] *= 0.2f;
		}
		else
		{
			colorData[0] *= 0.4f;
			colorData[1] *= 0.4f;
			colorData[2] *= 0.4f;
			colorData[3] *= 0.4f;
		}
	}

	data->device.SetVertexShaderConstantF(9, colorData, 1);

	float bias = 0.005f;
	float directionData[4] = { -direction.x, -direction.y, -direction.z, bias };
	data->device.SetVertexShaderConstantF(10, directionData, 1);

	for(int i = 0; i < 2; ++i)
	{
		data->angle[i] += data->speed[i] * timeFactor;
		D3DXVECTOR3 center(0.5f, 0.5f, 0.f);
		D3DXQUATERNION quat1;
		D3DXQuaternionRotationYawPitchRoll(&quat1, 0, 0, data->angle[i]);
		D3DXMATRIX rot1;
		D3DXMatrixAffineTransformation(&rot1, 1.f, &center, &quat1, 0);
		D3DXMatrixTranspose(&rot1, &rot1);
		
		if(i == 0)
			data->device.SetVertexShaderConstantF(16, rot1, 3);
		else
			data->device.SetVertexShaderConstantF(19, rot1, 3);
	}

	frozenbyte::storm::enableMipFiltering(data->device, 0, 0, false);
	data->coneVertexBuffer.apply(data->device, 0);
	data->coneIndexBuffer.render(data->device, CONE_FACES, CONE_VERTICES);
	frozenbyte::storm::enableMipFiltering(data->device, 0, 0, true);
}

void Storm3D_Spotlight::debugRender()
{
	float buffer[] = 
	{
		0.f, 300.f, 0.f, 1.f, 0.f, 1.f,
		0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
		300.f, 300.f, 0.f, 1.f, 1.f, 1.f,
		300.f, 0.f, 0.f, 1.f, 1.f, 0.f
	};

	data->device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	data->device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	data->device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	data->device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	data->device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	if(data->shadowMap)
		data->shadowMap->applyColor(0);

	data->device.SetPixelShader(0);
	data->device.SetVertexShader(0);
    data->storm.renderer.setFVF(FVF_PT4UV);

	data->storm.renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer, sizeof(float) *  6);
}

void Storm3D_Spotlight::releaseDynamicResources()
{
	data->shadowMap.reset();
}

void Storm3D_Spotlight::recreateDynamicResources()
{
	data->updateBuffer();
}

void Storm3D_Spotlight::querySizes(Storm3D &storm, int shadowQuality)
{
	if(shadowQuality >= 100)
	{
		SHADOW_WIDTH = 1024;
		SHADOW_HEIGHT = 1024;
	}
	else if(shadowQuality >= 75)
	{
		SHADOW_WIDTH = 1024;
		SHADOW_HEIGHT = 512;
	}
	else if(shadowQuality >= 50)
	{
		SHADOW_WIDTH = 512;
		SHADOW_HEIGHT = 512;
	}
	else if(shadowQuality >= 25)
	{
		SHADOW_WIDTH = 512;
		SHADOW_HEIGHT = 256;
	}
	else
	{
		SHADOW_WIDTH = 256;
		SHADOW_HEIGHT = 256;
	}
}

void Storm3D_Spotlight::createShadowBuffers(Storm3D &storm, gfx::Device &device, int shadowQuality)
{
	bool status = staticBuffers.createAll(storm, device, shadowQuality);
	while(!status && shadowQuality > 0)
	{
		IStorm3D_Logger *logger = storm.getLogger();
		if(logger)
			logger->warning("Failed creating light's shadow depth rendertargets - trying again using lower resolution");

		shadowQuality -= 50;
		querySizes(storm, shadowQuality);
		status = staticBuffers.createAll(storm, device, shadowQuality);
	}

	IStorm3D_Logger *logger = storm.getLogger();
	if(logger && !staticBuffers.buffers[0]->hasInitialized())
		logger->warning("Failed creating light's shadow depth rendertargets - shadows disabled!");


	Storm3D_SpotlightData::nvSmoothShadowPixelShader = new frozenbyte::storm::PixelShader(device);
	Storm3D_SpotlightData::nvSmoothShadowPixelShader->createNvSmoothShadowShader();

	Storm3D_SpotlightData::nvShadowPixelShader = new frozenbyte::storm::PixelShader(device);
	Storm3D_SpotlightData::nvShadowPixelShader->createNvShadowShader();
	Storm3D_SpotlightData::nvNoShadowPixelShader = new frozenbyte::storm::PixelShader(device);
	Storm3D_SpotlightData::nvNoShadowPixelShader->createNvNoShadowShader();

	Storm3D_SpotlightData::coneNvPixelShader_Texture = new frozenbyte::storm::PixelShader(device);
	Storm3D_SpotlightData::coneNvPixelShader_Texture->createNvConeShader_Texture();
	Storm3D_SpotlightData::coneNvPixelShader_NoTexture = new frozenbyte::storm::PixelShader(device);
	Storm3D_SpotlightData::coneNvPixelShader_NoTexture->createNvConeShader_NoTexture();

	Storm3D_SpotlightData::coneStencilVertexShader = new frozenbyte::storm::VertexShader(device);
	Storm3D_SpotlightData::coneStencilVertexShader->createConeStencilShader();
}

void Storm3D_Spotlight::freeShadowBuffers()
{
	delete Storm3D_SpotlightData::nvShadowPixelShader; Storm3D_SpotlightData::nvShadowPixelShader = 0;
	delete Storm3D_SpotlightData::nvSmoothShadowPixelShader; Storm3D_SpotlightData::nvSmoothShadowPixelShader = 0;
	delete Storm3D_SpotlightData::nvNoShadowPixelShader; Storm3D_SpotlightData::nvNoShadowPixelShader = 0;

	delete Storm3D_SpotlightData::coneNvPixelShader_Texture; Storm3D_SpotlightData::coneNvPixelShader_Texture = 0;
	delete Storm3D_SpotlightData::coneNvPixelShader_NoTexture; Storm3D_SpotlightData::coneNvPixelShader_NoTexture = 0;

	delete Storm3D_SpotlightData::coneStencilVertexShader; Storm3D_SpotlightData::coneStencilVertexShader = 0;

	staticBuffers.freeAll();
}

void Storm3D_Spotlight::clearCache()
{
	spotTargetActive = false;
}

void Storm3D_Spotlight::setEnableClip(bool enableClip)
{
	data->spotlightAlwaysVisible = !enableClip;
}

bool Storm3D_Spotlight::hasShadow()
{
    return data->shadowActive;
}
