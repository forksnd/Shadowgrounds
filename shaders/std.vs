uniform float4x4  uWVP : register(c0);

struct VS_IN
{
#ifdef VS_2D_POS
    float2 pos   : POSITION;
#else
    float3 pos   : POSITION;
#endif
#ifdef ENABLE_COLOR
    float4 color : COLOR;
#endif
#ifdef ENABLE_TEXTURE
    float2 uv    : TEXCOORD0;
#endif
};

struct VS_OUT
{
    float4 pos   : POSITION;
#ifdef ENABLE_COLOR
    float4 color : COLOR;
#endif
#ifdef ENABLE_TEXTURE
    float2 uv    : TEXCOORD0;
#endif
};

VS_OUT main(VS_IN vs_in)
{
    VS_OUT vs_out = (VS_OUT)0;

#ifdef VS_2D_POS
    vs_out.pos = float4(vs_in.pos, 0, 1);
#else
    vs_out.pos = mul(uWVP, float4(vs_in.pos, 1.0));
#endif

#ifdef ENABLE_COLOR
    vs_out.color = vs_in.color;
#endif

#ifdef ENABLE_TEXTURE
    vs_out.uv = vs_in.uv;
#endif

    return vs_out;
}
