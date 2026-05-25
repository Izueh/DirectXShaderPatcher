#pragma once

#include "Container.h"
#include "Model.h"

#include <cstdint>
#include <vector>

namespace dxp::sm5 {

bool ParseShaderChunk(const Container &container, Program &program);

}
