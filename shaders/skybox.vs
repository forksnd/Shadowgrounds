struct VS_IN
{
    float4 p : POSITION;
    float3 n : NORMAL;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct VS_OUT
{
    float4 pos       : POSITION;
    float2 uv0       : TEXCOORD0;
};

// World x View x Projection matrix
float4x4 uWVP        : register(c0);

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

    vs_out.pos = mul(uWVP, float4(vs_in.p.xyz, 1.0)).xyww;

    vs_out.uv0 = vs_in.uv0;

    return vs_out;
}