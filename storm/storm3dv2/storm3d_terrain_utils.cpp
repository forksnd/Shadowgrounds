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

extern int storm3d_dip_calls;

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

void VertexShader::createTerrainShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

	//handle = createVertexShader(device, "");
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createNvTerrainShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\nv_terrain_vertex_shader.txt");
#else
	handle = createVertexShader(device, "data\\shader\\nv_terrain_vertex_shader.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createNvLightingShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\nv_terrain_lighting_vertex_shader.txt");
#else
	handle = createVertexShader(device, "data\\shader\\nv_terrain_lighting_vertex_shader.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createAtiDepthTerrainShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\ati_depth_default_vertex_shader.txt");
#else
	handle = createVertexShader(device, "data\\shader\\ati_depth_default_vertex_shader.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createNvTerrainShadowShaderDirectional()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\nv_shadow_terrain_vertex_shader_dir.txt");
#else
	handle = createVertexShader(device, "data\\shader\\nv_shadow_terrain_vertex_shader_dir.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createNvTerrainShadowShaderPoint()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\nv_shadow_terrain_vertex_shader_point.txt");
#else
	handle = createVertexShader(device, "data\\shader\\nv_shadow_terrain_vertex_shader_point.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createNvTerrainShadowShaderFlat()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(1, 0, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(1, 2*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(createElement(1, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 2));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\nv_shadow_terrain_vertex_shader_flat.txt");
#else
	handle = createVertexShader(device, "data\\shader\\nv_shadow_terrain_vertex_shader_flat.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
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

void VertexShader::createDecalShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(0, 6*4, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR));
	elements.push_back(createElement(0, 7*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(0, 9*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\decal_vertex_shader.txt");
#else
	handle = createVertexShader(device, "data\\shader\\decal_vertex_shader.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createDecalPointShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(0, 6*4, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR));
	elements.push_back(createElement(0, 7*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(0, 9*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\decal_projection_vertex_shader_point.txt");
#else
	handle = createVertexShader(device, "data\\shader\\decal_projection_vertex_shader_point.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createDecalDirShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(0, 6*4, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR));
	elements.push_back(createElement(0, 7*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(0, 9*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\decal_projection_vertex_shader_dir.txt");
#else
	handle = createVertexShader(device, "data\\shader\\decal_projection_vertex_shader_dir.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createDecalFlatShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 3*4, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL));
	elements.push_back(createElement(0, 6*4, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR));
	elements.push_back(createElement(0, 7*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(0, 9*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\decal_projection_vertex_shader_flat.txt");
#else
	handle = createVertexShader(device, "data\\shader\\decal_projection_vertex_shader_flat.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createConeStencilShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\cone_stencil_vertex_shader.txt");
#else
	handle = createVertexShader(device, "data\\shader\\cone_stencil_vertex_shader.fvs");
#endif
	declaration = 0;
	device.CreateVertexDeclaration(&elements[0], &declaration);
}

void VertexShader::createProceduralShader()
{
	elements.clear();
	elements.push_back(createElement(0, 0, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION));
	elements.push_back(createElement(0, 4*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 0));
	elements.push_back(createElement(0, 6*4, D3DDECLTYPE_FLOAT2, D3DDECLUSAGE_TEXCOORD, 1));
	elements.push_back(end);

#ifdef LEGACY_FILES
	handle = createVertexShader(device, "Data\\Shaders\\procedural_vertex_shader.txt");
#else
	handle = createVertexShader(device, "data\\shader\\procedural_vertex_shader.fvs");
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

void PixelShader::createTerrainShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\terrain_pixel_shader.txt");
}

void PixelShader::createTerrainLightShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\terrain_lighting_pixel_shader.txt");
}

void PixelShader::createGlowTex8Shader()
{
	handle = createPixelShader(device, "Data\\Shaders\\glow_8tex_pixel_shader.txt");
}

void PixelShader::createLightShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\lightmap_pixel_shader.txt");
}

void PixelShader::createNvShadowShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_shadow_pixel_shader.txt");
}

void PixelShader::createNvSmoothShadowShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_shadow_pixel_shader_smooth.txt");
}

void PixelShader::createNvNoShadowShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_noshadow_pixel_shader.txt");
}

void PixelShader::createNvConeShader_Texture()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_cone_pixel_shader_texture.txt");
}

void PixelShader::createNvConeShader_NoTexture()
{
	handle = createPixelShader(device, "Data\\Shaders\\nv_cone_pixel_shader_notexture.txt");
}

void PixelShader::createDecalPixelShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\decal_pixel_shader.txt");
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

void PixelShader::createProceduralShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\procedural_pixel_shader.txt");
}

void PixelShader::createProceduralOffsetShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\procedural_offset_pixel_shader.txt");
}

void PixelShader::createProceduralOffsetBaseShader()
{
	handle = createPixelShader(device, "Data\\Shaders\\procedural_offset_base_pixel_shader.txt");
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



void PixelShader::createTerrainShader()
{
	handle = createPixelShader(device, "data\\shader\\terrain_pixel_shader.fps");
}

void PixelShader::createTerrainLightShader()
{
	handle = createPixelShader(device, "data\\shader\\terrain_lighting_pixel_shader.fps");
}

void PixelShader::createGlowTex8Shader()
{
	handle = createPixelShader(device, "data\\shader\\glow_8tex_pixel_shader.fps");
}

void PixelShader::createLightShader()
{
	handle = createPixelShader(device, "data\\shader\\lightmap_pixel_shader.fps");
}

void PixelShader::createNvConeShader_Texture()
{
	handle = createPixelShader(device, "data\\shader\\nv_cone_pixel_shader_texture.fps");
}

void PixelShader::createNvConeShader_NoTexture()
{
	handle = createPixelShader(device, "data\\shader\\nv_cone_pixel_shader_notexture.fps");
}

void PixelShader::createDecalPixelShader()
{
	handle = createPixelShader(device, "data\\shader\\decal_pixel_shader.fps");
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

void PixelShader::createProceduralShader()
{
	handle = createPixelShader(device, "data\\shader\\procedural_pixel_shader.fps");
}

void PixelShader::createProceduralOffsetShader()
{
	handle = createPixelShader(device, "data\\shader\\procedural_offset_pixel_shader.fps");
}

void PixelShader::createProceduralOffsetBaseShader()
{
	handle = createPixelShader(device, "data\\shader\\procedural_offset_base_pixel_shader.fps");
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

// ---
/*
StateBlock::StateBlock()
:	handle(0),
	device(0)
{
}

StateBlock::~StateBlock()
{
	if(handle)
		device->DeleteStateBlock(handle);
}

void StateBlock::setDevice(IDirect3DDevice9 &device_)
{
	device = &device_;
}

void StateBlock::startRecording()
{
	assert(device);
	device->BeginStateBlock();
}

void StateBlock::endRecording()
{
	assert(device);
	device->EndStateBlock(&handle);
}

void StateBlock::apply() const
{
	assert(device);
	assert(handle);

	device->ApplyStateBlock(handle);
}
*/
// --

VertexBuffer::VertexBuffer()
:	vertexSize(0),
	vertexAmount(0),
	dynamic(false)
{
}

VertexBuffer::~VertexBuffer()
{
}

void VertexBuffer::release()
{
	buffer.Release();
}

void VertexBuffer::create(gfx::Device& device, int vertexAmount_, int vertexSize_, bool dynamic_)
{
	if(vertexSize_ == vertexSize)
	if(vertexAmount >= vertexAmount_)
	if(buffer)
	if(dynamic == dynamic_)
		return;

	buffer.Release();
	vertexSize = vertexSize_;
	vertexAmount = vertexAmount_;
	dynamic = dynamic_;

	DWORD usage = D3DUSAGE_WRITEONLY;
	D3DPOOL pool = D3DPOOL_MANAGED;

	if(dynamic)
	{
		usage |= D3DUSAGE_DYNAMIC;
		pool = D3DPOOL_DEFAULT;

		//device.EvictManagedResources();
	}

	device.CreateVertexBuffer(vertexAmount * vertexSize, usage, 0, pool, &buffer, 0);
}

void *VertexBuffer::lock()
{
	void *pointer = 0;
	DWORD flags = dynamic ? D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK : 0;

	buffer->Lock(0, 0, &pointer, flags);
	return pointer;
}

void *VertexBuffer::unsafeLock(int offset, int amount)
{
	void *pointer = 0;
	DWORD flags = dynamic ? D3DLOCK_NOOVERWRITE | D3DLOCK_NOSYSLOCK : 0;

	buffer->Lock(offset * vertexSize, amount * vertexSize, &pointer, flags);
	return pointer;
}

void VertexBuffer::unlock()
{
	buffer->Unlock();
}

void VertexBuffer::apply(gfx::Device& device, int stream) const
{
	device.SetStreamSource(stream, buffer, 0, vertexSize);
}

VertexBuffer::operator bool() const
{
	return buffer != 0;
}

// --

IndexBuffer::IndexBuffer()
:	faceAmount(0),
	dynamic(false),
	logger(0)
{
}

IndexBuffer::~IndexBuffer()
{
}

void IndexBuffer::setLogger(IStorm3D_Logger *logger_)
{
	logger = logger_;
}

void IndexBuffer::release()
{
	buffer.Release();
}

void IndexBuffer::create(gfx::Device& device, int faceAmount_, bool dynamic_)
{
	if(faceAmount >= faceAmount_)
	if(buffer)
	if(dynamic == dynamic_)
		return;

	faceAmount = faceAmount_;
	dynamic = dynamic_;
	buffer.Release();

	DWORD usage = D3DUSAGE_WRITEONLY;
	D3DPOOL pool = D3DPOOL_MANAGED;

	if(dynamic)
	{
		usage = D3DUSAGE_DYNAMIC;
		pool = D3DPOOL_DEFAULT;

		//device.EvictManagedResources();
	}

	device.CreateIndexBuffer(faceAmount * 3 * sizeof(unsigned short), usage, D3DFMT_INDEX16, pool, &buffer, 0);
}

unsigned short *IndexBuffer::lock()
{
	void *pointer = 0;
	//DWORD flags = dynamic ? D3DLOCK_DISCARD : 0;
	DWORD flags = dynamic ? D3DLOCK_DISCARD | D3DLOCK_NOSYSLOCK : 0;

	buffer->Lock(0, 0, &pointer, flags);
	return reinterpret_cast<unsigned short *> (pointer);
}

void IndexBuffer::unlock()
{
	buffer->Unlock();
}

void IndexBuffer::render(gfx::Device& device, int faceAmount, int maxIndex, int vertexOffset, int startIndex) const
{
	device.SetIndices(buffer);
	
	device.DrawIndexedPrimitive(D3DPT_TRIANGLELIST, vertexOffset, 0, maxIndex, startIndex, faceAmount);

	++storm3d_dip_calls;
}

IndexBuffer::operator bool() const
{
	return buffer != 0;
}

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

void IndexStorage16::init(gfx::Device& device, uint32_t size, uint16_t max_allocs)
{
    device.CreateIndexBuffer(size, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &indices, NULL);
    allocator = etlsf_create(size, max_allocs);
    locked    = 0;
}

void IndexStorage16::fini()
{
    etlsf_destroy(allocator);
    indices->Release();

    locked    = 0;
    indices   = 0;
    allocator = 0;
}

uint16_t IndexStorage16::alloc(uint32_t numIndices)
{
    return etlsf_alloc(allocator, numIndices * sizeof(uint16_t));
}

void IndexStorage16::free(uint16_t id)
{
    etlsf_free(allocator, id);
}

uint16_t* IndexStorage16::lock(uint16_t id)
{
    assert(locked == 0);
    assert(etlsf_is_block_valid(allocator, id));
    
    locked = id;

    uint32_t offset = etlsf_block_offset(allocator, id);
    uint32_t size   = etlsf_block_size(allocator, id);

    void* ptr = 0;
    indices->Lock(offset, size, &ptr, 0);

    return (uint16_t*)ptr;
}

void IndexStorage16::unlock()
{
    assert(locked != 0);

    indices->Unlock();

    locked = 0;
}

uint32_t IndexStorage16::baseIndex(uint16_t id)
{
    return etlsf_block_offset(allocator, id) / sizeof(uint16_t);
}



void VertexStorage::init(gfx::Device& device, uint32_t size, uint16_t max_allocs)
{
    device.CreateVertexBuffer(size, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &vertices, NULL);
    allocator = etlsf_create(size, max_allocs);
    locked    = 0;
}

void VertexStorage::fini()
{
    etlsf_destroy(allocator);
    vertices->Release();

    locked    = 0;
    vertices  = 0;
    allocator = 0;
}

uint16_t VertexStorage::alloc(uint32_t numVertices, uint32_t stride)
{
    return etlsf_alloc(allocator, numVertices * stride + stride);
}

void VertexStorage::free(uint16_t id)
{
    etlsf_free(allocator, id);
}

void* VertexStorage::lock(uint16_t id, uint32_t stride)
{
    assert(locked == 0);
    assert(etlsf_is_block_valid(allocator, id));
    
    locked = id;

    uint32_t offset = etlsf_block_offset(allocator, id);
    uint32_t size   = etlsf_block_size(allocator, id);
    uint32_t rem    = offset % stride;

    uint8_t* ptr = 0;
    vertices->Lock(offset, size, (void**)&ptr, 0);

    return ptr + ((rem==0) ? 0 : stride-rem);
}

void VertexStorage::unlock()
{
    assert(locked != 0);

    vertices->Unlock();

    locked = 0;
}

uint32_t VertexStorage::baseVertex(uint16_t id, uint32_t stride)
{
    return offset(id, stride) / stride;
}

uint32_t VertexStorage::offset(uint16_t id, uint32_t stride)
{
    uint32_t offset = etlsf_block_offset(allocator, id);

    uint32_t rem = offset % stride;
    offset += (rem==0) ? 0 : stride-rem;

    return offset;
}
