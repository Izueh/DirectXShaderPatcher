#include "dxp/sm5/Sm51.hpp"

#include <algorithm>
#include <format>
#include <set>

#include "dxp/sm5/Model.hpp"

namespace dxp::sm5::sm51 {
namespace {

using model::Opcode;
using model::OperandType;

constexpr uint32_t kOpcodeMask = 0x7FF;
constexpr uint32_t kLengthShift = 24;
constexpr uint32_t kLengthMask = 0x7F;
constexpr uint32_t kExtendedBit = 0x80000000U;
constexpr uint32_t kExtendedTypeMask = 0x3F;
constexpr uint32_t kExtendedResourceDim = 2;
constexpr uint32_t kExtendedResourceReturnType = 3;

// Operand token layout (d3d11TokenizedProgramFormat): components 0-1, mode and
// selection 2-11, type 12-19, index dimension 20-21, per-dimension index
// representation 3 bits each from bit 22, extended bit 31.
constexpr uint32_t kIndexDimShift = 20;
constexpr uint32_t kIndexRepShift = 22;
constexpr uint32_t kIndexBitsMask = (3U << kIndexDimShift) | (0x1FFU << kIndexRepShift);
constexpr uint32_t kRepImmediate32 = 0;
constexpr uint32_t kRepImmediate64 = 1;
constexpr uint32_t kRepRelative = 2;
constexpr uint32_t kRepImmediate32PlusRelative = 3;
constexpr uint32_t kRepImmediate64PlusRelative = 4;
// 5.1 declaration operands are written as four components, xyzw swizzle (what
// constant buffer declarations always were).
constexpr uint32_t kDeclOperandComponents = 0xE46;
constexpr uint32_t kUnusedSpaceDimension = 3;

uint32_t OpcodeOf(uint32_t token) { return token & kOpcodeMask; }
uint32_t LengthOf(uint32_t token) { return (token >> kLengthShift) & kLengthMask; }
bool IsExtended(uint32_t token) { return (token & kExtendedBit) != 0; }
uint32_t SetLength(uint32_t token, size_t length) {
  return (token & ~(kLengthMask << kLengthShift)) | (static_cast<uint32_t>(length) << kLengthShift);
}

bool ClassOfOperandType(uint32_t type, RegClass& cls) {
  switch (static_cast<OperandType>(type)) {
    case OperandType::CBuffer: cls = RegClass::CBuffer; return true;
    case OperandType::Sampler: cls = RegClass::Sampler; return true;
    case OperandType::Resource: cls = RegClass::Resource; return true;
    case OperandType::UAV: cls = RegClass::Uav; return true;
    default: return false;
  }
}

uint32_t OperandTypeOfClass(RegClass cls) {
  switch (cls) {
    case RegClass::CBuffer: return static_cast<uint32_t>(OperandType::CBuffer);
    case RegClass::Sampler: return static_cast<uint32_t>(OperandType::Sampler);
    case RegClass::Resource: return static_cast<uint32_t>(OperandType::Resource);
    case RegClass::Uav: return static_cast<uint32_t>(OperandType::UAV);
  }
  return 0;
}

const char* ClassPrefix(RegClass cls) {
  switch (cls) {
    case RegClass::CBuffer: return "cb";
    case RegClass::Sampler: return "s";
    case RegClass::Resource: return "t";
    case RegClass::Uav: return "u";
  }
  return "?";
}

bool IsResourceDeclaration(Opcode op) {
  switch (op) {
    case Opcode::DclResource:
    case Opcode::DclConstantBuffer:
    case Opcode::DclSampler:
    case Opcode::DclUnorderedAccessViewTyped:
    case Opcode::DclUnorderedAccessViewRaw:
    case Opcode::DclUnorderedAccessViewStructured:
    case Opcode::DclResourceRaw:
    case Opcode::DclResourceStructured:
      return true;
    default:
      return false;
  }
}

bool IsUnsupportedOpcode(Opcode op) {
  // Class linkage has no SM5.1 form.
  return op == Opcode::InterfaceCall || op == Opcode::DclFunctionBody || op == Opcode::DclFunctionTable ||
         op == Opcode::DclInterface;
}

// ---- operand reading / writing ----------------------------------------------------------------

struct Operand {
  uint32_t token = 0;
  std::vector<uint32_t> extended;
  std::vector<std::vector<uint32_t>> index;  ///< Tokens of each index dimension (immediates, then any relative operand).
  std::vector<uint32_t> immediates;

  [[nodiscard]] uint32_t Type() const { return (token >> 12) & 0xFF; }
  [[nodiscard]] uint32_t Dim() const { return (token >> kIndexDimShift) & 3; }
  [[nodiscard]] uint32_t Rep(uint32_t dim) const { return (token >> (kIndexRepShift + (3 * dim))) & 7; }
};

void Emit(const Operand& operand, std::vector<uint32_t>& out) {
  out.push_back(operand.token);
  out.insert(out.end(), operand.extended.begin(), operand.extended.end());
  for (const auto& part : operand.index) {
    out.insert(out.end(), part.begin(), part.end());
  }
  out.insert(out.end(), operand.immediates.begin(), operand.immediates.end());
}

std::expected<Operand, std::string> ReadOperand(std::span<const uint32_t> words, size_t& cursor, size_t end) {
  Operand operand;
  if (cursor >= end) {
    return std::unexpected("operand runs past the instruction");
  }
  operand.token = words[cursor++];
  if (operand.token & kExtendedBit) {
    uint32_t extension = 0;
    do {
      if (cursor >= end) {
        return std::unexpected("operand extension runs past the instruction");
      }
      extension = words[cursor++];
      operand.extended.push_back(extension);
    } while (extension & kExtendedBit);
  }
  const uint32_t dimensions = operand.Dim();
  operand.index.resize(dimensions);
  for (uint32_t dim = 0; dim < dimensions; ++dim) {
    const uint32_t rep = operand.Rep(dim);
    auto& part = operand.index[dim];
    const auto take = [&](size_t count) -> bool {
      if (cursor + count > end) {
        return false;
      }
      part.insert(part.end(), words.begin() + static_cast<ptrdiff_t>(cursor), words.begin() + static_cast<ptrdiff_t>(cursor + count));
      cursor += count;
      return true;
    };
    bool ok = true;
    switch (rep) {
      case kRepImmediate32: ok = take(1); break;
      case kRepImmediate64: ok = take(2); break;
      case kRepRelative: break;
      case kRepImmediate32PlusRelative: ok = take(1); break;
      case kRepImmediate64PlusRelative: ok = take(2); break;
      default: return std::unexpected("unknown operand index representation");
    }
    if (!ok) {
      return std::unexpected("operand index runs past the instruction");
    }
    if (rep == kRepRelative || rep == kRepImmediate32PlusRelative || rep == kRepImmediate64PlusRelative) {
      auto relative = ReadOperand(words, cursor, end);
      if (!relative) {
        return std::unexpected(std::move(relative.error()));
      }
      Emit(*relative, part);
    }
  }
  const uint32_t components = operand.token & 3;
  size_t immediate_count = 0;
  if (operand.Type() == static_cast<uint32_t>(OperandType::Immediate32)) {
    immediate_count = components == 1 ? 1 : 4;
  } else if (operand.Type() == static_cast<uint32_t>(OperandType::Immediate64)) {
    immediate_count = components == 1 ? 2 : 8;
  }
  if (cursor + immediate_count > end) {
    return std::unexpected("immediate operand runs past the instruction");
  }
  operand.immediates.assign(words.begin() + static_cast<ptrdiff_t>(cursor), words.begin() + static_cast<ptrdiff_t>(cursor + immediate_count));
  cursor += immediate_count;
  return operand;
}

/// Moves the index dimension count by +1 (a new immediate32 dimension in front) or -1 (the front one dropped).
uint32_t PushFrontDimension(uint32_t token) {
  const uint32_t dims = (token >> kIndexDimShift) & 3;
  uint32_t reps = (token >> kIndexRepShift) & 0x1FF;
  reps = ((reps << 3) | kRepImmediate32) & 0x1FF;
  return (token & ~kIndexBitsMask) | ((dims + 1) << kIndexDimShift) | (reps << kIndexRepShift);
}

uint32_t PopFrontDimension(uint32_t token) {
  const uint32_t dims = (token >> kIndexDimShift) & 3;
  uint32_t reps = (token >> kIndexRepShift) & 0x1FF;
  reps >>= 3;
  return (token & ~kIndexBitsMask) | ((dims - 1) << kIndexDimShift) | (reps << kIndexRepShift);
}

uint32_t DeclarationOperandToken(RegClass cls, uint32_t dimensions) {
  return (OperandTypeOfClass(cls) << 12) | kDeclOperandComponents | (dimensions << kIndexDimShift);
}

/// 5.0 declaration operands: constant buffers keep four components and a swizzle, the others have none.
uint32_t DeclarationOperandToken50(RegClass cls, uint32_t dimensions) {
  const uint32_t components = cls == RegClass::CBuffer ? kDeclOperandComponents : 0;
  return (OperandTypeOfClass(cls) << 12) | components | (dimensions << kIndexDimShift);
}

struct DeclarationLayout {
  size_t operand_at = 0;  ///< Index of the operand token inside the instruction.
  RegClass cls = RegClass::Resource;
};

std::expected<DeclarationLayout, std::string> LocateDeclarationOperand(std::span<const uint32_t> instruction) {
  DeclarationLayout layout;
  size_t at = 1;
  if (IsExtended(instruction[0])) {
    uint32_t extension = 0;
    do {
      if (at >= instruction.size()) {
        return std::unexpected("declaration extension runs past the instruction");
      }
      extension = instruction[at++];
    } while (extension & kExtendedBit);
  }
  if (at >= instruction.size()) {
    return std::unexpected("declaration has no operand");
  }
  layout.operand_at = at;
  if (!ClassOfOperandType((instruction[at] >> 12) & 0xFF, layout.cls)) {
    return std::unexpected("declaration operand is not a resource");
  }
  return layout;
}

std::string Name(RegClass cls, uint32_t reg) { return std::format("{}{}", ClassPrefix(cls), reg); }

}  // namespace

bool IsSm51VersionToken(uint32_t version_token) noexcept {
  return ((version_token >> 4) & 0xF) == 5 && (version_token & 0xF) == 1;
}

// ---- 5.0 -> 5.1 --------------------------------------------------------------------------------

std::expected<std::vector<uint32_t>, std::string> UpgradeTokens(std::span<const uint32_t> words, const PlacementMap& placements) {
  if (words.size() < 2) {
    return std::unexpected("[sm5] shader chunk too small to upgrade");
  }
  std::map<std::pair<RegClass, uint32_t>, uint32_t> range_ids;
  uint32_t next_id[4] = {0, 0, 0, 0};
  const auto real_of = [&](RegClass cls, uint32_t program_reg) {
    const auto it = placements.find({cls, program_reg});
    return it == placements.end() ? Placement{0, program_reg} : it->second;
  };

  std::vector<uint32_t> out;
  out.reserve(words.size() + words.size() / 8);
  out.push_back((words[0] & ~0xFU) | 1U);
  out.push_back(0);

  const size_t end = std::min<size_t>(words[1], words.size());
  size_t at = 2;
  while (at < end) {
    const uint32_t token = words[at];
    const Opcode opcode = static_cast<Opcode>(OpcodeOf(token));
    if (opcode == Opcode::CustomData) {
      if (at + 1 >= end) {
        return std::unexpected("[sm5] truncated custom data block");
      }
      const size_t length = words[at + 1];
      if (length < 2 || at + length > end) {
        return std::unexpected("[sm5] custom data block runs past the shader");
      }
      out.insert(out.end(), words.begin() + static_cast<ptrdiff_t>(at), words.begin() + static_cast<ptrdiff_t>(at + length));
      at += length;
      continue;
    }
    const size_t length = LengthOf(token);
    if (length == 0 || at + length > end) {
      return std::unexpected("[sm5] instruction length runs past the shader");
    }
    if (IsUnsupportedOpcode(opcode)) {
      return std::unexpected("[sm5] SM5.1 has no class linkage (interfaces and function tables), can't upgrade this shader");
    }
    const auto instruction = words.subspan(at, length);

    if (IsResourceDeclaration(opcode)) {
      auto layout = LocateDeclarationOperand(instruction);
      if (!layout) {
        return std::unexpected("[sm5] " + layout.error());
      }
      const size_t register_at = layout->operand_at + 1;
      // 5.0: operand token, register (cbuffers: then the size), then the tail (return type, stride)
      if (register_at >= instruction.size()) {
        return std::unexpected("[sm5] declaration is missing its register");
      }
      const RegClass cls = layout->cls;
      const uint32_t program_reg = instruction[register_at];
      if (!range_ids.emplace(std::pair{cls, program_reg}, next_id[static_cast<size_t>(cls)]).second) {
        return std::unexpected(std::format("[sm5] {} is declared twice", Name(cls, program_reg)));
      }
      const uint32_t id = next_id[static_cast<size_t>(cls)]++;
      const Placement real = real_of(cls, program_reg);
      std::vector<uint32_t> upgraded(instruction.begin(), instruction.begin() + static_cast<ptrdiff_t>(layout->operand_at));
      upgraded.push_back(DeclarationOperandToken(cls, kUnusedSpaceDimension));
      upgraded.push_back(id);
      upgraded.push_back(real.reg);
      upgraded.push_back(real.reg);  // one register per range
      upgraded.insert(upgraded.end(), instruction.begin() + static_cast<ptrdiff_t>(register_at + 1), instruction.end());
      upgraded.push_back(real.space);
      upgraded[0] = SetLength(upgraded[0], upgraded.size());
      out.insert(out.end(), upgraded.begin(), upgraded.end());
      at += length;
      continue;
    }
    if (model::OpcodeIsDeclaration(opcode)) {
      out.insert(out.end(), instruction.begin(), instruction.end());
      at += length;
      continue;
    }

    // An instruction: extended opcode tokens, then operands.
    std::vector<uint32_t> upgraded;
    upgraded.push_back(token);
    size_t cursor = 1;
    if (IsExtended(token)) {
      std::vector<uint32_t> kept;
      uint32_t extension = 0;
      do {
        if (cursor >= length) {
          return std::unexpected("[sm5] instruction extension runs past the instruction");
        }
        extension = instruction[cursor++];
        const uint32_t type = extension & kExtendedTypeMask;
        // fxc doesn't write the resource dimension and return type tokens in 5.1
        if (type != kExtendedResourceDim && type != kExtendedResourceReturnType) {
          kept.push_back(extension & ~kExtendedBit);
        }
      } while (extension & kExtendedBit);
      for (size_t i = 0; i < kept.size(); ++i) {
        upgraded.push_back(kept[i] | (i + 1 < kept.size() ? kExtendedBit : 0U));
      }
      if (kept.empty()) {
        upgraded[0] &= ~kExtendedBit;
      }
    }
    while (cursor < length) {
      auto operand = ReadOperand(instruction, cursor, length);
      if (!operand) {
        return std::unexpected("[sm5] " + operand.error());
      }
      RegClass cls = RegClass::Resource;
      if (ClassOfOperandType(operand->Type(), cls)) {
        if (operand->Dim() == 0 || operand->Rep(0) != kRepImmediate32 || operand->index[0].size() != 1) {
          return std::unexpected(std::format("[sm5] {} operand is not immediately indexed", ClassPrefix(cls)));
        }
        const uint32_t program_reg = operand->index[0][0];
        const auto id = range_ids.find({cls, program_reg});
        if (id == range_ids.end()) {
          return std::unexpected(std::format("[sm5] {} is used but not declared", Name(cls, program_reg)));
        }
        if (operand->Dim() >= 3) {
          return std::unexpected("[sm5] operand already has the maximum number of index dimensions");
        }
        operand->token = PushFrontDimension(operand->token);
        operand->index[0][0] = real_of(cls, program_reg).reg;
        operand->index.insert(operand->index.begin(), std::vector<uint32_t>{id->second});
      }
      Emit(*operand, upgraded);
    }
    upgraded[0] = SetLength(upgraded[0], upgraded.size());
    out.insert(out.end(), upgraded.begin(), upgraded.end());
    at += length;
  }
  out[1] = static_cast<uint32_t>(out.size());
  return out;
}

// ---- 5.1 -> 5.0-shaped -------------------------------------------------------------------------

namespace {

struct Declared {
  RegClass cls = RegClass::Resource;
  uint32_t id = 0;
  uint32_t space = 0;
  uint32_t reg = 0;
  uint32_t program_reg = 0;
};

constexpr uint32_t kFirstSyntheticRegister = 4096;

}  // namespace

std::expected<std::vector<uint32_t>, std::string> DowngradeTokens(std::span<const uint32_t> words, PlacementMap& placements) {
  if (words.size() < 2) {
    return std::unexpected("[sm5] shader chunk too small");
  }
  const size_t end = std::min<size_t>(words[1], words.size());

  // Pass 1: the declarations
  std::vector<Declared> declared;
  for (size_t at = 2; at < end;) {
    const uint32_t token = words[at];
    const Opcode opcode = static_cast<Opcode>(OpcodeOf(token));
    size_t length = LengthOf(token);
    if (opcode == Opcode::CustomData) {
      length = at + 1 < end ? words[at + 1] : 0;
    }
    if (length == 0 || at + length > end) {
      return std::unexpected("[sm5] instruction length runs past the shader");
    }
    if (IsUnsupportedOpcode(opcode)) {
      return std::unexpected("[sm5] SM5.1 class linkage isn't supported");
    }
    if (IsResourceDeclaration(opcode)) {
      const auto instruction = words.subspan(at, length);
      auto layout = LocateDeclarationOperand(instruction);
      if (!layout) {
        return std::unexpected("[sm5] " + layout.error());
      }
      const size_t operand_at = layout->operand_at;
      if (((instruction[operand_at] >> kIndexDimShift) & 3) != 3 || operand_at + 4 >= instruction.size()) {
        return std::unexpected("[sm5] SM5.1 declaration without range id, bounds and space");
      }
      Declared decl;
      decl.cls = layout->cls;
      decl.id = instruction[operand_at + 1];
      decl.reg = instruction[operand_at + 2];
      const uint32_t upper = instruction[operand_at + 3];
      decl.space = instruction[instruction.size() - 1];
      if (upper != decl.reg) {
        return std::unexpected(std::format("[sm5] {} space{} is a range of registers ({}..{}), which isn't supported", Name(decl.cls, decl.reg), decl.space, decl.reg, upper));
      }
      declared.push_back(decl);
    }
    at += length;
  }

  // Program registers: space 0 keeps its own register, the rest get free ones
  std::set<std::pair<RegClass, uint32_t>> claimed;
  for (Declared& decl : declared) {
    if (decl.space == 0 && claimed.insert({decl.cls, decl.reg}).second) {
      decl.program_reg = decl.reg;
    } else {
      decl.program_reg = UINT32_MAX;
    }
  }
  uint32_t next_synthetic[4] = {kFirstSyntheticRegister, kFirstSyntheticRegister, kFirstSyntheticRegister, kFirstSyntheticRegister};
  std::map<std::pair<RegClass, uint32_t>, size_t> by_id;
  for (size_t i = 0; i < declared.size(); ++i) {
    Declared& decl = declared[i];
    if (decl.program_reg == UINT32_MAX) {
      uint32_t& next = next_synthetic[static_cast<size_t>(decl.cls)];
      while (claimed.contains({decl.cls, next})) {
        ++next;
      }
      decl.program_reg = next;
      claimed.insert({decl.cls, next});
    }
    if (!by_id.emplace(std::pair{decl.cls, decl.id}, i).second) {
      return std::unexpected(std::format("[sm5] range id {} of {} is declared twice", decl.id, ClassPrefix(decl.cls)));
    }
    if (decl.space != 0 || decl.program_reg != decl.reg) {
      placements[{decl.cls, decl.program_reg}] = {decl.space, decl.reg};
    }
  }

  // Pass 2: rewrite
  std::vector<uint32_t> out;
  out.reserve(words.size());
  out.push_back(words[0]);
  out.push_back(0);
  for (size_t at = 2; at < end;) {
    const uint32_t token = words[at];
    const Opcode opcode = static_cast<Opcode>(OpcodeOf(token));
    if (opcode == Opcode::CustomData) {
      const size_t length = words[at + 1];
      out.insert(out.end(), words.begin() + static_cast<ptrdiff_t>(at), words.begin() + static_cast<ptrdiff_t>(at + length));
      at += length;
      continue;
    }
    const size_t length = LengthOf(token);
    const auto instruction = words.subspan(at, length);
    if (IsResourceDeclaration(opcode)) {
      const auto layout = LocateDeclarationOperand(instruction);
      const size_t operand_at = layout->operand_at;
      const auto& decl = declared[by_id.at({layout->cls, instruction[operand_at + 1]})];
      std::vector<uint32_t> downgraded(instruction.begin(), instruction.begin() + static_cast<ptrdiff_t>(operand_at));
      const bool cbuffer = layout->cls == RegClass::CBuffer;
      // 5.0 constant buffer operands carry their size as a second index
      downgraded.push_back(DeclarationOperandToken50(layout->cls, cbuffer ? 2 : 1));
      downgraded.push_back(decl.program_reg);
      downgraded.insert(downgraded.end(), instruction.begin() + static_cast<ptrdiff_t>(operand_at + 4), instruction.end() - 1);
      downgraded[0] = SetLength(downgraded[0], downgraded.size());
      out.insert(out.end(), downgraded.begin(), downgraded.end());
      at += length;
      continue;
    }
    if (model::OpcodeIsDeclaration(opcode)) {
      out.insert(out.end(), instruction.begin(), instruction.end());
      at += length;
      continue;
    }
    std::vector<uint32_t> downgraded;
    size_t cursor = 1;
    downgraded.push_back(token);
    if (IsExtended(token)) {
      uint32_t extension = 0;
      do {
        if (cursor >= length) {
          return std::unexpected("[sm5] instruction extension runs past the instruction");
        }
        extension = instruction[cursor++];
        downgraded.push_back(extension);
      } while (extension & kExtendedBit);
    }
    while (cursor < length) {
      auto operand = ReadOperand(instruction, cursor, length);
      if (!operand) {
        return std::unexpected("[sm5] " + operand.error());
      }
      RegClass cls = RegClass::Resource;
      if (ClassOfOperandType(operand->Type(), cls)) {
        if (operand->Dim() < 2 || operand->Rep(0) != kRepImmediate32 || operand->Rep(1) != kRepImmediate32 ||
            operand->index[0].size() != 1 || operand->index[1].size() != 1) {
          return std::unexpected(std::format("[sm5] {} operand is dynamically indexed, which isn't supported", ClassPrefix(cls)));
        }
        const auto it = by_id.find({cls, operand->index[0][0]});
        if (it == by_id.end()) {
          return std::unexpected(std::format("[sm5] {} range id {} is used but not declared", ClassPrefix(cls), operand->index[0][0]));
        }
        operand->token = PopFrontDimension(operand->token);
        operand->index.erase(operand->index.begin());
        operand->index[0][0] = declared[it->second].program_reg;
      }
      Emit(*operand, downgraded);
    }
    downgraded[0] = SetLength(downgraded[0], downgraded.size());
    out.insert(out.end(), downgraded.begin(), downgraded.end());
    at += length;
  }
  out[1] = static_cast<uint32_t>(out.size());
  return out;
}

}  // namespace dxp::sm5::sm51
