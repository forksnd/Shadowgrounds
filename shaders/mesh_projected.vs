//#define ENABLE_SKELETAL_ANIMATION 0
//#define LIGHT_TYPE 0

#define LTYPE_FLAT 0
#define LTYPE_POINT 1
#define LTYPE_DIRECTIONAL 2

struct VS_IN
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float4 uv2 : TEXCOORD2;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float4 diffuse   : COLOR0;
    float4 uv0       : TEXCOORD0;
    float4 uv1       : TEXCOORD1;
    float2 uv2       : TEXCOORD2;
    float  uv3       : TEXCOORD3;
};

// World x View x Projection matrix
float4x4 uWVP         : register(c0);
// World x View x Projection matrix
float3x4 uW           : register(c8);
// Shadow and projection texture matrices
float4x4 uProjTexMat  : register(c4);
float4x4 uShadowMat   : register(c12);
float4 uLightPosRange : register(c11); 
// Texture scrolling animation offset
float2   uTexOffset   : register(c16);
// Diffuse color
float4   uDiffuse     : register(c17);

#if ENABLE_SKELETAL_ANIMATION
    // Skeleton bones
    float3x4 uBoneXF[56] : register(c42);
#endif

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    float3 pos, worldPos;
    float3 normal;

#if ENABLE_SKELETAL_ANIMATION
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

    vs_out.pos = mul(uWVP, float4(pos, 1.0));

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
    float4 shadowPos  = mul(uShadowMat, float4(pos, 1.0));
    float4 texturePos = mul(uProjTexMat, float4(pos, 1.0));
    vs_out.uv0 = texturePos;
    vs_out.uv1 = shadowPos;
    vs_out.uv2 = vs_in.uv0 + uTexOffset; //apply texture offset

    float lighting = 1.0;

#if LIGHT_TYPE == LTYPE_POINT
    float3 lightDir = uLightPosRange.xyz - worldPos;

    vs_out.uv3 = length(lightDir)*uLightPosRange.w;

    lighting = dot(normal, normalize(lightDir));
    lighting = max(lighting, uLightPosRange.w);
#elif LIGHT_TYPE == LTYPE_DIRECTIONAL
    vs_out.uv3 = shadowPos.z*uLightPosRange.w;

    lighting = dot(normal, uLightPosRange.xyz);
    lighting = max(lighting, uLightPosRange.w);
#else
    vs_out.uv3 = shadowPos.z*uLightPosRange.w;
#endif

    vs_out.diffuse = float4(uDiffuse.xyz * lighting, uDiffuse.w);
    //vs_out.diffuse = float4(lighting.xxx, 1.0);


    return vs_out;
}
