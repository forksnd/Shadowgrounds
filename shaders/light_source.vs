//#define LIGHT_TYPE 0
//#define MESH_TYPE 0

#define MESH_STATIC 0
#define MESH_SKELETAL 1
#define MESH_TERRAIN 2

#define LTYPE_FLAT 0
#define LTYPE_POINT 1
#define LTYPE_DIRECTIONAL 2

struct VS_IN
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float2 uv0 : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float4 diffuse   : COLOR0;
    float4 uv0       : TEXCOORD0;
    float4 uv1       : TEXCOORD1;
#if MESH_TYPE == MESH_TERRAIN
    float4 uv2       : TEXCOORD2;
#else
    float2 uv2       : TEXCOORD2;
#endif
    float  uv3       : TEXCOORD3;
};

// World x View x Projection matrix
float4x4 uWVP         : register(c0);
// World x View x Projection matrix
float3x4 uW           : register(c4);

// Shadow and projection texture matrices
float4x4 uProjTexMat  : register(c8);
float4x4 uShadowMat   : register(c12);
#if MESH_TYPE == MESH_TERRAIN
float4x4 uTerrainMat   : register(c16);
#else
// Texture scrolling animation offset
float2   uTexOffset   : register(c16);
#endif

// Diffuse color
float4 uDiffuse     : register(c21);
float4 uLightSource : register(c23); 

#if MESH_TYPE == MESH_SKELETAL
    // Skeleton bones
    float3x4 uBoneXF[63] : register(c34);
#endif

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    float3 pos, worldPos;
    float4 projPos;
    float3 normal;

#if MESH_TYPE == MESH_SKELETAL
    float index0  = vs_in.uv2.x;
    float weight0 = vs_in.uv2.y;
    float index1  = vs_in.uv2.z;
    float weight1 = vs_in.uv2.w;
    float weight2 = 1.0 - weight0 - weight1;

    pos  = weight0 * mul(uBoneXF[index0], float4(vs_in.p.xyz, 1.0));
    pos += weight1 * mul(uBoneXF[index1], float4(vs_in.p.xyz, 1.0));
    pos += weight2 * vs_in.p.xyz;

    normal = mul(uBoneXF[index0], float4(vs_in.n, 0.0));
#else
    normal = vs_in.n;
    pos    = vs_in.p.xyz;
#endif

    worldPos = mul(uW, float4(pos, 1.0));
    normal   = mul(uW, float4(normal, 0.0));
    projPos  = mul(uWVP, float4(pos, 1.0));

    vs_out.pos = projPos;
    //; ** TEXTURING **
    //; Move texture coordinates
    //dp4 r1.x, r0, c[4]
    //dp4 r1.y, r0, c[5]
    //dp4 r1.z, r0, c[6]
    //dp4 r1.w, r0, c[7]
    //dp4 r2.x, r0, c[12]
    //dp4 r2.y, r0, c[13]
    //dp4 r2.z, r0, c[14]
    //dp4 r2.w, r0, c[15]
    //
    //mov oT0, r1
    //mov oT1, r2
    //add oT2.xy, v1, c[16]
    float4 texturePos = mul(uProjTexMat, float4(pos, 1.0));
    float4 shadowPos  = mul(uShadowMat, float4(pos, 1.0));
    vs_out.uv0 = texturePos;
    vs_out.uv1 = shadowPos;
    
#if MESH_TYPE == MESH_TERRAIN    
    vs_out.uv2 = mul(uTerrainMat, projPos);
#else
    vs_out.uv2 = vs_in.uv0 + uTexOffset; //apply texture offset
#endif

    float lighting = 1.0;

#if LIGHT_TYPE == LTYPE_POINT
    float3 lightDir = uLightSource.xyz - worldPos;

    vs_out.uv3 = length(lightDir)*uLightSource.w;

    lighting = dot(normal, normalize(lightDir));
    lighting = max(lighting, uLightSource.w);
#elif LIGHT_TYPE == LTYPE_DIRECTIONAL
    vs_out.uv3 = shadowPos.z*uLightSource.w;

    lighting = dot(normal, uLightSource.xyz);
    lighting = max(lighting, uLightSource.w);
#else
    vs_out.uv3 = shadowPos.z*uLightSource.w;
#endif

    vs_out.diffuse = float4(uDiffuse.xyz * lighting, uDiffuse.w);
    //vs_out.diffuse = float4(lighting.xxx, 1.0);


    return vs_out;
}
