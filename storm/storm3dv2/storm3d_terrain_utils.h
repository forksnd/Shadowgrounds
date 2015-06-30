// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_STORM3D_TERRAIN_UTILS_H
#define INCLUDED_STORM3D_TERRAIN_UTILS_H

#include <boost/shared_ptr.hpp>
#include <string>
#include "GfxDevice.h"
#include <atlbase.h>
#include <vector>

class Storm3D_Texture;
class IStorm3D_Logger;
struct IDirect3DDevice9;

#ifndef NDEBUG
void activeShaderNames();
void setTracing(bool tracing_);

#else
#define activeShaderNames() ;
#endif

namespace frozenbyte {
namespace storm {

class VertexShader
{
	CComPtr<IDirect3DVertexShader9> handle;
	CComPtr<IDirect3DVertexDeclaration9> declaration;
	
	GfxDevice& device;
	std::vector<D3DVERTEXELEMENT9> elements;

	std::string name;
	CComPtr<IDirect3DVertexShader9> createVertexShader(GfxDevice& device, const std::string &fileName);

public:
	VertexShader(GfxDevice& device);
	~VertexShader();

	void create2DShader2Tex();

	void createTerrainShader();
	void createAtiTerrainShader();
	void createAtiLightingShader();
	void createNvTerrainShader();
	void createNvLightingShader();

	void createDefaultShader();
	//void createLightingShader();
	void createLightingShader_0light_noreflection();
	void createLightingShader_0light_localreflection();
	void createLightingShader_0light_reflection();
	void createLightingShader_1light_noreflection();
	void createLightingShader_1light_localreflection();
	void createLightingShader_1light_reflection();
	void createLightingShader_2light_noreflection();
	void createLightingShader_2light_localreflection();
	void createLightingShader_2light_reflection();
	void createLightingShader_3light_noreflection();
	void createLightingShader_3light_localreflection();
	void createLightingShader_3light_reflection();
	void createLightingShader_4light_noreflection();
	void createLightingShader_4light_localreflection();
	void createLightingShader_4light_reflection();
	void createLightingShader_5light_noreflection();
	void createLightingShader_5light_localreflection();
	void createLightingShader_5light_reflection();

	void createSkyboxShader();
	void createDefaultProjectionShaderDirectional();
	void createDefaultProjectionShaderPoint();
	void createDefaultProjectionShaderFlat();
	void createBoneShader();
	void createBasicBoneLightingShader();
	//void createBoneLightingShader();
	void createBoneLightingShader_0light_noreflection();
	void createBoneLightingShader_0light_reflection();
	void createBoneLightingShader_1light_noreflection();
	void createBoneLightingShader_1light_reflection();
	void createBoneLightingShader_2light_noreflection();
	void createBoneLightingShader_2light_reflection();
	void createBoneLightingShader_3light_noreflection();
	void createBoneLightingShader_3light_reflection();
	void createBoneLightingShader_4light_noreflection();
	void createBoneLightingShader_4light_reflection();
	void createBoneLightingShader_5light_noreflection();
	void createBoneLightingShader_5light_reflection();

	void createBoneProjectionShaderDirectional();
	void createBoneProjectionShaderPoint();
	void createBoneProjectionShaderFlat();

	void createAtiDepthShader();
	void createAtiDepthTerrainShader();
	void createAtiBoneDepthShader();
	void createAtiShadowShaderDirectional();
	void createAtiShadowShaderPoint();
	void createAtiShadowShaderFlat();
	void createAtiBoneShadowShaderDirectional();
	void createAtiBoneShadowShaderPoint();
	void createAtiBoneShadowShaderFlat();
	void createAtiTerrainShadowShaderDirectional();
	void createAtiTerrainShadowShaderPoint();
	void createAtiTerrainShadowShaderFlat();
	void createNvTerrainShadowShaderDirectional();
	void createNvTerrainShadowShaderPoint();
	void createNvTerrainShadowShaderFlat();

	void createAtiConeShader();
	void createNvConeShader();
	void createConeStencilShader();
	void createDecalShader();
	void createDecalPointShader();
	void createDecalDirShader();
	void createDecalFlatShader();
	void createFakeDepthShader();
	void createFakeShadowShader();
	void createFakeDepthBoneShader();
	void createFakeShadowBoneShader();
	void createFakePlaneShadowShader();
	void createProceduralShader();

	void applyDeclaration() const;
	void apply() const;
};

class PixelShader
{
	CComPtr<IDirect3DPixelShader9> handle;
	GfxDevice& device;

	std::vector<D3DVERTEXELEMENT9> elements;

	std::string name;

	CComPtr<IDirect3DPixelShader9> createPixelShader(GfxDevice& device, const std::string &fileName);
public:
	PixelShader(GfxDevice &device);
	~PixelShader();

	void createTerrainShader();
	void createTerrainLightShader();
	void createGlowShader();
	void createGlowTex8Shader();
	void createGlowPs14Shader();
	void createGlowFinalShader();
	void createLightShader();
	void createAtiLightConeShader_Texture();
	void createAtiLightConeShader_NoTexture();
	void createAtiFloatLightConeShader_Texture();
	void createAtiFloatLightConeShader_NoTexture();
	void createAtiDepthPixelShader();
	void createAtiShadowPixelShader();
	void createAtiShadowSolidPixelShader();
	void createAtiShadowTerrainPixelShader();
	void createAtiNoShadowPixelShader();
	void createAtiNoShadowTerrainPixelShader();
	void createAtiFloatDepthPixelShader();
	void createAtiFloatShadowPixelShader();
	void createAtiFloatShadowTerrainPixelShader();
	void createAtiFloatNoShadowPixelShader();
	void createAtiFloatNoShadowTerrainPixelShader();
	void createNvShadowShader();
	void createNvSmoothShadowShader();
	void createNvNoShadowShader();
	void createNvConeShader_Texture();
	void createNvConeShader_NoTexture();

	void createBasePixelShader();
	void createLightingPixelShader_Lightmap();
	void createLightingPixelShader_Lightmap_Reflection();
	void createLightingPixelShader_Lightmap_LocalReflection();
	void createLightingPixelShader_LightmapNoTexture();
	void createLightingPixelShader_NoLightmap();
	void createLightingPixelShader_NoLightmap_Reflection();
	void createLightingPixelShader_NoLightmap_LocalReflection();
	void createLightingPixelShader_NoLightmapNoTexture();
	void createDepthShader();
	void createShadowPixelShader();
	void createFakeDepthPixelShader();
	void createFakeShadowPixelShader();
	void createDecalPixelShader();
	void createColorEffectPixelShader();
	void createColorEffectOffsetPixelShader();
	void createColorEffectOffsetPixelShader_NoGamma();
	void createProceduralShader();
	void createProceduralOffsetShader();
	void createProceduralOffsetBaseShader();
	void createBlackWhiteShader();
	void createOffsetBlendShader();

	void apply() const;
	bool hasShader() const;
};

class VertexBuffer
{
	CComPtr<IDirect3DVertexBuffer9> buffer;
	int vertexSize;
	int vertexAmount;
	bool dynamic;

public:
	VertexBuffer();
	~VertexBuffer();

	void release();
	void create(GfxDevice& device, int vertexAmount, int vertexSize, bool dynamic);
	void *lock();
	void *unsafeLock(int offset, int amount);
	void unlock();

	void apply(GfxDevice& device, int stream) const;
	operator bool() const;
};

class IndexBuffer
{
	CComPtr<IDirect3DIndexBuffer9> buffer;
	int faceAmount;
	bool dynamic;

	IStorm3D_Logger *logger;

public:
	IndexBuffer();
	~IndexBuffer();

	void setLogger(IStorm3D_Logger *logger);

	void release();
	void create(GfxDevice& device, int faceAmount, bool dynamic);
	unsigned short *lock();
	void unlock();

	void render(GfxDevice& device, int faceAmount, int maxIndex, int vertexOffset = 0, int startIndex = 0) const;
	operator bool() const;
};

inline float convX_SCtoDS(float x, float pixszx) {return x*pixszx - 1.0f;}
inline float convY_SCtoDS(float y, float pixszy) {return 1.0f - y*pixszy;}

void readFile(std::string &result, const std::string &fileName);

boost::shared_ptr<Storm3D_Texture> createSharedTexture(Storm3D_Texture *texture);

void setCurrentAnisotrophy(int max);
void applyMaxAnisotrophy(GfxDevice& device, int stageAmount);
void enableMinMagFiltering(GfxDevice& device, int startStage, int endStage, bool enable);
void enableMipFiltering(GfxDevice& device, int startStage, int endStage, bool enable);

void setInverseCulling(bool enable);
void setCulling(GfxDevice& device, DWORD type);

} // storm
} // frozenbyte

#endif
