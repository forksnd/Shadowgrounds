// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <queue>
#include <vector>

#include "storm3d_terrain_renderer.h"
#include "storm3d_terrain_heightmap.h"
#include "storm3d_terrain_groups.h"
#include "storm3d_terrain_models.h"
#include "storm3d_terrain_decalsystem.h"
#include "storm3d_spotlight.h"
#include "storm3d_particle.h"
#include "storm3d_fakespotlight.h"
#include "storm3d_scene.h"
#include "storm3d_texture.h"
#include "storm3d.h"
#include "storm3d_terrain_utils.h"
#include "storm3d_adapter.h"
#include "Storm3D_ShaderManager.h"
#include "storm3d_model.h"
#include <IStorm3D_Logger.h>
#include "VertexFormats.h"

#include <d3d9.h>
#include <atlbase.h>
#include "../../util/Debug_MemoryManager.h"

namespace {

    struct Storm3D_LightTexture
    {
        VC2 start;
        VC2 end;

        std::shared_ptr<Storm3D_Texture> texture;
        COL color;

        Storm3D_LightTexture(const VC2 &start_, const VC2 &end_, IStorm3D_Texture &texture_, const COL &color_)
        :   start(start_),
            end(end_),
            color(color_)
        {
            texture = std::shared_ptr<Storm3D_Texture>(static_cast<Storm3D_Texture *> (&texture_), [](Storm3D_Texture* tex) {tex->Release();});
            texture->AddRef();
        }

        ~Storm3D_LightTexture(){}
    };

    typedef std::vector<std::shared_ptr<Storm3D_Spotlight> > SpotList;
    typedef std::vector<std::shared_ptr<Storm3D_FakeSpotlight> > FakeSpotList;
    typedef std::vector<Storm3D_LightTexture> FakeLightList;

    static const int MAX_SIZES = 2;
};

void toNearestPow(int &v);
extern int active_visibility;

static D3DVERTEXELEMENT9 VertexDesc_P3UV3[] = {
    {0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
    {0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0},
    {0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1},
    {0, 32, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2},
    D3DDECL_END()
};

static D3DVERTEXELEMENT9 VertexDesc_P3UV4[] = {
    {0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
    {0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0},
    {0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1},
    {0, 32, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2},
    {0, 40, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  3},
    D3DDECL_END()
};

static D3DVERTEXELEMENT9 VertexDesc_P3UV6[] = {
    {0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
    {0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0},
    {0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1},
    {0, 32, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2},
    {0, 40, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  3},
    {0, 48, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  4},
    {0, 56, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  5},
    D3DDECL_END()
};

static D3DVERTEXELEMENT9 VertexDesc_P3UV8[] = {
    {0,  0, D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0},
    {0, 16, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  0},
    {0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  1},
    {0, 32, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  2},
    {0, 40, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  3},
    {0, 48, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  4},
    {0, 56, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  5},
    {0, 64, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  6},
    {0, 72, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,  7},
    D3DDECL_END()
};

struct Storm3D_TerrainRendererData
{
	Storm3D &storm;
	Storm3D_TerrainHeightmap &heightMap;
	Storm3D_TerrainGroup &groups;
	Storm3D_TerrainModels &models;
	Storm3D_TerrainDecalSystem &decalSystem;

	SpotList spots;
	FakeSpotList fakeSpots;
	FakeLightList fakeLights;

	COL ambient;
	COL clearColor;
	COL lightmapMultiplier;
	COL outdoorLightmapMultiplier;
	Storm3D_Model *skyBox;

	static CComPtr<IDirect3DTexture9> renderTexture;
	static CComPtr<IDirect3DTexture9> terrainTexture;
	static CComPtr<IDirect3DTexture9> fakeTexture;
	static CComPtr<IDirect3DTexture9> glowTarget;
	static CComPtr<IDirect3DTexture9> glowTexture1;
	static CComPtr<IDirect3DTexture9> glowTexture2;
	static CComPtr<IDirect3DTexture9> offsetTexture;

	static VC2I renderSize;
	static VC2I fakeSize;
	static VC2I fakeTargetSize;
	static VC2I glowSize;

    LPDIRECT3DVERTEXDECLARATION9 VtxFmt_P3UV3;
    LPDIRECT3DVERTEXDECLARATION9 VtxFmt_P3UV4;
    LPDIRECT3DVERTEXDECLARATION9 VtxFmt_P3UV6;
    LPDIRECT3DVERTEXDECLARATION9 VtxFmt_P3UV8;

	CComPtr<IDirect3DTexture9> depthLookupTexture;
	CComPtr<IDirect3DTexture9> spotFadeTexture;
	CComPtr<IDirect3DTexture9> fakeFadeTexture;
	CComPtr<IDirect3DTexture9> fake2DFadeTexture;
	CComPtr<IDirect3DTexture9> coneFadeTexture;
	CComPtr<IDirect3DTexture9> noFadeTexture;

	frozenbyte::storm::VertexShader coneNvVertexShader;

    frozenbyte::storm::PixelShader glowPs2Shader;
	frozenbyte::storm::PixelShader lightShader;
	frozenbyte::storm::PixelShader colorEffectShader;
	frozenbyte::storm::PixelShader colorEffectOffsetShader;
	frozenbyte::storm::PixelShader colorEffectOffsetShader_NoGamma;
	frozenbyte::storm::PixelShader blackWhiteShader;

	IStorm3D_TerrainRenderer::RenderMode renderMode;
	bool renderRenderTargets;
	bool renderGlows;
	bool renderOffsets;
	bool renderCones;
	bool renderFakeLights;
	bool renderFakeShadows;
	bool renderSpotShadows;
	bool renderWireframe;
	bool renderBlackWhite;
	bool renderCollision;
	bool renderModels;
	bool renderTerrainObjects;
	bool renderHeightmap;

	bool renderSkyBox;
	bool renderDecals;
	bool renderSpotDebug;
	bool renderFakeShadowDebug;
	bool renderGlowDebug;
	bool renderBoned;
	bool renderTerrainTextures;
	bool renderParticles;
	bool renderParticleReflection;
	bool skyModelGlowAllowed;
	bool additionalAlphaTestPassAllowed;

	bool freezeCameraCulling;
	bool freezeSpotCulling;
	bool movieAspect;
	bool betterGlowSampling;
	bool multipassGlow;
	bool smoothShadows;
	bool proceduralFallback;
	bool reflection;
	bool halfRendering;

	D3DXMATRIX terrainTextureProjectionSizes[MAX_SIZES];
	D3DXMATRIX terrainTextureProjection;
	D3DXMATRIX fakeTextureProjection;

	float contrast;
	float brightness;
	COL colorFactors;
	bool colorEffectOn;

	VC2I backbufferSize;
	RECT scissorRectSizes[MAX_SIZES];
	RECT scissorRect;
	int movieAspectPad;
	int movieAspectStartTime;

	D3DVIEWPORT9 viewportSizes[MAX_SIZES];
	D3DVIEWPORT9 viewport;

	bool fogEnable;
	float fogStart;
	float fogEnd;
	COL fogColor;
	float glowFactor;
	float glowAdditiveFactor;
	float glowTransparencyFactor;

	bool hasStretchFiltering;
	bool forceDraw;

	Storm3D_ParticleSystem *particleSystem;
	std::shared_ptr<Storm3D_Texture> offsetFade;

	int activeSize;

	bool forcedDirectionalLightEnabled;
	COL forcedDirectionalLightColor;
	VC3 forcedDirectionalLightDirection;

	Storm3D_TerrainRendererData(Storm3D &storm_, Storm3D_TerrainHeightmap &heightMap_, Storm3D_TerrainGroup &groups_, Storm3D_TerrainModels &models_, Storm3D_TerrainDecalSystem &decalSystem_)
	:	storm(storm_),
		heightMap(heightMap_),
		groups(groups_),
		models(models_),
		decalSystem(decalSystem_),
		lightmapMultiplier(1.f, 1.f, 1.f),
		outdoorLightmapMultiplier(1.f, 1.f, 1.f),
		skyBox(0),
		glowPs2Shader(storm.GetD3DDevice()),
		lightShader(storm.GetD3DDevice()),
		colorEffectShader(storm.GetD3DDevice()),
		colorEffectOffsetShader(storm.GetD3DDevice()),
		colorEffectOffsetShader_NoGamma(storm.GetD3DDevice()),
		blackWhiteShader(storm.GetD3DDevice()),

		coneNvVertexShader(storm.GetD3DDevice()),

		renderMode(IStorm3D_TerrainRenderer::Normal),
		renderRenderTargets(true),
		renderGlows(false),
		renderOffsets(false),
		renderCones(true),
		renderFakeLights(true),
		renderFakeShadows(true),
		renderSpotShadows(true),
		renderWireframe(false),
		renderBlackWhite(false),
		renderCollision(false),
		renderModels(true),
		renderTerrainObjects(true),
		renderHeightmap(true),

		renderSkyBox(true),
		renderDecals(true),
		renderSpotDebug(false),
		renderFakeShadowDebug(false),
		renderGlowDebug(false),
		renderBoned(true),
		renderTerrainTextures(true),
		renderParticles(true),
		renderParticleReflection(false),
		skyModelGlowAllowed(true),
		additionalAlphaTestPassAllowed(false),

		freezeCameraCulling(false),
		freezeSpotCulling(false),
		movieAspect(false),
		betterGlowSampling(false),
		multipassGlow(false),
		smoothShadows(false),
		proceduralFallback(false),
		reflection(true),
		halfRendering(false),

		contrast(0.f),
		brightness(0.f),
		colorFactors(0.f, 0.f, 0.f),
		colorEffectOn(false),

		fogEnable(false),
		fogStart(50.f),
		fogEnd(-50.f),
		glowFactor(0.f),
		glowAdditiveFactor(1.0f),
		glowTransparencyFactor(0.0f),
		hasStretchFiltering(false),
		forceDraw(false),

		particleSystem(0),
		activeSize(0),

		forcedDirectionalLightEnabled(false)
	{
		gfx::Device &device = storm.GetD3DDevice();
		lightShader.createLightShader();
		colorEffectShader.createColorEffectPixelShader();
		blackWhiteShader.createBlackWhiteShader();

		colorEffectOffsetShader.createColorEffectOffsetPixelShader();
		colorEffectOffsetShader_NoGamma.createColorEffectOffsetPixelShader_NoGamma();
		glowPs2Shader.createGlowTex8Shader();

        coneNvVertexShader.createNvConeShader();

		device.CreateTexture(2048, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &depthLookupTexture, 0);

		D3DLOCKED_RECT lockedRect = { 0 };
		depthLookupTexture->LockRect(0, &lockedRect, 0, 0);

		DWORD *buffer = reinterpret_cast<DWORD *> (lockedRect.pBits);
		for(int i = 0; i < 2048; ++i)
			*buffer++ = D3DCOLOR_RGBA(i & 0xFF, (i & 0xFF00) >> 3, 0, 0);

		depthLookupTexture->UnlockRect(0);

		// Spot fade texture
		{
			device.CreateTexture(128, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &spotFadeTexture, 0);

			D3DLOCKED_RECT lockedRect = { 0 };
			spotFadeTexture->LockRect(0, &lockedRect, 0, 0);

			DWORD *buffer = reinterpret_cast<DWORD *> (lockedRect.pBits);
			for(int i = 0; i < 128; ++i)
			{
				int value = 255;
				//if(i > 64)
				//	value -= ((i - 64) * 4);
				if(i > 96)
					value -= ((i - 96) * 8);
				if(i < 2)
					value = 0;

				unsigned char c = 0;
				if(value > 0 && value <= 255)
					c = value;

				*buffer++ = D3DCOLOR_RGBA(c, c, c, c);
			}

			spotFadeTexture->UnlockRect(0);
		}

		// Fake spot fade texture
		{
			device.CreateTexture(128, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &fakeFadeTexture, 0);

			D3DLOCKED_RECT lockedRect = { 0 };
			fakeFadeTexture->LockRect(0, &lockedRect, 0, 0);

			DWORD *buffer = reinterpret_cast<DWORD *> (lockedRect.pBits);
			for(int i = 0; i < 128; ++i)
			{
				//int value = 127 + int(i * 1.1f);
				int value = 0;
				//value = i;
				//if(i > 96)
				//	value = (i - 96) * 8;
				//if(i > 64)
				//	value = (i - 64) * 2;
				//value = int((i/128.f) * 170.f);

				//if(i > 64)
				//	value = 6 * (i - 64) / 2;
				//value = (3 * i) / 2;
				value = i * 2;

				unsigned char c = 0;
				if(value >= 0 && value <= 255)
					c = value;

				*buffer++ = D3DCOLOR_RGBA(c, c, c, 0);
				//*buffer++ = D3DCOLOR_RGBA(c, 0, 0, 0);
			}

			fakeFadeTexture->UnlockRect(0);
		}

		// Fake 2D texture
		{
			device.CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &fake2DFadeTexture, 0);

			D3DLOCKED_RECT lockedRect = { 0 };
			fake2DFadeTexture->LockRect(0, &lockedRect, 0, 0);

			DWORD *buffer = reinterpret_cast<DWORD *> (lockedRect.pBits);
			int pitch = lockedRect.Pitch / sizeof(DWORD);

			float max = sqrtf(31*31 + 31*31);

			for(int y = 0; y < 64; ++y)
			for(int x = 0; x < 64; ++x)
			{
				float yd = float(y - 32);
				float xd = float(x - 32);
				float result = sqrtf(xd*xd + yd*yd) / max;

				if(result < 0.5f)
					result = 0.f;
				else
					result = (result - 0.5f) * 2.f;

				if(result > 1.f)
					result = 1.f;

				unsigned char c = (unsigned char)(result * 255);
				buffer[y * pitch + x] = D3DCOLOR_RGBA(c, c, c, 0);
			}

			fake2DFadeTexture->UnlockRect(0);
		}

		// No-fade texture
		{
			device.CreateTexture(128, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &noFadeTexture, 0);

			D3DLOCKED_RECT lockedRect = { 0 };
			noFadeTexture->LockRect(0, &lockedRect, 0, 0);

			DWORD *buffer = reinterpret_cast<DWORD *> (lockedRect.pBits);
			for(int i = 0; i < 128; ++i)
			{
				unsigned char c = 255;
				*buffer++ = D3DCOLOR_RGBA(c, c, c, 0);
			}

			noFadeTexture->UnlockRect(0);
		}

		// Cone fade
		{
			device.CreateTexture(128, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &coneFadeTexture, 0);

			D3DLOCKED_RECT lockedRect = { 0 };
			coneFadeTexture->LockRect(0, &lockedRect, 0, 0);

			DWORD *buffer = reinterpret_cast<DWORD *> (lockedRect.pBits);
			for(int i = 0; i < 128; ++i)
			{
				int value = 255 - (i * 6);
				
				unsigned char c = 0;
				if(value > 0)
					c = value;

				*buffer++ = D3DCOLOR_RGBA(c, c, c, c);
			}

			coneFadeTexture->UnlockRect(0);
		}

		if(glowTarget && glowTexture1 && glowTexture2)
			renderGlows = true;

		float xf = float(fakeSize.x) / fakeTargetSize.x * .5f;
		float yf = float(fakeSize.y) / fakeTargetSize.y * .5f;
		fakeTextureProjection = D3DXMATRIX (xf,		0.0f,	0.0f,	xf,
											0.0f,	-yf,	0.0f,	yf,
											0.0f,	0.0f,	0.0f,	0.0f,
											0.0f,   0.0f,	0.0f,	1.0f);
		/*
		float xf = float(fakeSize.x) / fakeTargetSize.x * .5f;
		float yf = float(fakeSize.y) / fakeTargetSize.y * .5f;
		fakeTextureProjection = D3DXMATRIX (0.5f,	0.0f,	0.0f,	0.5f,
											0.0f,	-0.5f,	0.0f,	0.5f,
											0.0f,	0.0f,	0.0f,	0.0f,
											0.0f,   0.0f,	0.0f,	1.0f);
		*/

		if(terrainTexture)
		{
			CComPtr<IDirect3DSurface9> terrainSurface;
			terrainTexture->GetSurfaceLevel(0, &terrainSurface);
			D3DSURFACE_DESC sourceDesc;
			terrainSurface->GetDesc(&sourceDesc);

			{
				float xs = float(renderSize.x) / (sourceDesc.Width) * .5f;
				float ys = float(renderSize.y) / (sourceDesc.Height) * .5f;
				float xd = 1.f / float(sourceDesc.Width) * .5f;
				float yd = 1.f / float(sourceDesc.Height) * .5f;

				terrainTextureProjectionSizes[0] = D3DXMATRIX (	 xs,	0.0f,	0.0f,	xs + xd,
																0.0f,	-ys,	0.0f,	ys + yd,
																0.0f,    0.0f,	0.0f,	0.0f,
																0.0f,    0.0f,	0.0f,	1.0f);
			}
			{
				float xs = float(renderSize.x) / (sourceDesc.Width) * .25f;
				float ys = float(renderSize.y) / (sourceDesc.Height) * .25f;
				float xd = 1.f / float(sourceDesc.Width) * .25f;
				float yd = 1.f / float(sourceDesc.Height) * .25f;

				terrainTextureProjectionSizes[1] = D3DXMATRIX (	 xs,	0.0f,	0.0f,	xs + xd,
																0.0f,	-ys,	0.0f,	ys + yd,
																0.0f,    0.0f,	0.0f,	0.0f,
																0.0f,    0.0f,	0.0f,	1.0f);
			}

			terrainTextureProjection = terrainTextureProjectionSizes[activeSize];
		}

		{
			Storm3D_SurfaceInfo info = storm.GetScreenSize();
			backbufferSize.x = info.width;
			backbufferSize.y = info.height;

			viewportSizes[0].X = 0;
			viewportSizes[0].Y = 0;
			viewportSizes[0].Width = backbufferSize.x;
			viewportSizes[0].Height = backbufferSize.y;
			viewportSizes[0].MinZ = 0.f;;
			viewportSizes[0].MaxZ = 1.f;
			viewportSizes[1] = viewportSizes[0];
			viewportSizes[1].Width /= 2;
			viewportSizes[1].Height /= 2;
			viewport = viewportSizes[activeSize];

			movieAspect = false;
			scissorRectSizes[0].left = 0;
			scissorRectSizes[0].top = 0;
			scissorRectSizes[0].right = backbufferSize.x;
			scissorRectSizes[0].bottom = backbufferSize.y;
			scissorRectSizes[1].left = scissorRectSizes[0].left / 2;
			scissorRectSizes[1].top = scissorRectSizes[0].top / 2;
			scissorRectSizes[1].right = scissorRectSizes[0].right / 2;
			scissorRectSizes[1].bottom = scissorRectSizes[0].bottom / 2;
			scissorRect = scissorRectSizes[activeSize];
			setMovieAspectRatio(false, false);
		}

		offsetFade.reset(static_cast<Storm3D_Texture *> (storm.CreateNewTexture("distortion_fade.dds")));
		if(!offsetFade && storm.getLogger())
			storm.getLogger()->error("Missing distortion mask texture. Distortion will not work properly.");

		hasStretchFiltering = storm.adapters[storm.active_adapter].stretchFilter;

        device.CreateVertexDeclaration(VertexDesc_P3UV3, &VtxFmt_P3UV3);
        device.CreateVertexDeclaration(VertexDesc_P3UV4, &VtxFmt_P3UV4);
        device.CreateVertexDeclaration(VertexDesc_P3UV6, &VtxFmt_P3UV6);
        device.CreateVertexDeclaration(VertexDesc_P3UV8, &VtxFmt_P3UV8);
	}

	~Storm3D_TerrainRendererData()
	{
        VtxFmt_P3UV3->Release();
        VtxFmt_P3UV4->Release();
        VtxFmt_P3UV6->Release();
        VtxFmt_P3UV8->Release();
	}

	void updateMovieAspectRatio(void)
	{
		if(movieAspectPad == scissorRectSizes[0].top)
		{
			// borders faded out
			if(movieAspectPad == 0)
				movieAspect = false;

			return;
		}

		// interpolate towards target
		int time = timeGetTime() - movieAspectStartTime;
		int target = movieAspectPad * (time) / 500;
		int current = (7*scissorRectSizes[0].top + target) / 8;

		// clamping (note: needs to handle both larger and smaller targets)
		if((current < target && current >= movieAspectPad)
			 || (current > target && current < movieAspectPad))
		{
			current = movieAspectPad;
		}


		scissorRectSizes[0].top = current;
		scissorRectSizes[0].bottom = backbufferSize.y - current;
		scissorRectSizes[1].top = scissorRectSizes[0].top / 2;
		scissorRectSizes[1].bottom = scissorRectSizes[0].bottom / 2;

		scissorRect.top = scissorRectSizes[activeSize].top;
		scissorRect.bottom = scissorRectSizes[activeSize].bottom;
	}

	void setMovieAspectRatio(bool enable, bool fade)
	{
		if(enable)
		{
			if(!fade)
			{
				// claw: borked 'real' aspect ratio
				int height = backbufferSize.x / 2;
				int padd = (backbufferSize.y - height) / 2;
				movieAspectPad = padd;
			}
			else
			{
				// survivor: nice looking aspect ratio
				int padd = backbufferSize.y / 6;
				movieAspectPad = padd;
			}
		}
		else
		{
			movieAspectPad = 0;
		}

		if(fade)
		{
			movieAspect = true;
			movieAspectStartTime = timeGetTime();
			updateMovieAspectRatio();
		}
		else
		{
			movieAspect = enable;

			scissorRectSizes[0].left = 0;
			scissorRectSizes[0].top = movieAspectPad;
			scissorRectSizes[0].right = backbufferSize.x;
			scissorRectSizes[0].bottom = backbufferSize.y - movieAspectPad;

			scissorRectSizes[1].left = scissorRectSizes[0].left / 2;
			scissorRectSizes[1].top = scissorRectSizes[0].top / 2;
			scissorRectSizes[1].right = scissorRectSizes[0].right / 2;
			scissorRectSizes[1].bottom = scissorRectSizes[0].bottom / 2;

			scissorRect = scissorRectSizes[activeSize];
		}
	}

	void renderConeLights(Storm3D_Scene &scene, bool glowsEnabled)
	{
        GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderCones");
		gfx::Device &device = storm.GetD3DDevice();
		device.SetTexture(2, coneFadeTexture);

		//lightManager.renderCones(scene, renderSpotShadows, glowsEnabled);

    	float timeFactor = float(scene.time_dif) * 0.001f;
    		// this draws spotlight cone
		device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_PROJECTED);
		device.SetPixelShader(0);

        //TODO: next line unnecessary?????
		Storm3D_ShaderManager::GetSingleton()->setProjectedShaders();
		coneNvVertexShader.apply();

		Storm3D_Camera &camera = *static_cast<Storm3D_Camera *> (scene.GetCamera());

		device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

		SpotList::iterator it = spots.begin();
		for(; it != spots.end(); ++it)
		{
			Storm3D_Spotlight *spot = it->get();
			if(!spot || !spot->enabled() || !spot->featureEnabled(IStorm3D_Spotlight::ConeVisualization))
				continue;

			if(renderSpotShadows)
				spot->renderCone(camera, timeFactor, renderGlows);
		}

		Storm3D_ShaderManager::GetSingleton()->setNormalShaders();
		device.SetPixelShader(0);

		device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    }

    void renderLightTargets(Storm3D_Scene &scene)
    {
        GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderProjectedRenderTargets");
        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;
        gfx::ProgramManager& programManager = renderer.programManager;

        Storm3D_Camera &camera = *static_cast<Storm3D_Camera *> (scene.GetCamera());

        //renderSpotBuffers(scene, renderSpotShadows);
        if (renderSpotShadows)
        {
            GFX_TRACE_SCOPE("renderSpotBuffers");

            device.SetRenderState(D3DRS_COLORWRITEENABLE, 0);
            Storm3D_ShaderManager::GetSingleton()->setNormalShaders();

            Storm3D_Spotlight::clearCache();

            SpotList::iterator it = spots.begin();
            for (; it != spots.end(); ++it)
            {
                Storm3D_Spotlight *spot = it->get();
                if (!spot || !spot->enabled() || !spot->featureEnabled(IStorm3D_Spotlight::Shadows))
                    continue;

                // Test spot visibility with foo scissor rect -- not really an optimal way of doing this
                if (!spot->setScissorRect(camera, VC2I(100, 100), scene))
                    continue;
                device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

                const float *cameraView = camera.GetView4x4Matrix();
                if (!spot->setAsRenderTarget(cameraView))
                    continue;

                // removing this will break shadows
                if (renderModels)
                    models.renderDepth(scene, camera, *spot, spot->getNoShadowModel());

                if (renderHeightmap)
                {
                    D3DXMATRIX dm;
                    D3DXMatrixIdentity(&dm);

                    programManager.setWorldMatrix(dm);
                    programManager.setStdProgram(device, gfx::ProgramManager::SSF_DEFAULT);
                    programManager.applyState(device);
                    heightMap.renderDepth(scene, &camera, Storm3D_TerrainHeightmap::Depth, spot->getType(), spot);
                }

            }

            //render(IStorm3D_TerrainRendererBase::SpotBuffer, scene, spot);
            device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

            Storm3D_ShaderManager::GetSingleton()->setNormalShaders();
            device.SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA | D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
            device.SetPixelShader(0);

            camera.Apply();
        }

        if (renderFakeShadows)
        {
            //renderFakeSpotBuffers(scene, renderFakeShadows);
            GFX_TRACE_SCOPE("renderFakeSpotBuffers");

            // Renders fake shadows to texture?
            Storm3D_ShaderManager::GetSingleton()->setFakeDepthShaders();

            device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            //device.SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_PROJECTED);

            Storm3D_FakeSpotlight::clearCache();

            FakeSpotList::iterator it = fakeSpots.begin();
            for (; it != fakeSpots.end(); ++it)
            {
                GFX_TRACE_SCOPE("renderFakeSpot");
                Storm3D_FakeSpotlight *fakeSpot = it->get();
                if (!fakeSpot || !fakeSpot->enabled())
                    continue;

                const float *cameraView = camera.GetView4x4Matrix();
                if (!fakeSpot->setAsRenderTarget(cameraView))
                    continue;

                //device.SetTexture(1, data->fake2DFadeTexture);
                bool needRender = false;

                if (renderModels)
                {
                    if (models.renderDepth(scene, camera, *fakeSpot))
                        needRender = true;
                }

                if (!needRender)
                    fakeSpot->disableVisibility();

                //renderer.render(IStorm3D_TerrainRendererBase::FakeSpotBuffer, scene, 0, spot);
            }

            device.SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            device.SetPixelShader(0);

            Storm3D_ShaderManager::GetSingleton()->setNormalShaders();
            camera.Apply();
        }

        //lightManager.renderProjectedRenderTargets(scene, renderSpotShadows, renderFakeShadows);
    }

	enum Pass
	{
		Solid,
		Alpha
	};

	void renderPass(Storm3D_Scene &scene, Pass pass)
	{
		Storm3D_ShaderManager::GetSingleton()->setLightingShaders();

        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;
        gfx::ProgramManager& programManager = renderer.programManager;

		D3DXMATRIX dm;
		D3DXMatrixIdentity(&dm);

		device.SetTransform(D3DTS_WORLD, &dm);
		Storm3D_ShaderManager::GetSingleton()->SetAmbient(ambient);
		Storm3D_ShaderManager::GetSingleton()->SetModelAmbient(COL(0,0,0));
		Storm3D_ShaderManager::GetSingleton()->SetObjectAmbient(COL(0,0,0));
		Storm3D_ShaderManager::GetSingleton()->SetObjectDiffuse(COL(1.f, 1.f, 1.f));
		Storm3D_ShaderManager::GetSingleton()->SetShaderAmbient(device, ambient);
		device.SetRenderState(D3DRS_AMBIENT, ambient.GetAsD3DCompatibleARGB());

		// update only in first pass
		if(movieAspect && pass == Solid)
		{
			updateMovieAspectRatio();
		}

		device.SetScissorRect(&scissorRect);
		device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
/*
// !!!!!!!!!!!!
//Storm3D_Texture *t = (Storm3D_Texture *) storm.CreateNewTexture("noise_02.dds");
//Storm3D_Texture *t = (Storm3D_Texture *) storm.CreateNewTexture("dirt_01.dds");
//Storm3D_Texture *t = (Storm3D_Texture *) storm.CreateNewTexture("military_tech_wall_01.dds");
Storm3D_Texture *t = (Storm3D_Texture *) storm.CreateNewTexture("water_1.dds");
device.SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
device.SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
t->Apply(4);
*/
		if(pass == Solid)
		{

            GFX_TRACE_SCOPE("Solid pass");
            if (renderHeightmap)
            {
                GFX_TRACE_SCOPE("Render heightmap");

                device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                // fakeTexture contains lights
                device.SetTexture(2, fakeTexture);
                device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                // terrainTexture contains terrain textures
                device.SetTexture(3, terrainTexture);
                device.SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device.SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

                D3DXMATRIX dm;
                D3DXMatrixIdentity(&dm);

                programManager.setWorldMatrix(dm);

                programManager.setTextureMatrix(1, terrainTextureProjection);
                programManager.setTextureMatrix(2, fakeTextureProjection);
                programManager.setAmbient(ambient);
                programManager.setDiffuse(COL(1.f, 1.f, 1.f));
                programManager.setLightmapColor(outdoorLightmapMultiplier);

                if (forcedDirectionalLightEnabled)
                {
                    VC3 sunDir = forcedDirectionalLightDirection;
                    float sunStrength = (forcedDirectionalLightColor.r + forcedDirectionalLightColor.g + forcedDirectionalLightColor.b) / 3.f;
                    sunDir *= sunStrength;

                    Storm3D_ShaderManager::GetSingleton()->SetSun(sunDir, sunStrength);
                    programManager.setDirectLight(sunDir, sunStrength);
                }
                else
                {
                    Storm3D_ShaderManager::GetSingleton()->SetSun(VC3(), 0.f);
                    programManager.setDirectLight(VC3(), 0.f);
                }

                programManager.setProgram(gfx::ProgramManager::TERRAIN_LIGHTING);
                programManager.applyState(device);

                frozenbyte::storm::enableMipFiltering(device, 2, 3, false);
                heightMap.renderDepth(scene, 0, Storm3D_TerrainHeightmap::Lighting, IStorm3D_Spotlight::None, 0);
                frozenbyte::storm::enableMipFiltering(device, 2, 3, true);

                programManager.setProgram(0);

                device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

                device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

                device.SetTexture(3, 0);
                device.SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                device.SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            }

			if(renderModels)
			{
                GFX_TRACE_SCOPE("Render models");
				device.SetVertexShaderConstantF(14, fakeTextureProjection, 4);
				device.SetTexture(2, fakeTexture);
				device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
				device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

				Storm3D_ShaderManager::GetSingleton()->SetAmbient(ambient);
				Storm3D_ShaderManager::GetSingleton()->SetObjectDiffuse(COL(1.f, 1.f, 1.f));

				const COL  &c = lightmapMultiplier;
				float mult[4] = { c.r, c.g, c.b, 1.f };
				device.SetPixelShaderConstantF(0, mult, 1);

				frozenbyte::storm::enableMipFiltering(device, 2, 2, false);
				models.renderLighting(Storm3D_TerrainModels::SolidOnly, scene);
				frozenbyte::storm::enableMipFiltering(device, 2, 2, true);

				device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
				device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			}

		}
		else if(pass == Alpha)
		{
            GFX_TRACE_SCOPE("Alpha pass");
			device.SetTexture(2, fakeTexture);
			device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
			device.SetVertexShaderConstantF(14, fakeTextureProjection, 4);

            programManager.setTextureMatrix(2, fakeTextureProjection);

			if(renderDecals)
			{
                GFX_TRACE_SCOPE("Render decals");
				frozenbyte::storm::enableMipFiltering(device, 0, 0, false);

				decalSystem.renderTextures(scene);

				frozenbyte::storm::enableMipFiltering(device, 0, 0, true);
			}

			device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

			Storm3D_ShaderManager::GetSingleton()->SetAmbient(ambient);
			Storm3D_ShaderManager::GetSingleton()->SetObjectDiffuse(COL(1.f, 1.f, 1.f));

			if(renderModels)
			{
                GFX_TRACE_SCOPE("Render models");
				device.SetTexture(2, fakeTexture);

				if(renderWireframe)
					models.setFillMode(Storm3D_TerrainModels::Wireframe);
				else
					models.setFillMode(Storm3D_TerrainModels::Solid);

				const COL  &c = lightmapMultiplier;
				float mult[4] = { c.r, c.g, c.b, 1.f };
				device.SetPixelShaderConstantF(0, mult, 1);

				device.SetVertexShaderConstantF(14, fakeTextureProjection, 4);
				device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
				device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

				frozenbyte::storm::enableMipFiltering(device, 2, 2, false);
				models.renderLighting(Storm3D_TerrainModels::AlphaOnly, scene);
				device.SetRenderState(D3DRS_FOGENABLE, FALSE);
				frozenbyte::storm::enableMipFiltering(device, 2, 2, true);

				device.SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
				device.SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			}
		}
	}

    void renderFakelights(Storm3D_Scene &scene)
    {
        if (renderFakeShadows)
        {
            GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderFakeSpotLights");

            // Renders fake shadows to screen?
            gfx::Device &device = storm.GetD3DDevice();
            Storm3D_Camera &camera = *static_cast<Storm3D_Camera *> (scene.GetCamera());

            device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
            device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
            device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
            device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

            device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

            frozenbyte::storm::enableMipFiltering(device, 0, 4, false);

            storm.renderer.setFVF(FVF_P3UV);
            //TODO: remove later - hack to ensure constants are set
            Storm3D_ShaderManager::GetSingleton()->setFakeShadowShaders();
            Storm3D_ShaderManager::GetSingleton()->SetShaders(
                device,
                Storm3D_ShaderManager::VERTEX_FAKE_SPOT_SHADOW,
                Storm3D_ShaderManager::PIXEL_FAKE_SPOT_SHADOW,
                0
                );

            FakeSpotList::iterator it = fakeSpots.begin();
            for (; it != fakeSpots.end(); ++it)
            {
                Storm3D_FakeSpotlight *spot = it->get();
                if (!spot || !spot->enabled())
                    continue;

                const float *cameraView = camera.GetView4x4Matrix();
                spot->applyTextures(cameraView);

                //renderer.render(IStorm3D_TerrainRendererBase::FakeSpotProjection, scene, 0, spot);
                spot->renderProjection();
            }

            frozenbyte::storm::enableMipFiltering(device, 0, 4, true);

            //TODO: remove after cleanup
            device.SetPixelShader(0);
            device.SetVertexShader(0);
            Storm3D_ShaderManager::GetSingleton()->setNormalShaders();

            device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

            device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
            device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
            device.SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
            //lightManager.renderProjectedFakeLights(scene, renderSpotShadows);
        }
    }

	enum LightType
	{
		RealSolid,
		RealAlpha,
		Fake
	};

    void renderSpotLights(Storm3D_Scene &scene, bool renderShadows, LightType type)
    {
        GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderSpotLights");

        // this renders spotlight light & shadows
        //setTracing(true);
        gfx::Renderer& renderer = storm.renderer;
        gfx::Device& device = renderer.device;
        gfx::ProgramManager& programManager = renderer.programManager;

        Storm3D_Camera &camera = *static_cast<Storm3D_Camera *> (scene.GetCamera());

        Storm3D_ShaderManager::GetSingleton()->setProjectedShaders();

        device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device.SetRenderState(D3DRS_ALPHAREF, 0x1);
        device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

        for (int i = 0; i < 4; ++i)
        {
            if (i != 2)
            {
                device.SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                device.SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            }
        }

        SpotList::iterator it = spots.begin();
        for (; it != spots.end(); ++it)
        {
            Storm3D_Spotlight *spot = it->get();
            if (!spot || !spot->enabled())
                continue;

            // Test spot visibility with foo scissor rect -- not really an optimal way of doing this
            if (!spot->setScissorRect(camera, VC2I(100, 100), scene))
                continue;
            device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

            if (!false)
            {
                device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_PROJECTED);
                device.SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_PROJECTED);
                device.SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
                device.SetTextureStageState(3, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            }

            const float *cameraView = camera.GetView4x4Matrix();
            const float *cameraViewProjection = camera.GetViewProjection4x4Matrix();
            spot->applyTextures(cameraView, cameraViewProjection, storm, renderShadows);

            Storm3D_Camera spotCamera = spot->getCamera();
            VC2I renderSize = this->renderSize;
            if (this->halfRendering)
                renderSize /= 2;

            if (!spot->setScissorRect(camera, renderSize, scene))
                return;

            if (spot->featureEnabled(IStorm3D_Spotlight::Fade))
                device.SetTexture(3, spotFadeTexture);
            else
                device.SetTexture(3, noFadeTexture);

            frozenbyte::storm::enableMipFiltering(device, 1, 1, false);

            if (type == RealSolid)
            {
                //renderer.render(IStorm3D_TerrainRendererBase::SpotProjectionSolid, scene, spot);

                // removing this breaks spotlight shadows
                spot->applySolidShader(renderSpotShadows);
                if (renderModels)
                    models.renderProjection(Storm3D_TerrainModels::SolidOnly, scene, *spot);

                spot->applyTerrainShader(renderSpotShadows);
                if (renderHeightmap)
                {
                    D3DXMATRIX dm;
                    D3DXMatrixIdentity(&dm);

                    programManager.setWorldMatrix(dm);

                    device.SetTexture(2, terrainTexture);
                    programManager.setTextureMatrix(2, terrainTextureProjection);

                    frozenbyte::storm::enableMipFiltering(device, 2, 2, false);
                    programManager.applyState(device);
                    heightMap.renderDepth(
                        scene, &spotCamera,
                        Storm3D_TerrainHeightmap::Projection,
                        spot->getType(), spot);
                    frozenbyte::storm::enableMipFiltering(device, 2, 2, true);
                }
            }
            else
            {
                //renderer.render(IStorm3D_TerrainRendererBase::SpotProjectionDecal, scene, spot);
                //renderer.render(IStorm3D_TerrainRendererBase::SpotProjectionAlpha, scene, spot);
                spot->applyNormalShader(renderSpotShadows);
                if (renderDecals)
                    decalSystem.renderProjection(scene, spot);
                spot->applyNormalShader(renderSpotShadows);
                if (renderModels)
                    models.renderProjection(Storm3D_TerrainModels::AlphaOnly, scene, *spot);
            }
        }

        Storm3D_ShaderManager::GetSingleton()->setNormalShaders();
        for (int i = 0; i < 4; ++i)
        {
            device.SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            device.SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        }
        //setTracing(false);

        device.SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        device.SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        device.SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        device.SetTextureStageState(3, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);

        device.SetTexture(1, 0);
        device.SetTexture(2, 0);
        device.SetTexture(3, 0);
        device.SetTexture(4, 0);

        device.SetPixelShader(0);
        device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        device.SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
        frozenbyte::storm::enableMinMagFiltering(device, 0, 1, true);
    }

    void renderProjectedLightsSolid(Storm3D_Scene &scene)
    {
        //lightManager.renderProjectedLightsSolid(scene, renderSpotShadows);
        GFX_TRACE_SCOPE("Storm3D_TerrainLightManager::renderProjectedLightsSolid");
        //data->renderLights(Data::RealSolid, scene, renderShadows);
        renderSpotLights(scene, renderSpotShadows, RealSolid);
    }

    void renderProjectedLightsAlpha(Storm3D_Scene &scene)
    {
        //lightManager.renderProjectedLightsAlpha(scene, renderSpotShadows);
        renderSpotLights(scene, renderSpotShadows, RealAlpha);
    }

	static void querySizes(Storm3D &storm, bool enableGlow)
	{
		Storm3D_SurfaceInfo screen = storm.GetScreenSize();
		VC2I screenSize(screen.width, screen.height);
		VC2I screenHalfSize(screen.width / 2, screen.height / 2);
		
		storm.setNeededDepthTarget(screenSize);
		storm.setNeededSecondaryColorTarget(screenSize);

		if(enableGlow)
		{
			storm.setNeededColorTarget(screenSize);
			storm.setNeededSecondaryColorTarget(screenHalfSize);
		}
	}

	static void createTextures(Storm3D &storm, int lightmapQuality)
	{
		gfx::Device &device = storm.GetD3DDevice();
		Storm3D_SurfaceInfo screen = storm.GetScreenSize();

		renderSize.x = screen.width;
		renderSize.y = screen.height;
		glowSize = renderSize / 2;
		glowSize.x = std::min(512, glowSize.x);
		glowSize.y = std::min(384, glowSize.y);

		fakeSize = renderSize / 2;

		device.CreateTexture(renderSize.x, renderSize.y, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &renderTexture, 0);
		terrainTexture = storm.getColorSecondaryTarget();

		fakeTargetSize = fakeSize;
		toNearestPow(fakeTargetSize.x);
		toNearestPow(fakeTargetSize.y);
		//device.CreateTexture(fakeTargetSize.x, fakeTargetSize.y, 1, D3DUSAGE_RENDERTARGET, D3DFMT_R5G6B5, D3DPOOL_DEFAULT, &fakeTexture, 0);
		device.CreateTexture(fakeTargetSize.x, fakeTargetSize.y, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &fakeTexture, 0);

		if(!renderTexture || !terrainTexture || !fakeTexture)
		{
			IStorm3D_Logger *logger = storm.getLogger();
			if(logger)
				logger->error("Failed creating base rendertargets - GAME WILL NOT RUN CORRECTLY!");

			return;
		}
	}

	static void createSecondaryTextures(Storm3D &storm, bool enableGlow)
	{
		if(enableGlow)
		{
			// ToDo: Test dev caps for D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES!

			glowTarget = storm.getColorTarget();
			//glowTexture1 = storm.getColorSecondaryTarget();
			//device.CreateTexture(glowSize.x, glowSize.y, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &glowTexture2, 0);
			glowTexture1 = storm.getColorTarget();
			glowTexture2 = storm.getColorSecondaryTarget();

			if(!glowTarget || !glowTexture1 || !glowTexture2)
			{
				glowTarget.Release();
				glowTexture1.Release();
				glowTexture2.Release();

				IStorm3D_Logger *logger = storm.getLogger();
				if(logger)
					logger->warning("Failed creating glow rendertargets - glow disabled!");
			}
		}

		offsetTexture = storm.getColorTarget();
		//device.CreateTexture(renderSize.x, renderSize.y, 1, D3DUSAGE_RENDERTARGET, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &offsetTexture, 0);
	}

	static void freeTextures()
	{
		renderTexture.Release();
		terrainTexture.Release();
		fakeTexture.Release();
	}

	static void freeSecondaryTextures()
	{
		glowTarget.Release();
		glowTexture1.Release();
		glowTexture2.Release();
		offsetTexture.Release();
	}
};

CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::renderTexture;
CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::terrainTexture;
CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::fakeTexture;
CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::glowTarget;
CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::glowTexture1;
CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::glowTexture2;
CComPtr<IDirect3DTexture9> Storm3D_TerrainRendererData::offsetTexture;

VC2I Storm3D_TerrainRendererData::renderSize;
VC2I Storm3D_TerrainRendererData::fakeSize;
VC2I Storm3D_TerrainRendererData::fakeTargetSize;
VC2I Storm3D_TerrainRendererData::glowSize;

Storm3D_TerrainRenderer::Storm3D_TerrainRenderer(Storm3D &storm, Storm3D_TerrainHeightmap &heightMap, Storm3D_TerrainGroup &groups, Storm3D_TerrainModels &models, Storm3D_TerrainDecalSystem &decalSystem)
{
	data = new Storm3D_TerrainRendererData(storm, heightMap, groups, models, decalSystem);

	setFog(false, 150.f, -50.f, COL(1.f, 0.5f, 0.5f));

	// Foofoo
	//setFog(true, 150.f, -50.f, COL(1.f, 0.5f, 0.5f));
}

Storm3D_TerrainRenderer::~Storm3D_TerrainRenderer()
{
    assert(data);
    delete data;
}

std::shared_ptr<IStorm3D_Spotlight> Storm3D_TerrainRenderer::createSpot()
{
	gfx::Device &device = data->storm.GetD3DDevice();

	std::shared_ptr<Storm3D_Spotlight> spot(new Storm3D_Spotlight(data->storm, device));
	spot->enableSmoothing(data->smoothShadows);
	data->spots.push_back(spot);

	return std::static_pointer_cast<IStorm3D_Spotlight> (spot);
}

void Storm3D_TerrainRenderer::deleteSpot(std::shared_ptr<IStorm3D_Spotlight> &spot)
{
	for(unsigned int i = 0; i < data->spots.size(); ++i)
	{
		if(data->spots[i].get() == spot.get())
		{
			data->spots.erase(data->spots.begin() + i);
			return;
		}
	}
}

std::shared_ptr<IStorm3D_FakeSpotlight> Storm3D_TerrainRenderer::createFakeSpot()
{
	gfx::Device &device = data->storm.GetD3DDevice();

	std::shared_ptr<Storm3D_FakeSpotlight> spot(new Storm3D_FakeSpotlight(data->storm, device));
	data->fakeSpots.push_back(spot);

	return std::static_pointer_cast<IStorm3D_FakeSpotlight> (spot);;
}

void Storm3D_TerrainRenderer::deleteFakeSpot(std::shared_ptr<IStorm3D_FakeSpotlight> &spot)
{
	for(unsigned int i = 0; i < data->fakeSpots.size(); ++i)
	{
		if(data->fakeSpots[i].get() == spot.get())
		{
			data->fakeSpots.erase(data->fakeSpots.begin() + i);
			return;
		}
	}
}

void Storm3D_TerrainRenderer::setMovieAspectRatio(bool enable, bool fade)
{
	data->setMovieAspectRatio(enable, fade);
}

void Storm3D_TerrainRenderer::setParticleSystem(Storm3D_ParticleSystem *particlesystem)
{
	data->particleSystem = particlesystem;
}

void Storm3D_TerrainRenderer::setRenderMode(IStorm3D_TerrainRenderer::RenderMode mode)
{
	data->renderMode = mode;

	if(mode == LightOnly)
		data->models.forceWhiteBaseTexture(true);
	else
		data->models.forceWhiteBaseTexture(false);
}

bool Storm3D_TerrainRenderer::enableFeature(RenderFeature feature, bool enable)
{
	bool oldValue = false;

	if(feature == IStorm3D_TerrainRenderer::RenderTargets)
	{
		oldValue = data->renderRenderTargets;
		data->renderRenderTargets = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::Glow)
	{
		oldValue = data->renderGlows;
		if(data->glowTarget && data->glowTexture1 && data->glowTexture2)
			data->renderGlows = enable;
		else
			data->renderGlows = false;
	}
	else if(feature == IStorm3D_TerrainRenderer::BetterGlowSampling)
	{
		oldValue = data->multipassGlow;
		data->multipassGlow = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::Distortion)
	{
		oldValue = data->renderOffsets;
		data->renderOffsets = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::SmoothShadows)
	{
		oldValue = data->smoothShadows;
		data->smoothShadows = enable;
		for(unsigned int i = 0; i < data->spots.size(); ++i)
			data->spots[i]->enableSmoothing(enable);
	}
	else if(feature == IStorm3D_TerrainRenderer::SpotShadows)
	{
		oldValue = data->renderSpotShadows;
		data->renderSpotShadows = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::FakeLights)
	{
		oldValue = data->renderFakeLights;
		data->renderFakeLights = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::Wireframe)
	{
		oldValue = data->renderWireframe;
		data->renderWireframe = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::BlackAndWhite)
	{
		oldValue = data->renderBlackWhite;
		data->renderBlackWhite = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::LightmapFiltering)
	{
		data->models.filterLightmap(enable);
	}
	else if(feature == IStorm3D_TerrainRenderer::Collision)
	{
		oldValue = data->renderCollision;
		data->renderCollision = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::ModelRendering)
	{
		oldValue = data->renderModels;
		data->renderModels = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::TerrainObjectRendering)
	{
		oldValue = data->renderTerrainObjects;
		data->renderTerrainObjects = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::HeightmapRendering)
	{
		oldValue = data->renderHeightmap;
		data->renderHeightmap = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::TerrainTextures)
	{
		oldValue = data->renderTerrainTextures;
		data->renderTerrainTextures = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::Particles)
	{
		oldValue = data->renderParticles;
		data->renderParticles = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::AlphaTest)
	{
		data->models.enableAlphaTest(enable);
	}
	else if(feature == IStorm3D_TerrainRenderer::MaterialAmbient)
	{
		data->models.enableMaterialAmbient(enable);
	}
	else if(feature == IStorm3D_TerrainRenderer::FreezeCameraCulling)
	{
		oldValue = data->freezeCameraCulling;
		data->freezeCameraCulling = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::FreezeSpotCulling)
	{
		oldValue = data->freezeSpotCulling;
		data->freezeSpotCulling = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::LightmappedCollision)
	{
		data->groups.enableLightmapCollision(enable);
	}
	else if(feature == IStorm3D_TerrainRenderer::RenderSkyModel)
	{
		oldValue = data->renderSkyBox;
		data->renderSkyBox = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::RenderDecals)
	{
		oldValue = data->renderDecals;
		data->renderDecals = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::RenderSpotDebug)
	{
		oldValue = data->renderSpotDebug;
		data->renderSpotDebug = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::RenderFakeShadowDebug)
	{
		oldValue = data->renderFakeShadowDebug;
		data->renderFakeShadowDebug = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::RenderGlowDebug)
	{
		oldValue = data->renderGlowDebug;
		data->renderGlowDebug = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::ProceduralFallback)
	{
		oldValue = data->proceduralFallback;
		data->proceduralFallback = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::Reflection)
	{
		oldValue = data->reflection;
		data->reflection = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::HalfRendering)
	{
		oldValue = data->halfRendering;
		if(enable)
		{
			data->halfRendering = true;
			data->activeSize = 1;
		}
		else
		{
			data->halfRendering = false;
			data->activeSize = 0;
		}

		data->terrainTextureProjection = data->terrainTextureProjectionSizes[data->activeSize];
		data->scissorRect = data->scissorRectSizes[data->activeSize];
		data->viewport = data->viewportSizes[data->activeSize];
	}
	else if(feature == IStorm3D_TerrainRenderer::RenderBoned)
	{
		oldValue = data->renderBoned;
		data->models.renderBoned(enable);
		data->renderBoned = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::FakeShadows)
	{
		oldValue = data->renderFakeShadows;
		data->renderFakeShadows = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::ParticleReflection)
	{
		oldValue = data->renderParticleReflection;
		data->renderParticleReflection = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::ForcedDirectionalLightEnabled)
	{
		oldValue = data->forcedDirectionalLightEnabled;
		data->forcedDirectionalLightEnabled = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::AdditionalAlphaTestPass)
	{
		oldValue = data->additionalAlphaTestPassAllowed;
		data->additionalAlphaTestPassAllowed = enable;
	}
	else if(feature == IStorm3D_TerrainRenderer::SkyModelGlow)
	{
		oldValue = data->skyModelGlowAllowed;
		data->skyModelGlowAllowed = enable;
	}

	return oldValue;
}

void Storm3D_TerrainRenderer::setFloatValue(FloatValue type, float value)
{
	if(type == LightmapMultiplier)
	{
		data->lightmapMultiplier = COL(value, value, value);
		data->decalSystem.setLightmapFactor(data->lightmapMultiplier);
	}
	else if(type == OutdoorLightmapMultiplier)
	{
		data->outdoorLightmapMultiplier = COL(value, value, value);
		data->decalSystem.setOutdoorLightmapFactor(data->outdoorLightmapMultiplier);
	}
	else if(type == ForceAmbient)
	{
		COL c(value, value, value);
		Storm3D_ShaderManager::GetSingleton()->SetForceAmbient(c);
	}
	else if(type == GlowFactor)
		data->glowFactor = value;
	else if(type == GlowTransparencyFactor)
		data->glowTransparencyFactor = value;
	else if(type == GlowAdditiveFactor)
		data->glowAdditiveFactor = value;
}

void Storm3D_TerrainRenderer::setColorValue(ColorValue type, const COL &value)
{
	if(type == LightmapMultiplierColor)
	{
		data->lightmapMultiplier = value;
		data->decalSystem.setLightmapFactor(data->lightmapMultiplier);
	}
	else if(type == OutdoorLightmapMultiplierColor)
	{
		data->outdoorLightmapMultiplier = value;
		data->decalSystem.setOutdoorLightmapFactor(data->outdoorLightmapMultiplier);
	}
	else if(type == TerrainObjectOutsideFactor)
	{
		data->models.setTerrainObjectColorFactorOutside(value);
	}
	else if(type == TerrainObjectInsideFactor)
	{
		data->models.setTerrainObjectColorFactorBuilding(value);
	}
	else if(type == ForcedDirectionalLightColor)
		data->forcedDirectionalLightColor = value;
}

COL Storm3D_TerrainRenderer::getColorValue(ColorValue type) const
{
	if(type == LightmapMultiplierColor)
		return data->lightmapMultiplier;
	else if(type == ForcedDirectionalLightColor)
		return data->forcedDirectionalLightColor;

	assert(!"Invalid color type");
	return COL();
}

void Storm3D_TerrainRenderer::setVectorValue(VectorValue type, const VC3 &value)
{
	if(type == ForcedDirectionalLightDirection)
		data->forcedDirectionalLightDirection = value;
}

VC3 Storm3D_TerrainRenderer::getVectorValue(VectorValue type) const
{
	if(type == ForcedDirectionalLightDirection)
		return data->forcedDirectionalLightDirection;

	assert(!"Invalid vector type");
	return VC3();
}

void Storm3D_TerrainRenderer::setColorEffect(float contrast, float brightness, const COL &colorFactors)
{
	data->contrast = contrast;
	data->brightness = brightness;
	data->colorFactors = colorFactors;

	if(fabsf(contrast) > 0.01f)
		data->colorEffectOn = true;
	else if(fabsf(brightness) > 0.01f)
		data->colorEffectOn = true;
	else if(fabsf(colorFactors.r) > 0.01f)
		data->colorEffectOn = true;
	else if(fabsf(colorFactors.g) > 0.01f)
		data->colorEffectOn = true;
	else if(fabsf(colorFactors.b) > 0.01f)
		data->colorEffectOn = true;
	else
		data->colorEffectOn = false;
}

void Storm3D_TerrainRenderer::renderLightTexture(const VC2 &start, const VC2 &end, IStorm3D_Texture &texture, const COL &color)
{
	data->fakeLights.push_back(Storm3D_LightTexture(start, end, texture, color));
}

void Storm3D_TerrainRenderer::setAmbient(const COL &color)
{
	data->ambient = color;
}

void Storm3D_TerrainRenderer::setClearColor(const COL &color)
{
	data->clearColor = color;
}

void Storm3D_TerrainRenderer::setSkyBox(Storm3D_Model *model)
{
	data->skyBox = model;
}

void Storm3D_TerrainRenderer::setFog(bool enable, float startHeight, float endHeight, const COL &color)
{
	data->fogEnable = enable;
	data->fogStart = startHeight;
	data->fogEnd = endHeight;
	data->fogColor = color;

	if(!data->fogEnable)
	{
		data->fogStart = -100000.f;
		data->fogEnd = -100001.f;
	}

    data->storm.renderer.programManager.setFog(data->fogStart, data->fogEnd);
    data->storm.renderer.programManager.setFogColor(color);
	Storm3D_ShaderManager::GetSingleton()->SetFog(data->fogStart, data->fogStart - data->fogEnd);
	data->decalSystem.setFog(data->fogEnd, data->fogStart - data->fogEnd);
}

void Storm3D_TerrainRenderer::updateVisibility(Storm3D_Scene &scene, int timeDelta)
{
	Storm3D_Camera &camera = static_cast<Storm3D_Camera &> (*scene.GetCamera());

	SpotList::iterator is = data->spots.begin();
	for(; is != data->spots.end(); ++is)
		is->get()->testVisibility(camera);

	FakeSpotList::iterator isf = data->fakeSpots.begin();
	for(; isf != data->fakeSpots.end(); ++isf)
		isf->get()->testVisibility(camera);

	if(!data->freezeCameraCulling && !data->freezeSpotCulling)
	{
		data->heightMap.calculateVisibility(scene);
		data->models.calculateVisibility(scene, timeDelta);
	}
}

void Storm3D_TerrainRenderer::renderTargets(Storm3D_Scene &scene)
{
    gfx::Renderer& renderer = data->storm.renderer;
    gfx::Device& device = renderer.device;
    gfx::ProgramManager& programManager = renderer.programManager;

    GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderTargets");
	if(!data->forceDraw && !data->renderRenderTargets)
	{
		data->decalSystem.clearShadowDecals();
		if(data->particleSystem)
			data->particleSystem->Clear();

		return;
	}

	data->models.setForcedDirectional(data->forcedDirectionalLightEnabled, data->forcedDirectionalLightDirection, data->forcedDirectionalLightColor);

	data->forceDraw = false;
	data->storm.getProceduralManagerImp().enableDistortionMode(data->renderOffsets);
	data->storm.getProceduralManagerImp().useFallback(data->proceduralFallback);

	data->models.enableGlow(data->renderGlows);
	data->models.enableDistortion(data->renderOffsets);
	data->models.enableReflection(data->reflection);
	data->models.enableAdditionalAlphaTestPass(data->additionalAlphaTestPassAllowed);
	data->models.enableSkyModelGlow(data->skyModelGlowAllowed);

	Storm3D_Camera &camera = static_cast<Storm3D_Camera &> (*scene.GetCamera());
	camera.Apply();

	device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device.SetRenderState(D3DRS_SPECULARENABLE, FALSE);
	device.SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
	device.LightEnable(0, FALSE);
	device.SetRenderState(D3DRS_LIGHTING, TRUE);

	D3DMATERIAL9 material = { { 0 } };
	material.Diffuse.r = material.Diffuse.g = material.Diffuse.b = 1.f;
	material.Ambient.r = material.Ambient.g = material.Ambient.b = 1.f;

	device.SetMaterial(&material);
	device.SetRenderState(D3DRS_AMBIENT, data->ambient.GetAsD3DCompatibleARGB());
	frozenbyte::storm::setCulling(device, D3DCULL_CCW);

	D3DXMATRIX dm;
	D3DXMatrixIdentity(&dm);

	data->models.setCollisionRendering(data->renderCollision);

	// Rendertarget stuff
	{		
		CComPtr<IDirect3DSurface9> originalFrameBuffer;
		CComPtr<IDirect3DSurface9> originalDepthBuffer;
		device.GetRenderTarget(0, &originalFrameBuffer);
		device.GetDepthStencilSurface(&originalDepthBuffer);

		RECT frameRect = { 0 };
		frameRect.right = data->renderSize.x;
		frameRect.bottom = data->renderSize.y;
		if(data->halfRendering)
		{
			frameRect.right /= 2;
			frameRect.bottom /= 2;
		}

		device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

		CComPtr<IDirect3DSurface9> tempDepthSurface = data->storm.getDepthTarget();
		device.SetDepthStencilSurface(tempDepthSurface);

		// 2D lights
		// draws lights to fakeTexture
		{
			CComPtr<IDirect3DSurface9> colorSurface;
			data->fakeTexture->GetSurfaceLevel(0, &colorSurface);
			device.SetRenderTarget(0, colorSurface);

			//device.Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);
			device.Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);

			D3DVIEWPORT9 viewport = { 0 };
			viewport.X = 0;
			viewport.Y = 0;
			viewport.Width = data->fakeSize.x;
			viewport.Height = data->fakeSize.y;
			viewport.MinZ = 0.f;;
			viewport.MaxZ = 1.f;
			device.SetViewport(&viewport);

            if (data->renderFakeLights)
            {
                GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderFakeLights");
                //data->lightManager.renderFakeLights(data->fakeSize);
                // Renders fake light (not shadows)?
                device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                device.SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

                //device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
                device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

                programManager.setStdProgram(device, gfx::ProgramManager::SSF_2D_POS | gfx::ProgramManager::SSF_COLOR | gfx::ProgramManager::SSF_TEXTURE);
                renderer.setFVF(FVF_P2DUV);

                VC2 pixsz = VC2(2.0f / data->fakeSize.x, 2.0f / data->fakeSize.y);
                float scalex = data->fakeSize.x * pixsz.x;
                float scaley = data->fakeSize.y * pixsz.y;

                FakeLightList::iterator it = data->fakeLights.begin();
                for (; it != data->fakeLights.end(); ++it)
                {
                    Storm3D_LightTexture &lightTexture = *it;
                    DWORD color = 0xFF000000 | lightTexture.color.GetAsD3DCompatibleARGB();

                    float x1 = frozenbyte::storm::convX_SCtoDS(lightTexture.start.x, scalex);
                    float y1 = frozenbyte::storm::convY_SCtoDS(lightTexture.start.y, scaley);
                    float x2 = frozenbyte::storm::convX_SCtoDS(lightTexture.end.x, scalex);
                    float y2 = frozenbyte::storm::convY_SCtoDS(lightTexture.end.y, scaley);

                    Vertex_P2DUV buffer[4] = {
                        { VC2(x1, y2), color, VC2(0.f, 1.f) },
                        { VC2(x1, y1), color, VC2(0.f, 0.f) },
                        { VC2(x2, y2), color, VC2(1.f, 1.f) },
                        { VC2(x2, y1), color, VC2(1.f, 0.f) },
                    };

                    lightTexture.texture->Apply(0);
                    data->storm.renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer, sizeof(Vertex_P2DUV));
                }

                device.SetTexture(0, 0);
                device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
                device.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

                device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
            }
		}

		// Light targets
		// spotlight depth maps

		{
			//data->lightManager.setFog(data->fogStart, data->fogEnd);
            float range = data->fogStart - data->fogEnd;

            FakeSpotList::iterator it = data->fakeSpots.begin();
            for (; it != data->fakeSpots.end(); ++it)
            {
                float factor = (*it)->getPlaneHeight() - data->fogEnd;
                factor /= range;
                if (factor < 0.f)
                    factor = 0.f;
                if (factor > 1.f)
                    factor = 1.f;

                factor = 1.f - factor;
                (*it)->setFogFactor(factor);
            }

			Storm3D_ShaderManager::GetSingleton()->SetAmbient(COL(0,0,0));
			Storm3D_ShaderManager::GetSingleton()->SetModelAmbient(COL(0,0,0));
			Storm3D_ShaderManager::GetSingleton()->SetObjectAmbient(COL(0,0,0));
			Storm3D_ShaderManager::GetSingleton()->SetObjectDiffuse(COL(1.f, 1.f, 1.f));
			data->renderLightTargets(scene);

			Storm3D_ShaderManager::GetSingleton()->setLightingShaders();
		}

		device.SetRenderTarget(0, originalFrameBuffer);
		device.SetDepthStencilSurface(originalDepthBuffer);
		device.SetViewport(&data->viewport);

		device.SetScissorRect(&data->scissorRect);
		device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
		device.SetRenderState(D3DRS_FOGENABLE, TRUE);
		device.SetRenderState(D3DRS_FOGCOLOR, data->fogColor.GetAsD3DCompatibleARGB());

		CComPtr<IDirect3DSurface9> renderSurface;
		data->renderTexture->GetSurfaceLevel(0, &renderSurface);

		device.SetRenderState(D3DRS_FOGENABLE, FALSE);

		// Terrain textures
		{
			int clearFlag = D3DCLEAR_ZBUFFER;
			if(data->storm.support_stencil)
				clearFlag |= D3DCLEAR_STENCIL;

			if(data->renderTerrainTextures)
				device.Clear(0, 0, D3DCLEAR_TARGET | clearFlag, 0x00000000, 1.f, 0);
			else
				device.Clear(0, 0, D3DCLEAR_TARGET | clearFlag, 0xFFFFFFFF, 1.f, 0);

			device.SetScissorRect(&data->scissorRect);
			device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			// this draws terrain textures (snow etc.)
			// (not rocks etc.)
			if(data->renderHeightmap && data->renderTerrainTextures)
				data->heightMap.renderTextures(scene);

			CComPtr<IDirect3DSurface9> terrainSurface;
			data->terrainTexture->GetSurfaceLevel(0, &terrainSurface);
			device.StretchRect(originalFrameBuffer, &frameRect, terrainSurface, &frameRect, D3DTEXF_NONE);
		}

		device.SetRenderState(D3DRS_FOGENABLE, TRUE);
		device.SetPixelShader(0);

		// (Ambient + lightmaps + fake) * base to fb
		// this renders pretty much everything...
		{
            GFX_TRACE_SCOPE("Main lighting: (Ambient + lightmaps + fake) * base");
			device.Clear(0, 0, D3DCLEAR_TARGET, data->clearColor.GetAsD3DCompatibleARGB(), 1.f, 0);
			device.SetScissorRect(&data->scissorRect);
			device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			// this renders terrain
            // not lights and terrain models
			device.SetRenderState(D3DRS_FOGENABLE, TRUE);
			data->renderPass(scene, data->Solid);
			device.SetRenderState(D3DRS_FOGENABLE, FALSE);

			if(data->skyBox && !data->renderWireframe && data->renderSkyBox)
			{
                GFX_TRACE_SCOPE("Skybox");
				data->skyBox->SetPosition(camera.GetPosition());
				// sideways mode, rotate skybox --jpk
				VC3 camup = camera.GetUpVec();
				if (camup.x == 0
					&& camup.y == 0
					&& camup.z == 1)
				{
					data->skyBox->SetRotation(QUAT(3.1415926f/2.0f,0,0));
				} else {
					data->skyBox->SetRotation(QUAT(0,0,0));
				}
				data->skyBox->SetScale(VC3(20.f, 20.f, 20.f));

				device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
				data->models.renderBackground(data->skyBox);
			}

			// this changes d3d state so that bullet trails, fire etc get rendered
			// critical state is: device.SetRenderState(D3DRS_LIGHTING, FALSE);
			data->decalSystem.renderShadows(scene);


			// does nothing?
			data->renderFakelights(scene);

			device.SetRenderState(D3DRS_AMBIENT, 0);
			Storm3D_ShaderManager::GetSingleton()->SetAmbient(COL(0,0,0));
			Storm3D_ShaderManager::GetSingleton()->SetModelAmbient(COL(0,0,0));
			Storm3D_ShaderManager::GetSingleton()->SetObjectAmbient(COL(0,0,0));
			Storm3D_ShaderManager::GetSingleton()->SetObjectDiffuse(COL(1.f, 1.f, 1.f));

			// this renders spot lights
			data->renderProjectedLightsSolid(scene);
			device.SetScissorRect(&data->scissorRect);
			device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			data->renderPass(scene, data->Alpha);
			data->renderProjectedLightsAlpha(scene);
			device.SetScissorRect(&data->scissorRect);
			device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			if(data->renderCones)
				data->renderConeLights(scene, data->renderGlows);

			// this renders bullet trails, fire etc.
			if(data->particleSystem && data->renderParticles)
			{
				data->particleSystem->Render(&scene, false);
				frozenbyte::storm::setCulling(device, D3DCULL_CCW);
			}

			if(data->movieAspect)
			{
#ifdef _MSC_VER
#pragma message ("***************************")
#pragma message ("*****   HALF SUPPORT  *****")
#pragma message ("***************************")
#endif

				D3DRECT rc[2] = { { 0 } };
				rc[0].x1 = 0;
				rc[0].y1 = 0;
				rc[0].x2 = data->renderSize.x;
				rc[0].y2 = data->scissorRect.top;
				rc[1].x1 = 0;
				rc[1].y1 = data->scissorRect.bottom;
				rc[1].x2 = data->renderSize.x;
				rc[1].y2 = data->renderSize.y;

				device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
				device.Clear(2, rc, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);
				device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
			}

			device.StretchRect(originalFrameBuffer, &frameRect, renderSurface, &frameRect, D3DTEXF_NONE);
		}

        if (data->renderGlows)
        {
            GFX_TRACE_SCOPE("Render Glows");
            Storm3D_ShaderManager::GetSingleton()->setNormalShaders();

            CComPtr<IDirect3DSurface9> glowSurface1;
            data->glowTexture1->GetSurfaceLevel(0, &glowSurface1);
            D3DSURFACE_DESC sourceDesc1;
            glowSurface1->GetDesc(&sourceDesc1);
            CComPtr<IDirect3DSurface9> glowSurface2;
            data->glowTexture2->GetSurfaceLevel(0, &glowSurface2);
            D3DSURFACE_DESC sourceDesc2;
            glowSurface2->GetDesc(&sourceDesc2);

            int glowWidth1 = sourceDesc1.Width;
            int glowHeight1 = sourceDesc1.Height;
            int glowWidth2 = sourceDesc2.Width;
            int glowHeight2 = sourceDesc2.Height;

            // Material self illuminations to glow texture
            {
                {
                    device.SetRenderTarget(0, glowSurface2);
                    device.SetDepthStencilSurface(tempDepthSurface);

                    int xs = int(15.f * data->renderSize.x / 1024.f + 0.5f);
                    int ys = int(15.f * data->renderSize.y / 800.f + 0.5f);

                    D3DRECT rc[2] = { { 0 } };
                    rc[0].x1 = data->glowSize.x - xs;
                    rc[0].y1 = 0;
                    rc[0].x2 = data->glowSize.x + xs;
                    rc[0].y2 = data->glowSize.y + ys;

                    rc[1].x1 = 0;
                    rc[1].y1 = data->glowSize.y - ys;
                    rc[1].x2 = data->glowSize.x + xs;
                    rc[1].y2 = data->glowSize.y + ys;
                    device.Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);

                    device.SetRenderTarget(0, glowSurface1);
                    device.Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);
                }

                device.SetRenderTarget(0, originalFrameBuffer);
                device.SetDepthStencilSurface(originalDepthBuffer);
                device.SetScissorRect(&data->scissorRect);
                device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
                device.SetViewport(&data->viewport);

                if (data->glowFactor > 0.001f)
                {
                    device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
                    device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
                    device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                    device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                    device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

                    data->storm.renderer.setFVF(FVF_PT4DUV);
                    int c = (unsigned char)(data->glowFactor * 255.f);
                    if (c > 255)
                        c = 255;
                    else if (c < 1)
                        c = 1;

                    DWORD color = c << 24 | c << 16 | c << 8 | c;
                    Vertex_P4DUV buffer[4];
                    float x2 = float(data->renderSize.x);
                    float y2 = float(data->renderSize.y);
                    buffer[0] = { VC4(0, y2, 1.f, 1.f), color, VC2(0.f, 1.f) };
                    buffer[1] = { VC4(0, 0, 1.f, 1.f), color, VC2(0.f, 0.f) };
                    buffer[2] = { VC4(x2, y2, 1.f, 1.f), color, VC2(1.f, 1.f) };
                    buffer[3] = { VC4(x2, 0, 1.f, 1.f), color, VC2(1.f, 0.f) };

                    device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
                    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);

                    device.SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
                    device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                    data->storm.renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer, sizeof(Vertex_P4DUV));
                    device.SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
                }
                else
                    device.Clear(0, 0, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);

                if (data->renderModels)
                {
                    device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
                    device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
                    device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
                    device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);

                    data->models.renderGlows(scene);

                    if (data->renderCones)
                        data->renderConeLights(scene, data->renderGlows);
                }

                device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
                device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

                RECT dst = { 0 };
                dst.right = data->glowSize.x;
                dst.bottom = data->glowSize.y;

                if (data->hasStretchFiltering)
                    device.StretchRect(originalFrameBuffer, &frameRect, glowSurface2, &dst, D3DTEXF_LINEAR);
                else
                    device.StretchRect(originalFrameBuffer, &frameRect, glowSurface2, &dst, D3DTEXF_NONE);
            }

            device.SetDepthStencilSurface(tempDepthSurface);

            frozenbyte::storm::setCulling(device, D3DCULL_NONE);
            {
                GFX_TRACE_SCOPE("Glow pass");
                // Filter glow texture
                {
                    device.SetVertexShader(0);

                    int stages = 8;

                    float af = .6f;
                    float bf = .3f;
                    float cf = .15f;
                    float df = .075f;
                    float ef = 0.8f;

                    //4,3,2,1

                    float a[] = { af, af, af, af };
                    float b[] = { bf, bf, bf, bf };
                    float c[] = { cf, cf, cf, cf };
                    float d[] = { df, df, df, df };
                    float e[] = { ef, ef, ef, ef };

                    device.SetPixelShaderConstantF(0, a, 1);
                    device.SetPixelShaderConstantF(1, b, 1);
                    device.SetPixelShaderConstantF(2, c, 1);
                    device.SetPixelShaderConstantF(3, d, 1);
                    device.SetPixelShaderConstantF(4, e, 1);

                    for (int i = 0; i < stages; ++i)
                    {
                        device.SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
                        device.SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
                    }

                    data->glowPs2Shader.apply();
                    device.SetVertexDeclaration(data->VtxFmt_P3UV8);

                    frozenbyte::storm::enableMipFiltering(device, 0, stages, false);
                    frozenbyte::storm::enableMinMagFiltering(device, 0, stages, true);

                    {
                        device.SetRenderTarget(0, glowSurface1);
                        device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
                        device.SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

                        D3DRECT rcs[2] = { { 0 }, { 0 } };
                        rcs[0].x1 = data->glowSize.x - 1;
                        rcs[0].y1 = 0;
                        rcs[0].x2 = data->glowSize.x + 11;
                        rcs[0].y2 = data->glowSize.y;
                        rcs[1].x1 = 0;
                        rcs[1].y1 = data->glowSize.y - 1;
                        rcs[1].x2 = data->glowSize.x;
                        rcs[1].y2 = data->glowSize.y + 11;
                        device.Clear(2, rcs, D3DCLEAR_TARGET, 0x00000000, 1.f, 0);

                        float xs = float(data->glowSize.x) / (glowWidth2);
                        float ys = float(data->glowSize.y) / (glowHeight2);

                        float xdp = (1.f / glowWidth2);
                        //xdp *= data->renderSize.x / 1024.f;

                        float xd1 = 0.5f * xdp;
                        float xd2 = 2.5f * xdp;
                        float xd3 = 4.5f * xdp;
                        float xd4 = 6.5f * xdp;
                        float xd5 = 8.5f * xdp;
                        float xd6 = 10.5f * xdp;

                        float width = float(data->glowSize.x) - .5f;
                        float height = float(data->glowSize.y) - .5f;

                        float buffer3[] =
                        {
                            // Position, w,             uv1,              uv2,              uv3,        uv4
                            -.5f, height, 1.f, 1.f, 0.f - xd1, ys, 0.f - xd2, ys, 0.f - xd3, ys, 0.f - xd4, ys, 0.f + xd1, ys, 0.f + xd2, ys, 0.f + xd3, ys, 0.f + xd4, ys,
                            -.5f, -.5f, 1.f, 1.f, 0.f - xd1, 0.f, 0.f - xd2, 0.f, 0.f - xd3, 0.f, 0.f - xd4, 0.f, 0.f + xd1, 0.f, 0.f + xd2, 0.f, 0.f + xd3, 0.f, 0.f + xd4, 0.f,
                            width, height, 1.f, 1.f, xs - xd1, ys, xs - xd2, ys, xs - xd3, ys, xs - xd4, ys, xs + xd1, ys, xs + xd2, ys, xs + xd3, ys, xs + xd4, ys,
                            width, -.5f, 1.f, 1.f, xs - xd1, 0.f, xs - xd2, 0.f, xs - xd3, 0.f, xs - xd4, 0.f, xs + xd1, 0.f, xs + xd2, 0.f, xs + xd3, 0.f, xs + xd4, 0.f
                        };


                        device.SetTexture(0, data->glowTexture2);
                        renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer3, sizeof(float) * 20);
                    }

                    // Blur to final
                    {
                        device.SetRenderTarget(0, glowSurface2);

                        float xs = float(data->glowSize.x) / (glowWidth1);
                        float ys = float(data->glowSize.y) / (glowHeight1);
                        float ydp = (1.f / glowHeight1);
                        //ydp *= data->renderSize.y / 768.f;

                        float yd1 = 0.5f * ydp;
                        float yd2 = 2.5f * ydp;
                        float yd3 = 4.5f * ydp;
                        float yd4 = 6.5f * ydp;
                        float yd5 = 8.5f * ydp;
                        float yd6 = 10.5f * ydp;

                        float width = float(data->glowSize.x) - .5f;
                        float height = float(data->glowSize.y) - .5f;

                        float buffer3[] =
                        {
                            // Position, w,             uv1,              uv2,              uv3,        uv4
                            -.5f, height, 1.f, 1.f, 0.f, ys - yd1, 0.f, ys - yd2, 0.f, ys - yd3, 0.f, ys - yd4, 0.f, ys + yd1, 0.f, ys + yd2, 0.f, ys + yd3, 0.f, ys + yd4,
                            -.5f, -.5f, 1.f, 1.f, 0.f, 0.f - yd1, 0.f, 0.f - yd2, 0.f, 0.f - yd3, 0.f, 0.f - yd4, 0.f, 0.f + yd1, 0.f, 0.f + yd2, 0.f, 0.f + yd3, 0.f, 0.f + yd4,
                            width, height, 1.f, 1.f, xs, ys - yd1, xs, ys - yd2, xs, ys - yd3, xs, ys - yd4, xs, ys + yd1, xs, ys + yd2, xs, ys + yd3, xs, ys + yd4,
                            width, -.5f, 1.f, 1.f, xs, 0.f - yd1, xs, 0.f - yd2, xs, 0.f - yd3, xs, 0.f - yd4, xs, 0.f + yd1, xs, 0.f + yd2, xs, 0.f + yd3, xs, 0.f + yd4
                        };

                        device.SetTexture(0, data->glowTexture1);

                        renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer3, sizeof(float) * 20);
                    }

                    for (int i = 0; i < stages; ++i)
                    {
                        device.SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
                        device.SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
                    }

                    frozenbyte::storm::enableMipFiltering(device, 0, stages, true);
                }
            }

            frozenbyte::storm::setCulling(device, D3DCULL_CCW);
            device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
            device.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

            device.SetTexture(0, 0);
            device.SetTexture(1, 0);
            device.SetTexture(2, 0);
            device.SetTexture(3, 0);
            device.SetPixelShader(0);

            device.SetRenderTarget(0, originalFrameBuffer);
            device.SetDepthStencilSurface(originalDepthBuffer);
            device.SetViewport(&data->viewport);
        }

		if(data->renderOffsets && data->offsetTexture)
		{
			device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

			CComPtr<IDirect3DSurface9> colorSurface;
			data->offsetTexture->GetSurfaceLevel(0, &colorSurface);
			D3DSURFACE_DESC sourceDesc;
			colorSurface->GetDesc(&sourceDesc);

			D3DRECT rc = { 0 };
			rc.x1 = data->scissorRect.left;
			rc.y1 = data->scissorRect.top;
			rc.x2 = data->scissorRect.right;
			rc.y2 = data->scissorRect.bottom;

			device.Clear(1, &rc, D3DCLEAR_TARGET, 0x80808080, 1.f, 0);
			device.Clear(1, &rc, D3DCLEAR_TARGET, 0x80808080, 1.f, 0);

			device.SetScissorRect(&data->scissorRect);
			device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

			// render the distortion effect
			if(data->particleSystem && data->renderParticles)
			{
				Storm3D_ShaderManager::GetSingleton()->setNormalShaders();
				if(data->renderModels)
					data->models.renderDistortion(scene);

				data->particleSystem->Render(&scene, true);
				frozenbyte::storm::setCulling(device, D3DCULL_CCW);
			}

			device.StretchRect(originalFrameBuffer, &frameRect, colorSurface, &frameRect, D3DTEXF_NONE);
			device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
		}

		device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

		if(data->renderGlows)
		{
			frozenbyte::storm::setCulling(device, D3DCULL_NONE);
			device.SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			device.SetRenderTarget(0, renderSurface);
			device.SetDepthStencilSurface(tempDepthSurface);
			device.SetViewport(&data->viewport);

			Storm3D_SurfaceInfo info = data->storm.GetScreenSize();
			float width = float(info.width) - .5f;
			float height = float(info.height) - .5f;
			float textureOffset = -1.f;
			float glowWidth = 0.f;
			float glowHeight = 0.f;

			if(data->halfRendering)
			{
				width *= 0.5f;
				height *= 0.5f;
			}

			{
				CComPtr<IDirect3DSurface9> glowSourceSurface;
				data->glowTexture2->GetSurfaceLevel(0, &glowSourceSurface);
				D3DSURFACE_DESC sourceDesc;
				glowSourceSurface->GetDesc(&sourceDesc);
				glowWidth = float(data->glowSize.x) / (sourceDesc.Width);
				glowHeight = float(data->glowSize.y) / (sourceDesc.Height);
			}

			float glowTex1[] = 
			{
				textureOffset, height,         1.f, 1.f, 0.f, glowHeight,
				textureOffset, textureOffset,  1.f, 1.f, 0.f, 0.f,
				width, height,                 1.f, 1.f, glowWidth, glowHeight,
				width, textureOffset,          1.f, 1.f, glowWidth, 0.f
			};

			device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
			device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

			device.SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
			
			device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
			device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
			device.SetTexture(0, data->glowTexture2);

			if(data->renderGlowDebug)
			{
				device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			}

			frozenbyte::storm::enableMipFiltering(device, 0, 0, false);

			device.SetPixelShader(0);
			device.SetVertexShader(0);
            renderer.setFVF(FVF_PT4UV);

			// Transparency
			if(data->glowTransparencyFactor > 0.001f)
			{
				int c = (unsigned char) (data->glowTransparencyFactor * 255.f);
				if(c > 255)
					c = 255;
				else if(c < 1)
					c = 1;
				DWORD color = c << 24 | c << 16 | c << 8 | c;
				device.SetRenderState(D3DRS_BLENDFACTOR, color);

				device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_BLENDFACTOR);
				device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVBLENDFACTOR);
                renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, glowTex1, sizeof(float) *  6);
			}

			// Additive
			if(data->glowAdditiveFactor > 0.001f)
			{
				int c = (unsigned char) (data->glowAdditiveFactor * 255.f);
				if(c > 255)
					c = 255;
				else if(c < 1)
					c = 1;
				DWORD color = c << 24 | c << 16 | c << 8 | c;
				device.SetRenderState(D3DRS_TEXTUREFACTOR, color);

				device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				device.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
				device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR);
				device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

				device.SetRenderState(D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR);
				device.SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
                renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, glowTex1, sizeof(float) *  6);
			}

			device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			frozenbyte::storm::enableMipFiltering(device, 0, 0, true);

			device.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
			device.SetRenderTarget(0, originalFrameBuffer);
			device.SetDepthStencilSurface(originalDepthBuffer);
			frozenbyte::storm::setCulling(device, D3DCULL_CCW);
			device.SetViewport(&data->viewport);
		}

		device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	}

	Storm3D_ShaderManager::GetSingleton()->setNormalShaders();

	device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	device.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

	if(data->particleSystem && active_visibility == 0)
		data->particleSystem->Clear();

	data->fakeLights.clear();
}

void Storm3D_TerrainRenderer::renderBase(Storm3D_Scene &scene)
{
    GFX_TRACE_SCOPE("Storm3D_TerrainRenderer::renderBase");
    gfx::Renderer& renderer = data->storm.renderer;
	gfx::Device& device = renderer.device;

	Storm3D_Camera &camera = static_cast<Storm3D_Camera &> (*scene.GetCamera());
	camera.Apply();

	device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	device.SetRenderState(D3DRS_SPECULARENABLE, FALSE);
	device.SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
	device.SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

	D3DMATERIAL9 material;
	material.Diffuse.r = material.Diffuse.g = material.Diffuse.b = 1.f;
	material.Ambient.r = material.Ambient.g = material.Ambient.b = 1.f;
	material.Specular.r = material.Specular.g = material.Specular.b = 0;
	material.Emissive.r = material.Emissive.g = material.Emissive.b = 0;
	material.Power = 0;

	device.SetMaterial(&material);
	device.SetRenderState(D3DRS_AMBIENT, data->ambient.GetAsD3DCompatibleARGB());
	frozenbyte::storm::setCulling(device, D3DCULL_NONE);

	D3DXMATRIX dm;
	D3DXMatrixIdentity(&dm);

	data->models.setCollisionRendering(data->renderCollision);
	data->models.setForcedDirectional(data->forcedDirectionalLightEnabled, data->forcedDirectionalLightDirection, data->forcedDirectionalLightColor);

	Storm3D_ShaderManager::GetSingleton()->setNormalShaders();

	Storm3D_SurfaceInfo info = data->storm.GetScreenSize();
	float width = float(info.width) - .5f;
	float height = float(info.height) - .5f;

	// HAX
	if(data->halfRendering)
	{
		//width *= 2.f;
		//height *= 2.f;
	}

	float textureOffset = -1.f;
/*
	float glowWidth = 0.f;
	float glowHeight = 0.f;

	if(data->glowTexture2)
	{
		CComPtr<IDirect3DSurface9> glowSourceSurface;
		data->glowTexture2->GetSurfaceLevel(0, &glowSourceSurface);
		D3DSURFACE_DESC sourceDesc;
		glowSourceSurface->GetDesc(&sourceDesc);
		glowWidth = float(data->glowSize.x) / (sourceDesc.Width);
		glowHeight = float(data->glowSize.y) / (sourceDesc.Height);
	}

	float glowTex1[] = 
	{
		textureOffset, height,         1.f, 1.f, 0.f, glowHeight,
		textureOffset, textureOffset,  1.f, 1.f, 0.f, 0.f,
		width, height,                 1.f, 1.f, glowWidth, glowHeight,
		width, textureOffset,          1.f, 1.f, glowWidth, 0.f
	};
*/
	textureOffset = -.5f;
    /*
	float bufferTex2[] = 
	{
		textureOffset, height,         1.f, 1.f, 0.f, 1.f,
		textureOffset, textureOffset,  1.f, 1.f, 0.f, 0.f,
		width, height,                 1.f, 1.f, 1.f, 1.f,
		width, textureOffset,          1.f, 1.f, 1.f, 0.f
	};

	float bufferTex3[] = 
	{
		textureOffset, height,         1.f, 1.f, 0.f, 1.f, 0.f, 1.f,
		textureOffset, textureOffset,  1.f, 1.f, 0.f, 0.f, 0.f, 0.f,
		width, height,                 1.f, 1.f, 1.f, 1.f, 1.f, 1.f,
		width, textureOffset,          1.f, 1.f, 1.f, 0.f, 1.f, 0.f
	};
	*/

	device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device.SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
	device.SetVertexShader(0);
    renderer.setFVF(FVF_PT4UV);

	Storm3D_ShaderManager::GetSingleton()->setNormalShaders();

	device.SetViewport(&data->viewportSizes[0]);
	device.SetScissorRect(&data->scissorRectSizes[0]);
	device.SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);

	if(data->movieAspect && data->halfRendering)
		device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);


	// Render map
	if(data->renderMode == IStorm3D_TerrainRenderer::Normal || data->renderMode == IStorm3D_TerrainRenderer::LightOnly)
	{
		device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		//frozenbyte::storm::enableMipFiltering(device, 0, 0, false);
		
		device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);

		float offsetWidth = 0.f;
		float offsetHeight = 0.f;
		{
			if(data->offsetTexture)
			{
				CComPtr<IDirect3DSurface9> colorSurface;
				data->offsetTexture->GetSurfaceLevel(0, &colorSurface);
				D3DSURFACE_DESC sourceDesc;
				colorSurface->GetDesc(&sourceDesc);

				offsetWidth = float(data->renderSize.x) / sourceDesc.Width;
				offsetHeight = float(data->renderSize.y) / sourceDesc.Height;
			}
		}

		float xv = 1.f;
		float yv = 1.f;
		if(data->halfRendering)
		{
			xv = 0.5f;
			yv = 0.5f;
			offsetWidth *= 0.5f;
			offsetHeight *= 0.5f;
		}

		float bufferTex[] = 
		{
			textureOffset, height,         1.f, 1.f, 0.f, yv, 0.f, offsetHeight,0.f, 1.f,
			textureOffset, textureOffset,  1.f, 1.f, 0.f, 0.f,0.f, 0.f,0.f, 0.f,
			width, height,                 1.f, 1.f, xv,  yv, offsetWidth, offsetHeight,1.f, 1.f,
			width, textureOffset,          1.f, 1.f, xv,  0.f,offsetWidth, 0.f,1.f, 0.f
		};

		if(!data->renderBlackWhite && (data->colorEffectOn || data->renderOffsets))
		{
			float c = data->contrast;
			float b = data->brightness;
			const COL &col = data->colorFactors;

			float c2[4] = { c, c, c, c };
			float c3[4] = { b * (col.r + 1.f), b * (col.g + 1.f), b * (col.b + 1.f), 0 };
			float c4[4] = { col.r, col.g, col.b, 1.f };

			device.SetPixelShaderConstantF(2, c2, 1);
			device.SetPixelShaderConstantF(3, c3, 1);
			device.SetPixelShaderConstantF(4, c4, 1);

			if(data->renderOffsets && data->offsetTexture)
			{
				if(data->colorEffectOn)
					data->colorEffectOffsetShader.apply();
				else
					data->colorEffectOffsetShader_NoGamma.apply();
			}
			else
				data->colorEffectShader.apply();
		}
		else if(data->renderBlackWhite)
		{
			data->blackWhiteShader.apply();
		}
		else
		{
			device.SetPixelShader(0);
			device.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			device.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			device.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		}	

		device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);

		frozenbyte::storm::enableMinMagFiltering(device, 0, 1, false);
		frozenbyte::storm::enableMipFiltering(device, 0, 1, false);

		// renderTexture has everything
		device.SetTexture(0, data->renderTexture);
		// offsetTexture has distortion displacement texture
		device.SetTexture(1, data->offsetTexture);
		if(data->offsetFade)
			data->offsetFade->Apply(2);

		float offsetScale = 0.06f;
		offsetScale *= data->renderSize.x / 1024.f;
		if(data->halfRendering)
			offsetScale *= 0.5f;
		float offsetValues[4] = { offsetScale, offsetScale, offsetScale, offsetScale };
		device.SetPixelShaderConstantF(5, offsetValues, 1);

        device.SetVertexDeclaration(data->VtxFmt_P3UV3);

        renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, bufferTex, sizeof(float) *  10);

        renderer.setFVF(FVF_PT4UV);
		device.SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device.SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
		device.SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
		device.SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

		frozenbyte::storm::enableMinMagFiltering(device, 0, 1, true);
		frozenbyte::storm::enableMipFiltering(device, 0, 1, true);

		device.SetPixelShader(0);
	}

	device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device.SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	device.SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);

	data->fakeLights.clear();

	if(data->renderSpotDebug && data->spots[0])
		data->spots[0]->debugRender();
	if(data->renderFakeShadowDebug && data->fakeSpots[0])
		data->fakeSpots[0]->debugRender();

	device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	frozenbyte::storm::setCulling(device, D3DCULL_CCW);

	// debugging code
	if (false)
	{
		IDirect3DStateBlock9 *state;
		device.CreateStateBlock(D3DSBT_ALL, &state);
		state->Capture();

		D3DXMATRIX id;
		D3DXMatrixIdentity(&id);
		device.SetTransform(D3DTS_WORLD, &id);
		device.SetTransform(D3DTS_VIEW, &id);
		device.SetTransform(D3DTS_PROJECTION, &id);

		for (int i = 1; i < 8; i++) {
			device.SetTexture(i, 0);
			device.SetTextureStageState(i, D3DTSS_COLOROP, D3DTOP_DISABLE);
			device.SetTextureStageState(i, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			device.SetTextureStageState(i, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			device.SetTextureStageState(i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		}

		device.SetTexture(0, data->terrainTexture);
		device.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);

		device.SetPixelShader(0);
		device.SetVertexShader(0);
		device.SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		device.SetRenderState(D3DRS_ZENABLE, FALSE);
		device.SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		device.SetRenderState(D3DRS_ALPHAREF, (DWORD)0x00000001);
		device.SetRenderState(D3DRS_FOGENABLE, FALSE);
		device.SetRenderState(D3DRS_STENCILENABLE, FALSE);


		float x2 = 300, y2 = 300;
		Vertex_P4DUV buffer[4];
		DWORD color = 0xFFFFFFFF;
		buffer[0] = {VC4( 0, y2, 1.f, 1.f), color, VC2(0.f, 1.f)};
		buffer[1] = {VC4( 0,  0, 1.f, 1.f), color, VC2(0.f, 0.f)};
		buffer[2] = {VC4(x2, y2, 1.f, 1.f), color, VC2(1.f, 1.f)};
		buffer[3] = {VC4(x2,  0, 1.f, 1.f), color, VC2(1.f, 0.f)};
		renderer.setFVF(FVF_PT4DUV);
		renderer.drawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, buffer, sizeof(Vertex_P4DUV));

		state->Apply();
	}

}

void Storm3D_TerrainRenderer::releaseDynamicResources()
{
	SpotList::iterator its = data->spots.begin();
	for(; its != data->spots.end(); ++its)
		(*its)->releaseDynamicResources();

	FakeSpotList::iterator itf = data->fakeSpots.begin();
	for(; itf != data->fakeSpots.end(); ++itf)
		(*itf)->releaseDynamicResources();
}

void Storm3D_TerrainRenderer::recreateDynamicResources()
{
	data->forceDraw = true;

	SpotList::iterator its = data->spots.begin();
	for(; its != data->spots.end(); ++its)
		(*its)->recreateDynamicResources();

	FakeSpotList::iterator itf = data->fakeSpots.begin();
	for(; itf != data->fakeSpots.end(); ++itf)
		(*itf)->recreateDynamicResources();
}

void Storm3D_TerrainRenderer::querySizes(Storm3D &storm, bool enableGlow)
{
	Storm3D_TerrainRendererData::querySizes(storm, enableGlow);
}

void Storm3D_TerrainRenderer::createRenderBuffers(Storm3D &storm, int lightmapQuality)
{
	Storm3D_TerrainRendererData::createTextures(storm, lightmapQuality);
}

void Storm3D_TerrainRenderer::freeRenderBuffers()
{
	Storm3D_TerrainRendererData::freeTextures();
}

void Storm3D_TerrainRenderer::createSecondaryRenderBuffers(Storm3D &storm, bool enableGlow)
{
	Storm3D_TerrainRendererData::createSecondaryTextures(storm, enableGlow);
}

void Storm3D_TerrainRenderer::freeSecondaryRenderBuffers()
{
	Storm3D_TerrainRendererData::freeSecondaryTextures();
}

bool Storm3D_TerrainRenderer::hasNeededBuffers()
{
	if(!Storm3D_TerrainRendererData::renderTexture)
		return false;
	if(!Storm3D_TerrainRendererData::fakeTexture)
		return false;

	return true;
}
