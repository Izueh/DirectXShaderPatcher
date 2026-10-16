#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm6/Recipe.hpp"

namespace {

template <typename Recipe>
bool CheckVersion(const Recipe& recipe, uint32_t expected, const std::string& context) {
  if (recipe.GetVersion() != expected) {
    std::cerr << context << ": expected version " << expected << ", got " << recipe.GetVersion() << '\n';
    return false;
  }
  return true;
}

template <typename Recipe>
bool TestRecipeVersion(const std::string& backend) {
  if (!CheckVersion(Recipe{}, 1, backend + " default constructor") ||
      !CheckVersion(Recipe{17}, 17, backend + " version constructor")) {
    return false;
  }

  const std::string steps = R"(steps:
  - kind: check_shader_version
    name: shader_model
    major: 5
    minor: 0
)";
  struct TestCase {
    std::string header;
    uint32_t expected;
  };
  const std::array<TestCase, 5> cases = {{{"", 1},
                                        {"version: 1\n", 1},
                                        {"version: 42\n", 42},
                                        {"version: 0\n", 0},
                                        {"version: 4294967295\n", UINT32_MAX}}};
  const auto path = std::filesystem::path{DXP_TEST_OUTPUT_DIR} / (backend + "_version.recipe.yml");
  for (const auto& test : cases) {
    const std::string yaml = test.header + steps;
    auto parsed = Recipe::ParseFromText(yaml);
    if (!parsed) {
      std::cerr << backend << " text parse failed: " << parsed.error() << '\n';
      return false;
    }
    if (!CheckVersion(*parsed, test.expected, backend + " text parse") || parsed->GetStepCount() != 1) {
      return false;
    }

    Recipe copied(*parsed);
    Recipe moved(std::move(copied));
    Recipe copy_assigned(99);
    copy_assigned = *parsed;
    Recipe move_assigned(99);
    move_assigned = std::move(copy_assigned);
    if (!CheckVersion(moved, test.expected, backend + " copy/move construction") ||
        !CheckVersion(move_assigned, test.expected, backend + " copy/move assignment")) {
      return false;
    }

    {
      std::ofstream file(path);
      file << yaml;
      file.close();
      if (!file) {
        std::cerr << "Could not write " << path << '\n';
        return false;
      }
    }
    auto from_file = Recipe::ParseFromFile(path.string());
    std::filesystem::remove(path);
    if (!from_file) {
      std::cerr << backend << " file parse failed: " << from_file.error() << '\n';
      return false;
    }
    if (!CheckVersion(*from_file, test.expected, backend + " file parse") || from_file->GetStepCount() != 1) {
      return false;
    }
  }

  for (const auto* invalid : {"-1", "4294967296", "invalid"}) {
    if (Recipe::ParseFromText(std::string("version: ") + invalid + '\n' + steps)) {
      std::cerr << backend << " accepted invalid version: " << invalid << '\n';
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  if (!TestRecipeVersion<dxp::sm5::Recipe>("sm5") || !TestRecipeVersion<dxp::sm6::Recipe>("sm6")) {
    return 1;
  }
  std::cout << "Recipe version tests passed\n";
  return 0;
}
