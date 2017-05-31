struct VS_IN
{
    float4 p : POSITION;
    float4 diffuse   : COLOR0;
    float2 uv0 : TEXCOORD0;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float4 diffuse   : COLOR0;
    float4 lighting  : COLOR1;
    float2 uv0       : TEXCOORD0;
    float4 uv1       : TEXCOORD1;
    float fogFactor  : FOG;
};

float4x4 uWVP              : register(c0);
float3x4 uW                : register(c4);
float4x4 uFakeLightMatrix  : register(c16);
float2   uFog              : register(c20);
float4   uDiffuse          : register(c21);

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    float3 pos, worldPos;
    float4 projPos;

    pos = vs_in.p.xyz;
    
    worldPos = mul(uW, float4(pos, 1.0));
    projPos  = mul(uWVP, float4(pos, 1.0));

    vs_out.pos = projPos;
    vs_out.diffuse = uDiffuse;
    vs_out.lighting = vs_in.diffuse;
    vs_out.uv0 = vs_in.uv0;
    vs_out.uv1= mul(uFakeLightMatrix, projPos);
    vs_out.fogFactor = saturate((worldPos.y - uFog.x) * uFog.y); //Make a mad

    return vs_out;
}
