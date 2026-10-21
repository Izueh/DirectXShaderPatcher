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
     "JSON Schema for dxp::sm6 YAML recipes: add_resource, declare_function, apply_rule, check_shader_version, "
     "check_opcode_count, and check_resource_count steps.",
     "docs/sm6_recipe_schema.json"},
};

bool IsNullType(const glz::json_t& node) {
  return node.is_object() && node.get_object().size() == 1 && node.contains("type") && node["type"].is_string() &&
         node["type"].get_string() == "null";
}

/// Recipes are YAML: an optional key is omitted, never written as null. glaze renders
/// std::optional members as `anyOf: [T, {type: null}]` (or a type list with "null"),
/// which reads as if null were a meaningful value, so that alternative is dropped.
void StripNullAlternatives(glz::json_t& node) {
  if (node.is_array()) {
    for (auto& element : node.get_array()) StripNullAlternatives(element);
    return;
  }
  if (!node.is_object()) return;

  auto& object = node.get_object();
  if (auto any_of = object.find("anyOf"); any_of != object.end() && any_of->second.is_array()) {
    auto& alternatives = any_of->second.get_array();
    std::erase_if(alternatives, IsNullType);
    if (alternatives.size() == 1 && alternatives.front().is_object()) {
      glz::json_t only = std::move(alternatives.front());
      object.erase("anyOf");
      for (auto& [key, value] : only.get_object()) object[std::string(key)] = std::move(value);
    }
  }
  if (auto type = object.find("type"); type != object.end() && type->second.is_array()) {
    auto& types = type->second.get_array();
    std::erase_if(types, [](const glz::json_t& t) { return t.is_string() && t.get_string() == "null"; });
    if (types.size() == 1) {
      glz::json_t single = std::move(types.front());
      type->second = std::move(single);
    }
  }
  for (auto& [key, value] : object) StripNullAlternatives(value);
}

/// `meta:` and cbuffer-field `default:` are parsed as glz::generic, whose generated schema
/// spells out the C++ variant (null alternative, C++ type names). Describe them instead.
void DescribeGenericProperties(glz::json_t& node) {
  if (node.is_array()) {
    for (auto& element : node.get_array()) DescribeGenericProperties(element);
    return;
  }
  if (!node.is_object()) return;
  auto& object = node.get_object();
  if (auto properties = object.find("properties"); properties != object.end() && properties->second.is_object()) {
    auto& props = properties->second.get_object();
    if (auto meta = props.find("meta"); meta != props.end()) {
      glz::json_t described;
      described["description"] = "Opaque metadata for hosts: any YAML value, reported verbatim and never interpreted by DXP.";
      meta->second = std::move(described);
    }
    if (auto def = props.find("default"); def != props.end() && def->second.contains("$ref")) {
      glz::json_t described;
      described["type"] = glz::json_t::array_t{"number", "boolean", "array"};
      described["items"]["type"] = "number";
      described["description"] = "Initial value: a number or bool broadcast to every component, or one number per component.";
      def->second = std::move(described);
    }
  }
  for (auto& [key, value] : object) DescribeGenericProperties(value);
}

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
    DescribeGenericProperties(schema);
    if (schema.contains("$defs")) schema["$defs"].get_object().erase("glz::generic");
    StripNullAlternatives(schema);
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
