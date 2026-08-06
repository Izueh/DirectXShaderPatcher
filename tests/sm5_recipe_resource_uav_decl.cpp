#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <span>
#include <vector>

#include "dxp/ExportTypes.hpp"
#include "dxp/sm5/Recipe.hpp"
#include "tests/helper/TestHelper.hpp"

int main(int argc, char** argv_) {
  const std::span<char*> args(argv_, static_cast<size_t>(argc));
  if (argc != 2) {
    std::cerr << "Usage: sm5_recipe_resource_uav_decl <input.ps_5_0.cso>\n";
    return 1;
  }

  std::vector<uint8_t> input_bytes;
  if (!ReadFile(args[1], input_bytes)) {
    std::cerr << "Failed to read input file: " << args[1] << "\n";
    return 1;
  }

  const std::filesystem::path recipe_path = RepoRootPath() / "tests/recipes/sm5_resource_uav_decl.recipe.yml";
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path.string());
  if (!parse_result) {
    std::cerr << "Failed to parse SM5 recipe file: " << parse_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_bytes);
  if (!patch_result) {
    std::cerr << "Failed to patch SM5 shader: " << patch_result.error() << "\n";
    return 1;
  }

  const auto& report = patch_result.value();

  const auto raw_srv_binding_it = report.new_bindings.find("injected_raw_srv");
  const auto structured_srv_binding_it = report.new_bindings.find("injected_structured_srv");
  const auto raw_uav_binding_it = report.new_bindings.find("injected_raw_uav");
  if (raw_srv_binding_it == report.new_bindings.end() || structured_srv_binding_it == report.new_bindings.end() || raw_uav_binding_it == report.new_bindings.end()) {
    std::cerr << "Expected injected declaration handles to be exported in the "
                 "patch report.\n";
    return 1;
  }

  if (raw_srv_binding_it->second.resource_kind != dxp::ResourceKind::RawResource || raw_srv_binding_it->second.handle != "injected_raw_srv" || raw_srv_binding_it->second.space != 0u) {
    std::cerr << "Expected raw SRV export to expose the resolved binding.\n";
    return 1;
  }

  if (structured_srv_binding_it->second.resource_kind != dxp::ResourceKind::StructuredResource || structured_srv_binding_it->second.handle != "injected_structured_srv" || structured_srv_binding_it->second.space != 0u) {
    std::cerr << "Expected structured SRV export to expose the resolved "
                 "binding.\n";
    return 1;
  }

  if (raw_uav_binding_it->second.resource_kind != dxp::ResourceKind::Uav || raw_uav_binding_it->second.handle != "injected_raw_uav" || raw_uav_binding_it->second.space != 0u) {
    std::cerr << "Expected raw UAV export to expose the resolved binding.\n";
    return 1;
  }

  std::cout << "SM5 recipe added resource declarations (raw SRV, structured SRV, raw UAV).\n";
  return 0;
}
