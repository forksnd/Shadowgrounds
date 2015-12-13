//#define ENABLE_REFLECTION
//#define ENABLE_LOCAL_REFLECTION
//#define ENABLE_SKELETAL_ANIMATION
 
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
    float4 p : POSITION;
    float4 color : COLOR;
    float4 color1 : COLOR1;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
    float4 uv2 : TEXCOORD2;
#if defined(ENABLE_REFLECTION)
    float3 uv3 : TEXCOORD3;
#elif defined(ENABLE_LOCAL_REFLECTION)
    float4 uv3 : TEXCOORD3;
#endif
    float4 fog : FOG;
};

// Shader with bone deforming (1 weight):
//   -> Transform vertex/normals
//   -> Apply ambient color
//   -> Apply surface color
//   -> Apply directional light
//   -> Apply base texturing 
   
// Constant declarations:
//   -> c[0..3] -> World x View x Projection matrix
//   -> c[4..7] -> World matrix (first 3 rows)   
//   -> c[7]    -> Ambient color (.w transparency)
//   -> c[8]    -> Diffuse color
//   -> c[9]    -> Light position1 (.w 0.f for clamping)
//   -> c[10]   -> Light color1 (.w -> 1 / range)
//   -> c[11]   -> Sun direction (.w offset.x)
//   -> c[12]   -> Light position2 (.w offset.y)
//   -> c[13]   -> Light color2 (.w -> 1 / range)
//   -> c[18]   -> Camera position
//   -> c[19]   -> Fog
//   -> c[21+]  -> Lights 3-5
   
// Vertex data:
//   -> v0 -> position
//   -> v1,2 -> texture coordinates
//   -> v3 -> normal
//   -> v6 -> index 1 / weight 1 (x,y) & index 2 / weight 2 (z,w)

float4x4 uWVP       : register(c0);
float3x4 uW         : register(c4);
float4   uAmbient   : register(c7); 
float4   uDiffuse   : register(c8); 
float2   uTexOffset : register(c9);
float3   uSunDir    : register(c11);
float4x4 uTexUVProj : register(c14);
float3   uCameraPos : register(c18);
float4   uFog       : register(c19);

int uNumLights : register(i0);

#if defined(ENABLE_LOCAL_REFLECTION)
float uReflMatrix : register(c27);
#endif

struct Light
{
    float4 posInvRange;
    float3 color;
};

Light uLights[5] : register(c32);

float3x4 uBoneXF[48] : register(c42);

static const float hack_base_index = 27;

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    float index0, weight0;
    float index1, weight1;
    float weight2;
    float3 pos, weightedPos, worldPos;
    float3 normal;
    float4 projPos;
    float3 lighting;

    pos = vs_in.p.xyz;

#if defined(ENABLE_SKELETAL_ANIMATION)    
    //a0.x = vs_in.uv2.x;
    //r0.xyz = m4x3(vs_in.p, a0.x);
    //r0.xyz = r0.xyz * vs_in.uv2.yyy;
    index0      = (vs_in.uv2.x - hack_base_index) / 3;
    weight0     = vs_in.uv2.y;
    weightedPos = weight0 * mul(uBoneXF[index0], float4(pos, 1.0));

    //a0.x = vs_in.uv2.z;
    //r1.xyz = m4x3(vs_in.p, a0.x);
    //r0.xyz = r1.xyz*vs_in.uv2.www + r0.xyz;
    index1       = (vs_in.uv2.z - hack_base_index) / 3;
    weight1      = vs_in.uv2.w;
    weightedPos += weight1 * mul(uBoneXF[index1], float4(pos, 1.0));

    //r0.w = vs_in.p.w;
    //r4.x = r0.w - vs_in.uv2.y;
    //r4.x = r4.x - vs_in.uv2.w;
    //r0.xyz = vs_in.p.xyz*r4.xxx + r0.xyz;
    weight2 = 1.0 - weight0 - weight1;
    weightedPos += weight2 * pos;

    //r7.xyz = m4x3(r0, 4); //world_pos
    worldPos = mul(uW, float4(weightedPos, 1.0));

    //r8 = m4x4(r0, 0);
    //vs_out.p = r8.xyzw;
    projPos = mul(uWVP, float4(weightedPos, 1.0));

    //a0.x = vs_in.uv2.x;
    //r2.xyz = m3x3(vs_in.n.xyz, a0.x);
    normal = mul(uBoneXF[index0], float4(vs_in.n, 0.0));
    //r5.xyz = m3x3(r2.xyz, 4);
    normal = mul(uW, float4(normal, 0.0));
#else
    // m4x3 r7, v0, c[4]
    worldPos = mul(uW, float4(pos, 1.0));

    // m4x4 r8, v0, c[0]
    // mov oPos, r8
    projPos = mul(uWVP, float4(pos, 1.0));

    // m3x3 r5, v3, c[4]
    normal = mul(uW, float4(vs_in.n, 0.0));
#endif

    vs_out.p = projPos;

    normal = normalize(normal);
    
    lighting = 0;
    for (int i = 0; i < uNumLights; ++i)
    {
        //r1.xyz = c[9].xyz - r7.xyz;
        float3 vec = uLights[i].posInvRange.xyz - worldPos;
        
        //r2.x = dp3(r1.xyz, r1.xyz);
        //r2.x = rsq(r2.x);
        //r1 = r1.xyzz * r2.xxxx;
        float3 dir = normalize(vec);

        //r6.x = dp3(r5.xyz, r1.xyz);
        //r6.x = max(r6.x, c[9].w);
        float LdotN = saturate(dot(normal, dir));
        
        //r3.x = rcp(r2.x);
        //r3 = r3.xyzw * c[10].wwww;
        //r3 = vs_in.p.wwww - r3.xxxx;
        //r3 = max(r3.xxxx, c[9].wwww);
        float dist  = length(dir);
        float att   = saturate(1.0 - dist * uLights[i].posInvRange.w);

        //r6.x = r6.x * r3.x;
        //r0 = r6.xxxx * c[10].xyzw;
        lighting += (att * LdotN) * uLights[i].color;
    }
    
    //r1.x = dp3(r5.xyz, c[11].xyz);
    //r1.x = max(r1.x, c[9].w);
    float sunNdotL = saturate(dot(normal, uSunDir));

    //r0.xyz = r0.xyz + r1.xxx;
    lighting += sunNdotL;
    
    //r0 = c[7].xyzz + r0.xyzz;
    lighting += uAmbient.xyz;
    
    //vs_out.color.xyz = min(r0.xyz, vs_in.p.www);
    //vs_out.color.w = c[7].w;
    vs_out.color.xyz = saturate(lighting);
    vs_out.color.w = uAmbient.w; //Transparency
    
    //vs_out.color1 = c[8].xyzw;
    vs_out.color1 = uDiffuse;
    
    //vs_out.uv0.x = vs_in.uv0.x + c[11].w;
    //vs_out.uv0.y = vs_in.uv0.y + c[12].w;
    vs_out.uv0.xy = vs_in.uv0.xy + uTexOffset; //apply texture offset

    vs_out.uv1.xy = vs_in.uv1.xy;

    //r0 = m4x4(r8, 14);
    //vs_out.uv2 = r0.xyzw;
    vs_out.uv2 = mul(uTexUVProj, projPos);
 
#if defined(ENABLE_REFLECTION)
    //sub r6.xyz, c[18].xyz, r7.xyz
    //dp3 r8, r6, r5
    //add r8, r8, r8
    //mul r8, r8, r5.xyz
    //sub oT3.xyz, r6.xyz, r8 ; -> reflected normal
    float3 v = uCameraPos - worldPos;
    vs_out.uv3 = v - (2 * dot(v, normal)) * normal;
#elif defined(ENABLE_LOCAL_REFLECTION)
    //m4x4 r0, r8, c[27]
    //mov oT3, r0
    vs_out.uv3 = mul(uReflMatrix, projPos);
#endif
 
    //r0 = r7.yyyy - c[19].xxxx;
    //vs_out.fog = r0.xxxx * c[19].yyyy;
    vs_out.fog = (worldPos.y - uFog.x) * uFog.y; //Make a mad

    return vs_out;
}
