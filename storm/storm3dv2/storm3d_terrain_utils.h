// Copyright 2002-2004 Frozenbyte Ltd.

#ifndef INCLUDED_STORM3D_TERRAIN_UTILS_H
#define INCLUDED_STORM3D_TERRAIN_UTILS_H

#include <string>
#include "GfxDevice.h"
#include <atlbase.h>
#include <vector>
#include <memory>

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
	
	gfx::Device& device;
	std::vector<D3DVERTEXELEMENT9> elements;

	std::string name;
	CComPtr<IDirect3DVertexShader9> createVertexShader(gfx::Device& device, const std::string &fileName);

public:
	VertexShader(gfx::Device& device);
	~VertexShader();

	void createTerrainShader();
	void createNvTerrainShader();
	void createNvLightingShader();

	void createAtiDepthTerrainShader();
	void createNvTerrainShadowShaderDirectional();
	void createNvTerrainShadowShaderPoint();
	void createNvTerrainShadowShaderFlat();

	void createNvConeShader();
	void createConeStencilShader();
	void createDecalShader();
	void createDecalPointShader();
	void createDecalDirShader();
	void createDecalFlatShader();
	void createProceduralShader();

	void applyDeclaration() const;
	void apply() const;
};

class PixelShader
{
	CComPtr<IDirect3DPixelShader9> handle;
	gfx::Device& device;

	std::vector<D3DVERTEXELEMENT9> elements;

	std::string name;

	CComPtr<IDirect3DPixelShader9> createPixelShader(gfx::Device& device, const std::string &fileName);
public:
	PixelShader(gfx::Device &device);
	~PixelShader();

	void createTerrainShader();
	void createTerrainLightShader();
	void createGlowTex8Shader();
	void createLightShader();
	void createNvShadowShader();
	void createNvSmoothShadowShader();
	void createNvNoShadowShader();
	void createNvConeShader_Texture();
	void createNvConeShader_NoTexture();

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
	void create(gfx::Device& device, int vertexAmount, int vertexSize, bool dynamic);
	void *lock();
	void *unsafeLock(int offset, int amount);
	void unlock();

	void apply(gfx::Device& device, int stream) const;
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
	void create(gfx::Device& device, int faceAmount, bool dynamic);
	unsigned short *lock();
	void unlock();

	void render(gfx::Device& device, int faceAmount, int maxIndex, int vertexOffset = 0, int startIndex = 0) const;
	operator bool() const;
};

inline float convX_SCtoDS(float x, float pixszx) {return x*pixszx - 1.0f;}
inline float convY_SCtoDS(float y, float pixszy) {return 1.0f - y*pixszy;}

void readFile(std::string &result, const std::string &fileName);

std::shared_ptr<Storm3D_Texture> createSharedTexture(Storm3D_Texture *texture);

void setCurrentAnisotrophy(int max);
void applyMaxAnisotrophy(gfx::Device& device, int stageAmount);
void enableMinMagFiltering(gfx::Device& device, int startStage, int endStage, bool enable);
void enableMipFiltering(gfx::Device& device, int startStage, int endStage, bool enable);

void setInverseCulling(bool enable);
void setCulling(gfx::Device& device, DWORD type);

} // storm
} // frozenbyte

#endif
