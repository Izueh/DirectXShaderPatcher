// A "game" shader that knows nothing of Luma's cbuffer: what a recipe patches in the SM5.1 smoke test.
cbuffer G : register(b0) { float4 g; };
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
struct VSOut { float4 p : SV_Position; float2 uv : TEXCOORD0; };
float4 ps(VSOut i) : SV_Target { return g + tex.Sample(smp, i.uv); }
