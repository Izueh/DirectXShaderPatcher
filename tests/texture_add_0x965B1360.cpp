#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/sm6/Recipe.hpp"
#include "dxp/sm6/ResourceTypes.hpp"
#include "dxp/sm6/step/AddResourceStep.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: texture_add_0x965B1360 <input.cso>\n";
    return 1;
  }

  const ScopedCoInitialize coinit;

  // Read shader bytes using public API (no ShaderProgram)
  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input shader: " << args[1] << "\n";
    return 1;
  }

  // Build a recipe using AddResourceStep to add a texture SRV
  dxp::sm6::Recipe recipe;

  // Construct AddResourceStep directly with hardcoded bind point
  dxp::sm6::step::AddResourceStep step;
  step.name = "AddMyTex";

  dxp::sm6::TextureResourceDesc tex_decl;
  tex_decl.name = "MyTex";
  tex_decl.kind = dxp::sm6::DxilResourceKind::Texture2D;
  tex_decl.element_kind = dxp::ComponentType::F32;
  tex_decl.vector_width = 4;
  tex_decl.binding = dxp::sm6::ResourceBindingDesc{.resource_class = dxp::sm6::ResourceClass::SRV, .register_index = std::nullopt, .space = std::nullopt};
  step.textures.push_back(std::move(tex_decl));

  recipe.AddStep(std::move(step));

  // Execute recipe
  auto result = recipe.Execute(input_bytes);
  if (!result) {
    std::cerr << "Recipe execution failed: " << result.error() << "\n";
    return 1;
  }

  // Verify serialization produced valid output
  if (result.value().output_bytes.empty()) {
    std::cerr << "Serialization produced empty output.\n";
    return 1;
  }

  // Verify the step results contain the expected side effect via new_bindings
  bool found_srv = false;
  std::string srv_handle;
  uint32_t srv_register = 0;
  uint32_t srv_space = 0;

  for (const auto& [handle, binding] : result.value().new_bindings) {
    if (binding.resource_kind == dxp::ResourceKind::Texture && binding.handle == "MyTex") {
      found_srv = true;
      srv_handle = binding.handle;
      srv_register = binding.register_index;
      srv_space = binding.space;
    }
  }

  if (!found_srv) {
    std::cerr << "AddResourceStep did not report 'MyTex' in resource_bindings.\n";
    return 1;
  }

  std::cout << "Added SRV '" << srv_handle << "' at t" << srv_register << ", space " << srv_space << "\n";
  return 0;
}
