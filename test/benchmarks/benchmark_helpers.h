#pragma once

#include "dxp/sm5/Container.h"
#include "dxp/sm5/Model.h"
#include "dxp/sm5/Parse.h"
#include "dxp/sm5/Patch.h"
#include "dxp/sm5/Recipe.h"
#include "dxp/sm5/Serialize.h"
#include "dxp/sm5/Transforms.h"

#include <filesystem>
#include <string>
#include <vector>

// Load a .cso file into memory.
std::vector<uint8_t> LoadShaderBytes(const std::filesystem::path& path);

// Parse a DXBC container and extract the Program.
bool ParseShaderToProgram(const std::vector<uint8_t>& containerBytes,
                          dxp::sm5::Program& program, std::string& error);

// Build a simple SM5 recipe with a single rule that matches MOV instructions
// and re-emits them (noop rewrite — useful for measuring match/rewrite overhead
// without changing shader semantics).
dxp::sm5::Recipe BuildNoopMovRecipe();

// Build a recipe that exercises sequence matching (two-instruction chain).
dxp::sm5::Recipe BuildSequenceMatchRecipe();

// Get the default test shader path used by existing SM5 tests.
std::filesystem::path DefaultTestShaderPath();
