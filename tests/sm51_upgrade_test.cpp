// SM5.0 -> SM5.1 upgrade of whole shaders, checked three ways:
//  1. against fxc: the oracle pair (same HLSL as ps_5_0 and ps_5_1) must match declaration for declaration and
//     instruction for instruction (fxc orders independent instructions differently between runs, so the
//     instructions are compared as a multiset),
//  2. against Microsoft's disassembler: every shader given on the command line, upgraded, must disassemble to
//     the same instructions as the original once range ids and the 5.1 declaration syntax are folded away,
//  3. against itself: reading the 5.1 output back and writing it again changes nothing.
#include <d3dcompiler.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "dxp/sm5/ShaderProgram.hpp"
#include "dxp/sm5/Sm51.hpp"

namespace {

int failures = 0;

void Fail(const std::string& what) {
  std::cerr << "FAIL: " << what << "\n";
  ++failures;
}

bool ReadAll(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return false;
  out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return true;
}

// The dwords of a container's SHDR/SHEX chunk
std::vector<uint32_t> ShaderWords(const std::vector<uint8_t>& bytes) {
  uint32_t count = 0;
  std::memcpy(&count, &bytes[28], 4);
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t offset = 0;
    std::memcpy(&offset, &bytes[32 + (i * 4)], 4);
    if (std::memcmp(&bytes[offset], "SHEX", 4) == 0 || std::memcmp(&bytes[offset], "SHDR", 4) == 0) {
      uint32_t size = 0;
      std::memcpy(&size, &bytes[offset + 4], 4);
      std::vector<uint32_t> words(size / 4);
      std::memcpy(words.data(), &bytes[offset + 8], size);
      return words;
    }
  }
  return {};
}

std::vector<std::vector<uint32_t>> Instructions(const std::vector<uint32_t>& words) {
  std::vector<std::vector<uint32_t>> out;
  for (size_t at = 2; at < words.size();) {
    uint32_t length = (words[at] >> 24) & 0x7F;
    if ((words[at] & 0x7FF) == 53) length = words[at + 1];  // custom data
    if (length == 0 || at + length > words.size()) break;
    out.emplace_back(words.begin() + static_cast<ptrdiff_t>(at), words.begin() + static_cast<ptrdiff_t>(at + length));
    at += length;
  }
  return out;
}

std::string Disassemble(const std::vector<uint8_t>& bytes) {
  Microsoft::WRL::ComPtr<ID3DBlob> blob;
  if (FAILED(D3DDisassemble(bytes.data(), bytes.size(), D3D_DISASM_DISABLE_DEBUG_INFO, nullptr, &blob)) || !blob) return {};
  return std::string(static_cast<const char*>(blob->GetBufferPointer()), blob->GetBufferSize());
}

struct Folded {
  std::string version;
  std::multiset<std::string> declarations;
  std::vector<std::string> instructions;
  std::set<std::string> spaces;
};

std::string ReplaceAll(const std::string& text, const std::regex& pattern, const std::function<std::string(const std::smatch&)>& replacement) {
  std::string out;
  auto last = text.cbegin();
  for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
    out.append(last, text.cbegin() + it->position());
    out += replacement(*it);
    last = text.cbegin() + it->position() + it->length();
  }
  out.append(last, text.cend());
  return out;
}

std::string LowerClass(const std::string& letters) {
  std::string out = letters;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

// Folds the 5.1 syntax back to the 5.0 one (and drops 5.0's `_indexable(dim)(types)`), so the two compare equal
Folded Fold(const std::string& text) {
  // 5.0 text: sample_aoffimmi_indexable(1,-2,0)(texture2d)(float,float,float,float); 5.1 keeps only the offsets
  static const std::regex indexable(R"(_indexable(\(-?\d+,-?\d+,-?\d+\))?\([^)]*\)\([^)]*\))");
  static const std::regex range(R"(\b(CB|T|S|U)\d+\[(\d+):\2\])");  // CB0[1:1] -> cb1
  static const std::regex operand_cb(R"(\bCB\d+\[(\d+)\]\[)");  // CB0[1][r1.z] -> cb1[r1.z]
  static const std::regex operand_other(R"(\b(T|S|U)\d+\[(\d+)\])");  // T1[7] -> t7
  static const std::regex space(R"(, space=(\d+))");
  Folded folded;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line.starts_with("//")) continue;
    std::smatch match;
    while (std::regex_search(line, match, space)) {
      folded.spaces.insert(match[1]);
      line = match.prefix().str() + match.suffix().str();
    }
    line = std::regex_replace(line, indexable, "$1");
    // constant buffer declarations keep their upper case in 5.0 text (CB1[3]), the others are lower case (t7)
    line = ReplaceAll(line, range, [](const std::smatch& m) { return (m[1] == "CB" ? std::string("CB") : LowerClass(m[1])) + m[2].str(); });
    line = ReplaceAll(line, operand_cb, [](const std::smatch& m) { return "cb" + m[1].str() + "["; });
    line = ReplaceAll(line, operand_other, [](const std::smatch& m) { return LowerClass(m[1]) + m[2].str(); });
    if (line.starts_with("ps_") || line.starts_with("vs_") || line.starts_with("cs_") || line.starts_with("gs_") ||
        line.starts_with("hs_") || line.starts_with("ds_")) {
      folded.version = line;
      continue;
    }
    if (line.starts_with("dcl_")) {
      folded.declarations.insert(line);
    } else {
      folded.instructions.push_back(line);
    }
  }
  return folded;
}

std::string Lower(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

void CheckOracle(const std::string& dir) {
  std::vector<uint8_t> v50;
  std::vector<uint8_t> v51;
  if (!ReadAll(dir + "/sm51_oracle.ps_5_0.cso", v50) || !ReadAll(dir + "/sm51_oracle.ps_5_1.cso", v51)) {
    Fail("oracle fixtures not found in " + dir);
    return;
  }
  auto program = dxp::sm5::ShaderProgram::FromBytes(v50);
  if (!program) return Fail("oracle: " + program.error());
  program->force_sm51 = true;
  auto upgraded = program->Serialize();
  if (!upgraded) return Fail("oracle: " + upgraded.error());

  const auto ours = Instructions(ShaderWords(*upgraded));
  const auto theirs = Instructions(ShaderWords(v51));
  if (ours.size() != theirs.size()) {
    return Fail("oracle: " + std::to_string(ours.size()) + " instructions, fxc has " + std::to_string(theirs.size()));
  }
  // declarations (the part before the first non-declaration) in order, the rest as a multiset
  auto is_declaration = [](const std::vector<uint32_t>& ins) {
    const uint32_t op = ins[0] & 0x7FF;
    return (op >= 88 && op <= 106) || (op >= 143 && op <= 162);
  };
  size_t split = 0;
  while (split < theirs.size() && is_declaration(theirs[split])) ++split;
  for (size_t i = 0; i < split; ++i) {
    if (ours[i] != theirs[i]) Fail("oracle: declaration " + std::to_string(i) + " differs from fxc's");
  }
  std::multiset<std::vector<uint32_t>> a(ours.begin() + static_cast<ptrdiff_t>(split), ours.end());
  std::multiset<std::vector<uint32_t>> b(theirs.begin() + static_cast<ptrdiff_t>(split), theirs.end());
  if (a != b) Fail("oracle: the instructions differ from fxc's");
  if ((ShaderWords(*upgraded)[0] & 0xFF) != 0x51) Fail("oracle: version token isn't 5.1");
}

void CheckShader(const std::string& path) {
  std::vector<uint8_t> input;
  if (!ReadAll(path, input)) return Fail("cannot read " + path);
  auto program = dxp::sm5::ShaderProgram::FromBytes(input);
  if (!program) return Fail(path + ": " + program.error());
  if (program->minor_version != 0 || program->major_version != 5) return;  // not a 5.0 fixture
  program->force_sm51 = true;
  auto upgraded = program->Serialize();
  if (!upgraded) return Fail(path + ": upgrade: " + upgraded.error());

  const std::string before = Disassemble(input);
  const std::string after = Disassemble(*upgraded);
  if (before.empty()) return Fail(path + ": the original doesn't disassemble");
  if (after.empty()) return Fail(path + ": the 5.1 output doesn't disassemble");
  const Folded a = Fold(before);
  const Folded b = Fold(after);
  if (Lower(b.version).find("_5_1") == std::string::npos) Fail(path + ": output is not 5.1: " + b.version);
  if (b.spaces.size() > 1 || (b.spaces.size() == 1 && *b.spaces.begin() != "0")) Fail(path + ": a resource ended up outside space 0");
  if (a.declarations != b.declarations) {
    Fail(path + ": declarations differ");
    for (const auto& d : a.declarations) {
      if (b.declarations.count(d) == 0) std::cerr << "  only before: " << d << "\n";
    }
    for (const auto& d : b.declarations) {
      if (a.declarations.count(d) == 0) std::cerr << "  only after:  " << d << "\n";
    }
  }
  if (a.instructions != b.instructions) {
    Fail(path + ": instructions differ");
    for (size_t i = 0; i < std::min(a.instructions.size(), b.instructions.size()); ++i) {
      if (a.instructions[i] != b.instructions[i]) {
        std::cerr << "  [" << i << "] before: " << a.instructions[i] << "\n  [" << i << "] after:  " << b.instructions[i] << "\n";
        break;
      }
    }
    if (a.instructions.size() != b.instructions.size()) std::cerr << "  instruction counts " << a.instructions.size() << " vs " << b.instructions.size() << "\n";
  }

  // 5.1 in, 5.1 out: nothing changes
  auto again = dxp::sm5::ShaderProgram::FromBytes(*upgraded);
  if (!again) return Fail(path + ": reading the 5.1 output: " + again.error());
  if (!again->input_sm51 || !again->placements.empty()) Fail(path + ": 5.1 output read back wrong");
  auto rewritten = again->Serialize();
  if (!rewritten) return Fail(path + ": writing the 5.1 output again: " + rewritten.error());
  if (*rewritten != *upgraded) Fail(path + ": 5.1 -> 5.1 isn't stable");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: sm51_upgrade_test <shaders dir> <shader.cso>...\n";
    return 1;
  }
  CheckOracle(argv[1]);
  for (int i = 2; i < argc; ++i) {
    CheckShader(argv[i]);
  }
  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  std::cout << "SM5.1 upgrade: " << (argc - 2) << " shaders ok\n";
  return 0;
}
