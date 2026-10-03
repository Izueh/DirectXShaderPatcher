#include "dxp/sm6/step/common/Rule_impl.hpp"
#include <algorithm>
#include "dxp/sm6/step/common/Emit.hpp"

namespace dxp::sm6::step {
using common::CompileValueTypePattern;

auto OperandPatternData::Compile() const -> std::expected<OperandPattern, std::string> {
  OperandPattern result;
  result.operand_index = index;
  result.capture_name = capture;
  result.match_capture = match_capture;
  result.export_as = export_as;
  if (kind.has_value()) {
    result.kind = *kind;
  }
  if (result.kind.has_value()) {
    switch (*result.kind) {
      case OperandKind::Call:
        if (instruction) {
          auto instr = instruction->Compile();
          if (!instr) return std::unexpected(std::move(instr.error()));
          result.instruction = xyz::indirect<InstructionPattern>(std::move(*instr));
        }
        break;
      case OperandKind::Constant:
        result.constant_int_values = constant_int_values;
        result.constant_float_values = constant_float_values;
        result.component_type = component_type;
        break;
      case OperandKind::Resource:
        result.resource_class = resource_class;
        result.resource_kind = resource_kind;
        if (!resource_name.empty()) result.resource_name = resource_name;
        if (!resource_name_like.empty()) result.resource_name_like_pattern = resource_name_like;
        result.resource_register_index = register_index;
        result.resource_space = space;
        break;
      case OperandKind::Undefined:
        break;
    }
  }
  return result;
}

auto EmitOperandPatternData::Compile() const -> std::expected<EmitOperand, std::string> {
  EmitOperand result;
  result.operand_index = index;
  if (kind.has_value()) result.kind = *kind;
  if (!capture.empty()) result.capture = capture;
  result.constant_int_values = constant_int_values;
  result.constant_float_values = constant_float_values;
  result.component_type = component_type;
  if (!handle.empty()) result.handle = handle;
  if (instruction) {
    auto compiled = instruction->Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.instruction = xyz::indirect<InstructionPattern>(std::move(*compiled));
  }
  return result;
}

auto RuleData::Compile() const -> std::expected<Rule, std::string> {
  Rule result;
  result.name = name;
  for (const auto& m : match) {
    auto compiled = m.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.match_patterns.push_back(std::move(*compiled));
  }
  for (const auto& e : emit) {
    auto compiled = e.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.emit_patterns.push_back(std::move(*compiled));
  }
  return result;
}

auto MatchInstructionPatternData::Compile() const -> std::expected<InstructionPattern, std::string> {
  if (extract.has_value() && opcode != "extractvalue") {
    return std::unexpected("extract is only valid with opcode 'extractvalue'");
  }
  if (extract_index.has_value() && opcode != "extractvalue") {
    return std::unexpected("extract_index is only valid with opcode 'extractvalue'");
  }
  if (extract.has_value() && extract_index.has_value()) {
    return std::unexpected("extract and extract_index cannot both be specified");
  }
  if (extract.has_value() && extract->indices.empty()) {
    return std::unexpected("extract.indices requires at least one index");
  }
  const bool is_extractvalue = opcode == "extractvalue" || extract.has_value() || extract_index.has_value();
  for (const auto& op : operands) {
    if (is_extractvalue && op.index > 0) {
      return std::unexpected("extractvalue has only aggregate operand 0; operand index " + std::to_string(op.index) + " is invalid");
    }
  }
  InstructionPattern result;
  result.capture_name = capture;
  result.match_capture = match_capture;
  if (extract.has_value()) {
    ExtractValuePattern compiled_extract;
    compiled_extract.indices = extract->indices;
    if (extract->aggregate_type.has_value()) {
      auto aggregate_type = CompileValueTypePattern(*extract->aggregate_type);
      if (!aggregate_type) return std::unexpected("extract.aggregate_type: " + std::move(aggregate_type.error()));
      compiled_extract.aggregate_type = std::move(*aggregate_type);
    }
    if (extract->result_type.has_value()) {
      auto result_type = CompileValueTypePattern(*extract->result_type);
      if (!result_type) return std::unexpected("extract.result_type: " + std::move(result_type.error()));
      compiled_extract.result_type = std::move(*result_type);
    }
    result.extract = std::move(compiled_extract);
  } else if (extract_index.has_value()) {
    // v1 normalization: legacy single index becomes a single-element path.
    ExtractValuePattern compiled_extract;
    compiled_extract.indices = {*extract_index};
    result.extract = std::move(compiled_extract);
  }
  if (!opcode.empty()) result.opcode = opcode;
  for (const auto& op : operands) {
    auto compiled = op.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.operand_patterns.push_back(std::move(*compiled));
  }
  return result;
}

auto EmitPatternData::Compile() const -> std::expected<EmitPattern, std::string> {
  const std::string emit_name = name.empty() ? (opcode.empty() ? std::string("<unnamed>") : opcode) : name;
  if (!function_name.empty() && (!opcode.empty() || cast_opcode.has_value() || !capture.empty() || extract.has_value() || !aggregate.empty() || extract_index.has_value() || result_component_type.has_value())) {
    return std::unexpected("emit '" + emit_name + "': function cannot be combined with other emit modes or result_component_type");
  }
  const bool legacy_extract_mode = !aggregate.empty() || extract_index.has_value();
  if (extract.has_value()) {
    if (!opcode.empty() || cast_opcode.has_value() || !capture.empty() || !operands.empty()) {
      return std::unexpected("emit '" + emit_name + "': extract cannot be combined with opcode, cast_opcode, capture, or operands");
    }
    if (legacy_extract_mode) {
      return std::unexpected("emit '" + emit_name + "': extract cannot be combined with legacy aggregate/extract_index");
    }
    if (extract->indices.empty()) {
      return std::unexpected("emit '" + emit_name + "': extract.indices requires at least one index");
    }
    if (extract->aggregate.empty()) {
      return std::unexpected("emit '" + emit_name + "': extract.aggregate is required");
    }
    if (result_component_type.has_value()) {
      return std::unexpected("emit '" + emit_name + "': extract cannot be combined with result_component_type");
    }
  }
  if (legacy_extract_mode) {
    if (!opcode.empty() || cast_opcode.has_value() || !capture.empty() || !operands.empty()) {
      return std::unexpected("emit '" + emit_name + "': aggregate cannot be combined with opcode, cast_opcode, capture, or operands");
    }
    if (extract_index.has_value() && aggregate.empty()) {
      return std::unexpected("emit '" + emit_name + "': legacy extract_index requires aggregate");
    }
    if (result_component_type.has_value()) {
      return std::unexpected("emit '" + emit_name + "': aggregate extraction cannot be combined with result_component_type");
    }
  }
  EmitPattern result;
  result.name = name;
  result.result_component_type = result_component_type;
  result.capture = capture;
  result.replace_captured = replace_captured;
  result.function_name = function_name;
  if (extract.has_value()) {
    EmitExtractValue compiled_extract;
    compiled_extract.aggregate = extract->aggregate;
    for (const auto& index : extract->indices) {
      if (const auto* literal = std::get_if<uint32_t>(&index)) {
        compiled_extract.indices.push_back(*literal);
      } else if (const auto* cap = std::get_if<CaptureExtractIndexData>(&index)) {
        if (cap->capture.empty()) {
          return std::unexpected("emit '" + emit_name + "': extract index capture requires a name");
        }
        compiled_extract.indices.emplace_back(CaptureExtractIndex{cap->capture});
      }
    }
    if (extract->aggregate_type.has_value()) {
      auto aggregate_type = CompileValueTypePattern(*extract->aggregate_type);
      if (!aggregate_type) return std::unexpected("emit '" + emit_name + "': extract.aggregate_type: " + std::move(aggregate_type.error()));
      compiled_extract.aggregate_type = std::move(*aggregate_type);
    }
    if (extract->result_type.has_value()) {
      auto result_type = CompileValueTypePattern(*extract->result_type);
      if (!result_type) return std::unexpected("emit '" + emit_name + "': extract.result_type: " + std::move(result_type.error()));
      compiled_extract.result_type = std::move(*result_type);
    }
    result.extract = std::move(compiled_extract);
  } else if (!aggregate.empty()) {
    // v1 normalization: legacy aggregate (with optional index, default 0).
    // Legacy result_component_type is rejected above; the canonical
    // extract.result_type is the explicit mechanism.
    EmitExtractValue compiled_extract;
    compiled_extract.aggregate = aggregate;
    compiled_extract.indices.push_back(extract_index.value_or(0));
    result.extract = std::move(compiled_extract);
  }
  if (!opcode.empty()) result.opcode = opcode;
  if (cast_opcode.has_value()) result.cast_opcode = *cast_opcode;
  for (const auto& op : operands) {
    auto compiled = op.Compile();
    if (!compiled) return std::unexpected(std::move(compiled.error()));
    result.operands.push_back(std::move(*compiled));
  }
  std::ranges::sort(result.operands, {}, &EmitOperand::operand_index);
  return result;
}

}  // namespace dxp::sm6::step
