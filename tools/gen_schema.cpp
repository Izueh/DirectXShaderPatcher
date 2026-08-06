#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <glaze/glaze.hpp>
#include <glaze/json/schema.hpp>

#include "dxp/sm5/Recipe_impl.hpp"
#include "dxp/sm6/Recipe_impl.hpp"

namespace {

struct SchemaDoc {
  const char* id;
  const char* title;
  const char* description;
  const char* out_path;
};

constexpr SchemaDoc kSchemas[] = {
    {"https://github.com/DirectXShaderPatcher/docs/sm5_recipe_schema.json",
     "SM5 YAML Recipe Schema",
     "JSON Schema for dxp::sm5 YAML recipes: add_resource, apply_rule, check_shader_version, "
     "check_opcode_count, and check_resource_count steps.",
     "docs/sm5_recipe_schema.json"},
    {"https://github.com/DirectXShaderPatcher/docs/sm6_recipe_schema.json",
     "SM6 YAML Recipe Schema",
     "JSON Schema for dxp::sm6 YAML recipes: add_resource, apply_rule, check_shader_version, "
     "check_opcode_count, and check_resource_count steps.",
     "docs/sm6_recipe_schema.json"},
};

std::string GenerateFor(const SchemaDoc& doc, const glz::json_t& schema) {
  std::string out;
  auto ec = glz::write<glz::opts{.prettify = true}>(schema, out);
  if (ec) {
    std::cerr << "gen_schema: failed to serialize schema for " << doc.out_path << ": " << glz::format_error(ec) << "\n";
    return {};
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const bool check_only = argc == 2 && std::string(argv[1]) == "--check";
  if (argc != 1 && !check_only) {
    std::cerr << "Usage: gen_schema [--check]\n";
    return 1;
  }

  const std::filesystem::path repo_root = std::filesystem::current_path();
  bool all_match = true;

  const auto write_or_compare = [&](const std::string& path, const std::string& content) -> bool {
    const auto full_path = repo_root / path;
    if (check_only) {
      std::ifstream existing(full_path);
      std::string existing_content((std::istreambuf_iterator<char>(existing)), std::istreambuf_iterator<char>());
      if (existing_content != content) {
        std::cerr << "gen_schema: " << path << " is stale — run gen_schema to regenerate.\n";
        return false;
      }
      return true;
    }
    std::ofstream out(full_path, std::ios::trunc);
    out << content;
    return true;
  };

  const auto emit = [&](const SchemaDoc& doc, glz::json_t schema) {
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema";
    schema["$id"] = doc.id;
    schema["title"] = doc.title;
    schema["description"] = doc.description;
    std::string content = GenerateFor(doc, schema);
    if (content.empty()) {
      all_match = false;
      return;
    }
    content += "\n";
    all_match = write_or_compare(doc.out_path, content) && all_match;
  };

  // SM5
  {
    auto schema = glz::write_json_schema<dxp::sm5::RecipeData>();
    if (!schema) {
      std::cerr << "gen_schema: SM5 schema generation failed: " << schema.error() << "\n";
      return 1;
    }
    glz::json_t parsed;
    auto ec = glz::read_json(parsed, *schema);
    if (ec) {
      std::cerr << "gen_schema: SM5 schema re-parse failed\n";
      return 1;
    }
    emit(kSchemas[0], std::move(parsed));
  }

  // SM6
  {
    auto schema = glz::write_json_schema<dxp::sm6::RecipeData>();
    if (!schema) {
      std::cerr << "gen_schema: SM6 schema generation failed: " << schema.error() << "\n";
      return 1;
    }
    glz::json_t parsed;
    auto ec = glz::read_json(parsed, *schema);
    if (ec) {
      std::cerr << "gen_schema: SM6 schema re-parse failed\n";
      return 1;
    }
    emit(kSchemas[1], std::move(parsed));
  }

  if (check_only && !all_match) {
    return 1;
  }
  if (check_only) {
    std::cout << "gen_schema: schemas are up to date.\n";
  } else {
    std::cout << "gen_schema: wrote docs/sm5_recipe_schema.json, docs/sm6_recipe_schema.json\n";
  }
  return 0;
}
