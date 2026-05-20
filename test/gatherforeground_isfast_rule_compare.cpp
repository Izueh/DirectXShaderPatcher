#include "TestSupport.h"

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

struct ISFastFrameConstantsCpu {
  uint32_t FrameIndex;
  uint32_t Padding[3];
};

struct RewriteMetrics {
  size_t srvCount = 0;
  size_t cbufferCount = 0;
  unsigned textureLoadCount = 0;
  unsigned createHandleCount = 0;
  unsigned annotateHandleCount = 0;
  unsigned cbufferLoadCount = 0;
  unsigned ignCount = 0;
  unsigned blueNoiseCount = 0;
};

struct RewriteRunResult {
  RewriteMetrics metrics;
  std::vector<uint8_t> outputContainer;
};

static RewriteMetrics CaptureMetrics(hlsl::DxilModule &dxilModule) {
  RewriteMetrics metrics;
  llvm::Function *entryFunction = dxilModule.GetEntryFunction();
  if (entryFunction == nullptr)
    return metrics;

  metrics.srvCount = dxilModule.GetSRVs().size();
  metrics.cbufferCount = dxilModule.GetCBuffers().size();
  metrics.textureLoadCount = CountDxOpCalls(*entryFunction, "dx.op.textureLoad.f32");
  metrics.createHandleCount =
      CountDxOpCalls(*entryFunction, "dx.op.createHandleFromBinding");
  metrics.annotateHandleCount =
      CountDxOpCalls(*entryFunction, "dx.op.annotateHandle");
  metrics.cbufferLoadCount =
      CountDxOpCalls(*entryFunction, "dx.op.cbufferLoadLegacy.i32");
  metrics.ignCount = CountIgnNoiseChains(*entryFunction);
  metrics.blueNoiseCount = CountBlueNoiseTextureLoads(*entryFunction, dxilModule);
  return metrics;
}

static bool ConfigureNoiseResources(LoadedDxilShader &shader,
                                    TextureResourceDesc &noiseTextureDesc,
                                    CBufferSchema &frameIndexSchema,
                                    CBufferDesc &frameIndexCBufferDesc) {
  noiseTextureDesc = TextureResourceBuilder("FASTNoiseTexture")
                         .Texture2DArray()
                         .Float2()
                         .Register(0, 50)
                         .Build();

  frameIndexSchema =
      CBufferSchemaBuilder<ISFastFrameConstantsCpu>("ISFastFrameConstants")
          .UInt("FrameIndex",
                static_cast<unsigned>(offsetof(ISFastFrameConstantsCpu, FrameIndex)))
          .UInt3("Padding",
                 static_cast<unsigned>(offsetof(ISFastFrameConstantsCpu, Padding)))
          .Build();

    frameIndexCBufferDesc.name = "ISFastFrameConstantsCB";
    frameIndexCBufferDesc.binding.Set(
      FindNextAvailableBinding(shader.dxilModule->GetCBuffers(), 0, 1),
      0,
      hlsl::DXIL::ResourceClass::CBuffer);
  frameIndexCBufferDesc.sizeInBytes =
      static_cast<unsigned>(sizeof(ISFastFrameConstantsCpu));
  frameIndexCBufferDesc.schema = &frameIndexSchema;

  if (!AddTextureSRV(*shader.module, *shader.dxilModule, noiseTextureDesc)) {
    std::cerr << "AddTextureSRV returned false for FASTNoiseTexture.\n";
    return false;
  }

  if (!AddCBuffer(*shader.module, *shader.dxilModule, frameIndexCBufferDesc)) {
    std::cerr << "AddCBuffer returned false for ISFastFrameConstantsCB.\n";
    return false;
  }

  return true;
}

static bool VerifyRunState(const RewriteMetrics &initialMetrics,
                           LoadedDxilShader &shader,
                           const TextureResourceDesc &noiseTextureDesc,
                           const CBufferDesc &frameIndexCBufferDesc,
                           RewriteRunResult &result) {
  result.metrics = CaptureMetrics(*shader.dxilModule);

  if (HasTypedHandleDxilOpOverloads(*shader.module)) {
    std::cerr << "Patched module introduced typed DXIL handle op overloads instead of reusing the shader's existing prototypes.\n";
    return false;
  }

  if (result.metrics.srvCount != initialMetrics.srvCount + 1) {
    std::cerr << "Expected SRV count to increase by one.\n";
    return false;
  }

  if (result.metrics.cbufferCount != initialMetrics.cbufferCount + 1) {
    std::cerr << "Expected cbuffer count to increase by one.\n";
    return false;
  }

  if (result.metrics.ignCount != 0 || result.metrics.blueNoiseCount != 0) {
    std::cerr << "Expected all IGN and BlueNoise patterns to be replaced.\n";
    return false;
  }

  if (result.metrics.textureLoadCount !=
      initialMetrics.textureLoadCount + initialMetrics.ignCount) {
    std::cerr << "Unexpected textureLoad.f32 count after rewrite.\n";
    return false;
  }

  const hlsl::DxilResource *addedSrv = nullptr;
  if (!FindSrvByName(*shader.dxilModule, noiseTextureDesc.name, &addedSrv) ||
      addedSrv == nullptr) {
    std::cerr << "Injected FASTNoiseTexture SRV was not present after mutation.\n";
    return false;
  }

  const hlsl::DxilCBuffer *addedCBuffer = nullptr;
  if (!FindCBufferByName(*shader.dxilModule,
                         frameIndexCBufferDesc.name,
                         &addedCBuffer) ||
      addedCBuffer == nullptr) {
    std::cerr << "Injected ISFastFrameConstantsCB cbuffer was not present after mutation.\n";
    return false;
  }

  RefreshDxilAfterResourceMutation(*shader.dxilModule);
  if (!VerifyModuleOrReport(*shader.module))
    return false;

  if (!SerializePatchedContainer(*shader.dxilModule,
                                 SerializeModuleToBitcode(*shader.module),
                                 result.outputContainer)) {
    std::cerr << "SerializePatchedContainer failed.\n";
    return false;
  }

  return true;
}

static bool RunRewrite(const std::string &inputPath,
                       bool useRuleEngine,
                       RewriteRunResult &result) {
  ScopedCoInitialize *coinit = new ScopedCoInitialize();
  if (!coinit->IsInitialized()) {
    std::cerr << "Failed to initialize COM.\n";
    return false;
  }

  LoadedDxilShader *shader = new LoadedDxilShader();
  if (!LoadShaderForMutation(inputPath, *shader, true))
    return false;

  RewriteMetrics initialMetrics = CaptureMetrics(*shader->dxilModule);
  if (initialMetrics.ignCount == 0 && initialMetrics.blueNoiseCount == 0) {
    std::cerr << "The test shader did not contain any IGN or BlueNoise patterns.\n";
    return false;
  }

  TextureResourceDesc noiseTextureDesc;
  CBufferSchema frameIndexSchema;
  CBufferDesc frameIndexCBufferDesc;
  if (!ConfigureNoiseResources(*shader,
                               noiseTextureDesc,
                               frameIndexSchema,
                               frameIndexCBufferDesc)) {
    return false;
  }

  const bool replaceSucceeded = useRuleEngine
              ? ApplyComputeNoiseRewriteUsingRules(
                                          *shader->module,
                                          *shader->dxilModule,
                                          noiseTextureDesc,
                                          frameIndexCBufferDesc)
                                    : ReplaceIgnNoiseInComputeShaderWithTextureLoad(
                                          *shader->module,
                                          *shader->dxilModule,
                                          noiseTextureDesc,
                                          frameIndexCBufferDesc);
  if (!replaceSucceeded) {
    std::cerr << (useRuleEngine ? "Rule-based" : "Procedural")
              << " IGN rewrite returned false.\n";
    return false;
  }

  return VerifyRunState(initialMetrics,
                        *shader,
                        noiseTextureDesc,
                        frameIndexCBufferDesc,
                        result);
}

static bool MetricsMatch(const RewriteMetrics &lhs, const RewriteMetrics &rhs) {
  return lhs.srvCount == rhs.srvCount &&
         lhs.cbufferCount == rhs.cbufferCount &&
         lhs.textureLoadCount == rhs.textureLoadCount &&
         lhs.createHandleCount == rhs.createHandleCount &&
         lhs.annotateHandleCount == rhs.annotateHandleCount &&
         lhs.cbufferLoadCount == rhs.cbufferLoadCount &&
         lhs.ignCount == rhs.ignCount &&
         lhs.blueNoiseCount == rhs.blueNoiseCount;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Usage: gatherforeground_isfast_rule_compare <input.cso>\n";
    return 1;
  }

  RewriteRunResult proceduralResult;
  if (!RunRewrite(argv[1], false, proceduralResult))
    return 1;

  RewriteRunResult ruleResult;
  if (!RunRewrite(argv[1], true, ruleResult))
    return 1;

  if (!MetricsMatch(proceduralResult.metrics, ruleResult.metrics)) {
    std::cerr << "Procedural and rule-based rewrites produced different DXIL metrics.\n";
    return 1;
  }

  std::cout << "Procedural and rule-based rewrites matched for: " << argv[1]
            << "\n";
  std::cout.flush();
  std::cerr.flush();
  std::_Exit(0);
}