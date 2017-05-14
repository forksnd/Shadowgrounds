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

	void createNvConeShader();

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

	void createGlowTex8Shader();

	void createNvConeShader_Texture();
	void createNvConeShader_NoTexture();

	void createColorEffectPixelShader();
	void createColorEffectOffsetPixelShader();
	void createColorEffectOffsetPixelShader_NoGamma();
	void createBlackWhiteShader();
	void createOffsetBlendShader();

	void apply() const;
	bool hasShader() const;
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
