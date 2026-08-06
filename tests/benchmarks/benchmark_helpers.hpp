#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm5/ShaderProgram.hpp"

std::vector<uint8_t> LoadShaderBytes(const std::filesystem::path& path);

std::filesystem::path DefaultTestShaderPath();

std::filesystem::path DefaultBenchNoopMovRecipePath();

std::filesystem::path DefaultBenchSequenceRecipePath();

std::filesystem::path DefaultBenchCombinedRecipePath();
