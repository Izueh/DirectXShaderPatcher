cbuffer G : register(b0) { float4 g; };
cbuffer L : register(b7) { float4 l; };
Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);
struct VSOut { float4 p : SV_Position; float2 uv : TEXCOORD0; };
VSOut vs(uint id : SV_VertexID) {
  VSOut o; float2 uv = float2((id << 1) & 2, id & 2);
  o.p = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); o.uv = uv; return o; }
float4 ps(VSOut i) : SV_Target { return g + l + tex.Sample(smp, i.uv); }
