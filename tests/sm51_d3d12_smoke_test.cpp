// A pipeline with a SM5.0 vertex shader and a pixel shader that DXP patched into SM5.1, reading a constant buffer
// the recipe added at b13 in register space 100 through a root CBV, on WARP with the debug layer when it's installed.
// The game's own resources (b0, t0, s0) stay in space 0 and the vertex shader isn't touched.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl/client.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"

using Microsoft::WRL::ComPtr;

namespace {

std::vector<uint8_t> ReadAll(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

D3D12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE type) {
  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = type;
  return heap;
}

D3D12_RESOURCE_DESC BufferDesc(UINT64 size) {
  D3D12_RESOURCE_DESC desc{};
  desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  desc.Width = size;
  desc.Height = 1;
  desc.DepthOrArraySize = 1;
  desc.MipLevels = 1;
  desc.SampleDesc.Count = 1;
  desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  return desc;
}

#define CHECK_HR(x)                                                                           \
  do {                                                                                        \
    const HRESULT hr_ = (x);                                                                  \
    if (FAILED(hr_)) {                                                                        \
      std::cerr << "FAIL: 0x" << std::hex << static_cast<unsigned>(hr_) << std::dec << " at " << #x << " (line " << __LINE__ << ")\n"; \
      return 2;                                                                               \
    }                                                                                         \
  } while (0)

// 0 ok, 1 skipped (no device), anything else a failure
int Run(const std::vector<uint8_t>& vs, const std::vector<uint8_t>& ps) {
  ComPtr<ID3D12Debug> debug;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
    debug->EnableDebugLayer();
  }
  ComPtr<IDXGIFactory4> factory;
  CHECK_HR(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));
  ComPtr<IDXGIAdapter> adapter;
  if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))) {
    return 1;
  }
  ComPtr<ID3D12Device> device;
  if (FAILED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
    return 1;
  }
  ComPtr<ID3D12InfoQueue> queue;
  device.As(&queue);

  // Root signature: the game's b0 (space 0), Luma's b13 (space 100), the game's t0 in a table, the game's s0 static
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
  D3D12_ROOT_PARAMETER parameters[3]{};
  parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[0].Descriptor = {0, 0};
  parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
  parameters[1].Descriptor = {13, 100};
  parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  parameters[2].DescriptorTable = {1, &range};
  parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  D3D12_STATIC_SAMPLER_DESC sampler{};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
  D3D12_ROOT_SIGNATURE_DESC root_desc{3, parameters, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_NONE};
  ComPtr<ID3DBlob> blob;
  ComPtr<ID3DBlob> error;
  CHECK_HR(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error));
  ComPtr<ID3D12RootSignature> root;
  CHECK_HR(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&root)));

  D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
  pipeline.pRootSignature = root.Get();
  pipeline.VS = {vs.data(), vs.size()};
  pipeline.PS = {ps.data(), ps.size()};
  pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = 0xF;
  pipeline.SampleMask = ~0U;
  pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  pipeline.NumRenderTargets = 1;
  pipeline.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
  pipeline.SampleDesc.Count = 1;
  ComPtr<ID3D12PipelineState> pso;
  const HRESULT created = device->CreateGraphicsPipelineState(&pipeline, IID_PPV_ARGS(&pso));
  if (FAILED(created)) {
    std::cerr << "FAIL: the pipeline with the SM5.1 pixel shader was rejected (0x" << std::hex << static_cast<unsigned>(created) << std::dec << ")\n";
    return 3;
  }

  // Constant data: the game's cbuffer, then Luma's one 256 bytes on
  ComPtr<ID3D12Resource> constants;
  auto upload = HeapProps(D3D12_HEAP_TYPE_UPLOAD);
  auto buffer = BufferDesc(512);
  CHECK_HR(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constants)));
  float* mapped = nullptr;
  CHECK_HR(constants->Map(0, nullptr, reinterpret_cast<void**>(&mapped)));
  for (int i = 0; i < 128; ++i) mapped[i] = 0.0F;
  mapped[0] = 0.10F;
  mapped[1] = 0.20F;
  mapped[2] = 0.30F;
  mapped[64] = 0.01F;
  mapped[65] = 0.02F;
  mapped[66] = 0.03F;
  constants->Unmap(0, nullptr);

  auto defaults = HeapProps(D3D12_HEAP_TYPE_DEFAULT);
  D3D12_RESOURCE_DESC texture_desc{};
  texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  texture_desc.Width = 1;
  texture_desc.Height = 1;
  texture_desc.DepthOrArraySize = 1;
  texture_desc.MipLevels = 1;
  texture_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  texture_desc.SampleDesc.Count = 1;
  ComPtr<ID3D12Resource> texture;
  CHECK_HR(device->CreateCommittedResource(&defaults, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture)));
  texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_CLEAR_VALUE clear{};
  clear.Format = texture_desc.Format;
  ComPtr<ID3D12Resource> target;
  CHECK_HR(device->CreateCommittedResource(&defaults, D3D12_HEAP_FLAG_NONE, &texture_desc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clear, IID_PPV_ARGS(&target)));
  ComPtr<ID3D12Resource> staging;
  buffer = BufferDesc(1024);
  CHECK_HR(device->CreateCommittedResource(&upload, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&staging)));
  float* texel = nullptr;
  CHECK_HR(staging->Map(0, nullptr, reinterpret_cast<void**>(&texel)));
  texel[0] = 0.4F;
  texel[1] = 0.5F;
  texel[2] = 0.6F;
  texel[3] = 1.0F;
  staging->Unmap(0, nullptr);
  ComPtr<ID3D12Resource> readback;
  auto readback_heap = HeapProps(D3D12_HEAP_TYPE_READBACK);
  CHECK_HR(device->CreateCommittedResource(&readback_heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));

  D3D12_DESCRIPTOR_HEAP_DESC heap_desc{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
  ComPtr<ID3D12DescriptorHeap> srv_heap;
  CHECK_HR(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&srv_heap)));
  D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
  srv.Format = texture_desc.Format;
  srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv.Texture2D.MipLevels = 1;
  device->CreateShaderResourceView(texture.Get(), &srv, srv_heap->GetCPUDescriptorHandleForHeapStart());
  D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
  ComPtr<ID3D12DescriptorHeap> rtv_heap;
  CHECK_HR(device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtv_heap)));
  device->CreateRenderTargetView(target.Get(), nullptr, rtv_heap->GetCPUDescriptorHandleForHeapStart());

  ComPtr<ID3D12CommandAllocator> allocator;
  CHECK_HR(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
  ComPtr<ID3D12GraphicsCommandList> list;
  CHECK_HR(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));
  D3D12_TEXTURE_COPY_LOCATION copy_to{};
  copy_to.pResource = texture.Get();
  copy_to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  D3D12_TEXTURE_COPY_LOCATION copy_from{};
  copy_from.pResource = staging.Get();
  copy_from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  copy_from.PlacedFootprint.Footprint = {DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, 1, 256};
  list->CopyTextureRegion(&copy_to, 0, 0, 0, &copy_from, nullptr);
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition = {texture.Get(), 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
  list->ResourceBarrier(1, &barrier);
  const auto rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
  list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
  const D3D12_VIEWPORT viewport{0, 0, 1, 1, 0, 1};
  const D3D12_RECT scissor{0, 0, 1, 1};
  list->RSSetViewports(1, &viewport);
  list->RSSetScissorRects(1, &scissor);
  list->SetGraphicsRootSignature(root.Get());
  list->SetPipelineState(pso.Get());
  ID3D12DescriptorHeap* heaps[] = {srv_heap.Get()};
  list->SetDescriptorHeaps(1, heaps);
  list->SetGraphicsRootConstantBufferView(0, constants->GetGPUVirtualAddress());
  list->SetGraphicsRootConstantBufferView(1, constants->GetGPUVirtualAddress() + 256);
  list->SetGraphicsRootDescriptorTable(2, srv_heap->GetGPUDescriptorHandleForHeapStart());
  list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  list->DrawInstanced(3, 1, 0, 0);
  barrier.Transition = {target.Get(), 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE};
  list->ResourceBarrier(1, &barrier);
  D3D12_TEXTURE_COPY_LOCATION read_from{};
  read_from.pResource = target.Get();
  read_from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  D3D12_TEXTURE_COPY_LOCATION read_to{};
  read_to.pResource = readback.Get();
  read_to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  read_to.PlacedFootprint.Footprint = {DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 1, 1, 256};
  list->CopyTextureRegion(&read_to, 0, 0, 0, &read_from, nullptr);
  CHECK_HR(list->Close());

  ComPtr<ID3D12CommandQueue> command_queue;
  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  CHECK_HR(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));
  ID3D12CommandList* lists[] = {list.Get()};
  command_queue->ExecuteCommandLists(1, lists);
  ComPtr<ID3D12Fence> fence;
  CHECK_HR(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
  command_queue->Signal(fence.Get(), 1);
  const HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  fence->SetEventOnCompletion(1, event);
  WaitForSingleObject(event, 10000);
  CloseHandle(event);

  float* pixel = nullptr;
  CHECK_HR(readback->Map(0, nullptr, reinterpret_cast<void**>(&pixel)));
  const bool right = std::fabs(pixel[0] - 0.51F) < 1e-3F && std::fabs(pixel[1] - 0.72F) < 1e-3F && std::fabs(pixel[2] - 0.93F) < 1e-3F;
  std::cout << "pixel " << pixel[0] << " " << pixel[1] << " " << pixel[2] << " (expect 0.51 0.72 0.93)\n";
  readback->Unmap(0, nullptr);

  int problems = 0;
  if (queue) {
    for (UINT64 i = 0; i < queue->GetNumStoredMessages(); ++i) {
      SIZE_T length = 0;
      queue->GetMessage(i, nullptr, &length);
      std::vector<char> storage(length);
      auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
      queue->GetMessage(i, message, &length);
      if (message->Severity <= D3D12_MESSAGE_SEVERITY_WARNING) {
        std::cerr << "debug layer: " << message->pDescription << "\n";
        ++problems;
      }
    }
  }
  if (!right) {
    std::cerr << "FAIL: the patched pixel shader read the wrong values\n";
    return 4;
  }
  return problems == 0 ? 0 : 5;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: sm51_d3d12_smoke_test <sm51_probe.vs_5_0.cso> <sm51_probe_game.ps_5_0.cso>\n";
    return 1;
  }
  const auto vs = ReadAll(argv[1]);
  const auto game = ReadAll(argv[2]);
  if (vs.empty() || game.empty()) {
    std::cerr << "fixtures not found\n";
    return 1;
  }
  // Luma's shape: a recipe adds a cbuffer in its own space and reads it (the space comes from an env variable)
  const char* recipe_text = R"YAML(version: 1
steps:
  - kind: add_resource
    name: add_luma
    temps: [scratch]
    cbuffers:
      - handle: luma
        elements: 1
        register_index: 13
        space: LUMA_SPACE
  - kind: apply_rule
    name: read_luma
    rule:
      match:
        - opcode: add
          operands:
            - type: output
              capture: dst
            - type: temp
              capture: a
            - type: constant_buffer
              capture: c
      emit:
        - opcode: add
          operands:
            - type: temp
              handle: { name: scratch }
              components: {selection_mode: mask, value: xyzw}
            - capture: a
            - capture: c
        - opcode: add
          operands:
            - capture: dst
            - type: temp
              handle: { name: scratch }
              components: {selection_mode: swizzle, value: xyzw}
            - type: constant_buffer
              handle: { name: luma, element_index: 0 }
              components: {selection_mode: swizzle, value: xyzw}
)YAML";
  auto recipe = dxp::sm5::Recipe::ParseFromText(recipe_text, "smoke");
  if (!recipe) {
    std::cerr << "recipe: " << recipe.error() << "\n";
    return 1;
  }
  dxp::PatchOptions options;
  options.SetEnv("LUMA_SPACE", uint32_t{100});
  const auto report = recipe->Execute(game, options);
  if (!report || !report->modified) {
    std::cerr << "patching failed: " << (report ? "nothing changed" : report.error()) << "\n";
    return 1;
  }
  const int result = Run(vs, report->output_bytes);
  if (result == 1) {
    std::cout << "No D3D12 device (WARP) here, skipped\n";
    return 0;
  }
  if (result == 0) {
    std::cout << "SM5.0 vertex shader + patched SM5.1 pixel shader (b13 space 100) ok on WARP\n";
  }
  return result;
}
