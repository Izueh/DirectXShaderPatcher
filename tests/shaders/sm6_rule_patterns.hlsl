// Purpose-built input for Execute regression tests. Compile once using the
// command in README.md; tests load the checked-in binary without invoking DXC.
Texture2D<float4> source : register(t7);
RWStructuredBuffer<float4> destination : register(u0);

cbuffer Settings : register(b0) {
  float4 offsets;
  uint4 parameters;
};

[numthreads(1, 1, 1)] void main(uint3 id : SV_DispatchThreadID) {
  float4 sample = source.Load(int3(id.xy, 0));
  // One Frc call, a texture aggregate, cbuffer extracts, and live arithmetic.
  destination[id.x] = float4(frac(sample.x + offsets.x),
                             sample.y + offsets.y,
                             sample.z + offsets.z,
                             sample.w + offsets.w + asfloat(parameters.x));
}
