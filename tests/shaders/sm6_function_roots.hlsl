// Regenerate: dxc -T hs_6_6 -E main -Fo sm6_function_roots.hs_6_6.cso sm6_function_roots.hlsl
struct Vertex {
  float4 position : SV_Position;
};
struct Factors {
  float edges[3] : SV_TessFactor;
  float inside : SV_InsideTessFactor;
};
Factors Patch(InputPatch<Vertex, 3> vertices) {
  Factors factors;
  factors.edges[0] = frac(vertices[0].position.x) + 1.0;
  factors.edges[1] = 1.0;
  factors.edges[2] = 1.0;
  factors.inside = 1.0;
  return factors;
}
[domain("tri")]
    [partitioning("integer")]
    [outputtopology("triangle_cw")]
    [outputcontrolpoints(3)]
    [patchconstantfunc("Patch")] Vertex
    main(InputPatch<Vertex, 3> vertices, uint id : SV_OutputControlPointID) {
      return vertices[id];
    }
