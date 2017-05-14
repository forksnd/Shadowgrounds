// Copyright 2002-2004 Frozenbyte Ltd.

#ifdef _MSC_VER
#pragma warning(disable:4103)
#endif

#include <fstream>
#include <vector>
#include <d3d9.h>
#include <stdio.h>

#include "storm3d_terrain_utils.h"
#include "storm3d_texture.h"
#include "IStorm3D_Logger.h"

#include "../../filesystem/input_stream.h"
#include "../../filesystem/file_package_manager.h"
#include "../../util/Debug_MemoryManager.h"

#ifndef NDEBUG
static std::string activePixelShader;
static std::string activeVertexShader;
static bool tracing = false;

void activeShaderNames() {
	fprintf(stderr, "vertex: %s\npixel: %s\n", activeVertexShader.c_str(), activePixelShader.c_str());
}

void setTracing(bool tracing_) {
	if (tracing != tracing_) {
		tracing = tracing_;
		fprintf(stderr, "Tracing %s\nCurrent shaders:", tracing_ ? "enabled" : "disabled");
		activeShaderNames();
	}
}

#endif

namespace frozenbyte {
namespace storm {

	void readFile(std::string &result, const std::string &fileName)
	{
        result.clear();
		filesystem::InputStream stream = filesystem::FilePackageManager::getInstance().getFile(fileName);
		while(!stream.isEof())
		{
			std::string line;
			while(!stream.isEof())
			{
				signed char c;
				stream >> c;

				if(c == '\r')
					continue;
				else if(c == '\n')
					break;
				
				line += c;
			}

			result += line;
			result += "\r\n";
		}

		/*
		std::ifstream stream(fileName.c_str());
		while(stream)
		{
			std::string line;
			std::getline(stream, line);

			result += line;
			result += "\r\n";
		}
		*/
	}

	CComPtr<IDirect3DVertexShader9> VertexShader::createVertexShader(gfx::Device& device, const std::string &fileName)
	{
		std::string shaderString;
		name = fileName;
		readFile(shaderString, fileName);

		ID3DXBuffer *assembledShader = 0;
		ID3DXBuffer *compilationErrors = 0;

		HRESULT hr = 0;

		if(!fileName.empty())
		{
			hr = D3DXAssembleShader(shaderString.c_str(), shaderString.size(), 0, 0, 0, &assembledShader, &compilationErrors);
			if(FAILED(hr))
			{
				char *foo = (char *) compilationErrors->GetBufferPointer();
				OutputDebugString(foo);

#ifdef LEGACY_FILES
				FILE *fp = fopen("vertex_shader_dbg.txt", "w");
#else
				FILE *fp = fopen("logs/vertex_shader_dbg.txt", "w");
#endif
				if (fp != NULL)
				{
					fprintf(fp, "File: %s\r\n", fileName.c_str());
					fprintf(fp, "Output: %s\r\n", foo);
					fclose(fp);
				}

				assert(!MessageBox(NULL, "D3DXAssembleShader failed!", "Shit happens", MB_OK));
			}
		}

		if(!assembledShader)
			return 0;

		CComPtr<IDirect3DVertexShader9> result;
		DWORD *buffer = (assembledShader) ? (DWORD*)assembledShader->GetBufferPointer() : 0;

		hr = device.CreateVertexShader(buffer, &result);
		if(FAILED(hr))
		{
			assert(MessageBox(NULL, "CreateVertexShader failed!", "Shit happens", MB_OK));
		}

		return result;
	}

	CComPtr<IDirect3DPixelShader9> PixelShader::createPixelShader(gfx::Device& device, const std::string &fileName)
	{
		std::string shaderString;
		readFile(shaderString, fileName);

		name = fileName;

		ID3DXBuffer *assembledShader = 0;
		ID3DXBuffer *compilationErrors = 0;

		HRESULT hr = D3DXAssembleShader(shaderString.c_str(), shaderString.size(), 0, 0, 0, &assembledShader, &compilationErrors);
		if(FAILED(hr))
		{
			char *foo = (char *) compilationErrors->GetBufferPointer();

#ifdef LEGACY_FILES
			FILE *fp = fopen("pixel_shader_dbg.txt", "w");
#else
			FILE *fp = fopen("logs/pixel_shader_dbg.txt", "w");
#endif
			if (fp != NULL)
			{
				fprintf(fp, "File: %s\r\n", fileName.c_str());
				fprintf(fp, "Output: %s\r\n", foo);
				fclose(fp);
			}

			assert(!MessageBox(NULL, "D3DXAssembleShader failed!", "Shit happens", MB_OK));
		}

		if(!assembledShader)
			return 0;

		CComPtr<IDirect3DPixelShader9> result;
		hr = device.CreatePixelShader((DWORD*)assembledShader->GetBufferPointer(), &result);

		if(FAILED(hr))
		{
			assert(MessageBox(NULL, "CreatePixelShader failed!", "Shit happens", MB_OK));
		}

		return result;
	}

	D3DVERTEXELEMENT9 createElement(int stream, int offset, int type, int usage, int index = 0)
	{
		D3DVERTEXELEMENT9 result;
		result.Stream = stream;
		result.Offset = offset;
		result.Type = type;
		result.Usage = usage;
		result.Method = 0;
		result.UsageIndex = index;

		return result;
	}

	static D3DVERTEXELEMENT9 end = D3DDECL_END();

VertexShader::VertexShader(gfx::Device& device_)
:	handle(0),
	device(device_)
{
}

VertexShader::~VertexShader()
{
}

void VertexShader::createNvConeShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(0, 6*4, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR));
	elements.push_back(createElement(0, 7*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\cone_vertex_shader_nv.txt");
#else
	handle = createVertexShader(device, "data\\shader\\cone_vertex_shader_nv.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::applyDeclaration() const
{
	device.SetVertexDeclaration(declaration);
}

void VertexShader::apply() const
{
#ifndef NDEBUG
	activeVertexShader = name;
	if (tracing) {
		fprintf(stderr, "activated vertex shader %s\n", name.c_str());
	}
#endif
	device.SetVertexDeclaration(declaration);
	device.SetVertexShader(handle);
}

// --

PixelShader::PixelShader(gfx::Device& device_)
:	handle(0),
	device(device_)
{
}

PixelShader::~PixelShader()
{
}

#ifdef LEGACY_FILES

void PixelShader::createGlowTex8Shader()
{
	handle = createPixelShader(device, "Data\\Shaders\\glow_8tex_pixel_shader.txt");
}

void PixelShader::createNvConeShader_Texture()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_cone_pixel_shader_texture.txt");
}

void PixelShader::createNvConeShader_NoTexture()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_cone_pixel_shader_notexture.txt");
}

void PixelShader::createColorEffectPixelShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\color_effect_pixel_shader.txt");
}

void PixelShader::createColorEffectOffsetPixelShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\color_effect_offset_pixel_shader.txt");
}

void PixelShader::createColorEffectOffsetPixelShader_NoGamma()
{
	handle = createPixelShader(device, "Data\\Shaders\\color_effect_offset_pixel_shader_nogamma.txt");
}

void PixelShader::createBlackWhiteShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\black_white_effect_pixel_shader.txt");
}

void PixelShader::createOffsetBlendShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\offset_blend_pixel_shader.txt");
}



#else

void PixelShader::createGlowTex8Shader()
{
	handle = createPixelShader(device, "data\\shader\\glow_8tex_pixel_shader.fps");
}

void PixelShader::createNvConeShader_Texture()
{
	handle = createPixelShader(device, "data\\shader\\nv_cone_pixel_shader_texture.fps");
}

void PixelShader::createNvConeShader_NoTexture()
{
	handle = createPixelShader(device, "data\\shader\\nv_cone_pixel_shader_notexture.fps");
}

void PixelShader::createColorEffectPixelShader()
{
	handle = createPixelShader(device, "data\\shader\\color_effect_pixel_shader.fps");
}

void PixelShader::createColorEffectOffsetPixelShader()
{
	handle = createPixelShader(device, "data\\shader\\color_effect_offset_pixel_shader.fps");
}

void PixelShader::createColorEffectOffsetPixelShader_NoGamma()
{
	handle = createPixelShader(device, "data\\shader\\color_effect_offset_pixel_shader_nogamma.fps");
}

void PixelShader::createBlackWhiteShader()
{
	handle = createPixelShader(device, "data\\shader\\black_white_effect_pixel_shader.fps");
}

void PixelShader::createOffsetBlendShader()
{
	handle = createPixelShader(device, "data\\shader\\offset_blend_pixel_shader.fps");
}


#endif

void PixelShader::apply() const
{
#ifndef NDEBUG
	activePixelShader = name;
	if (tracing) {
		fprintf(stderr, "activated pixel shader %s\n", name.c_str());
	}
#endif
	device.SetPixelShader(handle);
}

bool PixelShader::hasShader() const
{
	if(handle)
		return true;

	return false;
}

// --

std::shared_ptr<Storm3D_Texture> createSharedTexture(Storm3D_Texture *texture)
{
    if (texture)
    {
        texture->AddRef();
        return std::shared_ptr<Storm3D_Texture>(texture, [] (Storm3D_Texture* tex) {tex->Release();});
    }

    return std::shared_ptr<Storm3D_Texture>(texture);
}


// ...

static int currentAnisotrophy = 0;
void setCurrentAnisotrophy(int max)
{
	currentAnisotrophy = max;
}

void applyMaxAnisotrophy(gfx::Device& device, int stageAmount)
{
	if(currentAnisotrophy)
	{
		for(int i = 0; i < stageAmount; ++i)
			device.SetSamplerState(i, D3DSAMP_MAXANISOTROPY, currentAnisotrophy);
	}
}

void enableMinMagFiltering(gfx::Device& device, int startStage, int endStage, bool enable)
{
	DWORD minFilter = D3DTEXF_POINT;
	DWORD magFilter = D3DTEXF_POINT;

	if(enable)
	{
		magFilter = D3DTEXF_LINEAR;

		if(currentAnisotrophy)
			minFilter = D3DTEXF_ANISOTROPIC;
		else
			minFilter = D3DTEXF_LINEAR;
	}

	for(int i = startStage; i <= endStage; ++i)
	{
		device.SetSamplerState(i, D3DSAMP_MINFILTER, minFilter);
		device.SetSamplerState(i, D3DSAMP_MAGFILTER, magFilter);
	}
}

void enableMipFiltering(gfx::Device& device, int startStage, int endStage, bool enable)
{
	DWORD filter = D3DTEXF_NONE;
	if(enable)
		filter = D3DTEXF_LINEAR;

	for(int i = startStage; i <= endStage; ++i)
		device.SetSamplerState(i, D3DSAMP_MIPFILTER, filter);
}

static bool inverseCulling = false;
void setInverseCulling(bool enable)
{
	inverseCulling = enable;
}

void setCulling(gfx::Device& device, DWORD type)
{
	if(type == D3DCULL_NONE)
		device.SetRenderState(D3DRS_CULLMODE, type);
	else
	{
		if(type == D3DCULL_CCW)
		{
			if(!inverseCulling)
				device.SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
			else
				device.SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
		}
		else if(type == D3DCULL_CW)
		{
			if(!inverseCulling)
				device.SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
			else
				device.SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
		}
	}
}

} // storm
} // frozenbyte
