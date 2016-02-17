// Copyright 2002-2004 Frozenbyte Ltd.

#pragma once

#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

//------------------------------------------------------------------
// Includes
//------------------------------------------------------------------
#include "GfxDevice.h"
#include <cassert>
#include <d3dx9math.h>
#include "Storm3D_Datatypes.h"
#include <vector>
#include "storm3d_terrain_utils.h"

//------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------
class Storm3D_Model_Object;
class Storm3D_Model;

template <class T>
class Singleton
{
	static T *instance;

public:
	Singleton() 
	{
		assert(instance == 0);

		// Some pointer magic from 'Gems
		//	-> Cast dump pointer to both types and store relative address
		int pointer_super = (int) (T*) 1;
		int pointer_derived = (int) (Singleton<T>*)(T*) 1;
		int offset = pointer_super - pointer_derived;
		
		// Use offset to get our instance address
		instance = (T*) (int)(this) + offset;
	}
	
	~Singleton()
	{
		assert(instance);
		instance = 0;
	}

	static T* GetSingleton()
	{
		assert(instance);
		return instance;
	}
};

// Initialize static instance to 0
template<class T> T* Singleton<T>::instance = 0;


//------------------------------------------------------------------
// Storm3D_ShaderManager
//	-> Keeps track of related properties (lightning, ...)
//	-> Manages shaders with requested properties
//	-> 
//------------------------------------------------------------------
class Storm3D_ShaderManager: public Singleton<Storm3D_ShaderManager>
{
public:
    enum
    {
        MESH_BONE_SIMPLE,
        MESH_BONE_NOREFLECTION,
        MESH_BONE_REFLECTION,
        MESH_NOREFLECTION,
        MESH_REFLECTION,
        MESH_LOCAL_REFLECTION,
        MESH_BONE_PROJECTED_FLAT,
        MESH_BONE_PROJECTED_POINT,
        MESH_BONE_PROJECTED_DIRECTIONAL,
        MESH_PROJECTED_FLAT,
        MESH_PROJECTED_POINT,
        MESH_PROJECTED_DIRECTIONAL,
        VERTEX_SKYBOX,
        VERTEX_FAKE_SPOT_SHADOW,
        MESH_VS_SHADER_COUNT,
    };

    enum
    {
        DEBUG_PIXEL_SHADER,
        FAKE_DEPTH,
        LIGHTING_SIMPLE_NOTEXTURE,
        LIGHTING_SIMPLE_TEXTURE,
        LIGHTING_LMAP_NOTEXTURE,
        LIGHTING_LMAP_TEXTURE,
        LIGHTING_LMAP_REFLECTION,
        LIGHTING_LMAP_LOCAL_REFLECTION,
        LIGHTING_NOLMAP_NOTEXTURE,
        LIGHTING_NOLMAP_TEXTURE,
        LIGHTING_NOLMAP_REFLECTION,
        LIGHTING_NOLMAP_LOCAL_REFLECTION,
        PIXEL_FAKE_SPOT_SHADOW,
        WHITE_ONLY,
        TEXTURE_ONLY,
        TEXTURExCOLOR,
        PIXEL_SHADOW,
        PIXEL_NO_SHADOW,
        MESH_PS_SHADER_COUNT,
    };

public:
    void SetShaders(
        GfxDevice& device,
        uint32_t vertexShader,
        uint32_t pixelShader,
        Storm3D_Model_Object *object
    );

    void SetPixelShader(uint32_t pixelShader);

private:
    LPDIRECT3DVERTEXSHADER9 meshVS[MESH_VS_SHADER_COUNT];
    LPDIRECT3DPIXELSHADER9  meshPS[MESH_PS_SHADER_COUNT];

    uint32_t currentVertexShader = 0;
    uint32_t currentPixelShader = 0;

    // View matrix
	D3DXMATRIX view_projection_tm;
	D3DXMATRIX view_tm;
	D3DXMATRIX texture_matrix;
	D3DXVECTOR4 view_position;

	// Lighting
	D3DXVECTOR4 ambient_color;
	D3DXVECTOR4 ambient_force_color;
	D3DXVECTOR4 fog;
	D3DXVECTOR4 fog_color;
	D3DXVECTOR4 textureOffset;
	D3DXVECTOR4 fake_properties;

	/*
	D3DXVECTOR4 light_position1;
	D3DXVECTOR4 light_color1;
	D3DXVECTOR4 light_position2;
	D3DXVECTOR4 light_color2;
	*/
	D3DXVECTOR4 light_position[LIGHT_MAX_AMOUNT];
	D3DXVECTOR4 light_color[LIGHT_MAX_AMOUNT];

	D3DXVECTOR4 sun_properties;
	D3DXVECTOR4 spot_position;
	D3DXVECTOR4 spot_properties;
	D3DXVECTOR4 spot_color;
	D3DXMATRIX target_matrix;

	D3DXVECTOR4 model_ambient_color;
	D3DXVECTOR4 object_ambient_color;
	D3DXVECTOR4 object_diffuse_color;
	D3DXVECTOR4 lightmap_factor;

	// For lazy updating
	bool update_values;

    //TODO: refactor!!!!
	bool lighting_shaders;
	bool projected_shaders;
	bool fake_depth_shaders;
	bool fake_shadow_shaders;
    bool use_custom_shader = false;

	// Stored model pointer (on bone meshes)
	Storm3D_Model *model;

	VC2I targetPos;
	VC2I targetSize;

	float transparency_factor;

	bool reflection;
	bool local_reflection;
	int light_count;
	bool light_params_changed;

public:
	Storm3D_ShaderManager(GfxDevice& device);
	~Storm3D_ShaderManager();
	
	// Create shaders
	void CreateShaders(GfxDevice& device);

	void setLightingParameters(bool reflection_, bool local_reflection_, int light_count_);

	// Set properties
	void SetTransparencyFactor(float factor);
	void SetViewProjectionMatrix(const D3DXMATRIX &vp, const D3DXMATRIX &view);
	void SetViewPosition(const D3DXVECTOR4 &p);
	void SetAmbient(const Color &color);
	void SetForceAmbient(const Color &color);
	void SetLight(int index, const Vector &position, const Color &color, float range);
	void SetSun(const VC3 &direction, float strength);
	void SetFog(float start, float range);
	void SetFogColor(const Color& color);
	void SetTextureOffset(const VC2 &offset);
	void setFakeProperties(float plane, float factor, float add);

	void SetModelAmbient(const Color &color);
	void SetObjectAmbient(const Color &color);
	void SetObjectDiffuse(const Color &color);

	void setProjectedShaders();
	void setFakeDepthShaders();
	void setFakeShadowShaders();
	void setLightingShaders();
	void setNormalShaders();

	void setTextureTm(D3DXMATRIX &matrix);
	void setSpot(const COL &color, const VC3 &position, const VC3 &direction, float range, float fadeFactor);
	void setSpotTarget(const D3DXMATRIX &matrix);

	enum SpotType
	{
		Flat = 0,
		Point = 1,
		Directional = 2
	};

	void setSpotType(SpotType type);

	bool BoneShader();

	// Do the magic ;-)
	void SetShader(GfxDevice& device, Storm3D_Model_Object *object);
	void SetShader(GfxDevice& device, const std::vector<int> &bone_indices); // not including first identity
	void ResetShader();
	void ClearCache();

	void SetShaderDefaultValues(GfxDevice& device);
	void SetShaderAmbient(GfxDevice& device, const COL &color);
	void SetShaderDiffuse(GfxDevice& device, const COL &color);
	void SetLightmapFactor(float xf, float yf);

	void ApplyForceAmbient(GfxDevice& device);
	void SetWorldTransform(GfxDevice& device, const D3DXMATRIX &tm, bool forceTextureTm = false, bool terrain = false);

	// Public constants
	static const int BONE_INDEX_START; // First is identity
	static const int BONE_INDICES;

private:
	SpotType spot_type;
};
