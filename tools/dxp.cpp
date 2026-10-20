#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <format>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <dxp/Version.hpp>
#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm6/Recipe.hpp"

namespace {

void PrintUsage() {
  std::cerr << "Usage:\n"
            << "  dxp sm5 patch <recipe.recipe.yml> <input.cso> [output.cso] [--log-level <level>]\n"
            << "  dxp sm5 validate <recipe.recipe.yml>\n"
            << "  dxp sm5 inspect <recipe.recipe.yml>\n"
            << "  dxp sm6 patch <recipe.recipe.yml> <input.cso> [output.cso] [--log-level <level>]\n"
            << "  dxp sm6 validate <recipe.recipe.yml>\n"
            << "  dxp sm6 inspect <recipe.recipe.yml>   Print env variables and declared resources.\n"
            << "  dxp --help         Show this help.\n"
            << "  dxp --version      Print the library version.\n"
            << "Recipe files are YAML documents.\n"
            << "If output is omitted, defaults to <input>.patched.<ext>.\n"
            << "--log-level: error | warning (default) | info | debug | trace\n";
}

const char* LevelName(dxp::LogLevel level) {
  switch (level) {
    case dxp::LogLevel::Error:   return "error";
    case dxp::LogLevel::Warning: return "warning";
    case dxp::LogLevel::Info:    return "info";
    case dxp::LogLevel::Debug:   return "debug";
    case dxp::LogLevel::Trace:   return "trace";
  }
  return "unknown";
}

bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& data) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) {
    return false;
  }
  file.seekg(0, std::ios::beg);

  data.resize(static_cast<size_t>(size));
  if (size > 0) {
    file.read(reinterpret_cast<char*>(data.data()), size);
  }

  return !!file;
}

bool WriteBinaryFile(const std::string& path, std::span<const uint8_t> data) {
  const std::filesystem::path output_path(path);
  const std::filesystem::path parent_path = output_path.parent_path();
  if (!parent_path.empty()) {
    std::error_code error;
    if (!std::filesystem::create_directories(parent_path, error) && error) {
      return false;
    }
  }

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  if (!data.empty()) {
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }

  return !!file;
}

std::string FormatMeta(const dxp::MetaValue& meta) {
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
          return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
          return std::format("{}", v);
        } else if constexpr (std::is_same_v<T, std::string>) {
          return v;
        } else if constexpr (std::is_same_v<T, dxp::MetaValue::Array>) {
          std::string out = "[";
          for (size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + FormatMeta(v[i]);
          return out + "]";
        } else {
          std::string out = "{";
          for (size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + v[i].first + ": " + FormatMeta(v[i].second);
          return out + "}";
        }
      },
      meta.value);
}

const char* ComponentTypeName(dxp::ComponentType type) {
  switch (type) {
    case dxp::ComponentType::F32: return "F32";
    case dxp::ComponentType::U32: return "U32";
    case dxp::ComponentType::I32: return "I32";
    case dxp::ComponentType::F16: return "F16";
    case dxp::ComponentType::F64: return "F64";
    case dxp::ComponentType::I16: return "I16";
    case dxp::ComponentType::U16: return "U16";
    case dxp::ComponentType::I64: return "I64";
    case dxp::ComponentType::U64: return "U64";
    default:                      return "?";
  }
}

const char* DimensionName(dxp::ResourceDimension dimension) {
  constexpr const char* kNames[] = {"Unknown", "Buffer", "Texture1D", "Texture2D", "Texture2DMS", "Texture3D", "TextureCube",
                                    "Texture1DArray", "Texture2DArray", "Texture2DMSArray", "TextureCubeArray", "RawBuffer",
                                    "StructuredBuffer"};
  const auto index = static_cast<size_t>(dimension);
  return index < std::size(kNames) ? kNames[index] : "?";
}

std::pair<const char*, char> BindingClassName(dxp::BindingClass binding_class) {
  switch (binding_class) {
    case dxp::BindingClass::Texture:            return {"texture", 't'};
    case dxp::BindingClass::RawResource:        return {"raw buffer", 't'};
    case dxp::BindingClass::StructuredResource: return {"structured buffer", 't'};
    case dxp::BindingClass::TextureUav:
    case dxp::BindingClass::Uav:                return {"uav", 'u'};
    case dxp::BindingClass::CBuffer:            return {"cbuffer", 'b'};
    case dxp::BindingClass::Sampler:            return {"sampler", 's'};
    default:                                    return {"resource", '?'};
  }
}

template <typename Recipe>
int RunInspectCommand(const char* backend, const char* recipe_path) {
  auto parse_result = Recipe::ParseFromFile(recipe_path);
  if (!parse_result) {
    std::cerr << "[error] " << backend << " recipe parse failed: " << parse_result.error() << "\n";
    return 1;
  }
  const Recipe& recipe = parse_result.value();
  std::cout << backend << " recipe " << recipe_path << " (version " << recipe.GetVersion() << ", " << recipe.GetStepCount()
            << " step(s))\n";

  const std::map<std::string, dxp::PrimitiveValue> env(recipe.GetEnv().begin(), recipe.GetEnv().end());
  std::cout << "env:" << (env.empty() ? " none\n" : "\n");
  for (const auto& [name, value] : env) {
    std::visit([&](const auto& v) { std::cout << "  " << name << " = " << std::boolalpha << v << "\n"; }, value);
  }

  const auto declarations = recipe.GetDeclaredResources();
  std::cout << "resources:" << (declarations.empty() ? " none\n" : "\n");
  for (const auto& declaration : declarations) {
    const auto [class_name, register_letter] = BindingClassName(declaration.binding_class);
    std::string binding = declaration.register_index ? std::format("{}{}", register_letter, *declaration.register_index)
                                                     : std::format("{}<auto>", register_letter);
    binding += declaration.space ? std::format(" space{}", *declaration.space) : std::string(" space<auto>");
    std::string type;
    if (declaration.type.dimension) type += std::string(" ") + DimensionName(*declaration.type.dimension);
    if (declaration.type.element_type) {
      type += std::format(" {}x{}", ComponentTypeName(*declaration.type.element_type), declaration.type.vector_width.value_or(1));
    }
    if (declaration.type.structure_stride) type += std::format(" stride {}", *declaration.type.structure_stride);
    std::cout << "  " << class_name << " '" << declaration.handle << "'" << type << ", " << binding << " (step '" << declaration.step
              << "'" << (declaration.conditional ? ", conditional" : "") << ")\n";
    if (declaration.meta) std::cout << "    meta: " << FormatMeta(*declaration.meta) << "\n";
    if (const auto& layout = declaration.type.cbuffer) {
      std::cout << "    layout " << layout->type_name << ", " << layout->size_in_bytes << " bytes\n";
      for (const auto& field : layout->fields) {
        std::cout << std::format("      {:<16} {}x{}  offset {:>3} (row {}, component {})", field.name, ComponentTypeName(field.type),
                                 field.width, field.offset, field.offset / 16, (field.offset % 16) / 4);
        if (!field.default_value.empty()) {
          std::cout << "  default";
          for (const double value : field.default_value) std::cout << " " << value;
        }
        std::cout << "\n";
        if (field.meta) std::cout << "        meta: " << FormatMeta(*field.meta) << "\n";
      }
    }
  }
  return 0;
}

int RunValidateSm6Command(const char* recipe_path) {
  auto parse_result = dxp::sm6::Recipe::ParseFromFile(recipe_path);
  if (!parse_result) {
    std::cerr << "[error] SM6 recipe parse failed: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm6::ValidateRecipe(parse_result.value());
  if (!validate_result) {
    std::cerr << "[error] SM6 recipe validation failed: " << validate_result.error() << "\n";
    return 1;
  }

  std::cout << "SM6 recipe is valid: " << recipe_path << " (" << parse_result.value().GetStepCount() << " step(s))\n";
  return 0;
}

int RunValidateSm5Command(const char* recipe_path) {
  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path);
  if (!parse_result) {
    std::cerr << "[error] SM5 recipe parse failed: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (!validate_result) {
    std::cerr << "[error] SM5 recipe validation failed: " << validate_result.error() << "\n";
    return 1;
  }

  std::cout << "SM5 recipe is valid: " << recipe_path << " (" << parse_result.value().GetStepCount() << " step(s))\n";
  return 0;
}

int RunPatchSm6Command(const char* input_path, const char* recipe_path, const char* output_path,
                       const dxp::PatchOptions& options) {
  std::vector<uint8_t> input_shader;
  if (!ReadBinaryFile(input_path, input_shader)) {
    std::cerr << "[error] Failed to read input shader: " << input_path << "\n";
    return 1;
  }

  auto parse_result = dxp::sm6::Recipe::ParseFromFile(recipe_path);
  if (!parse_result) {
    std::cerr << "[error] Failed to parse SM6 recipe file: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm6::ValidateRecipe(parse_result.value());
  if (!validate_result) {
    std::cerr << "SM6 recipe validation failed: " << validate_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_shader, options);
  if (!patch_result) {
    std::cerr << "[error] SM6 patch operation failed: " << patch_result.error() << "\n";
    return 1;
  }

  if (!WriteBinaryFile(std::string(output_path), patch_result.value().output_bytes)) {
    std::cerr << "[error] Failed to write output shader: " << output_path << "\n";
    return 1;
  }

  std::cout << "Patched SM6 shader written to: " << output_path << "\n";
  return 0;
}

int RunPatchSm5Command(const char* input_path, const char* recipe_path, const char* output_path,
                       const dxp::PatchOptions& options) {
  std::vector<uint8_t> input_shader;
  if (!ReadBinaryFile(input_path, input_shader)) {
    std::cerr << "Failed to read input shader: " << input_path << "\n";
    return 1;
  }

  auto parse_result = dxp::sm5::Recipe::ParseFromFile(recipe_path);
  if (!parse_result) {
    std::cerr << "[error] Failed to parse SM5 recipe file: " << parse_result.error() << "\n";
    return 1;
  }

  auto validate_result = dxp::sm5::ValidateRecipe(parse_result.value());
  if (!validate_result) {
    std::cerr << "SM5 recipe validation failed: " << validate_result.error() << "\n";
    return 1;
  }

  const auto patch_result = parse_result.value().Execute(input_shader, options);
  if (!patch_result) {
    std::cerr << "[error] Recipe execution failed: " << patch_result.error() << "\n";
    return 1;
  }

  const bool modified = patch_result.value().output_bytes != input_shader;
  if (!WriteBinaryFile(std::string(output_path), patch_result.value().output_bytes)) {
    std::cerr << "Failed to write output shader: " << output_path << "\n";
    return 1;
  }

  if (modified) {
    std::cout << "Patched SM5 shader written to: " << output_path << "\n";
  } else {
    std::cout << "No rules matched — output identical to input: " << output_path << "\n";
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::span<char*> args(argv, static_cast<size_t>(argc));
  if (argc >= 2) {
    const std::string first = args[1];
    if (first == "--version" || first == "-V") {
      std::cout << "DirectXShaderPatcher " << DXP_VERSION_STRING << "\n";
      return 0;
    }
    if (first == "--help" || first == "-h") {
      PrintUsage();
      return 0;
    }
  }
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  const std::string first = args[1];
  if (first != "sm5" && first != "sm6") {
    std::cerr << "[error] backend must be specified as 'sm5' or 'sm6'.\n";
    PrintUsage();
    return 1;
  }

  const std::string& backend = first;
  const std::string cmd = (argc >= 3) ? args[2] : std::string();

  int exit_code = 1;

  if (cmd == "validate") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    exit_code = (backend == "sm5") ? RunValidateSm5Command(args[3]) : RunValidateSm6Command(args[3]);
    if (exit_code == 0) {
      std::cout.flush();
      std::cerr.flush();
    }
    return exit_code;
  }

  if (cmd == "inspect") {
    if (argc != 4) {
      PrintUsage();
      return 1;
    }
    return backend == "sm5" ? RunInspectCommand<dxp::sm5::Recipe>("SM5", args[3]) : RunInspectCommand<dxp::sm6::Recipe>("SM6", args[3]);
  }

  if (cmd == "patch") {
    // Parse optional --log-level <level> (anywhere after 'patch'); the remaining
    // arguments are positionals: <recipe> <input> [output].
    dxp::LogLevel log_level = dxp::LogLevel::Warning;
    std::vector<std::string> positionals;
    positionals.reserve(static_cast<size_t>(argc) - 3);
    for (int i = 3; i < argc; ++i) {
      const std::string arg = args[i];
      if (arg == "--log-level") {
        if (i + 1 >= argc) {
          std::cerr << "[error] --log-level requires a value (error|warning|info|trace).\n";
          return 1;
        }
        const std::string level = args[++i];
        if (level == "error") {
          log_level = dxp::LogLevel::Error;
        } else if (level == "warning") {
          log_level = dxp::LogLevel::Warning;
        } else if (level == "info") {
          log_level = dxp::LogLevel::Info;
        } else if (level == "debug") {
          log_level = dxp::LogLevel::Debug;
        } else if (level == "trace") {
          log_level = dxp::LogLevel::Trace;
        } else {
          std::cerr << "[error] unknown log level '" << level << "' (expected error|warning|info|debug|trace).\n";
          return 1;
        }
      } else {
        positionals.push_back(arg);
      }
    }

    if (positionals.size() < 2 || positionals.size() > 3) {
      PrintUsage();
      return 1;
    }
    const char* recipe_path = positionals[0].c_str();
    const char* input_path = positionals[1].c_str();

    std::string resolved_output_path;
    const char* output_path = nullptr;
    if (positionals.size() == 3) {
      output_path = positionals[2].c_str();
    } else {
      const std::filesystem::path input_p(input_path);
      resolved_output_path = input_p.stem().string() + ".patched" + input_p.extension().string();
      output_path = resolved_output_path.c_str();
    }

    dxp::PatchOptions options;
    options.log_level = log_level;
    // CLI routes recipe-execution logs to stderr. Error-level messages are
    // excluded here — they surface via the returned expected and the CLI's own
    // error prints, avoiding double-reporting.
    options.logger = [](dxp::LogLevel level, const std::string& message) {
      if (level != dxp::LogLevel::Error) {
        std::cerr << "[" << LevelName(level) << "] " << message << "\n";
      }
    };

    exit_code = (backend == "sm5") ? RunPatchSm5Command(input_path, recipe_path, output_path, options)
                                   : RunPatchSm6Command(input_path, recipe_path, output_path, options);

    if (exit_code == 0) {
      std::cout.flush();
      std::cerr.flush();
    }
    return exit_code;
  }

  std::cerr << "[error] Unknown command: " << cmd << "\n";
  PrintUsage();
  return 1;
}