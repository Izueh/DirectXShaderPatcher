#pragma once

#include <cstdint>

#include <dxc/DXIL/DxilConstants.h>

#include <dxp/ExportTypes.hpp>
#include <dxp/sm6/ResourceTypes.hpp>

namespace dxp::sm6 {

// ResourceKind ↔ hlsl::DXIL::ResourceKind
static_assert(static_cast<uint32_t>(ResourceKind::Invalid) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Invalid));
static_assert(static_cast<uint32_t>(ResourceKind::Texture1D) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture1D));
static_assert(static_cast<uint32_t>(ResourceKind::Texture2D) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture2D));
static_assert(static_cast<uint32_t>(ResourceKind::Texture2DMS) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture2DMS));
static_assert(static_cast<uint32_t>(ResourceKind::Texture3D) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture3D));
static_assert(static_cast<uint32_t>(ResourceKind::TextureCube) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::TextureCube));
static_assert(static_cast<uint32_t>(ResourceKind::Texture1DArray) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture1DArray));
static_assert(static_cast<uint32_t>(ResourceKind::Texture2DArray) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture2DArray));
static_assert(static_cast<uint32_t>(ResourceKind::Texture2DMSArray) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Texture2DMSArray));
static_assert(static_cast<uint32_t>(ResourceKind::TextureCubeArray) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::TextureCubeArray));
static_assert(static_cast<uint32_t>(ResourceKind::TypedBuffer) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::TypedBuffer));
static_assert(static_cast<uint32_t>(ResourceKind::RawBuffer) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::RawBuffer));
static_assert(static_cast<uint32_t>(ResourceKind::StructuredBuffer) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::StructuredBuffer));
static_assert(static_cast<uint32_t>(ResourceKind::CBuffer) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::CBuffer));
static_assert(static_cast<uint32_t>(ResourceKind::Sampler) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::Sampler));
static_assert(static_cast<uint32_t>(ResourceKind::TBuffer) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::TBuffer));
static_assert(static_cast<uint32_t>(ResourceKind::RTAccelerationStructure) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::RTAccelerationStructure));
static_assert(static_cast<uint32_t>(ResourceKind::FeedbackTexture2D) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::FeedbackTexture2D));
static_assert(static_cast<uint32_t>(ResourceKind::FeedbackTexture2DArray) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::FeedbackTexture2DArray));
static_assert(static_cast<uint32_t>(ResourceKind::NumEntries) == static_cast<uint32_t>(hlsl::DXIL::ResourceKind::NumEntries));

// ResourceClass ↔ hlsl::DXIL::ResourceClass
static_assert(static_cast<uint32_t>(ResourceClass::SRV) == static_cast<uint32_t>(hlsl::DXIL::ResourceClass::SRV));
static_assert(static_cast<uint32_t>(ResourceClass::UAV) == static_cast<uint32_t>(hlsl::DXIL::ResourceClass::UAV));
static_assert(static_cast<uint32_t>(ResourceClass::CBuffer) == static_cast<uint32_t>(hlsl::DXIL::ResourceClass::CBuffer));
static_assert(static_cast<uint32_t>(ResourceClass::Sampler) == static_cast<uint32_t>(hlsl::DXIL::ResourceClass::Sampler));
static_assert(static_cast<uint32_t>(ResourceClass::Invalid) == static_cast<uint32_t>(hlsl::DXIL::ResourceClass::Invalid));

// ComponentType ↔ hlsl::DXIL::ComponentType
static_assert(static_cast<uint32_t>(dxp::ComponentType::Invalid) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::Invalid));
static_assert(static_cast<uint32_t>(dxp::ComponentType::I1) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::I1));
static_assert(static_cast<uint32_t>(dxp::ComponentType::I16) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::I16));
static_assert(static_cast<uint32_t>(dxp::ComponentType::U16) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::U16));
static_assert(static_cast<uint32_t>(dxp::ComponentType::I32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::I32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::U32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::U32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::I64) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::I64));
static_assert(static_cast<uint32_t>(dxp::ComponentType::U64) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::U64));
static_assert(static_cast<uint32_t>(dxp::ComponentType::F16) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::F16));
static_assert(static_cast<uint32_t>(dxp::ComponentType::F32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::F32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::F64) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::F64));
static_assert(static_cast<uint32_t>(dxp::ComponentType::SNormF16) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::SNormF16));
static_assert(static_cast<uint32_t>(dxp::ComponentType::UNormF16) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::UNormF16));
static_assert(static_cast<uint32_t>(dxp::ComponentType::SNormF32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::SNormF32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::UNormF32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::UNormF32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::SNormF64) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::SNormF64));
static_assert(static_cast<uint32_t>(dxp::ComponentType::UNormF64) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::UNormF64));
static_assert(static_cast<uint32_t>(dxp::ComponentType::PackedS8x32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::PackedS8x32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::PackedU8x32) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::PackedU8x32));
static_assert(static_cast<uint32_t>(dxp::ComponentType::U8) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::U8));
static_assert(static_cast<uint32_t>(dxp::ComponentType::I8) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::I8));
static_assert(static_cast<uint32_t>(dxp::ComponentType::F8_E4M3FN) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::F8_E4M3));
static_assert(static_cast<uint32_t>(dxp::ComponentType::F8_E5M2) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::F8_E5M2));
static_assert(static_cast<uint32_t>(dxp::ComponentType::LastEntry) == static_cast<uint32_t>(hlsl::DXIL::ComponentType::LastEntry));

// InterpolationMode ↔ hlsl::DXIL::InterpolationMode
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::Undefined) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::Undefined));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::Constant) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::Constant));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::Linear) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::Linear));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::LinearCentroid) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::LinearCentroid));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::LinearNoperspective) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::LinearNoperspective));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::LinearNoperspectiveCentroid) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::LinearNoperspectiveCentroid));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::LinearSample) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::LinearSample));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::LinearNoperspectiveSample) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::LinearNoperspectiveSample));
static_assert(static_cast<uint32_t>(dxp::InterpolationMode::Invalid) == static_cast<uint32_t>(hlsl::DXIL::InterpolationMode::Invalid));

}  // namespace dxp::sm6
