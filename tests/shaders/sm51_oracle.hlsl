cbuffer A : register(b1) { float4 a[3]; };
cbuffer B : register(b5) { float4 b; };
Texture2D<float4> t0 : register(t0);
Texture2D<float4> t7 : register(t7);
RWTexture2D<float4> u2 : register(u2);
SamplerState s3 : register(s3);
float4 main(float2 uv : TEXCOORD0, uint i : TEXCOORD1) : SV_Target {
  float4 c = t0.Sample(s3, uv) + t7.Load(int3(uv,0)) + a[i] + b;
  u2[int2(uv)] = c;
  return c;
}
