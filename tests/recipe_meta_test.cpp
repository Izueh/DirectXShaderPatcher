// Recipe-level `meta:` (top-level key next to version/env/steps): parsed verbatim for SM5 and
// SM6, absent when omitted, kept by copy and move, and rejected when malformed.

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

#include "dxp/sm5/Recipe.hpp"
#include "dxp/sm6/Recipe.hpp"

namespace {

int failures = 0;

void Check(bool condition, const std::string& label) {
  if (condition) {
    std::cout << "ok: " << label << "\n";
  } else {
    std::cerr << "FAILED: " << label << "\n";
    ++failures;
  }
}

const char* kSteps = R"(steps:
  - kind: check_shader_version
    name: shader_model
    major: 5
    minor: 0
)";

const char* kMeta = R"(meta:
  name: Dithering Fix (Experimental)
  default_enabled: true
  strength: 0.5
  priority: 3
  tags: [dither, sr]
  env:
    SOME_VAR: { label: Strength, tooltip: Amount, min: 0, max: 1 }
  nothing: null
)";

bool HasExpectedMeta(const std::optional<dxp::MetaValue>& meta) {
  if (!meta) return false;
  const auto* object = meta->AsObject();
  if (object == nullptr || object->size() != 7) return false;
  // YAML order is kept.
  const char* const order[] = {"name", "default_enabled", "strength", "priority", "tags", "env", "nothing"};
  for (size_t i = 0; i < 7; ++i) {
    if ((*object)[i].first != order[i]) return false;
  }
  const auto* name = meta->Find("name");
  const auto* enabled = meta->Find("default_enabled");
  const auto* strength = meta->Find("strength");
  const auto* priority = meta->Find("priority");
  const auto* tags = meta->Find("tags");
  const auto* var = meta->Find("env") != nullptr ? meta->Find("env")->Find("SOME_VAR") : nullptr;
  if (!name || !name->AsString() || *name->AsString() != "Dithering Fix (Experimental)") return false;
  if (!enabled || !enabled->AsBool() || !*enabled->AsBool()) return false;
  if (!strength || !strength->AsNumber() || std::abs(*strength->AsNumber() - 0.5) > 1e-12) return false;
  if (!priority || !priority->AsNumber() || *priority->AsNumber() != 3.0) return false;
  if (!tags || !tags->AsArray() || tags->AsArray()->size() != 2) return false;
  if ((*tags->AsArray())[0].AsString() == nullptr || *(*tags->AsArray())[0].AsString() != "dither") return false;
  if ((*tags->AsArray())[1].AsString() == nullptr || *(*tags->AsArray())[1].AsString() != "sr") return false;
  if (!var || !var->AsObject() || var->AsObject()->size() != 4) return false;
  const auto* label = var->Find("label");
  const auto* max = var->Find("max");
  const auto* nothing = meta->Find("nothing");
  if (nothing == nullptr || !std::holds_alternative<std::monostate>(nothing->value)) return false;
  return label && label->AsString() && *label->AsString() == "Strength" && max && max->AsNumber() && *max->AsNumber() == 1.0;
}

template <typename Recipe>
void TestBackend(const std::string& backend) {
  const auto parsed = Recipe::ParseFromText(std::string(kMeta) + "version: 7\n" + kSteps);
  Check(parsed.has_value(), backend + " parses a recipe with meta");
  if (!parsed) {
    std::cerr << parsed.error() << "\n";
    return;
  }
  Check(HasExpectedMeta(parsed->GetMeta()), backend + " meta: nested objects, arrays, numbers, bools and strings in order");
  Check(parsed->GetVersion() == 7 && parsed->GetStepCount() == 1, backend + " meta does not disturb version or steps");

  const auto flow = Recipe::ParseFromText(std::string("meta: { name: Flow, n: 2 }\n") + kSteps);
  Check(flow && flow->GetMeta() && flow->GetMeta()->Find("name") && *flow->GetMeta()->Find("name")->AsString() == "Flow",
        backend + " flow-style meta parses");

  const auto absent = Recipe::ParseFromText(kSteps);
  Check(absent && !absent->GetMeta().has_value(), backend + " absent meta is nullopt");
  Check(!Recipe{}.GetMeta().has_value(), backend + " default recipe has no meta");

  Recipe copied(*parsed);
  Check(HasExpectedMeta(copied.GetMeta()), backend + " copy constructor keeps meta");
  Recipe copy_assigned;
  copy_assigned = *parsed;
  Check(HasExpectedMeta(copy_assigned.GetMeta()), backend + " copy assignment keeps meta");
  Recipe moved(std::move(copied));
  Check(HasExpectedMeta(moved.GetMeta()), backend + " move constructor keeps meta");
  Recipe move_assigned(5);
  move_assigned = std::move(copy_assigned);
  Check(HasExpectedMeta(move_assigned.GetMeta()), backend + " move assignment keeps meta");
  Check(HasExpectedMeta(parsed->GetMeta()), backend + " source of a copy is unchanged");

  Recipe cleared(*parsed);
  cleared = *absent;
  Check(!cleared.GetMeta().has_value(), backend + " assigning a recipe without meta clears it");

  // A scalar is as valid as a map: meta is opaque.
  const auto scalar = Recipe::ParseFromText(std::string("meta: hello\n") + kSteps);
  Check(scalar && scalar->GetMeta() && scalar->GetMeta()->AsString() && *scalar->GetMeta()->AsString() == "hello",
        backend + " scalar meta parses");

  // Malformed YAML under meta fails like malformed declaration meta does.
  const auto malformed = Recipe::ParseFromText(std::string("meta: { name: [unterminated\n") + kSteps);
  Check(!malformed.has_value(), backend + " malformed meta is a parse error");
}

}  // namespace

int main() {
  TestBackend<dxp::sm5::Recipe>("SM5");
  TestBackend<dxp::sm6::Recipe>("SM6");
  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }
  return 0;
}
