#include "../shared.h"

// IS-FAST noise texture for shadow ray dithering (injected via ViewBinding)
Texture2DArray<float2> ISFASTNoise : register(t0, space50);

struct FViewConstants {
  float4 TranslatedWorldToClip[4];
  float4 RelativeWorldToClip[4];
  float4 ClipToRelativeWorld[4];
  float4 TranslatedWorldToView[4];
  float4 ViewToTranslatedWorld[4];
  float4 TranslatedWorldToCameraView[4];
  float4 CameraViewToTranslatedWorld[4];
  float4 ViewToClip[4];
  float4 ViewToClipNoAA[4];
  float4 ClipToView[4];
  float4 ClipToTranslatedWorld[4];
  float4 SVPositionToTranslatedWorld[4];
  float4 ScreenToRelativeWorld[4];
  float4 ScreenToTranslatedWorld[4];
  float4 MobileMultiviewShadowTransform[4];
  float3 ViewOriginHigh;
  float Padding972;
  float3 ViewForward;
  float Padding988;
  float3 ViewUp;
  float Padding1004;
  float3 ViewRight;
  float Padding1020;
  float3 HMDViewNoRollUp;
  float Padding1036;
  float3 HMDViewNoRollRight;
  float Padding1052;
  float4 InvDeviceZToWorldZTransform;
  float4 ScreenPositionScaleBias;
  float3 ViewOriginLow;
  float Padding1100;
  float3 TranslatedWorldCameraOrigin;
  float Padding1116;
  float3 WorldViewOriginHigh;
  float Padding1132;
  float3 WorldViewOriginLow;
  float Padding1148;
  float3 PreViewTranslationHigh;
  float Padding1164;
  float3 PreViewTranslationLow;
  float Padding1180;
  float4 PrevViewToClip[4];
  float4 PrevClipToView[4];
  float4 PrevTranslatedWorldToClip[4];
  float4 PrevTranslatedWorldToView[4];
  float4 PrevViewToTranslatedWorld[4];
  float4 PrevTranslatedWorldToCameraView[4];
  float4 PrevCameraViewToTranslatedWorld[4];
  float3 PrevTranslatedWorldCameraOrigin;
  float Padding1644;
  float3 PrevWorldCameraOriginHigh;
  float Padding1660;
  float3 PrevWorldCameraOriginLow;
  float Padding1676;
  float3 PrevWorldViewOriginHigh;
  float Padding1692;
  float3 PrevWorldViewOriginLow;
  float Padding1708;
  float3 PrevPreViewTranslationHigh;
  float Padding1724;
  float3 PrevPreViewTranslationLow;
  float Padding1740;
  float3 ViewTilePosition;
  float Padding1756;
  float3 RelativeWorldCameraOriginTO;
  float Padding1772;
  float3 RelativeWorldViewOriginTO;
  float Padding1788;
  float3 RelativePreViewTranslationTO;
  float Padding1804;
  float3 PrevRelativeWorldCameraOriginTO;
  float Padding1820;
  float3 PrevRelativeWorldViewOriginTO;
  float Padding1836;
  float3 RelativePrevPreViewTranslationTO;
  float Padding1852;
  float4 PrevClipToRelativeWorld[4];
  float4 PrevScreenToTranslatedWorld[4];
  float4 ClipToPrevClip[4];
  float4 ClipToPrevClipWithAA[4];
  float4 TemporalAAJitter;
  float4 GlobalClippingPlane;
  float2 FieldOfViewWideAngles;
  float2 PrevFieldOfViewWideAngles;
  float4 ViewRectMin;
  float4 ViewSizeAndInvSize;
  uint4 ViewRectMinAndSize;
  float4 LightProbeSizeRatioAndInvSizeRatio;
  float4 BufferSizeAndInvSize;
  float4 BufferBilinearUVMinMax;
  float4 ScreenToViewSpace;
  float2 BufferToSceneTextureScale;
  float2 ResolutionFractionAndInv;
  int NumSceneColorMSAASamples;
  float ProjectionDepthThicknessScale;
  float PreExposure;
  float OneOverPreExposure;
  float4 DiffuseOverrideParameter;
  float4 SpecularOverrideParameter;
  float4 NormalOverrideParameter;
  float2 RoughnessOverrideParameter;
  float PrevFrameGameTime;
  float PrevFrameRealTime;
  float OutOfBoundsMask;
  float Padding2372;
  float Padding2376;
  float Padding2380;
  float3 WorldCameraMovementSinceLastFrame;
  float CullingSign;
  float NearPlane;
  float GameTime;
  float RealTime;
  float DeltaTime;
  float MaterialTextureMipBias;
  float MaterialTextureDerivativeMultiply;
  uint Random;
  uint FrameNumber;
  uint FrameCounter;
  uint StateFrameIndexMod8;
  uint StateFrameIndex;
  uint DebugViewModeMask;
  uint WorldIsPaused;
  float CameraCut;
  float UnlitViewmodeMask;
  float Padding2460;
  float4 DirectionalLightColor;
  float3 DirectionalLightDirection;
  float Padding2492;
  float4 TranslucencyLightingVolumeMin[2];
  float4 TranslucencyLightingVolumeInvSize[2];
  float4 TemporalAAParams;
  float4 CircleDOFParams;
  float DepthOfFieldSensorWidth;
  float DepthOfFieldFocalDistance;
  float DepthOfFieldScale;
  float DepthOfFieldFocalLength;
  float DepthOfFieldFocalRegion;
  float DepthOfFieldNearTransitionRegion;
  float DepthOfFieldFarTransitionRegion;
  float MotionBlurNormalizedToPixel;
  float GeneralPurposeTweak;
  float GeneralPurposeTweak2;
  float DemosaicVposOffset;
  float DecalDepthBias;
  float3 IndirectLightingColorScale;
  float Padding2652;
  float3 PrecomputedIndirectLightingColorScale;
  float Padding2668;
  float3 PrecomputedIndirectSpecularColorScale;
  float Padding2684;
  float4 AtmosphereLightDirection[2];
  float4 AtmosphereLightIlluminanceOnGroundPostTransmittance[2];
  float4 AtmosphereLightIlluminanceOuterSpace[2];
  float4 AtmosphereLightDiscLuminance[2];
  float4 AtmosphereLightDiscCosHalfApexAngle_PPTrans[2];
  float4 SkyViewLutSizeAndInvSize;
  float3 SkyCameraTranslatedWorldOrigin;
  float Padding2876;
  float4 SkyPlanetTranslatedWorldCenterAndViewHeight;
  float4 SkyViewLutReferential[4];
  float4 SkyAtmosphereSkyLuminanceFactor;
  float SkyAtmospherePresentInScene;
  float SkyAtmosphereHeightFogContribution;
  float SkyAtmosphereBottomRadiusKm;
  float SkyAtmosphereTopRadiusKm;
  float4 SkyAtmosphereCameraAerialPerspectiveVolumeSizeAndInvSize;
  float SkyAtmosphereAerialPerspectiveStartDepthKm;
  float SkyAtmosphereCameraAerialPerspectiveVolumeDepthResolution;
  float SkyAtmosphereCameraAerialPerspectiveVolumeDepthResolutionInv;
  float SkyAtmosphereCameraAerialPerspectiveVolumeDepthSliceLengthKm;
  float SkyAtmosphereCameraAerialPerspectiveVolumeDepthSliceLengthKmInv;
  float SkyAtmosphereApplyCameraAerialPerspectiveVolume;
  float Padding3032;
  float Padding3036;
  float3 NormalCurvatureToRoughnessScaleBias;
  float RenderingReflectionCaptureMask;
  float RealTimeReflectionCapture;
  float RealTimeReflectionCapturePreExposure;
  float Padding3064;
  float Padding3068;
  float4 AmbientCubemapTint;
  float AmbientCubemapIntensity;
  float SkyLightApplyPrecomputedBentNormalShadowingFlag;
  float SkyLightAffectReflectionFlag;
  float SkyLightAffectGlobalIlluminationFlag;
  float4 SkyLightColor;
  float SkyLightVolumetricScatteringIntensity;
  float Padding3124;
  float Padding3128;
  float Padding3132;
  float4 MobileSkyIrradianceEnvironmentMap[8];
  float MobilePreviewMode;
  float HMDEyePaddingOffset;
  float ReflectionCubemapMaxMip;
  float ShowDecalsMask;
  uint DistanceFieldAOSpecularOcclusionMode;
  float IndirectCapsuleSelfShadowingIntensity;
  float Padding3288;
  float Padding3292;
  float3 ReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight;
  int StereoPassIndex;
  float4 GlobalVolumeTranslatedCenterAndExtent[6];
  float4 GlobalVolumeTranslatedWorldToUVAddAndMul[6];
  float4 GlobalDistanceFieldMipTranslatedWorldToUVScale[6];
  float4 GlobalDistanceFieldMipTranslatedWorldToUVBias[6];
  float GlobalDistanceFieldMipFactor;
  float GlobalDistanceFieldMipTransition;
  int GlobalDistanceFieldClipmapSizeInPages;
  int Padding3708;
  float3 GlobalDistanceFieldInvPageAtlasSize;
  float Padding3724;
  float3 GlobalDistanceFieldInvCoverageAtlasSize;
  float GlobalVolumeDimension;
  float GlobalVolumeTexelSize;
  float MaxGlobalDFAOConeDistance;
  uint NumGlobalSDFClipmaps;
  float CoveredExpandSurfaceScale;
  float NotCoveredExpandSurfaceScale;
  float NotCoveredMinStepScale;
  float DitheredTransparencyStepThreshold;
  float DitheredTransparencyTraceThreshold;
  int2 CursorPosition;
  float bCheckerboardSubsurfaceProfileRendering;
  float Padding3788;
  float3 VolumetricFogInvGridSize;
  float Padding3804;
  float3 VolumetricFogGridZParams;
  float Padding3820;
  float2 VolumetricFogSVPosToVolumeUV;
  float2 VolumetricFogViewGridUVToPrevViewRectUV;
  float2 VolumetricFogPrevViewGridRectUVToResourceUV;
  float2 VolumetricFogPrevUVMax;
  float2 VolumetricFogPrevUVMaxForTemporalBlend;
  float2 VolumetricFogScreenToResourceUV;
  float2 VolumetricFogUVMax;
  float VolumetricFogMaxDistance;
  float Padding3884;
  float3 VolumetricLightmapWorldToUVScale;
  float Padding3900;
  float3 VolumetricLightmapWorldToUVAdd;
  float Padding3916;
  float3 VolumetricLightmapIndirectionTextureSize;
  float VolumetricLightmapBrickSize;
  float3 VolumetricLightmapBrickTexelSize;
  float IndirectLightingCacheShowFlag;
  float EyeToPixelSpreadAngle;
  float Padding3956;
  float Padding3960;
  float Padding3964;
  float4 XRPassthroughCameraUVs[2];
  float GlobalVirtualTextureMipBias;
  uint VirtualTextureFeedbackShift;
  uint VirtualTextureFeedbackMask;
  uint VirtualTextureFeedbackStride;
  uint VirtualTextureFeedbackJitterOffset;
  uint VirtualTextureFeedbackSampleOffset;
  uint Padding4024;
  uint Padding4028;
  float4 RuntimeVirtualTextureMipLevel;
  float2 RuntimeVirtualTexturePackHeight;
  float Padding4056;
  float Padding4060;
  float4 RuntimeVirtualTextureDebugParams;
  int FarShadowStaticMeshLODBias;
  float MinRoughness;
  float Padding4088;
  float Padding4092;
  float4 HairRenderInfo;
  uint EnableSkyLight;
  uint HairRenderInfoBits;
  uint HairComponents;
  float bSubsurfacePostprocessEnabled;
  float4 SSProfilesTextureSizeAndInvSize;
  float4 SSProfilesPreIntegratedTextureSizeAndInvSize;
  float4 SpecularProfileTextureSizeAndInvSize;
  float3 PhysicsFieldClipmapCenter;
  float PhysicsFieldClipmapDistance;
  int PhysicsFieldClipmapResolution;
  int PhysicsFieldClipmapExponent;
  int PhysicsFieldClipmapCount;
  int PhysicsFieldTargetCount;
  int4 PhysicsFieldTargets[32];
  uint GPUSceneViewId;
  float ViewResolutionFraction;
  float SubSurfaceColorAsTransmittanceAtDistanceInMeters;
  float Padding4732;
  float4 TanAndInvTanHalfFOV;
  float4 PrevTanAndInvTanHalfFOV;
  float2 WorldDepthToPixelWorldRadius;
  float Padding4776;
  float Padding4780;
  float4 ScreenRayLengthMultiplier;
  float4 GlintLUTParameters0;
  float4 GlintLUTParameters1;
  int4 EnvironmentComponentsFlags;
  uint BindlessSampler_MaterialTextureBilinearWrapedSampler;
  uint Padding4852;
  uint BindlessSampler_MaterialTextureBilinearClampedSampler;
  uint Padding4860;
  uint BindlessSRV_VolumetricLightmapIndirectionTexture;
  uint Padding4868;
  uint BindlessSRV_VolumetricLightmapBrickAmbientVector;
  uint Padding4876;
  uint BindlessSRV_VolumetricLightmapBrickSHCoefficients0;
  uint Padding4884;
  uint BindlessSRV_VolumetricLightmapBrickSHCoefficients1;
  uint Padding4892;
  uint BindlessSRV_VolumetricLightmapBrickSHCoefficients2;
  uint Padding4900;
  uint BindlessSRV_VolumetricLightmapBrickSHCoefficients3;
  uint Padding4908;
  uint BindlessSRV_VolumetricLightmapBrickSHCoefficients4;
  uint Padding4916;
  uint BindlessSRV_VolumetricLightmapBrickSHCoefficients5;
  uint Padding4924;
  uint BindlessSRV_SkyBentNormalBrickTexture;
  uint Padding4932;
  uint BindlessSRV_DirectionalLightShadowingBrickTexture;
  uint Padding4940;
  uint BindlessSampler_VolumetricLightmapBrickAmbientVectorSampler;
  uint Padding4948;
  uint BindlessSampler_VolumetricLightmapTextureSampler0;
  uint Padding4956;
  uint BindlessSampler_VolumetricLightmapTextureSampler1;
  uint Padding4964;
  uint BindlessSampler_VolumetricLightmapTextureSampler2;
  uint Padding4972;
  uint BindlessSampler_VolumetricLightmapTextureSampler3;
  uint Padding4980;
  uint BindlessSampler_VolumetricLightmapTextureSampler4;
  uint Padding4988;
  uint BindlessSampler_VolumetricLightmapTextureSampler5;
  uint Padding4996;
  uint BindlessSampler_SkyBentNormalTextureSampler;
  uint Padding5004;
  uint BindlessSampler_DirectionalLightShadowingTextureSampler;
  uint Padding5012;
  uint BindlessSRV_GlobalDistanceFieldPageAtlasTexture;
  uint Padding5020;
  uint BindlessSRV_GlobalDistanceFieldCoverageAtlasTexture;
  uint Padding5028;
  uint BindlessSRV_GlobalDistanceFieldPageTableTexture;
  uint Padding5036;
  uint BindlessSRV_GlobalDistanceFieldMipTexture;
  uint Padding5044;
  uint BindlessSampler_GlobalDistanceFieldPageAtlasTextureSampler;
  uint Padding5052;
  uint BindlessSampler_GlobalDistanceFieldCoverageAtlasTextureSampler;
  uint Padding5060;
  uint BindlessSampler_GlobalDistanceFieldMipTextureSampler;
  uint Padding5068;
  uint BindlessSRV_AtmosphereTransmittanceTexture;
  uint Padding5076;
  uint BindlessSampler_AtmosphereTransmittanceTextureSampler;
  uint Padding5084;
  uint BindlessSRV_AtmosphereIrradianceTexture;
  uint Padding5092;
  uint BindlessSampler_AtmosphereIrradianceTextureSampler;
  uint Padding5100;
  uint BindlessSRV_AtmosphereInscatterTexture;
  uint Padding5108;
  uint BindlessSampler_AtmosphereInscatterTextureSampler;
  uint Padding5116;
  uint BindlessSRV_PerlinNoiseGradientTexture;
  uint Padding5124;
  uint BindlessSampler_PerlinNoiseGradientTextureSampler;
  uint Padding5132;
  uint BindlessSRV_PerlinNoise3DTexture;
  uint Padding5140;
  uint BindlessSampler_PerlinNoise3DTextureSampler;
  uint Padding5148;
  uint BindlessSRV_SobolSamplingTexture;
  uint Padding5156;
  uint BindlessSampler_SharedPointWrappedSampler;
  uint Padding5164;
  uint BindlessSampler_SharedPointClampedSampler;
  uint Padding5172;
  uint BindlessSampler_SharedBilinearWrappedSampler;
  uint Padding5180;
  uint BindlessSampler_SharedBilinearClampedSampler;
  uint Padding5188;
  uint BindlessSampler_SharedBilinearAnisoClampedSampler;
  uint Padding5196;
  uint BindlessSampler_SharedTrilinearWrappedSampler;
  uint Padding5204;
  uint BindlessSampler_SharedTrilinearClampedSampler;
  uint Padding5212;
  uint BindlessSRV_PreIntegratedBRDF;
  uint Padding5220;
  uint BindlessSampler_PreIntegratedBRDFSampler;
  uint Padding5228;
  uint BindlessSRV_SkyIrradianceEnvironmentMap;
  uint Padding5236;
  uint BindlessSRV_TransmittanceLutTexture;
  uint Padding5244;
  uint BindlessSampler_TransmittanceLutTextureSampler;
  uint Padding5252;
  uint BindlessSRV_SkyViewLutTexture;
  uint Padding5260;
  uint BindlessSampler_SkyViewLutTextureSampler;
  uint Padding5268;
  uint BindlessSRV_DistantSkyLightLutTexture;
  uint Padding5276;
  uint BindlessSampler_DistantSkyLightLutTextureSampler;
  uint Padding5284;
  uint BindlessSRV_CameraAerialPerspectiveVolume;
  uint Padding5292;
  uint BindlessSampler_CameraAerialPerspectiveVolumeSampler;
  uint Padding5300;
  uint BindlessSRV_CameraAerialPerspectiveVolumeMieOnly;
  uint Padding5308;
  uint BindlessSampler_CameraAerialPerspectiveVolumeMieOnlySampler;
  uint Padding5316;
  uint BindlessSRV_CameraAerialPerspectiveVolumeRayOnly;
  uint Padding5324;
  uint BindlessSampler_CameraAerialPerspectiveVolumeRayOnlySampler;
  uint Padding5332;
  uint BindlessSRV_HairScatteringLUTTexture;
  uint Padding5340;
  uint BindlessSampler_HairScatteringLUTSampler;
  uint Padding5348;
  uint BindlessSRV_GGXLTCMatTexture;
  uint Padding5356;
  uint BindlessSampler_GGXLTCMatSampler;
  uint Padding5364;
  uint BindlessSRV_GGXLTCAmpTexture;
  uint Padding5372;
  uint BindlessSampler_GGXLTCAmpSampler;
  uint Padding5380;
  uint BindlessSRV_SheenLTCTexture;
  uint Padding5388;
  uint BindlessSampler_SheenLTCSampler;
  uint Padding5396;
  uint bShadingEnergyConservation;
  uint bShadingEnergyPreservation;
  uint BindlessSRV_ShadingEnergyGGXSpecTexture;
  uint Padding5412;
  uint BindlessSRV_ShadingEnergyGGXGlassTexture;
  uint Padding5420;
  uint BindlessSRV_ShadingEnergyClothSpecTexture;
  uint Padding5428;
  uint BindlessSRV_ShadingEnergyDiffuseTexture;
  uint Padding5436;
  uint BindlessSampler_ShadingEnergySampler;
  uint Padding5444;
  uint BindlessSRV_GlintTexture;
  uint Padding5452;
  uint BindlessSampler_GlintSampler;
  uint Padding5460;
  uint BindlessSRV_SimpleVolumeTexture;
  uint Padding5468;
  uint BindlessSampler_SimpleVolumeTextureSampler;
  uint Padding5476;
  uint BindlessSRV_SimpleVolumeEnvTexture;
  uint Padding5484;
  uint BindlessSampler_SimpleVolumeEnvTextureSampler;
  uint Padding5492;
  uint BindlessSRV_SSProfilesTexture;
  uint Padding5500;
  uint BindlessSampler_SSProfilesSampler;
  uint Padding5508;
  uint BindlessSampler_SSProfilesTransmissionSampler;
  uint Padding5516;
  uint BindlessSRV_SSProfilesPreIntegratedTexture;
  uint Padding5524;
  uint BindlessSampler_SSProfilesPreIntegratedSampler;
  uint Padding5532;
  uint BindlessSRV_SpecularProfileTexture;
  uint Padding5540;
  uint BindlessSampler_SpecularProfileSampler;
  uint Padding5548;
  uint BindlessSRV_WaterIndirection;
  uint Padding5556;
  uint BindlessSRV_WaterData;
  uint Padding5564;
  float4 RectLightAtlasSizeAndInvSize;
  float RectLightAtlasMaxMipLevel;
  float Padding5588;
  uint BindlessSRV_RectLightAtlasTexture;
  uint Padding5596;
  uint BindlessSampler_RectLightAtlasSampler;
  uint Padding5604;
  uint Padding5608;
  uint Padding5612;
  float4 IESAtlasSizeAndInvSize;
  uint BindlessSRV_IESAtlasTexture;
  uint Padding5636;
  uint BindlessSampler_IESAtlasSampler;
  uint Padding5644;
  uint BindlessSampler_LandscapeWeightmapSampler;
  uint Padding5652;
  uint BindlessSRV_LandscapeIndirection;
  uint Padding5660;
  uint BindlessSRV_LandscapePerComponentData;
  uint Padding5668;
  uint BindlessUAV_VTFeedbackBuffer;
  uint Padding5676;
  uint BindlessSRV_PhysicsFieldClipmapBuffer;
  uint Padding5684;
  uint Padding5688;
  uint Padding5692;
  float3 TLASPreViewTranslationHigh;
  float Padding5708;
  float3 TLASPreViewTranslationLow;
};

struct FBlueNoiseConstants {
  int3 Dimensions;
  int Padding12;
  int3 ModuloMasks;
  int Padding28;
  uint BindlessSRV_ScalarTexture;
  uint Padding36;
  uint BindlessSRV_Vec2Texture;
};

struct FPackedVirtualVoxelNodeDesc {
  float3 TranslatedWorldMinAABB;
  uint PackedPageIndexResolution;
  float3 TranslatedWorldMaxAABB;
  uint PageIndexOffset_VoxelWorldSize;
};

struct FVirtualShadowMapConstants {
  uint NumFullShadowMaps;
  uint NumSinglePageShadowMaps;
  uint MaxPhysicalPages;
  uint NumShadowMapSlots;
  uint StaticCachedArrayIndex;
  uint PhysicalPageRowMask;
  uint PhysicalPageRowShift;
  uint PackedShadowMaskMaxLightCount;
  float4 RecPhysicalPoolSize;
  int2 PhysicalPoolSize;
  int2 PhysicalPoolSizePages;
  uint bExcludeNonNaniteFromCoarsePages;
  float CoarsePagePixelThresholdDynamic;
  float CoarsePagePixelThresholdStatic;
  float CoarsePagePixelThresholdDynamicNanite;
  uint SceneFrameNumber;
  uint bClipmapGreedyLevelSelection;
  float GlobalResolutionLodBias;
  float Padding92;
  uint BindlessSRV_ProjectionData;
  uint Padding100;
  uint BindlessSRV_PageTable;
  uint Padding108;
  uint BindlessSRV_PageFlags;
  uint Padding116;
  uint BindlessSRV_PageRectBounds;
  uint Padding124;
  uint BindlessSRV_PhysicalPagePool;
  uint Padding132;
  uint BindlessSRV_CachePrimitiveAsDynamic;
  uint Padding140;
  uint BindlessSRV_LightGridData;
  uint Padding148;
  uint BindlessSRV_NumCulledLightsGrid;
};

struct FVirtualVoxelConstants {
  int3 PageCountResolution;
  float CPUMinVoxelWorldSize;
  int3 PageTextureResolution;
  uint PageCount;
  uint PageResolution;
  uint PageResolutionLog2;
  uint PageIndexCount;
  uint IndirectDispatchGroupSize;
  uint NodeDescCount;
  uint JitterMode;
  float DensityScale;
  float DensityScale_AO;
  float DensityScale_Shadow;
  float DensityScale_Transmittance;
  float DensityScale_Environment;
  float DensityScale_Raytracing;
  float DepthBiasScale_Shadow;
  float DepthBiasScale_Transmittance;
  float DepthBiasScale_Environment;
  float SteppingScale_Shadow;
  float SteppingScale_Transmittance;
  float SteppingScale_Environment;
  float SteppingScale_Raytracing;
  float HairCoveragePixelRadiusAtDepth1;
  float Raytracing_ShadowOcclusionThreshold;
  float Raytracing_SkyOcclusionThreshold;
  float Padding120;
  float Padding124;
  float3 TranslatedWorldOffset;
  float Padding140;
  float3 TranslatedWorldOffsetStereoCorrection;
  uint AllocationFeedbackEnable;
  uint BindlessSRV_AllocatedPageCountBuffer;
  uint Padding164;
  uint BindlessSRV_PageIndexBuffer;
  uint Padding172;
  uint BindlessSRV_PageIndexCoordBuffer;
  uint Padding180;
  uint BindlessSRV_NodeDescBuffer;
  uint Padding188;
  uint BindlessSRV_CurrGPUMinVoxelSize;
  uint Padding196;
  uint BindlessSRV_NextGPUMinVoxelSize;
  float Padding204;
  uint BindlessSRV_PageTexture;
};


Texture2D<float4> SceneTexturesStruct_SceneDepthTexture : register(t0);

Texture2D<float4> SceneTexturesStruct_GBufferATexture : register(t1);

Texture2D<float4> SceneTexturesStruct_GBufferBTexture : register(t2);

Texture2D<float4> SceneTexturesStruct_GBufferDTexture : register(t3);

ByteAddressBuffer VirtualShadowMap_ProjectionData : register(t4);

StructuredBuffer<uint> VirtualShadowMap_PageTable : register(t5);

Texture2DArray<uint> VirtualShadowMap_PhysicalPagePool : register(t6);

Texture2D<float4> BlueNoise_ScalarTexture : register(t7);

Texture2D<float4> BlueNoise_Vec2Texture : register(t8);

Texture2D<float4> HairStrands_HairOnlyDepthTexture : register(t9);

Buffer<uint> VirtualVoxel_PageIndexBuffer : register(t10);

StructuredBuffer<FPackedVirtualVoxelNodeDesc> VirtualVoxel_NodeDescBuffer : register(t11);

Texture3D<uint> VirtualVoxel_PageTexture : register(t12);

RWTexture2D<float2> OutShadowFactor : register(u0);

cbuffer _RootShaderParameters : register(b0) {
  float ScreenRayLength : packoffset(c007.x);
  int SMRTRayCount : packoffset(c007.y);
  int SMRTSamplesPerRay : packoffset(c007.z);
  float SMRTRayLengthScale : packoffset(c007.w);
  float SMRTTexelDitherScale : packoffset(c008.y);
  float SMRTExtrapolateSlope : packoffset(c008.z);
  uint SMRTAdaptiveRayCount : packoffset(c009.x);
  int4 ProjectionRect : packoffset(c010.x);
  float NormalBias : packoffset(c011.x);
  float SubsurfaceMinSourceRadius : packoffset(c011.y);
  uint InputType : packoffset(c011.z);
  uint bCullBackfacingPixels : packoffset(c011.w);
  float3 Light_TranslatedWorldPosition : packoffset(c014.x);
  float Light_InvRadius : packoffset(c014.w);
  float3 Light_Color : packoffset(c015.x);
  float Light_FalloffExponent : packoffset(c015.w);
  float3 Light_Direction : packoffset(c016.x);
  float Light_SpecularScale : packoffset(c016.w);
  float3 Light_Tangent : packoffset(c017.x);
  float Light_SourceRadius : packoffset(c017.w);
  float2 Light_SpotAngles : packoffset(c018.x);
  float Light_SoftSourceRadius : packoffset(c018.z);
  float Light_SourceLength : packoffset(c018.w);
  float Light_RectLightBarnCosAngle : packoffset(c019.x);
  float Light_RectLightBarnLength : packoffset(c019.y);
  float2 Light_RectLightAtlasUVOffset : packoffset(c019.z);
  float2 Light_RectLightAtlasUVScale : packoffset(c020.x);
  float Light_RectLightAtlasMaxLevel : packoffset(c020.z);
  int LightUniformVirtualShadowMapId : packoffset(c022.x);
  int VisualizeVirtualShadowMapId : packoffset(c023.w);
};

cbuffer View : register(b1) {
  FViewConstants View : packoffset(c000.x);
};

cbuffer VirtualShadowMap : register(b2) {
  FVirtualShadowMapConstants VirtualShadowMap : packoffset(c000.x);
};

cbuffer BlueNoise : register(b3) {
  FBlueNoiseConstants BlueNoise : packoffset(c000.x);
};

cbuffer VirtualVoxel : register(b4) {
  FVirtualVoxelConstants VirtualVoxel : packoffset(c000.x);
};

SamplerState SceneTexturesStruct_PointClampSampler : register(s0);

// DXIL FirstbitHi: returns bit position counting from MSB (leading zeros count)
uint firstbithigh_msb(int value) { return (value == 0) ? 0xFFFFFFFF : (31u - firstbithigh(value)); }
uint firstbithigh_msb(uint value) { return (value == 0) ? 0xFFFFFFFF : (31u - firstbithigh(value)); }

[numthreads(8, 8, 1)]
void main(
  uint3 SV_DispatchThreadID : SV_DispatchThreadID,
  uint3 SV_GroupID : SV_GroupID,
  uint3 SV_GroupThreadID : SV_GroupThreadID,
  uint SV_GroupIndex : SV_GroupIndex
) {
  int _31;
  int _34;
  int _37;
  int _39;
  int _45;
  int _48;
  int _51;
  int _53;
  uint _62;
  uint _64;
  float _83;
  float _210;
  float _211;
  float _228;
  float _254;
  float _414;
  float _419;
  int _623;
  int _665;
  int _672;
  int _676;
  int _677;
  int _683;
  float _784;
  int _818;
  int _819;
  float _820;
  float _986;
  float _987;
  float _988;
  float _989;
  int _990;
  float _999;
  int _1022;
  int _1107;
  int _1114;
  int _1115;
  float _1116;
  float _1117;
  int _1118;
  int _1119;
  bool _1137;
  float _1138;
  float _1163;
  float _1164;
  float _1165;
  float _1166;
  float _1173;
  float _1174;
  float _1175;
  float _1176;
  float _1180;
  bool _1181;
  float _1183;
  bool _1184;
  int _1197;
  float _1198;
  int _1214;
  int _1216;
  int _1217;
  float _1218;
  int _1397;
  int _1481;
  float _1488;
  float _1489;
  int _1490;
  int _1491;
  float _1492;
  float _1493;
  int _1494;
  int _1495;
  float _1513;
  float _1514;
  int _1515;
  int _1516;
  bool _1517;
  int _1518;
  float _1519;
  float _1604;
  float _1605;
  float _1617;
  float _1801;
  float _1802;
  float _1803;
  float _1804;
  float _1805;
  float _1806;
  bool _1807;
  float _1808;
  float _1809;
  float _1810;
  float _1834;
  int _1835;
  int _1933;
  int _1934;
  int _1935;
  int _1936;
  int _1937;
  int _1938;
  int _1939;
  float _1940;
  float _1941;
  float _1942;
  int _2019;
  int _2020;
  int _2021;
  int _2022;
  int _2023;
  int _2024;
  int _2025;
  float _2058;
  float _2062;
  float _2069;
  float _2071;
  float _2079;
  float _2105;
  bool _76;
  float _97;
  float _98;
  float _134;
  float _135;
  float _136;
  float _137;
  float _145;
  float _148;
  float _156;
  float4 _158;
  uint _167;
  int _168;
  float _172;
  float _173;
  float _174;
  float _176;
  int _182;
  bool _183;
  float _203;
  bool _220;
  float _233;
  float _234;
  float _235;
  float _241;
  float _261;
  bool _268;
  float _269;
  float _270;
  float _271;
  float _275;
  float _276;
  float _277;
  float _305;
  float _309;
  float _313;
  float _317;
  float _318;
  float _319;
  float _320;
  float _336;
  float _337;
  float _338;
  float _339;
  float _345;
  float _353;
  float _354;
  float _355;
  float _356;
  float _358;
  float _359;
  float _375;
  float _388;
  float _401;
  float _450;
  float _454;
  float _458;
  float _464;
  uint _472;
  int _476;
  int _477;
  int _478;
  int _484;
  int _485;
  int _486;
  int _492;
  int _496;
  int _497;
  int _498;
  int _504;
  int _507;
  float _528;
  float _529;
  float _530;
  int _546;
  uint _549;
  uint _550;
  int _553;
  int _554;
  int _559;
  int _560;
  int _565;
  int _566;
  int _571;
  int _572;
  int _577;
  int _578;
  int _579;
  int _585;
  int _586;
  int _587;
  float _600;
  float _601;
  float _602;
  uint _613;
  uint _614;
  int _626;
  int _627;
  int _628;
  bool _631;
  uint _633;
  int _642;
  int _645;
  int _647;
  int _651;
  int _654;
  int _667;
  uint _686;
  uint _687;
  int _689;
  float _690;
  int _693;
  int _694;
  int _695;
  float _696;
  float _697;
  float _698;
  int _701;
  int _702;
  int _703;
  float _704;
  float _705;
  float _706;
  int _709;
  int _710;
  int _711;
  float _712;
  float _713;
  float _714;
  int _717;
  int _718;
  int _719;
  int _725;
  int _726;
  int _727;
  int _733;
  int _734;
  int _735;
  int _741;
  int _742;
  int _743;
  int _749;
  int _750;
  int _751;
  int _757;
  int _758;
  int _759;
  int _765;
  float _767;
  int _772;
  int _775;
  float _793;
  float _802;
  float _823;
  float _831;
  float _832;
  float _838;
  uint _856;
  float4 _859;
  float4 _867;
  float _872;
  float _873;
  float _874;
  float _875;
  float _877;
  float _884;
  float _899;
  float _900;
  float _901;
  bool _903;
  float _904;
  float _905;
  float _907;
  float _908;
  float _911;
  float _928;
  float _930;
  float _932;
  float _934;
  float _935;
  float _936;
  float _937;
  float _940;
  float _941;
  float _945;
  float _946;
  float _947;
  float _948;
  float _949;
  float _950;
  float _977;
  float _979;
  float _996;
  float _1005;
  float _1008;
  float _1009;
  uint _1012;
  uint _1013;
  int _1025;
  int _1026;
  int _1027;
  bool _1030;
  uint _1032;
  int _1041;
  int _1045;
  int _1046;
  uint _1047;
  int _1050;
  int _1054;
  int _1055;
  int _1062;
  int _1067;
  uint _1069;
  uint _1070;
  float _1079;
  int _1109;
  float _1139;
  float _1146;
  float _1150;
  float _1167;
  int _1188;
  bool _1204;
  uint _1211;
  int _1220;
  uint _1230;
  int _1234;
  int _1235;
  int _1236;
  int _1242;
  int _1243;
  int _1244;
  int _1250;
  int _1254;
  int _1255;
  int _1256;
  int _1262;
  int _1265;
  float _1286;
  float _1287;
  float _1288;
  int _1304;
  int _1307;
  uint _1308;
  int _1311;
  float _1312;
  int _1315;
  int _1316;
  int _1317;
  int _1323;
  int _1324;
  int _1325;
  int _1331;
  int _1332;
  int _1333;
  int _1339;
  int _1340;
  int _1341;
  int _1347;
  int _1348;
  int _1349;
  int _1355;
  int _1356;
  int _1357;
  float _1370;
  float _1371;
  float _1372;
  float _1376;
  float _1380;
  float _1384;
  uint _1387;
  uint _1388;
  int _1400;
  int _1401;
  int _1402;
  bool _1405;
  uint _1407;
  float _1408;
  float _1409;
  int _1415;
  int _1416;
  uint _1417;
  int _1420;
  int _1421;
  int _1428;
  int _1433;
  uint _1435;
  uint _1436;
  int _1441;
  int _1445;
  float _1453;
  float _1466;
  float _1467;
  int _1483;
  uint _1521;
  int _1524;
  int _1528;
  int _1529;
  int _1530;
  int _1536;
  int _1537;
  int _1538;
  int _1544;
  int _1545;
  int _1546;
  int _1552;
  int _1553;
  int _1554;
  float _1559;
  float _1571;
  float _1611;
  float _1614;
  float _1626;
  float _1627;
  uint _1639;
  float4 _1642;
  float4 _1650;
  float _1683;
  float _1687;
  float _1691;
  float _1700;
  float4 _1716;
  float _1721;
  float _1722;
  float _1723;
  float _1724;
  float _1726;
  float _1733;
  float _1748;
  float _1749;
  float _1750;
  bool _1752;
  float _1753;
  float _1754;
  float _1756;
  float _1757;
  float _1760;
  float _1777;
  float _1779;
  float _1781;
  float _1784;
  float _1838;
  float _1839;
  float _1840;
  int _1842;
  float _1844;
  float _1845;
  float _1846;
  int _1848;
  int _1851;
  int _1853;
  int _1855;
  uint _1856;
  uint _1857;
  uint _1858;
  float _1861;
  float _1862;
  float _1871;
  float _1872;
  float _1873;
  float _1876;
  float _1877;
  float _1878;
  float _1879;
  float _1880;
  float _1881;
  float _1885;
  float _1886;
  float _1887;
  float _1891;
  float _1892;
  float _1893;
  float _1904;
  float _1905;
  float _1912;
  float _1914;
  float _1916;
  float _1923;
  float _1925;
  float _1928;
  float _1944;
  int _1990;
  int _1991;
  int _1992;
  int _1993;
  int _1994;
  int _1995;
  int _1996;
  uint _2013;
  uint _2014;
  float _2040;
  uint _2042;
  int _2043;
  float _2059;
  float _2065;
  float _2066;
  float _2074;
  uint _2075;
  _31 = (int)(SV_GroupIndex) & 1431655765;
  _34 = (((uint)(_31) >> 1) | _31) & 858993459;
  _37 = (((uint)(_34) >> 2) | _34) & 252645135;
  _39 = ((uint)(_37) >> 4) | _37;
  _45 = ((uint)(SV_GroupIndex) >> 1) & 1431655765;
  _48 = (((uint)(_45) >> 1) | _45) & 858993459;
  _51 = (((uint)(_48) >> 2) | _48) & 252645135;
  _53 = ((uint)(_51) >> 4) | _51;
  _62 = ((uint)(ProjectionRect.x) + ((int)(SV_GroupID.x) << 3)) + ((uint)((((uint)(_39) >> 8) & 65280) | (_39 & 255)));
  _64 = ((uint)(ProjectionRect.y) + ((int)(SV_GroupID.y) << 3)) + ((uint)((((uint)(_53) >> 8) & 65280) | (_53 & 255)));
  if ((int)((uint)_62 >= (uint)ProjectionRect.z) || (int)((uint)_64 >= (uint)ProjectionRect.w)) {
  } else {
    _76 = (InputType == 1);
    if (_76) {
      if ((((float4)(HairStrands_HairOnlyDepthTexture.Load(int3(_62, _64, 0)))).x) == 0.0f) {
      } else {
        _83 = (((float4)(HairStrands_HairOnlyDepthTexture.Load(int3(_62, _64, 0)))).x);
        _97 = float((uint)_62) + 0.5f;
        _98 = float((uint)_64) + 0.5f;
        _134 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].w), mad(_83, (View.SVPositionToTranslatedWorld[2].w), mad(_98, (View.SVPositionToTranslatedWorld[1].w), (_97 * (View.SVPositionToTranslatedWorld[0].w)))));
        _135 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].x), mad(_83, (View.SVPositionToTranslatedWorld[2].x), mad(_98, (View.SVPositionToTranslatedWorld[1].x), (_97 * (View.SVPositionToTranslatedWorld[0].x))))) / _134;
        _136 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].y), mad(_83, (View.SVPositionToTranslatedWorld[2].y), mad(_98, (View.SVPositionToTranslatedWorld[1].y), (_97 * (View.SVPositionToTranslatedWorld[0].y))))) / _134;
        _137 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].z), mad(_83, (View.SVPositionToTranslatedWorld[2].z), mad(_98, (View.SVPositionToTranslatedWorld[1].z), (_97 * (View.SVPositionToTranslatedWorld[0].z))))) / _134;
        _145 = ((ScreenRayLength * (((View.InvDeviceZToWorldZTransform.x * _83) + View.InvDeviceZToWorldZTransform.y) + (1.0f / ((View.InvDeviceZToWorldZTransform.z * _83) - View.InvDeviceZToWorldZTransform.w)))) * View.ScreenRayLengthMultiplier.y) + View.ScreenRayLengthMultiplier.w;
        _148 = float((uint)(uint)(View.StateFrameIndexMod8));
        // IS-FAST replacement for IGN
        if (InjectionToggle(TOGGLE_USE_ISFAST_SHADOWS)) {
          _156 = ISFASTNoise.Load(int4(int2(uint(_62) % 128u, uint(_64) % 128u), uint(float(InjectionFrameIndex())) % 32u, 0)).x;
        } else {
          _156 = frac(frac(dot(float2(((_148 * 32.665000915527344f) + _97), ((_148 * 11.8149995803833f) + _98)), float2(0.0671105608344078f, 0.005837149918079376f))) * 52.98291778564453f);
        }
        _158 = SceneTexturesStruct_GBufferATexture.Load(int3(_62, _64, 0));
        _167 = uint(((((float4)(SceneTexturesStruct_GBufferBTexture.Load(int3(_62, _64, 0)))).w) * 255.0f) + 0.5f);
        _168 = _167 & 15;
        _172 = (_158.x * 2.0f) + -1.0f;
        _173 = (_158.y * 2.0f) + -1.0f;
        _174 = (_158.z * 2.0f) + -1.0f;
        _176 = rsqrt(dot(float3(_172, _173, _174), float3(_172, _173, _174)));
        _182 = _167 & 14;
        _183 = (_182 == 2);
        if (((int)(_183 || (int)(_168 == 6))) && ((int)(!_76))) {
          _203 = min(select(((int)(_182 == 8) || ((int)((int)((_167 & 12) == 4) || _183))), (((float4)(SceneTexturesStruct_GBufferDTexture.Load(int3(_62, _64, 0)))).w), 0.0f), 0.9900000095367432f);
          _210 = ((log2(1.0f - min(_203, 0.9900000095367432f)) * -0.03465735912322998f) * -1.4426950216293335f);
          _211 = _203;
        } else {
          _210 = -0.0f;
          _211 = 1.0f;
        }
        _220 = (_211 < 1.0f);
        if (_220) {
          _228 = max(Light_SourceRadius, ((1.0f - _211) * SubsurfaceMinSourceRadius));
        } else {
          _228 = Light_SourceRadius;
        }
        _233 = _135 - View.TranslatedWorldCameraOrigin.x;
        _234 = _136 - View.TranslatedWorldCameraOrigin.y;
        _235 = _137 - View.TranslatedWorldCameraOrigin.z;
        _241 = sqrt((_235 * _235) + ((_233 * _233) + (_234 * _234)));
        if (!(!((View.ViewToClip[3].w) >= 1.0f))) {
          _254 = (_241 * (_241 / dot(float3(_233, _234, _235), float3(View.ViewForward.x, View.ViewForward.y, View.ViewForward.z))));
        } else {
          _254 = _241;
        }
        _261 = max(0.019999999552965164f, ((_254 * NormalBias) / View.TanAndInvTanHalfFOV.z));
        if ((int)(_168 != 0) || _76) {
          _268 = (int)(_168 == 7) || _76;
          _269 = select(_268, Light_Direction.x, (_172 * _176));
          _270 = select(_268, Light_Direction.y, (_173 * _176));
          _271 = select(_268, Light_Direction.z, (_174 * _176));
          _275 = _135 + (_269 * _261);
          _276 = _136 + (_270 * _261);
          _277 = _137 + (_271 * _261);
          if ((int)(_145 > 0.0f) && ((int)(!_76))) {
            _305 = mad(_277, (View.TranslatedWorldToClip[2].x), mad(_276, (View.TranslatedWorldToClip[1].x), ((View.TranslatedWorldToClip[0].x) * _275))) + (View.TranslatedWorldToClip[3].x);
            _309 = mad(_277, (View.TranslatedWorldToClip[2].y), mad(_276, (View.TranslatedWorldToClip[1].y), ((View.TranslatedWorldToClip[0].y) * _275))) + (View.TranslatedWorldToClip[3].y);
            _313 = mad(_277, (View.TranslatedWorldToClip[2].z), mad(_276, (View.TranslatedWorldToClip[1].z), ((View.TranslatedWorldToClip[0].z) * _275))) + (View.TranslatedWorldToClip[3].z);
            _317 = mad(_277, (View.TranslatedWorldToClip[2].w), mad(_276, (View.TranslatedWorldToClip[1].w), ((View.TranslatedWorldToClip[0].w) * _275))) + (View.TranslatedWorldToClip[3].w);
            _318 = Light_Direction.x * _145;
            _319 = Light_Direction.y * _145;
            _320 = Light_Direction.z * _145;
            _336 = mad(_320, (View.TranslatedWorldToClip[2].w), mad(_319, (View.TranslatedWorldToClip[1].w), ((View.TranslatedWorldToClip[0].w) * _318))) + _317;
            _337 = _305 / _317;
            _338 = _309 / _317;
            _339 = _313 / _317;
            _345 = ((mad(_320, (View.TranslatedWorldToClip[2].z), mad(_319, (View.TranslatedWorldToClip[1].z), ((View.TranslatedWorldToClip[0].z) * _318))) + _313) / _336) - _339;
            _353 = (View.ScreenPositionScaleBias.x * _337) + View.ScreenPositionScaleBias.w;
            _354 = (View.ScreenPositionScaleBias.y * _338) + View.ScreenPositionScaleBias.z;
            _355 = View.ScreenPositionScaleBias.x * (((mad(_320, (View.TranslatedWorldToClip[2].x), mad(_319, (View.TranslatedWorldToClip[1].x), ((View.TranslatedWorldToClip[0].x) * _318))) + _305) / _336) - _337);
            _356 = View.ScreenPositionScaleBias.y * (((mad(_320, (View.TranslatedWorldToClip[2].y), mad(_319, (View.TranslatedWorldToClip[1].y), ((View.TranslatedWorldToClip[0].y) * _318))) + _309) / _336) - _338);
            _358 = (_156 + -0.5f) * 0.25f;
            _359 = _358 + 0.25f;
            if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _359) + _353), ((_356 * _359) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _359) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _359) + _353), ((_356 * _359) + _354)), 0.0f))).x))) {
              _414 = _359;
              _419 = (max(0.0f, (_414 + -0.375f)) * _145);
            } else {
              _375 = _358 + 0.5f;
              if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _375) + _353), ((_356 * _375) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _375) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _375) + _353), ((_356 * _375) + _354)), 0.0f))).x))) {
                _414 = _375;
                _419 = (max(0.0f, (_414 + -0.375f)) * _145);
              } else {
                _388 = _358 + 0.75f;
                if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _388) + _353), ((_356 * _388) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _388) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _388) + _353), ((_356 * _388) + _354)), 0.0f))).x))) {
                  _414 = _388;
                  _419 = (max(0.0f, (_414 + -0.375f)) * _145);
                } else {
                  _401 = _358 + 1.0f;
                  if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _401) + _353), ((_356 * _401) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _401) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _401) + _353), ((_356 * _401) + _354)), 0.0f))).x))) {
                    _414 = _401;
                    _419 = (max(0.0f, (_414 + -0.375f)) * _145);
                  } else {
                    _419 = _145;
                  }
                }
              }
            }
          } else {
            _419 = _145;
          }
          if ((int)SMRTRayCount > (int)0) {
            _450 = mad(_277, (View.TranslatedWorldToView[2].x), mad(_276, (View.TranslatedWorldToView[1].x), ((View.TranslatedWorldToView[0].x) * _275))) + (View.TranslatedWorldToView[3].x);
            _454 = mad(_277, (View.TranslatedWorldToView[2].y), mad(_276, (View.TranslatedWorldToView[1].y), ((View.TranslatedWorldToView[0].y) * _275))) + (View.TranslatedWorldToView[3].y);
            _458 = mad(_277, (View.TranslatedWorldToView[2].z), mad(_276, (View.TranslatedWorldToView[1].z), ((View.TranslatedWorldToView[0].z) * _275))) + (View.TranslatedWorldToView[3].z);
            _464 = sqrt(((_454 * _454) + (_450 * _450)) + (_458 * _458));
            if ((((int)((int)(_168 == 9) || ((int)(_183 || (int)((uint)(_168 + -5) < (uint)3))))) || ((int)(_76 || (int)(bCullBackfacingPixels == 0)))) | !(dot(float3(_269, _270, _271), float3(Light_Direction.x, Light_Direction.y, Light_Direction.z)) < (-0.0f - max(abs(_228), 0.10000000149011612f)))) {
              _472 = LightUniformVirtualShadowMapId * 288;
              _476 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 208u)))).x;
              _477 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 208u)))).y;
              _478 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 208u)))).z;
              _484 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 224u)))).x;
              _485 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 224u)))).y;
              _486 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 224u)))).z;
              _492 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_472 + 236u))));
              _496 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 240u)))).x;
              _497 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 240u)))).y;
              _498 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 240u)))).z;
              _504 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_472 + 264u))));
              _507 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_472 + 268u))));
              _528 = _275 + (asfloat(_496) + ((asfloat(_476) - View.PreViewTranslationHigh.x) + (asfloat(_484) - View.PreViewTranslationLow.x)));
              _529 = _276 + (asfloat(_497) + ((asfloat(_477) - View.PreViewTranslationHigh.y) + (asfloat(_485) - View.PreViewTranslationLow.y)));
              _530 = _277 + (asfloat(_498) + ((asfloat(_478) - View.PreViewTranslationHigh.z) + (asfloat(_486) - View.PreViewTranslationLow.z)));
              _546 = max((int)(0), (int)((int(floor(log2(sqrt((_530 * _530) + ((_528 * _528) + (_529 * _529)))) + select((VirtualShadowMap.bClipmapGreedyLevelSelection != 0), 0.0f, asfloat(_492)))) - _504)));
              if ((int)_546 < (int)_507) {
                _549 = _546 + (uint)(LightUniformVirtualShadowMapId);
                _550 = _549 * 288;
                _553 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 64u)))).x;
                _554 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 64u)))).y;
                _559 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 80u)))).x;
                _560 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 80u)))).y;
                _565 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 96u)))).x;
                _566 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 96u)))).y;
                _571 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 112u)))).x;
                _572 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 112u)))).y;
                _577 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 208u)))).x;
                _578 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 208u)))).y;
                _579 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 208u)))).z;
                _585 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 224u)))).x;
                _586 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 224u)))).y;
                _587 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 224u)))).z;
                _600 = _275 + ((asfloat(_577) - View.PreViewTranslationHigh.x) + (asfloat(_585) - View.PreViewTranslationLow.x));
                _601 = _276 + ((asfloat(_578) - View.PreViewTranslationHigh.y) + (asfloat(_586) - View.PreViewTranslationLow.y));
                _602 = _277 + ((asfloat(_579) - View.PreViewTranslationHigh.z) + (asfloat(_587) - View.PreViewTranslationLow.z));
                _613 = uint(mad(1.0f, asfloat(_571), mad(_602, asfloat(_565), mad(_601, asfloat(_559), (asfloat(_553) * _600)))) * 128.0f);
                _614 = uint(mad(1.0f, asfloat(_572), mad(_602, asfloat(_566), mad(_601, asfloat(_560), (asfloat(_554) * _600)))) * 128.0f);
                if (!((uint)_549 < (uint)8192)) {
                  _623 = ((int)((((_549 * 21845) + (uint)(-178946048)) + _613) + (_614 << 7)));
                } else {
                  _623 = _549;
                }
                _626 = VirtualShadowMap_PageTable[_623];
                _627 = (uint)(_626) >> 20;
                _628 = _627 & 63;
                if ((int)_626 < (int)0) {
                  _631 = (_628 == 0);
                  _633 = _628 + _549;
                  if (!_631) {
                    if (!((uint)_633 < (uint)8192)) {
                      _642 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_550 + 256u)))).y;
                      _645 = asint(VirtualShadowMap_ProjectionData.Load2(((int)((_633 * 288) + 256u)))).y;
                      _647 = _627 & 31;
                      _651 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_550 + 256u)))).x;
                      _654 = asint(VirtualShadowMap_ProjectionData.Load2(((int)((_633 * 288) + 256u)))).x;
                      _665 = ((int)((((_633 * 21845) + (uint)(-178946048)) + ((uint)((uint)((_613 - (_651 << 5)) + (((int)(_654 << 5)) << _647)) >> _647))) + (((uint)((_614 - (_642 << 5)) + (((int)(_645 << 5)) << _647)) >> _647) << 7)));
                    } else {
                      _665 = _633;
                    }
                    _667 = VirtualShadowMap_PageTable[_665];
                    _672 = ((int)(uint)((int)((_667 & -2081423360) == -2147483648)));
                  } else {
                    _672 = ((int)(uint)(_631));
                  }
                  _676 = _672;
                  _677 = select((_672 != 0), _633, -1);
                } else {
                  _676 = 0;
                  _677 = -1;
                }
                _683 = select(((int)(_676 != 0) && (int)((int)_677 > (int)_549)), _677, _549);
              } else {
                _683 = -1;
              }
              if (!((int)_683 < (int)0)) {
                _686 = _683 * 288;
                _687 = _686 + 32u;
                _689 = asint(VirtualShadowMap_ProjectionData.Load4(_687)).z;
                _690 = asfloat(_689);
                _693 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 64u)))).x;
                _694 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 64u)))).y;
                _695 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 64u)))).z;
                _696 = asfloat(_693);
                _697 = asfloat(_694);
                _698 = asfloat(_695);
                _701 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 80u)))).x;
                _702 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 80u)))).y;
                _703 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 80u)))).z;
                _704 = asfloat(_701);
                _705 = asfloat(_702);
                _706 = asfloat(_703);
                _709 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 96u)))).x;
                _710 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 96u)))).y;
                _711 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 96u)))).z;
                _712 = asfloat(_709);
                _713 = asfloat(_710);
                _714 = asfloat(_711);
                _717 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 112u)))).x;
                _718 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 112u)))).y;
                _719 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 112u)))).z;
                _725 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 128u)))).x;
                _726 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 128u)))).y;
                _727 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 128u)))).z;
                _733 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 144u)))).x;
                _734 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 144u)))).y;
                _735 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 144u)))).z;
                _741 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 160u)))).x;
                _742 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 160u)))).y;
                _743 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 160u)))).z;
                _749 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 208u)))).x;
                _750 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 208u)))).y;
                _751 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 208u)))).z;
                _757 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 224u)))).x;
                _758 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 224u)))).y;
                _759 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 224u)))).z;
                _765 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_686 + 280u))));
                _767 = asfloat(_765) * SMRTTexelDitherScale;
                if (_767 > 0.0f) {
                  _772 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_686 + 264u))));
                  _775 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_686 + 236u))));
                  _784 = (((_464 * 3.0517578125e-05f) * _767) / exp2(float((int)(_772)) - asfloat(_775)));
                } else {
                  _784 = 0.0f;
                }
                _793 = mad(_271, asfloat(_743), mad(_270, asfloat(_735), (asfloat(_727) * _269)));
                _802 = _464 * SMRTRayLengthScale;
                if (!(SMRTRayCount == 0)) {
                  _818 = 0;
                  _819 = 0;
                  _820 = 0.0f;
                  while(true) {
                    _823 = float((uint)_819);
                    _831 = float((int)(BlueNoise.Dimensions.x));
                    _832 = float((int)(BlueNoise.Dimensions.y));
                    _838 = float((uint)(_819 + (uint)(SMRTRayCount)));
                    _856 = (BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y;
                    // IS-FAST replacement for Blue Noise Vec2 (SMRT ray dithering)
                    if (InjectionToggle(TOGGLE_USE_ISFAST_SHADOWS)) {
                      float2 _isfast_ray0 = ISFASTNoise.Load(int4(int2((uint(_62) + uint(_823)) % 128u, (uint(_64) + uint(_823) * 7u) % 128u), (uint(float(InjectionFrameIndex())) + uint(_823)) % 32u, 0));
                      float2 _isfast_ray1 = ISFASTNoise.Load(int4(int2((uint(_62) + uint(_838)) % 128u, (uint(_64) + uint(_838) * 7u) % 128u), (uint(float(InjectionFrameIndex())) + uint(_838)) % 32u, 0));
                      _859 = float4(_isfast_ray0.x, _isfast_ray0.y, 0, 0);
                      _867 = float4(_isfast_ray1.x, _isfast_ray1.y, 0, 0);
                    } else {
                      _859 = BlueNoise_Vec2Texture.Load(int3((((int)((uint)(int(_831 * frac(_823 * 0.7548776268959045f))) + _62)) & BlueNoise.ModuloMasks.x), ((int)(_856 + ((uint)(((int)((uint)(int(_832 * frac(_823 * 0.5698402523994446f))) + _64)) & BlueNoise.ModuloMasks.y)))), 0));
                      _867 = BlueNoise_Vec2Texture.Load(int3((BlueNoise.ModuloMasks.x & ((int)((uint)(int(_831 * frac(_838 * 0.7548776268959045f))) + _62))), ((int)(_856 + ((uint)(BlueNoise.ModuloMasks.y & ((int)((uint)(int(_832 * frac(_838 * 0.5698402523994446f))) + _64)))))), 0));
                    }
                    _872 = (_859.x * 2.0f) + -0.9999999403953552f;
                    _873 = (_859.y * 2.0f) + -0.9999999403953552f;
                    _874 = abs(_872);
                    _875 = abs(_873);
                    _877 = max(_874, _875);
                    _884 = ((min(_874, _875) / (_877 + 5.421010862427522e-20f)) + (float((bool)(uint)(_875 >= _874)) * 2.0f)) * 0.7853981852531433f;
                    _899 = _877 * _228;
                    _900 = _899 * asfloat(((asint(cos(_884)) & 2147483647) | (asint(_872) & -2147483648)));
                    _901 = _899 * asfloat(((asint(sin(_884)) & 2147483647) | (asint(_873) & -2147483648)));
                    _903 = (abs(Light_Direction.x) > 9.999999974752427e-07f);
                    _904 = select(_903, 1.0f, 0.0f);
                    _905 = select(_903, 0.0f, 1.0f);
                    _907 = -0.0f - (Light_Direction.z * _905);
                    _908 = _904 * Light_Direction.z;
                    _911 = (_905 * Light_Direction.x) - (_904 * Light_Direction.y);
                    _928 = ((_900 * _907) + Light_Direction.x) + (((_908 * Light_Direction.z) - (_911 * Light_Direction.y)) * _901);
                    _930 = ((_900 * _908) + Light_Direction.y) + (((_911 * Light_Direction.x) - (Light_Direction.z * _907)) * _901);
                    _932 = ((_911 * _900) + Light_Direction.z) + (((Light_Direction.y * _907) - (_908 * Light_Direction.x)) * _901);
                    _934 = rsqrt(dot(float3(_928, _930, _932), float3(_928, _930, _932)));
                    _935 = _928 * _934;
                    _936 = _930 * _934;
                    _937 = _932 * _934;
                    _940 = (_867.x + -0.5f) * _784;
                    _941 = (_867.y + -0.5f) * _784;
                    _945 = (((asfloat(_749) - View.PreViewTranslationHigh.x) + (asfloat(_757) - View.PreViewTranslationLow.x)) + _275) + (_935 * _419);
                    _946 = (((asfloat(_750) - View.PreViewTranslationHigh.y) + (asfloat(_758) - View.PreViewTranslationLow.y)) + _276) + (_936 * _419);
                    _947 = (((asfloat(_751) - View.PreViewTranslationHigh.z) + (asfloat(_759) - View.PreViewTranslationLow.z)) + _277) + (_937 * _419);
                    _948 = _935 * _802;
                    _949 = _936 * _802;
                    _950 = _937 * _802;
                    _977 = (mad(_947, _714, mad(_946, _706, (_945 * _698))) + asfloat(_719)) + max(0.0f, ((max(0.0f, dot(float2(min(max(((-0.0f - mad(_271, asfloat(_741), mad(_270, asfloat(_733), (asfloat(_725) * _269)))) / _793), -0.05000000074505806f), 0.05000000074505806f), min(max(((-0.0f - mad(_271, asfloat(_742), mad(_270, asfloat(_734), (asfloat(_726) * _269)))) / _793), -0.05000000074505806f), 0.05000000074505806f)), float2(_940, _941))) * 2.0f) - abs(_690 * _419)));
                    _979 = abs(_690 * SMRTExtrapolateSlope);
                    if ((int)SMRTSamplesPerRay > (int)-1) {
                      _986 = -10000.0f;
                      _987 = -1.0f;
                      _988 = 0.0f;
                      _989 = -1.0f;
                      _990 = 0;
                      while(true) {
                        if (!(_990 == SMRTSamplesPerRay)) {
                          _996 = ((float((int)(_990)) + (1.0f - _156)) * (-1.0f / float((int)(SMRTSamplesPerRay)))) + 1.0f;
                          _999 = (_996 * _996);
                        } else {
                          _999 = 0.0f;
                        }
                        _1005 = ((_940 + asfloat(_717)) + mad(_947, _712, mad(_946, _704, (_945 * _696)))) + (_999 * mad(_950, _712, mad(_949, _704, (_948 * _696))));
                        _1008 = ((_941 + asfloat(_718)) + mad(_947, _713, mad(_946, _705, (_945 * _697)))) + (_999 * mad(_950, _713, mad(_949, _705, (_948 * _697))));
                        _1009 = (_999 * mad(_950, _714, mad(_949, _706, (_948 * _698)))) + _977;
                        _1012 = uint(_1005 * 128.0f);
                        _1013 = uint(_1008 * 128.0f);
                        if (!((uint)_683 < (uint)8192)) {
                          _1022 = ((int)((((_683 * 21845) + (uint)(-178946048)) + _1012) + (_1013 << 7)));
                        } else {
                          _1022 = _683;
                        }
                        _1025 = VirtualShadowMap_PageTable[_1022];
                        _1026 = (uint)(_1025) >> 20;
                        _1027 = _1026 & 63;
                        if ((int)_1025 < (int)0) {
                          _1030 = (_1027 == 0);
                          _1032 = _1027 + _683;
                          if (!_1030) {
                            _1041 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 48u)))).z;
                            _1045 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_686 + 256u)))).x;
                            _1046 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_686 + 256u)))).y;
                            _1047 = _1032 * 288;
                            _1050 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1047 + 48u)))).z;
                            _1054 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1047 + 256u)))).x;
                            _1055 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1047 + 256u)))).y;
                            _1062 = _1026 & 31;
                            _1067 = (uint)((_1012 - (_1045 << 5)) + (((int)(_1054 << 5)) << _1062)) >> _1062;
                            _1069 = _1067 << 7;
                            _1070 = ((uint)((_1013 - (_1046 << 5)) + (((int)(_1055 << 5)) << _1062)) >> _1062) << 7;
                            _1079 = 1.0f / float((uint)(1 << _1062));
                            if (!((uint)_1032 < (uint)8192)) {
                              _1107 = ((int)((((_1032 * 21845) + (uint)(-178946048)) + _1067) + _1070));
                            } else {
                              _1107 = _1032;
                            }
                            _1109 = VirtualShadowMap_PageTable[_1107];
                            _1114 = ((int)min((uint)(((int)max((uint)((int)(uint((((float((int)(_1054)) - (_1079 * float((int)(_1045)))) * 0.25f) + (_1079 * _1005)) * 16384.0f))), (uint)(_1069)))), (uint)((_1069 | 127))));
                            _1115 = ((int)min((uint)(((int)max((uint)((int)(uint((((float((int)(_1055)) - (_1079 * float((int)(_1046)))) * 0.25f) + (_1079 * _1008)) * 16384.0f))), (uint)(_1070)))), (uint)((_1070 | 127))));
                            _1116 = _1079;
                            _1117 = (asfloat(_1050) - (_1079 * asfloat(_1041)));
                            _1118 = ((int)(uint)((int)((_1109 & -2081423360) == -2147483648)));
                            _1119 = _1109;
                          } else {
                            _1114 = (int)(uint(_1005 * 16384.0f));
                            _1115 = (int)(uint(_1008 * 16384.0f));
                            _1116 = 1.0f;
                            _1117 = 0.0f;
                            _1118 = ((int)(uint)(_1030));
                            _1119 = _1025;
                          }
                          if (!(_1118 == 0)) {
                            _1137 = true;
                            _1138 = ((asfloat((((uint)(VirtualShadowMap_PhysicalPagePool.Load(int4(((((int)(_1119 << 7)) & 130944) | (_1114 & 127)), ((((uint)(_1119) >> 3) & 130944) | (_1115 & 127)), 0, 0)))).x)) - _1117) / _1116);
                          } else {
                            _1137 = false;
                            _1138 = 0.0f;
                          }
                        } else {
                          _1137 = false;
                          _1138 = 0.0f;
                        }
                        _1139 = select(_1137, _1138, 0.0f);
                        if (_1137) {
                          if (_986 == -10000.0f) {
                            if (!(_1139 > _1009)) {
                              _1173 = _1138;
                              _1174 = _999;
                              _1175 = _988;
                              _1176 = _1009;
                              if ((int)_990 < (int)SMRTSamplesPerRay) {
                                _986 = _1173;
                                _987 = _1174;
                                _988 = _1175;
                                _989 = _1176;
                                _990 = (_990 + 1);
                                continue;
                              } else {
                                _1180 = -1.0f;
                                _1181 = false;
                              }
                            } else {
                              _1180 = _1138;
                              _1181 = true;
                            }
                          } else {
                            _1146 = abs(_1009 - _989);
                            _1150 = _999 - _987;
                            if ((_1139 - _1009) > (_1146 * 1.0499999523162842f)) {
                              _1163 = _986;
                              _1164 = _987;
                              _1165 = _988;
                              _1166 = ((_1150 * _988) + _986);
                            } else {
                              if (_1139 != _986) {
                                _1163 = _1138;
                                _1164 = _999;
                                _1165 = min(max(((_1139 - _986) / _1150), (-0.0f - _979)), _979);
                                _1166 = _1138;
                              } else {
                                _1163 = _986;
                                _1164 = _987;
                                _1165 = _988;
                                _1166 = _1138;
                              }
                            }
                            _1167 = _1146 * 0.5249999761581421f;
                            if (!(abs((_1167 + _1009) - _1166) < _1167)) {
                              _1173 = _1163;
                              _1174 = _1164;
                              _1175 = _1165;
                              _1176 = _1009;
                              if ((int)_990 < (int)SMRTSamplesPerRay) {
                                _986 = _1173;
                                _987 = _1174;
                                _988 = _1175;
                                _989 = _1176;
                                _990 = (_990 + 1);
                                continue;
                              } else {
                                _1180 = -1.0f;
                                _1181 = false;
                              }
                            } else {
                              _1180 = _1166;
                              _1181 = true;
                            }
                          }
                        } else {
                          _1173 = _986;
                          _1174 = _987;
                          _1175 = _988;
                          _1176 = _989;
                          if ((int)_990 < (int)SMRTSamplesPerRay) {
                            _986 = _1173;
                            _987 = _1174;
                            _988 = _1175;
                            _989 = _1176;
                            _990 = (_990 + 1);
                            continue;
                          } else {
                            _1180 = -1.0f;
                            _1181 = false;
                          }
                        }
                        _1183 = _1180;
                        _1184 = _1181;
                        break;
                      }
                    } else {
                      _1183 = -1.0f;
                      _1184 = false;
                    }
                    if (_1184) {
                      _1188 = asint(VirtualShadowMap_ProjectionData.Load4(_687)).z;
                      _1197 = _818;
                      _1198 = (max(9.999999974752427e-07f, ((_977 - _1183) / asfloat(_1188))) + _820);
                    } else {
                      _1197 = ((int)(_818 + 1u));
                      _1198 = _820;
                    }
                    if (!(SMRTAdaptiveRayCount == 0)) {
                      if (_819 == 0) {
                        _1204 = WaveActiveAllTrue(!_1184);
                        if (!_1204) {
                          _1211 = _819 + 1u;
                          if ((uint)_1211 < (uint)SMRTRayCount) {
                            _818 = _1197;
                            _819 = _1211;
                            _820 = _1198;
                            continue;
                          } else {
                            _1214 = _1211;
                          }
                        } else {
                          _1214 = 0;
                        }
                      } else {
                        if (((uint)_819 < (uint)SMRTAdaptiveRayCount) | !(WaveActiveAllTrue(_1197 == 0))) {
                          _1211 = _819 + 1u;
                          if ((uint)_1211 < (uint)SMRTRayCount) {
                            _818 = _1197;
                            _819 = _1211;
                            _820 = _1198;
                            continue;
                          } else {
                            _1214 = _1211;
                          }
                        } else {
                          _1214 = _819;
                        }
                      }
                    } else {
                      _1211 = _819 + 1u;
                      if ((uint)_1211 < (uint)SMRTRayCount) {
                        _818 = _1197;
                        _819 = _1211;
                        _820 = _1198;
                        continue;
                      } else {
                        _1214 = _1211;
                      }
                    }
                    _1216 = _1197;
                    _1217 = _1214;
                    _1218 = _1198;
                    break;
                  }
                } else {
                  _1216 = 0;
                  _1217 = 0;
                  _1218 = 0.0f;
                }
                _1220 = (int)min((uint)(((int)(_1217 + 1u))), (uint)(SMRTRayCount));
                _1604 = (float((uint)_1216) / float((uint)_1220));
                _1605 = (_1218 / float((uint)((uint)((int)max((uint)(1), (uint)(((int)(_1220 - _1216))))))));
              } else {
                _1604 = 1.0f;
                _1605 = -1.0f;
              }
            } else {
              _1604 = 0.0f;
              _1605 = -1.0f;
            }
          } else {
            _1230 = LightUniformVirtualShadowMapId * 288;
            _1234 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 208u)))).x;
            _1235 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 208u)))).y;
            _1236 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 208u)))).z;
            _1242 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 224u)))).x;
            _1243 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 224u)))).y;
            _1244 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 224u)))).z;
            _1250 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_1230 + 236u))));
            _1254 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 240u)))).x;
            _1255 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 240u)))).y;
            _1256 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 240u)))).z;
            _1262 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_1230 + 264u))));
            _1265 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_1230 + 268u))));
            _1286 = _275 + (asfloat(_1254) + ((asfloat(_1234) - View.PreViewTranslationHigh.x) + (asfloat(_1242) - View.PreViewTranslationLow.x)));
            _1287 = _276 + (asfloat(_1255) + ((asfloat(_1235) - View.PreViewTranslationHigh.y) + (asfloat(_1243) - View.PreViewTranslationLow.y)));
            _1288 = _277 + (asfloat(_1256) + ((asfloat(_1236) - View.PreViewTranslationHigh.z) + (asfloat(_1244) - View.PreViewTranslationLow.z)));
            _1304 = max((int)(0), (int)((int(floor(log2(sqrt((_1288 * _1288) + ((_1286 * _1286) + (_1287 * _1287)))) + select((VirtualShadowMap.bClipmapGreedyLevelSelection != 0), 0.0f, asfloat(_1250)))) - _1262)));
            if ((int)_1304 < (int)_1265) {
              _1307 = _1304 + LightUniformVirtualShadowMapId;
              _1308 = _1307 * 288;
              _1311 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 32u)))).z;
              _1312 = asfloat(_1311);
              _1315 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 64u)))).x;
              _1316 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 64u)))).y;
              _1317 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 64u)))).z;
              _1323 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 80u)))).x;
              _1324 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 80u)))).y;
              _1325 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 80u)))).z;
              _1331 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 96u)))).x;
              _1332 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 96u)))).y;
              _1333 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 96u)))).z;
              _1339 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 112u)))).x;
              _1340 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 112u)))).y;
              _1341 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 112u)))).z;
              _1347 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 208u)))).x;
              _1348 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 208u)))).y;
              _1349 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 208u)))).z;
              _1355 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 224u)))).x;
              _1356 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 224u)))).y;
              _1357 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 224u)))).z;
              _1370 = ((asfloat(_1347) - View.PreViewTranslationHigh.x) + (asfloat(_1355) - View.PreViewTranslationLow.x)) + _275;
              _1371 = ((asfloat(_1348) - View.PreViewTranslationHigh.y) + (asfloat(_1356) - View.PreViewTranslationLow.y)) + _276;
              _1372 = ((asfloat(_1349) - View.PreViewTranslationHigh.z) + (asfloat(_1357) - View.PreViewTranslationLow.z)) + _277;
              _1376 = mad(_1372, asfloat(_1331), mad(_1371, asfloat(_1323), (_1370 * asfloat(_1315)))) + asfloat(_1339);
              _1380 = mad(_1372, asfloat(_1332), mad(_1371, asfloat(_1324), (_1370 * asfloat(_1316)))) + asfloat(_1340);
              _1384 = mad(_1372, asfloat(_1333), mad(_1371, asfloat(_1325), (_1370 * asfloat(_1317)))) + asfloat(_1341);
              _1387 = uint(_1376 * 128.0f);
              _1388 = uint(_1380 * 128.0f);
              if (!((uint)_1307 < (uint)8192)) {
                _1397 = ((int)((((_1307 * 21845) + (uint)(-178946048)) + _1387) + (_1388 << 7)));
              } else {
                _1397 = _1307;
              }
              _1400 = VirtualShadowMap_PageTable[_1397];
              _1401 = (uint)(_1400) >> 20;
              _1402 = _1401 & 63;
              if ((int)_1400 < (int)0) {
                _1405 = (_1402 == 0);
                _1407 = _1402 + _1307;
                _1408 = _1376 * 16384.0f;
                _1409 = _1380 * 16384.0f;
                if (!_1405) {
                  _1415 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1308 + 256u)))).x;
                  _1416 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1308 + 256u)))).y;
                  _1417 = _1407 * 288;
                  _1420 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1417 + 256u)))).x;
                  _1421 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1417 + 256u)))).y;
                  _1428 = _1401 & 31;
                  _1433 = (uint)((_1387 - (_1415 << 5)) + (((int)(_1420 << 5)) << _1428)) >> _1428;
                  _1435 = _1433 << 7;
                  _1436 = ((uint)((_1388 - (_1416 << 5)) + (((int)(_1421 << 5)) << _1428)) >> _1428) << 7;
                  _1441 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 48u)))).z;
                  _1445 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1417 + 48u)))).z;
                  _1453 = 1.0f / float((uint)(1 << _1428));
                  _1466 = (((float((int)(_1420)) - (_1453 * float((int)(_1415)))) * 0.25f) + (_1453 * _1376)) * 16384.0f;
                  _1467 = (((float((int)(_1421)) - (_1453 * float((int)(_1416)))) * 0.25f) + (_1453 * _1380)) * 16384.0f;
                  if (!((uint)_1407 < (uint)8192)) {
                    _1481 = ((int)((((_1407 * 21845) + (uint)(-178946048)) + _1433) + _1436));
                  } else {
                    _1481 = _1407;
                  }
                  _1483 = VirtualShadowMap_PageTable[_1481];
                  _1488 = _1466;
                  _1489 = _1467;
                  _1490 = ((int)min((uint)(((int)max((uint)((int)(uint(_1466))), (uint)(_1435)))), (uint)((_1435 | 127))));
                  _1491 = ((int)min((uint)(((int)max((uint)((int)(uint(_1467))), (uint)(_1436)))), (uint)((_1436 | 127))));
                  _1492 = _1453;
                  _1493 = (asfloat(_1445) - (_1453 * asfloat(_1441)));
                  _1494 = ((int)(uint)((int)((_1483 & -2081423360) == -2147483648)));
                  _1495 = _1483;
                } else {
                  _1488 = _1408;
                  _1489 = _1409;
                  _1490 = (int)(uint(_1408));
                  _1491 = (int)(uint(_1409));
                  _1492 = 1.0f;
                  _1493 = 0.0f;
                  _1494 = ((int)(uint)(_1405));
                  _1495 = _1400;
                }
                if (!(_1494 == 0)) {
                  _1513 = _1488;
                  _1514 = _1489;
                  _1515 = _1490;
                  _1516 = _1491;
                  _1517 = true;
                  _1518 = _1407;
                  _1519 = ((asfloat((((uint)(VirtualShadowMap_PhysicalPagePool.Load(int4(((((int)(_1495 << 7)) & 130944) | (_1490 & 127)), ((((uint)(_1495) >> 3) & 130944) | (_1491 & 127)), 0, 0)))).x)) - _1493) / _1492);
                } else {
                  _1513 = _1488;
                  _1514 = _1489;
                  _1515 = _1490;
                  _1516 = _1491;
                  _1517 = false;
                  _1518 = -1;
                  _1519 = 0.0f;
                }
              } else {
                _1513 = 0.0f;
                _1514 = 0.0f;
                _1515 = 0;
                _1516 = 0;
                _1517 = false;
                _1518 = -1;
                _1519 = 0.0f;
              }
              if (_1517) {
                _1521 = _1518 * 288;
                _1524 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 32u)))).z;
                _1528 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 128u)))).x;
                _1529 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 128u)))).y;
                _1530 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 128u)))).z;
                _1536 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 144u)))).x;
                _1537 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 144u)))).y;
                _1538 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 144u)))).z;
                _1544 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 160u)))).x;
                _1545 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 160u)))).y;
                _1546 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 160u)))).z;
                _1552 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 176u)))).x;
                _1553 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 176u)))).y;
                _1554 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 176u)))).z;
                _1559 = -0.0f - dot(float3(_269, _270, _271), float3(_1370, _1371, _1372));
                _1571 = mad(_1559, asfloat(_1554), mad(_271, asfloat(_1546), mad(_270, asfloat(_1538), (asfloat(_1530) * _269))));
                if (((_1519 + (_1312 * max(_419, 0.0f))) - (min((max(0.0f, dot(float2(((-0.0f - mad(_1559, asfloat(_1552), mad(_271, asfloat(_1544), mad(_270, asfloat(_1536), (asfloat(_1528) * _269))))) / _1571), ((-0.0f - mad(_1559, asfloat(_1553), mad(_271, asfloat(_1545), mad(_270, asfloat(_1537), (asfloat(_1529) * _269))))) / _1571)), float2((((0.5f - _1513) + float((uint)_1515)) * 6.103515625e-05f), (((0.5f - _1514) + float((uint)_1516)) * 6.103515625e-05f)))) * 2.0f), abs(asfloat(_1524) * 100.0f)) * float((uint)(1 << ((_1518 - _1307) & 31))))) > _1384) {
                  _1604 = 0.0f;
                  _1605 = max(9.999999974752427e-07f, ((_1384 - _1519) / _1312));
                } else {
                  _1604 = 1.0f;
                  _1605 = -1.0f;
                }
              } else {
                _1604 = 1.0f;
                _1605 = -1.0f;
              }
            } else {
              _1604 = 1.0f;
              _1605 = -1.0f;
            }
          }
          if (_220 && (int)(_1604 < 1.0f)) {
            _1611 = saturate(exp2(_210 * _1605));
            _1614 = ((1.0f - _1611) * _1604) + _1611;
            _1617 = (_1614 * _1614);
          } else {
            _1617 = _1604;
          }
          _2079 = _1617;
        } else {
          _2079 = 1.0f;
        }
        if ((int)(_2079 > 0.01666666753590107f) && (int)(_2079 < 1.0f)) {
          if (InjectionToggle(TOGGLE_USE_ISFAST_SHADOWS)) {
            float _isfast_scalar = ISFASTNoise.Load(int4(int2(uint(_62) % 128u, uint(_64) % 128u), uint(float(InjectionFrameIndex())) % 32u, 0)).x;
            _2105 = saturate(((_isfast_scalar + -0.5f) * 0.06666667014360428f) + _2079);
          } else {
            _2105 = saturate((((((float4)(BlueNoise_ScalarTexture.Load(int3((BlueNoise.ModuloMasks.x & _62), ((int)(((BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y) + ((uint)(BlueNoise.ModuloMasks.y & _64)))), 0)))).x) + -0.5f) * 0.06666667014360428f) + _2079);
          }
        } else {
          _2105 = _2079;
        }
        // Debug: amplified difference between IS-FAST and Blue Noise
        // Output = abs(difference) * 50 — any difference shows as bright white shadow removal
        if ((InjectionEnum(ENUM_DEBUG_SHADOWS_SHIFT) > 0u)) {
          float _dbg_isfast_scalar = ISFASTNoise.Load(int4(int2(uint(_62) % 128u, uint(_64) % 128u), uint(float(InjectionFrameIndex())) % 32u, 0)).x;
          float _dbg_isfast_result = saturate(((_dbg_isfast_scalar + -0.5f) * 0.06666667014360428f) + _2079);
          float _dbg_bn_result = saturate((((((float4)(BlueNoise_ScalarTexture.Load(int3((BlueNoise.ModuloMasks.x & _62), ((int)(((BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y) + ((uint)(BlueNoise.ModuloMasks.y & _64)))), 0)))).x) + -0.5f) * 0.06666667014360428f) + _2079);
          float _dbg_diff = saturate(abs(_dbg_isfast_result - _dbg_bn_result) * 50.0f);
          // Bright = large difference, dark = no difference
          // Invert so difference shows as shadow (more visible on lit surfaces)
          OutShadowFactor[int2(_62, _64)] = float2(1.0f - _dbg_diff, 1.0f - _dbg_diff);
        } else {
          OutShadowFactor[int2(_62, _64)] = float2(_2105, _2105);
        }
      }
    } else {
      _83 = (((float4)(SceneTexturesStruct_SceneDepthTexture.Load(int3(_62, _64, 0)))).x);
      _97 = float((uint)_62) + 0.5f;
      _98 = float((uint)_64) + 0.5f;
      _134 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].w), mad(_83, (View.SVPositionToTranslatedWorld[2].w), mad(_98, (View.SVPositionToTranslatedWorld[1].w), (_97 * (View.SVPositionToTranslatedWorld[0].w)))));
      _135 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].x), mad(_83, (View.SVPositionToTranslatedWorld[2].x), mad(_98, (View.SVPositionToTranslatedWorld[1].x), (_97 * (View.SVPositionToTranslatedWorld[0].x))))) / _134;
      _136 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].y), mad(_83, (View.SVPositionToTranslatedWorld[2].y), mad(_98, (View.SVPositionToTranslatedWorld[1].y), (_97 * (View.SVPositionToTranslatedWorld[0].y))))) / _134;
      _137 = mad(1.0f, (View.SVPositionToTranslatedWorld[3].z), mad(_83, (View.SVPositionToTranslatedWorld[2].z), mad(_98, (View.SVPositionToTranslatedWorld[1].z), (_97 * (View.SVPositionToTranslatedWorld[0].z))))) / _134;
      _145 = ((ScreenRayLength * (((View.InvDeviceZToWorldZTransform.x * _83) + View.InvDeviceZToWorldZTransform.y) + (1.0f / ((View.InvDeviceZToWorldZTransform.z * _83) - View.InvDeviceZToWorldZTransform.w)))) * View.ScreenRayLengthMultiplier.y) + View.ScreenRayLengthMultiplier.w;
      _148 = float((uint)(uint)(View.StateFrameIndexMod8));
      // IS-FAST replacement for IGN (second path)
      if (InjectionToggle(TOGGLE_USE_ISFAST_SHADOWS)) {
        _156 = ISFASTNoise.Load(int4(int2(uint(_62) % 128u, uint(_64) % 128u), uint(float(InjectionFrameIndex())) % 32u, 0)).x;
      } else {
        _156 = frac(frac(dot(float2(((_148 * 32.665000915527344f) + _97), ((_148 * 11.8149995803833f) + _98)), float2(0.0671105608344078f, 0.005837149918079376f))) * 52.98291778564453f);
      }
      _158 = SceneTexturesStruct_GBufferATexture.Load(int3(_62, _64, 0));
      _167 = uint(((((float4)(SceneTexturesStruct_GBufferBTexture.Load(int3(_62, _64, 0)))).w) * 255.0f) + 0.5f);
      _168 = _167 & 15;
      _172 = (_158.x * 2.0f) + -1.0f;
      _173 = (_158.y * 2.0f) + -1.0f;
      _174 = (_158.z * 2.0f) + -1.0f;
      _176 = rsqrt(dot(float3(_172, _173, _174), float3(_172, _173, _174)));
      _182 = _167 & 14;
      _183 = (_182 == 2);
      if (((int)(_183 || (int)(_168 == 6))) && ((int)(!_76))) {
        _203 = min(select(((int)(_182 == 8) || ((int)((int)((_167 & 12) == 4) || _183))), (((float4)(SceneTexturesStruct_GBufferDTexture.Load(int3(_62, _64, 0)))).w), 0.0f), 0.9900000095367432f);
        _210 = ((log2(1.0f - min(_203, 0.9900000095367432f)) * -0.03465735912322998f) * -1.4426950216293335f);
        _211 = _203;
      } else {
        _210 = -0.0f;
        _211 = 1.0f;
      }
      _220 = (_211 < 1.0f);
      if (_220) {
        _228 = max(Light_SourceRadius, ((1.0f - _211) * SubsurfaceMinSourceRadius));
      } else {
        _228 = Light_SourceRadius;
      }
      _233 = _135 - View.TranslatedWorldCameraOrigin.x;
      _234 = _136 - View.TranslatedWorldCameraOrigin.y;
      _235 = _137 - View.TranslatedWorldCameraOrigin.z;
      _241 = sqrt((_235 * _235) + ((_233 * _233) + (_234 * _234)));
      if (!(!((View.ViewToClip[3].w) >= 1.0f))) {
        _254 = (_241 * (_241 / dot(float3(_233, _234, _235), float3(View.ViewForward.x, View.ViewForward.y, View.ViewForward.z))));
      } else {
        _254 = _241;
      }
      _261 = max(0.019999999552965164f, ((_254 * NormalBias) / View.TanAndInvTanHalfFOV.z));
      if ((int)(_168 != 0) || _76) {
        _268 = (int)(_168 == 7) || _76;
        _269 = select(_268, Light_Direction.x, (_172 * _176));
        _270 = select(_268, Light_Direction.y, (_173 * _176));
        _271 = select(_268, Light_Direction.z, (_174 * _176));
        _275 = _135 + (_269 * _261);
        _276 = _136 + (_270 * _261);
        _277 = _137 + (_271 * _261);
        if ((int)(_145 > 0.0f) && ((int)(!_76))) {
          _305 = mad(_277, (View.TranslatedWorldToClip[2].x), mad(_276, (View.TranslatedWorldToClip[1].x), ((View.TranslatedWorldToClip[0].x) * _275))) + (View.TranslatedWorldToClip[3].x);
          _309 = mad(_277, (View.TranslatedWorldToClip[2].y), mad(_276, (View.TranslatedWorldToClip[1].y), ((View.TranslatedWorldToClip[0].y) * _275))) + (View.TranslatedWorldToClip[3].y);
          _313 = mad(_277, (View.TranslatedWorldToClip[2].z), mad(_276, (View.TranslatedWorldToClip[1].z), ((View.TranslatedWorldToClip[0].z) * _275))) + (View.TranslatedWorldToClip[3].z);
          _317 = mad(_277, (View.TranslatedWorldToClip[2].w), mad(_276, (View.TranslatedWorldToClip[1].w), ((View.TranslatedWorldToClip[0].w) * _275))) + (View.TranslatedWorldToClip[3].w);
          _318 = Light_Direction.x * _145;
          _319 = Light_Direction.y * _145;
          _320 = Light_Direction.z * _145;
          _336 = mad(_320, (View.TranslatedWorldToClip[2].w), mad(_319, (View.TranslatedWorldToClip[1].w), ((View.TranslatedWorldToClip[0].w) * _318))) + _317;
          _337 = _305 / _317;
          _338 = _309 / _317;
          _339 = _313 / _317;
          _345 = ((mad(_320, (View.TranslatedWorldToClip[2].z), mad(_319, (View.TranslatedWorldToClip[1].z), ((View.TranslatedWorldToClip[0].z) * _318))) + _313) / _336) - _339;
          _353 = (View.ScreenPositionScaleBias.x * _337) + View.ScreenPositionScaleBias.w;
          _354 = (View.ScreenPositionScaleBias.y * _338) + View.ScreenPositionScaleBias.z;
          _355 = View.ScreenPositionScaleBias.x * (((mad(_320, (View.TranslatedWorldToClip[2].x), mad(_319, (View.TranslatedWorldToClip[1].x), ((View.TranslatedWorldToClip[0].x) * _318))) + _305) / _336) - _337);
          _356 = View.ScreenPositionScaleBias.y * (((mad(_320, (View.TranslatedWorldToClip[2].y), mad(_319, (View.TranslatedWorldToClip[1].y), ((View.TranslatedWorldToClip[0].y) * _318))) + _309) / _336) - _338);
          _358 = (_156 + -0.5f) * 0.25f;
          _359 = _358 + 0.25f;
          if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _359) + _353), ((_356 * _359) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _359) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _359) + _353), ((_356 * _359) + _354)), 0.0f))).x))) {
            _414 = _359;
            _419 = (max(0.0f, (_414 + -0.375f)) * _145);
          } else {
            _375 = _358 + 0.5f;
            if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _375) + _353), ((_356 * _375) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _375) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _375) + _353), ((_356 * _375) + _354)), 0.0f))).x))) {
              _414 = _375;
              _419 = (max(0.0f, (_414 + -0.375f)) * _145);
            } else {
              _388 = _358 + 0.75f;
              if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _388) + _353), ((_356 * _388) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _388) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _388) + _353), ((_356 * _388) + _354)), 0.0f))).x))) {
                _414 = _388;
                _419 = (max(0.0f, (_414 + -0.375f)) * _145);
              } else {
                _401 = _358 + 1.0f;
                if ((int)((((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _401) + _353), ((_356 * _401) + _354)), 0.0f))).x) != (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(_353, _354), 0.0f))).x)) && (int)(((_345 * _401) + _339) < (((float4)(SceneTexturesStruct_SceneDepthTexture.SampleLevel(SceneTexturesStruct_PointClampSampler, float2(((_355 * _401) + _353), ((_356 * _401) + _354)), 0.0f))).x))) {
                  _414 = _401;
                  _419 = (max(0.0f, (_414 + -0.375f)) * _145);
                } else {
                  _419 = _145;
                }
              }
            }
          }
        } else {
          _419 = _145;
        }
        if ((int)SMRTRayCount > (int)0) {
          _450 = mad(_277, (View.TranslatedWorldToView[2].x), mad(_276, (View.TranslatedWorldToView[1].x), ((View.TranslatedWorldToView[0].x) * _275))) + (View.TranslatedWorldToView[3].x);
          _454 = mad(_277, (View.TranslatedWorldToView[2].y), mad(_276, (View.TranslatedWorldToView[1].y), ((View.TranslatedWorldToView[0].y) * _275))) + (View.TranslatedWorldToView[3].y);
          _458 = mad(_277, (View.TranslatedWorldToView[2].z), mad(_276, (View.TranslatedWorldToView[1].z), ((View.TranslatedWorldToView[0].z) * _275))) + (View.TranslatedWorldToView[3].z);
          _464 = sqrt(((_454 * _454) + (_450 * _450)) + (_458 * _458));
          if ((((int)((int)(_168 == 9) || ((int)(_183 || (int)((uint)(_168 + -5) < (uint)3))))) || ((int)(_76 || (int)(bCullBackfacingPixels == 0)))) | !(dot(float3(_269, _270, _271), float3(Light_Direction.x, Light_Direction.y, Light_Direction.z)) < (-0.0f - max(abs(_228), 0.10000000149011612f)))) {
            _472 = LightUniformVirtualShadowMapId * 288;
            _476 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 208u)))).x;
            _477 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 208u)))).y;
            _478 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 208u)))).z;
            _484 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 224u)))).x;
            _485 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 224u)))).y;
            _486 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 224u)))).z;
            _492 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_472 + 236u))));
            _496 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 240u)))).x;
            _497 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 240u)))).y;
            _498 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_472 + 240u)))).z;
            _504 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_472 + 264u))));
            _507 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_472 + 268u))));
            _528 = _275 + (asfloat(_496) + ((asfloat(_476) - View.PreViewTranslationHigh.x) + (asfloat(_484) - View.PreViewTranslationLow.x)));
            _529 = _276 + (asfloat(_497) + ((asfloat(_477) - View.PreViewTranslationHigh.y) + (asfloat(_485) - View.PreViewTranslationLow.y)));
            _530 = _277 + (asfloat(_498) + ((asfloat(_478) - View.PreViewTranslationHigh.z) + (asfloat(_486) - View.PreViewTranslationLow.z)));
            _546 = max((int)(0), (int)((int(floor(log2(sqrt((_530 * _530) + ((_528 * _528) + (_529 * _529)))) + select((VirtualShadowMap.bClipmapGreedyLevelSelection != 0), 0.0f, asfloat(_492)))) - _504)));
            if ((int)_546 < (int)_507) {
              _549 = _546 + (uint)(LightUniformVirtualShadowMapId);
              _550 = _549 * 288;
              _553 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 64u)))).x;
              _554 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 64u)))).y;
              _559 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 80u)))).x;
              _560 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 80u)))).y;
              _565 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 96u)))).x;
              _566 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 96u)))).y;
              _571 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 112u)))).x;
              _572 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_550 + 112u)))).y;
              _577 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 208u)))).x;
              _578 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 208u)))).y;
              _579 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 208u)))).z;
              _585 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 224u)))).x;
              _586 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 224u)))).y;
              _587 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_550 + 224u)))).z;
              _600 = _275 + ((asfloat(_577) - View.PreViewTranslationHigh.x) + (asfloat(_585) - View.PreViewTranslationLow.x));
              _601 = _276 + ((asfloat(_578) - View.PreViewTranslationHigh.y) + (asfloat(_586) - View.PreViewTranslationLow.y));
              _602 = _277 + ((asfloat(_579) - View.PreViewTranslationHigh.z) + (asfloat(_587) - View.PreViewTranslationLow.z));
              _613 = uint(mad(1.0f, asfloat(_571), mad(_602, asfloat(_565), mad(_601, asfloat(_559), (asfloat(_553) * _600)))) * 128.0f);
              _614 = uint(mad(1.0f, asfloat(_572), mad(_602, asfloat(_566), mad(_601, asfloat(_560), (asfloat(_554) * _600)))) * 128.0f);
              if (!((uint)_549 < (uint)8192)) {
                _623 = ((int)((((_549 * 21845) + (uint)(-178946048)) + _613) + (_614 << 7)));
              } else {
                _623 = _549;
              }
              _626 = VirtualShadowMap_PageTable[_623];
              _627 = (uint)(_626) >> 20;
              _628 = _627 & 63;
              if ((int)_626 < (int)0) {
                _631 = (_628 == 0);
                _633 = _628 + _549;
                if (!_631) {
                  if (!((uint)_633 < (uint)8192)) {
                    _642 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_550 + 256u)))).y;
                    _645 = asint(VirtualShadowMap_ProjectionData.Load2(((int)((_633 * 288) + 256u)))).y;
                    _647 = _627 & 31;
                    _651 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_550 + 256u)))).x;
                    _654 = asint(VirtualShadowMap_ProjectionData.Load2(((int)((_633 * 288) + 256u)))).x;
                    _665 = ((int)((((_633 * 21845) + (uint)(-178946048)) + ((uint)((uint)((_613 - (_651 << 5)) + (((int)(_654 << 5)) << _647)) >> _647))) + (((uint)((_614 - (_642 << 5)) + (((int)(_645 << 5)) << _647)) >> _647) << 7)));
                  } else {
                    _665 = _633;
                  }
                  _667 = VirtualShadowMap_PageTable[_665];
                  _672 = ((int)(uint)((int)((_667 & -2081423360) == -2147483648)));
                } else {
                  _672 = ((int)(uint)(_631));
                }
                _676 = _672;
                _677 = select((_672 != 0), _633, -1);
              } else {
                _676 = 0;
                _677 = -1;
              }
              _683 = select(((int)(_676 != 0) && (int)((int)_677 > (int)_549)), _677, _549);
            } else {
              _683 = -1;
            }
            if (!((int)_683 < (int)0)) {
              _686 = _683 * 288;
              _687 = _686 + 32u;
              _689 = asint(VirtualShadowMap_ProjectionData.Load4(_687)).z;
              _690 = asfloat(_689);
              _693 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 64u)))).x;
              _694 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 64u)))).y;
              _695 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 64u)))).z;
              _696 = asfloat(_693);
              _697 = asfloat(_694);
              _698 = asfloat(_695);
              _701 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 80u)))).x;
              _702 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 80u)))).y;
              _703 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 80u)))).z;
              _704 = asfloat(_701);
              _705 = asfloat(_702);
              _706 = asfloat(_703);
              _709 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 96u)))).x;
              _710 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 96u)))).y;
              _711 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 96u)))).z;
              _712 = asfloat(_709);
              _713 = asfloat(_710);
              _714 = asfloat(_711);
              _717 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 112u)))).x;
              _718 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 112u)))).y;
              _719 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 112u)))).z;
              _725 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 128u)))).x;
              _726 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 128u)))).y;
              _727 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 128u)))).z;
              _733 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 144u)))).x;
              _734 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 144u)))).y;
              _735 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 144u)))).z;
              _741 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 160u)))).x;
              _742 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 160u)))).y;
              _743 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 160u)))).z;
              _749 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 208u)))).x;
              _750 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 208u)))).y;
              _751 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 208u)))).z;
              _757 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 224u)))).x;
              _758 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 224u)))).y;
              _759 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_686 + 224u)))).z;
              _765 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_686 + 280u))));
              _767 = asfloat(_765) * SMRTTexelDitherScale;
              if (_767 > 0.0f) {
                _772 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_686 + 264u))));
                _775 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_686 + 236u))));
                _784 = (((_464 * 3.0517578125e-05f) * _767) / exp2(float((int)(_772)) - asfloat(_775)));
              } else {
                _784 = 0.0f;
              }
              _793 = mad(_271, asfloat(_743), mad(_270, asfloat(_735), (asfloat(_727) * _269)));
              _802 = _464 * SMRTRayLengthScale;
              if (!(SMRTRayCount == 0)) {
                _818 = 0;
                _819 = 0;
                _820 = 0.0f;
                while(true) {
                  _823 = float((uint)_819);
                  _831 = float((int)(BlueNoise.Dimensions.x));
                  _832 = float((int)(BlueNoise.Dimensions.y));
                  _838 = float((uint)(_819 + (uint)(SMRTRayCount)));
                  _856 = (BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y;
                  // IS-FAST replacement for Blue Noise Vec2 (SMRT ray dithering, second path)
                  if (InjectionToggle(TOGGLE_USE_ISFAST_SHADOWS)) {
                    float2 _isfast_ray0 = ISFASTNoise.Load(int4(int2((uint(_62) + uint(_823)) % 128u, (uint(_64) + uint(_823) * 7u) % 128u), (uint(float(InjectionFrameIndex())) + uint(_823)) % 32u, 0));
                    float2 _isfast_ray1 = ISFASTNoise.Load(int4(int2((uint(_62) + uint(_838)) % 128u, (uint(_64) + uint(_838) * 7u) % 128u), (uint(float(InjectionFrameIndex())) + uint(_838)) % 32u, 0));
                    _859 = float4(_isfast_ray0.x, _isfast_ray0.y, 0, 0);
                    _867 = float4(_isfast_ray1.x, _isfast_ray1.y, 0, 0);
                  } else {
                    _859 = BlueNoise_Vec2Texture.Load(int3((((int)((uint)(int(_831 * frac(_823 * 0.7548776268959045f))) + _62)) & BlueNoise.ModuloMasks.x), ((int)(_856 + ((uint)(((int)((uint)(int(_832 * frac(_823 * 0.5698402523994446f))) + _64)) & BlueNoise.ModuloMasks.y)))), 0));
                    _867 = BlueNoise_Vec2Texture.Load(int3((BlueNoise.ModuloMasks.x & ((int)((uint)(int(_831 * frac(_838 * 0.7548776268959045f))) + _62))), ((int)(_856 + ((uint)(BlueNoise.ModuloMasks.y & ((int)((uint)(int(_832 * frac(_838 * 0.5698402523994446f))) + _64)))))), 0));
                  }
                  _872 = (_859.x * 2.0f) + -0.9999999403953552f;
                  _873 = (_859.y * 2.0f) + -0.9999999403953552f;
                  _874 = abs(_872);
                  _875 = abs(_873);
                  _877 = max(_874, _875);
                  _884 = ((min(_874, _875) / (_877 + 5.421010862427522e-20f)) + (float((bool)(uint)(_875 >= _874)) * 2.0f)) * 0.7853981852531433f;
                  _899 = _877 * _228;
                  _900 = _899 * asfloat(((asint(cos(_884)) & 2147483647) | (asint(_872) & -2147483648)));
                  _901 = _899 * asfloat(((asint(sin(_884)) & 2147483647) | (asint(_873) & -2147483648)));
                  _903 = (abs(Light_Direction.x) > 9.999999974752427e-07f);
                  _904 = select(_903, 1.0f, 0.0f);
                  _905 = select(_903, 0.0f, 1.0f);
                  _907 = -0.0f - (Light_Direction.z * _905);
                  _908 = _904 * Light_Direction.z;
                  _911 = (_905 * Light_Direction.x) - (_904 * Light_Direction.y);
                  _928 = ((_900 * _907) + Light_Direction.x) + (((_908 * Light_Direction.z) - (_911 * Light_Direction.y)) * _901);
                  _930 = ((_900 * _908) + Light_Direction.y) + (((_911 * Light_Direction.x) - (Light_Direction.z * _907)) * _901);
                  _932 = ((_911 * _900) + Light_Direction.z) + (((Light_Direction.y * _907) - (_908 * Light_Direction.x)) * _901);
                  _934 = rsqrt(dot(float3(_928, _930, _932), float3(_928, _930, _932)));
                  _935 = _928 * _934;
                  _936 = _930 * _934;
                  _937 = _932 * _934;
                  _940 = (_867.x + -0.5f) * _784;
                  _941 = (_867.y + -0.5f) * _784;
                  _945 = (((asfloat(_749) - View.PreViewTranslationHigh.x) + (asfloat(_757) - View.PreViewTranslationLow.x)) + _275) + (_935 * _419);
                  _946 = (((asfloat(_750) - View.PreViewTranslationHigh.y) + (asfloat(_758) - View.PreViewTranslationLow.y)) + _276) + (_936 * _419);
                  _947 = (((asfloat(_751) - View.PreViewTranslationHigh.z) + (asfloat(_759) - View.PreViewTranslationLow.z)) + _277) + (_937 * _419);
                  _948 = _935 * _802;
                  _949 = _936 * _802;
                  _950 = _937 * _802;
                  _977 = (mad(_947, _714, mad(_946, _706, (_945 * _698))) + asfloat(_719)) + max(0.0f, ((max(0.0f, dot(float2(min(max(((-0.0f - mad(_271, asfloat(_741), mad(_270, asfloat(_733), (asfloat(_725) * _269)))) / _793), -0.05000000074505806f), 0.05000000074505806f), min(max(((-0.0f - mad(_271, asfloat(_742), mad(_270, asfloat(_734), (asfloat(_726) * _269)))) / _793), -0.05000000074505806f), 0.05000000074505806f)), float2(_940, _941))) * 2.0f) - abs(_690 * _419)));
                  _979 = abs(_690 * SMRTExtrapolateSlope);
                  if ((int)SMRTSamplesPerRay > (int)-1) {
                    _986 = -10000.0f;
                    _987 = -1.0f;
                    _988 = 0.0f;
                    _989 = -1.0f;
                    _990 = 0;
                    while(true) {
                      if (!(_990 == SMRTSamplesPerRay)) {
                        _996 = ((float((int)(_990)) + (1.0f - _156)) * (-1.0f / float((int)(SMRTSamplesPerRay)))) + 1.0f;
                        _999 = (_996 * _996);
                      } else {
                        _999 = 0.0f;
                      }
                      _1005 = ((_940 + asfloat(_717)) + mad(_947, _712, mad(_946, _704, (_945 * _696)))) + (_999 * mad(_950, _712, mad(_949, _704, (_948 * _696))));
                      _1008 = ((_941 + asfloat(_718)) + mad(_947, _713, mad(_946, _705, (_945 * _697)))) + (_999 * mad(_950, _713, mad(_949, _705, (_948 * _697))));
                      _1009 = (_999 * mad(_950, _714, mad(_949, _706, (_948 * _698)))) + _977;
                      _1012 = uint(_1005 * 128.0f);
                      _1013 = uint(_1008 * 128.0f);
                      if (!((uint)_683 < (uint)8192)) {
                        _1022 = ((int)((((_683 * 21845) + (uint)(-178946048)) + _1012) + (_1013 << 7)));
                      } else {
                        _1022 = _683;
                      }
                      _1025 = VirtualShadowMap_PageTable[_1022];
                      _1026 = (uint)(_1025) >> 20;
                      _1027 = _1026 & 63;
                      if ((int)_1025 < (int)0) {
                        _1030 = (_1027 == 0);
                        _1032 = _1027 + _683;
                        if (!_1030) {
                          _1041 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_686 + 48u)))).z;
                          _1045 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_686 + 256u)))).x;
                          _1046 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_686 + 256u)))).y;
                          _1047 = _1032 * 288;
                          _1050 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1047 + 48u)))).z;
                          _1054 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1047 + 256u)))).x;
                          _1055 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1047 + 256u)))).y;
                          _1062 = _1026 & 31;
                          _1067 = (uint)((_1012 - (_1045 << 5)) + (((int)(_1054 << 5)) << _1062)) >> _1062;
                          _1069 = _1067 << 7;
                          _1070 = ((uint)((_1013 - (_1046 << 5)) + (((int)(_1055 << 5)) << _1062)) >> _1062) << 7;
                          _1079 = 1.0f / float((uint)(1 << _1062));
                          if (!((uint)_1032 < (uint)8192)) {
                            _1107 = ((int)((((_1032 * 21845) + (uint)(-178946048)) + _1067) + _1070));
                          } else {
                            _1107 = _1032;
                          }
                          _1109 = VirtualShadowMap_PageTable[_1107];
                          _1114 = ((int)min((uint)(((int)max((uint)((int)(uint((((float((int)(_1054)) - (_1079 * float((int)(_1045)))) * 0.25f) + (_1079 * _1005)) * 16384.0f))), (uint)(_1069)))), (uint)((_1069 | 127))));
                          _1115 = ((int)min((uint)(((int)max((uint)((int)(uint((((float((int)(_1055)) - (_1079 * float((int)(_1046)))) * 0.25f) + (_1079 * _1008)) * 16384.0f))), (uint)(_1070)))), (uint)((_1070 | 127))));
                          _1116 = _1079;
                          _1117 = (asfloat(_1050) - (_1079 * asfloat(_1041)));
                          _1118 = ((int)(uint)((int)((_1109 & -2081423360) == -2147483648)));
                          _1119 = _1109;
                        } else {
                          _1114 = (int)(uint(_1005 * 16384.0f));
                          _1115 = (int)(uint(_1008 * 16384.0f));
                          _1116 = 1.0f;
                          _1117 = 0.0f;
                          _1118 = ((int)(uint)(_1030));
                          _1119 = _1025;
                        }
                        if (!(_1118 == 0)) {
                          _1137 = true;
                          _1138 = ((asfloat((((uint)(VirtualShadowMap_PhysicalPagePool.Load(int4(((((int)(_1119 << 7)) & 130944) | (_1114 & 127)), ((((uint)(_1119) >> 3) & 130944) | (_1115 & 127)), 0, 0)))).x)) - _1117) / _1116);
                        } else {
                          _1137 = false;
                          _1138 = 0.0f;
                        }
                      } else {
                        _1137 = false;
                        _1138 = 0.0f;
                      }
                      _1139 = select(_1137, _1138, 0.0f);
                      if (_1137) {
                        if (_986 == -10000.0f) {
                          if (!(_1139 > _1009)) {
                            _1173 = _1138;
                            _1174 = _999;
                            _1175 = _988;
                            _1176 = _1009;
                            if ((int)_990 < (int)SMRTSamplesPerRay) {
                              _986 = _1173;
                              _987 = _1174;
                              _988 = _1175;
                              _989 = _1176;
                              _990 = (_990 + 1);
                              continue;
                            } else {
                              _1180 = -1.0f;
                              _1181 = false;
                            }
                          } else {
                            _1180 = _1138;
                            _1181 = true;
                          }
                        } else {
                          _1146 = abs(_1009 - _989);
                          _1150 = _999 - _987;
                          if ((_1139 - _1009) > (_1146 * 1.0499999523162842f)) {
                            _1163 = _986;
                            _1164 = _987;
                            _1165 = _988;
                            _1166 = ((_1150 * _988) + _986);
                          } else {
                            if (_1139 != _986) {
                              _1163 = _1138;
                              _1164 = _999;
                              _1165 = min(max(((_1139 - _986) / _1150), (-0.0f - _979)), _979);
                              _1166 = _1138;
                            } else {
                              _1163 = _986;
                              _1164 = _987;
                              _1165 = _988;
                              _1166 = _1138;
                            }
                          }
                          _1167 = _1146 * 0.5249999761581421f;
                          if (!(abs((_1167 + _1009) - _1166) < _1167)) {
                            _1173 = _1163;
                            _1174 = _1164;
                            _1175 = _1165;
                            _1176 = _1009;
                            if ((int)_990 < (int)SMRTSamplesPerRay) {
                              _986 = _1173;
                              _987 = _1174;
                              _988 = _1175;
                              _989 = _1176;
                              _990 = (_990 + 1);
                              continue;
                            } else {
                              _1180 = -1.0f;
                              _1181 = false;
                            }
                          } else {
                            _1180 = _1166;
                            _1181 = true;
                          }
                        }
                      } else {
                        _1173 = _986;
                        _1174 = _987;
                        _1175 = _988;
                        _1176 = _989;
                        if ((int)_990 < (int)SMRTSamplesPerRay) {
                          _986 = _1173;
                          _987 = _1174;
                          _988 = _1175;
                          _989 = _1176;
                          _990 = (_990 + 1);
                          continue;
                        } else {
                          _1180 = -1.0f;
                          _1181 = false;
                        }
                      }
                      _1183 = _1180;
                      _1184 = _1181;
                      break;
                    }
                  } else {
                    _1183 = -1.0f;
                    _1184 = false;
                  }
                  if (_1184) {
                    _1188 = asint(VirtualShadowMap_ProjectionData.Load4(_687)).z;
                    _1197 = _818;
                    _1198 = (max(9.999999974752427e-07f, ((_977 - _1183) / asfloat(_1188))) + _820);
                  } else {
                    _1197 = ((int)(_818 + 1u));
                    _1198 = _820;
                  }
                  if (!(SMRTAdaptiveRayCount == 0)) {
                    if (_819 == 0) {
                      _1204 = WaveActiveAllTrue(!_1184);
                      if (!_1204) {
                        _1211 = _819 + 1u;
                        if ((uint)_1211 < (uint)SMRTRayCount) {
                          _818 = _1197;
                          _819 = _1211;
                          _820 = _1198;
                          continue;
                        } else {
                          _1214 = _1211;
                        }
                      } else {
                        _1214 = 0;
                      }
                    } else {
                      if (((uint)_819 < (uint)SMRTAdaptiveRayCount) | !(WaveActiveAllTrue(_1197 == 0))) {
                        _1211 = _819 + 1u;
                        if ((uint)_1211 < (uint)SMRTRayCount) {
                          _818 = _1197;
                          _819 = _1211;
                          _820 = _1198;
                          continue;
                        } else {
                          _1214 = _1211;
                        }
                      } else {
                        _1214 = _819;
                      }
                    }
                  } else {
                    _1211 = _819 + 1u;
                    if ((uint)_1211 < (uint)SMRTRayCount) {
                      _818 = _1197;
                      _819 = _1211;
                      _820 = _1198;
                      continue;
                    } else {
                      _1214 = _1211;
                    }
                  }
                  _1216 = _1197;
                  _1217 = _1214;
                  _1218 = _1198;
                  break;
                }
              } else {
                _1216 = 0;
                _1217 = 0;
                _1218 = 0.0f;
              }
              _1220 = (int)min((uint)(((int)(_1217 + 1u))), (uint)(SMRTRayCount));
              _1604 = (float((uint)_1216) / float((uint)_1220));
              _1605 = (_1218 / float((uint)((uint)((int)max((uint)(1), (uint)(((int)(_1220 - _1216))))))));
            } else {
              _1604 = 1.0f;
              _1605 = -1.0f;
            }
          } else {
            _1604 = 0.0f;
            _1605 = -1.0f;
          }
        } else {
          _1230 = LightUniformVirtualShadowMapId * 288;
          _1234 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 208u)))).x;
          _1235 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 208u)))).y;
          _1236 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 208u)))).z;
          _1242 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 224u)))).x;
          _1243 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 224u)))).y;
          _1244 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 224u)))).z;
          _1250 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_1230 + 236u))));
          _1254 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 240u)))).x;
          _1255 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 240u)))).y;
          _1256 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1230 + 240u)))).z;
          _1262 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_1230 + 264u))));
          _1265 = asint(VirtualShadowMap_ProjectionData.Load(((int)(_1230 + 268u))));
          _1286 = _275 + (asfloat(_1254) + ((asfloat(_1234) - View.PreViewTranslationHigh.x) + (asfloat(_1242) - View.PreViewTranslationLow.x)));
          _1287 = _276 + (asfloat(_1255) + ((asfloat(_1235) - View.PreViewTranslationHigh.y) + (asfloat(_1243) - View.PreViewTranslationLow.y)));
          _1288 = _277 + (asfloat(_1256) + ((asfloat(_1236) - View.PreViewTranslationHigh.z) + (asfloat(_1244) - View.PreViewTranslationLow.z)));
          _1304 = max((int)(0), (int)((int(floor(log2(sqrt((_1288 * _1288) + ((_1286 * _1286) + (_1287 * _1287)))) + select((VirtualShadowMap.bClipmapGreedyLevelSelection != 0), 0.0f, asfloat(_1250)))) - _1262)));
          if ((int)_1304 < (int)_1265) {
            _1307 = _1304 + LightUniformVirtualShadowMapId;
            _1308 = _1307 * 288;
            _1311 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 32u)))).z;
            _1312 = asfloat(_1311);
            _1315 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 64u)))).x;
            _1316 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 64u)))).y;
            _1317 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 64u)))).z;
            _1323 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 80u)))).x;
            _1324 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 80u)))).y;
            _1325 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 80u)))).z;
            _1331 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 96u)))).x;
            _1332 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 96u)))).y;
            _1333 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 96u)))).z;
            _1339 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 112u)))).x;
            _1340 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 112u)))).y;
            _1341 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 112u)))).z;
            _1347 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 208u)))).x;
            _1348 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 208u)))).y;
            _1349 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 208u)))).z;
            _1355 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 224u)))).x;
            _1356 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 224u)))).y;
            _1357 = asint(VirtualShadowMap_ProjectionData.Load3(((int)(_1308 + 224u)))).z;
            _1370 = ((asfloat(_1347) - View.PreViewTranslationHigh.x) + (asfloat(_1355) - View.PreViewTranslationLow.x)) + _275;
            _1371 = ((asfloat(_1348) - View.PreViewTranslationHigh.y) + (asfloat(_1356) - View.PreViewTranslationLow.y)) + _276;
            _1372 = ((asfloat(_1349) - View.PreViewTranslationHigh.z) + (asfloat(_1357) - View.PreViewTranslationLow.z)) + _277;
            _1376 = mad(_1372, asfloat(_1331), mad(_1371, asfloat(_1323), (_1370 * asfloat(_1315)))) + asfloat(_1339);
            _1380 = mad(_1372, asfloat(_1332), mad(_1371, asfloat(_1324), (_1370 * asfloat(_1316)))) + asfloat(_1340);
            _1384 = mad(_1372, asfloat(_1333), mad(_1371, asfloat(_1325), (_1370 * asfloat(_1317)))) + asfloat(_1341);
            _1387 = uint(_1376 * 128.0f);
            _1388 = uint(_1380 * 128.0f);
            if (!((uint)_1307 < (uint)8192)) {
              _1397 = ((int)((((_1307 * 21845) + (uint)(-178946048)) + _1387) + (_1388 << 7)));
            } else {
              _1397 = _1307;
            }
            _1400 = VirtualShadowMap_PageTable[_1397];
            _1401 = (uint)(_1400) >> 20;
            _1402 = _1401 & 63;
            if ((int)_1400 < (int)0) {
              _1405 = (_1402 == 0);
              _1407 = _1402 + _1307;
              _1408 = _1376 * 16384.0f;
              _1409 = _1380 * 16384.0f;
              if (!_1405) {
                _1415 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1308 + 256u)))).x;
                _1416 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1308 + 256u)))).y;
                _1417 = _1407 * 288;
                _1420 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1417 + 256u)))).x;
                _1421 = asint(VirtualShadowMap_ProjectionData.Load2(((int)(_1417 + 256u)))).y;
                _1428 = _1401 & 31;
                _1433 = (uint)((_1387 - (_1415 << 5)) + (((int)(_1420 << 5)) << _1428)) >> _1428;
                _1435 = _1433 << 7;
                _1436 = ((uint)((_1388 - (_1416 << 5)) + (((int)(_1421 << 5)) << _1428)) >> _1428) << 7;
                _1441 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1308 + 48u)))).z;
                _1445 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1417 + 48u)))).z;
                _1453 = 1.0f / float((uint)(1 << _1428));
                _1466 = (((float((int)(_1420)) - (_1453 * float((int)(_1415)))) * 0.25f) + (_1453 * _1376)) * 16384.0f;
                _1467 = (((float((int)(_1421)) - (_1453 * float((int)(_1416)))) * 0.25f) + (_1453 * _1380)) * 16384.0f;
                if (!((uint)_1407 < (uint)8192)) {
                  _1481 = ((int)((((_1407 * 21845) + (uint)(-178946048)) + _1433) + _1436));
                } else {
                  _1481 = _1407;
                }
                _1483 = VirtualShadowMap_PageTable[_1481];
                _1488 = _1466;
                _1489 = _1467;
                _1490 = ((int)min((uint)(((int)max((uint)((int)(uint(_1466))), (uint)(_1435)))), (uint)((_1435 | 127))));
                _1491 = ((int)min((uint)(((int)max((uint)((int)(uint(_1467))), (uint)(_1436)))), (uint)((_1436 | 127))));
                _1492 = _1453;
                _1493 = (asfloat(_1445) - (_1453 * asfloat(_1441)));
                _1494 = ((int)(uint)((int)((_1483 & -2081423360) == -2147483648)));
                _1495 = _1483;
              } else {
                _1488 = _1408;
                _1489 = _1409;
                _1490 = (int)(uint(_1408));
                _1491 = (int)(uint(_1409));
                _1492 = 1.0f;
                _1493 = 0.0f;
                _1494 = ((int)(uint)(_1405));
                _1495 = _1400;
              }
              if (!(_1494 == 0)) {
                _1513 = _1488;
                _1514 = _1489;
                _1515 = _1490;
                _1516 = _1491;
                _1517 = true;
                _1518 = _1407;
                _1519 = ((asfloat((((uint)(VirtualShadowMap_PhysicalPagePool.Load(int4(((((int)(_1495 << 7)) & 130944) | (_1490 & 127)), ((((uint)(_1495) >> 3) & 130944) | (_1491 & 127)), 0, 0)))).x)) - _1493) / _1492);
              } else {
                _1513 = _1488;
                _1514 = _1489;
                _1515 = _1490;
                _1516 = _1491;
                _1517 = false;
                _1518 = -1;
                _1519 = 0.0f;
              }
            } else {
              _1513 = 0.0f;
              _1514 = 0.0f;
              _1515 = 0;
              _1516 = 0;
              _1517 = false;
              _1518 = -1;
              _1519 = 0.0f;
            }
            if (_1517) {
              _1521 = _1518 * 288;
              _1524 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 32u)))).z;
              _1528 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 128u)))).x;
              _1529 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 128u)))).y;
              _1530 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 128u)))).z;
              _1536 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 144u)))).x;
              _1537 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 144u)))).y;
              _1538 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 144u)))).z;
              _1544 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 160u)))).x;
              _1545 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 160u)))).y;
              _1546 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 160u)))).z;
              _1552 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 176u)))).x;
              _1553 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 176u)))).y;
              _1554 = asint(VirtualShadowMap_ProjectionData.Load4(((int)(_1521 + 176u)))).z;
              _1559 = -0.0f - dot(float3(_269, _270, _271), float3(_1370, _1371, _1372));
              _1571 = mad(_1559, asfloat(_1554), mad(_271, asfloat(_1546), mad(_270, asfloat(_1538), (asfloat(_1530) * _269))));
              if (((_1519 + (_1312 * max(_419, 0.0f))) - (min((max(0.0f, dot(float2(((-0.0f - mad(_1559, asfloat(_1552), mad(_271, asfloat(_1544), mad(_270, asfloat(_1536), (asfloat(_1528) * _269))))) / _1571), ((-0.0f - mad(_1559, asfloat(_1553), mad(_271, asfloat(_1545), mad(_270, asfloat(_1537), (asfloat(_1529) * _269))))) / _1571)), float2((((0.5f - _1513) + float((uint)_1515)) * 6.103515625e-05f), (((0.5f - _1514) + float((uint)_1516)) * 6.103515625e-05f)))) * 2.0f), abs(asfloat(_1524) * 100.0f)) * float((uint)(1 << ((_1518 - _1307) & 31))))) > _1384) {
                _1604 = 0.0f;
                _1605 = max(9.999999974752427e-07f, ((_1384 - _1519) / _1312));
              } else {
                _1604 = 1.0f;
                _1605 = -1.0f;
              }
            } else {
              _1604 = 1.0f;
              _1605 = -1.0f;
            }
          } else {
            _1604 = 1.0f;
            _1605 = -1.0f;
          }
        }
        if (_220 && (int)(_1604 < 1.0f)) {
          _1611 = saturate(exp2(_210 * _1605));
          _1614 = ((1.0f - _1611) * _1604) + _1611;
          _1617 = (_1614 * _1614);
        } else {
          _1617 = _1604;
        }
        if (_1617 > 0.0f) {
          _1626 = float((int)(BlueNoise.Dimensions.x));
          _1627 = float((int)(BlueNoise.Dimensions.y));
          _1639 = (BlueNoise.ModuloMasks.z & View.StateFrameIndexMod8) * BlueNoise.Dimensions.y;
          _1642 = BlueNoise_Vec2Texture.Load(int3((BlueNoise.ModuloMasks.x & _62), ((int)(_1639 + ((uint)(BlueNoise.ModuloMasks.y & _64)))), 0));
          _1650 = BlueNoise_Vec2Texture.Load(int3((BlueNoise.ModuloMasks.x & ((int)((uint)(int(_1626 * 0.7548776268959045f)) + _62))), ((int)(_1639 + ((uint)(BlueNoise.ModuloMasks.y & ((int)((uint)(int(_1627 * 0.5698402523994446f)) + _64)))))), 0));
          _1683 = mad(_277, (View.TranslatedWorldToView[2].x), mad(_276, (View.TranslatedWorldToView[1].x), ((View.TranslatedWorldToView[0].x) * _275))) + (View.TranslatedWorldToView[3].x);
          _1687 = mad(_277, (View.TranslatedWorldToView[2].y), mad(_276, (View.TranslatedWorldToView[1].y), ((View.TranslatedWorldToView[0].y) * _275))) + (View.TranslatedWorldToView[3].y);
          _1691 = mad(_277, (View.TranslatedWorldToView[2].z), mad(_276, (View.TranslatedWorldToView[1].z), ((View.TranslatedWorldToView[0].z) * _275))) + (View.TranslatedWorldToView[3].z);
          _1700 = float((uint)uint(min((float((int)(SMRTRayCount)) * _1650.y), float((int)(SMRTRayCount + -1)))));
          _1716 = BlueNoise_Vec2Texture.Load(int3((((int)((uint)(int(_1626 * frac(_1700 * 0.7548776268959045f))) + _62)) & BlueNoise.ModuloMasks.x), ((int)(((uint)(((int)((uint)(int(_1627 * frac(_1700 * 0.5698402523994446f))) + _64)) & BlueNoise.ModuloMasks.y)) + ((BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y))), 0));
          _1721 = (_1716.x * 2.0f) + -0.9999999403953552f;
          _1722 = (_1716.y * 2.0f) + -0.9999999403953552f;
          _1723 = abs(_1721);
          _1724 = abs(_1722);
          _1726 = max(_1723, _1724);
          _1733 = ((min(_1723, _1724) / (_1726 + 5.421010862427522e-20f)) + (float((bool)(uint)(_1724 >= _1723)) * 2.0f)) * 0.7853981852531433f;
          _1748 = _1726 * _228;
          _1749 = _1748 * asfloat(((asint(cos(_1733)) & 2147483647) | (asint(_1721) & -2147483648)));
          _1750 = _1748 * asfloat(((asint(sin(_1733)) & 2147483647) | (asint(_1722) & -2147483648)));
          _1752 = (abs(Light_Direction.x) > 9.999999974752427e-07f);
          _1753 = select(_1752, 1.0f, 0.0f);
          _1754 = select(_1752, 0.0f, 1.0f);
          _1756 = -0.0f - (Light_Direction.z * _1754);
          _1757 = _1753 * Light_Direction.z;
          _1760 = (_1754 * Light_Direction.x) - (_1753 * Light_Direction.y);
          _1777 = ((_1749 * _1756) + Light_Direction.x) + (((_1757 * Light_Direction.z) - (_1760 * Light_Direction.y)) * _1750);
          _1779 = ((_1749 * _1757) + Light_Direction.y) + (((_1760 * Light_Direction.x) - (Light_Direction.z * _1756)) * _1750);
          _1781 = ((_1760 * _1749) + Light_Direction.z) + (((Light_Direction.y * _1756) - (_1757 * Light_Direction.x)) * _1750);
          _1784 = rsqrt(dot(float3(_1777, _1779, _1781), float3(_1777, _1779, _1781))) * ((SMRTRayLengthScale * 100.0f) * sqrt(((_1687 * _1687) + (_1683 * _1683)) + (_1691 * _1691)));
          _1801 = _275;
          _1802 = _276;
          _1803 = _277;
          _1804 = ((_1784 * _1777) + _275);
          _1805 = ((_1784 * _1779) + _276);
          _1806 = ((_1784 * _1781) + _277);
          _1807 = true;
          _1808 = (((_1642.x * 2.0f) + -1.0f) * 0.5f);
          _1809 = (((_1642.y * 2.0f) + -1.0f) * 0.5f);
          _1810 = (((_1650.x * 2.0f) + -1.0f) * 0.5f);
        } else {
          _1801 = 0.0f;
          _1802 = 0.0f;
          _1803 = 0.0f;
          _1804 = 0.0f;
          _1805 = 0.0f;
          _1806 = 0.0f;
          _1807 = false;
          _1808 = -0.5f;
          _1809 = -0.5f;
          _1810 = -0.5f;
        }
        if (_1807) {
          if (!(VirtualVoxel.NodeDescCount == 0)) {
            _1834 = _1617;
            _1835 = 0;
            while(true) {
              _1838 = VirtualVoxel_NodeDescBuffer[_1835].TranslatedWorldMinAABB.x;
              _1839 = VirtualVoxel_NodeDescBuffer[_1835].TranslatedWorldMinAABB.y;
              _1840 = VirtualVoxel_NodeDescBuffer[_1835].TranslatedWorldMinAABB.z;
              _1842 = VirtualVoxel_NodeDescBuffer[_1835].PackedPageIndexResolution;
              _1844 = VirtualVoxel_NodeDescBuffer[_1835].TranslatedWorldMaxAABB.x;
              _1845 = VirtualVoxel_NodeDescBuffer[_1835].TranslatedWorldMaxAABB.y;
              _1846 = VirtualVoxel_NodeDescBuffer[_1835].TranslatedWorldMaxAABB.z;
              _1848 = VirtualVoxel_NodeDescBuffer[_1835].PageIndexOffset_VoxelWorldSize;
              _1851 = _1842 & 255;
              _1853 = ((uint)(_1842) >> 8) & 255;
              _1855 = ((uint)(_1842) >> 16) & 255;
              _1856 = VirtualVoxel.PageResolution * _1851;
              _1857 = VirtualVoxel.PageResolution * _1853;
              _1858 = VirtualVoxel.PageResolution * _1855;
              _1861 = float((uint)((uint)((uint)(_1848) >> 22)));
              _1862 = _1861 * 0.009775171056389809f;
              _1871 = (_1862 * ((VirtualVoxel.DepthBiasScale_Shadow * Light_Direction.x) + _1808)) + _1801;
              _1872 = (_1862 * ((VirtualVoxel.DepthBiasScale_Shadow * Light_Direction.y) + _1809)) + _1802;
              _1873 = (_1862 * ((VirtualVoxel.DepthBiasScale_Shadow * Light_Direction.z) + _1810)) + _1803;
              if ((int)(_1855 != 0) && ((int)((int)(_1851 != 0) && (int)(_1853 != 0)))) {
                _1876 = _1804 - _1871;
                _1877 = _1805 - _1872;
                _1878 = _1806 - _1873;
                _1879 = 1.0f / _1876;
                _1880 = 1.0f / _1877;
                _1881 = 1.0f / _1878;
                _1885 = _1879 * (_1838 - _1871);
                _1886 = _1880 * (_1839 - _1872);
                _1887 = _1881 * (_1840 - _1873);
                _1891 = _1879 * (_1844 - _1871);
                _1892 = _1880 * (_1845 - _1872);
                _1893 = _1881 * (_1846 - _1873);
                _1904 = saturate(max(min(_1885, _1891), max(min(_1886, _1892), min(_1887, _1893))));
                _1905 = saturate(min(max(_1885, _1891), min(max(_1886, _1892), max(_1887, _1893))));
                if (_1904 < _1905) {
                  _1912 = _1876 * (_1905 - _1904);
                  _1914 = _1877 * (_1905 - _1904);
                  _1916 = _1878 * (_1905 - _1904);
                  _1923 = min(sqrt(((_1912 * _1912) + (_1914 * _1914)) + (_1916 * _1916)), 1e+05f);
                  _1925 = rsqrt(dot(float3(_1912, _1914, _1916), float3(_1912, _1914, _1916)));
                  _1928 = min(ceil(_1923 / _1862), 1024.0f);
                  if (_1928 > 0.0f) {
                    _1933 = 9999;
                    _1934 = 9999;
                    _1935 = 9999;
                    _1936 = 0;
                    _1937 = 0;
                    _1938 = 0;
                    _1939 = 0;
                    _1940 = 1.0f;
                    _1941 = 0.0f;
                    _1942 = 0.0f;
                    while(true) {
                      _1944 = max((_1940 * (_1923 / _1928)), 0.0f);
                      _1990 = (int)min((uint)((int)(uint(saturate(((((_1871 - _1838) + (_1904 * _1876)) + (((_1912 * _1862) * _1925) * _1941)) + (_1808 * _1944)) / (_1844 - _1838)) * float((uint)_1856)))), (uint)(((int)(_1856 + (uint)(-1)))));
                      _1991 = (int)min((uint)((int)(uint(saturate(((((_1872 - _1839) + (_1904 * _1877)) + (((_1914 * _1862) * _1925) * _1941)) + (_1809 * _1944)) / (_1845 - _1839)) * float((uint)_1857)))), (uint)(((int)(_1857 + (uint)(-1)))));
                      _1992 = (int)min((uint)((int)(uint(saturate(((((_1873 - _1840) + (_1904 * _1878)) + (((_1916 * _1862) * _1925) * _1941)) + (_1810 * _1944)) / (_1846 - _1840)) * float((uint)_1858)))), (uint)(((int)(_1858 + (uint)(-1)))));
                      _1993 = VirtualVoxel.PageResolutionLog2 & 31;
                      _1994 = (uint)(_1990) >> _1993;
                      _1995 = (uint)(_1991) >> _1993;
                      _1996 = (uint)(_1992) >> _1993;
                      if (((int)((int)(_1994 != _1933) || (int)(_1995 != _1934))) || (int)(_1996 != _1935)) {
                        _2013 = VirtualVoxel.PageCountResolution.x * VirtualVoxel.PageCountResolution.y;
                        _2014 = (((uint)(VirtualVoxel_PageIndexBuffer.Load((int)((_1994 + ((uint)(_1848 & 4194303))) + (((int)((_1996 * _1853) + _1995)) * _1851))))).x) % _2013;
                        _2019 = _1994;
                        _2020 = _1995;
                        _2021 = _1996;
                        _2022 = ((int)(uint)((int)((((uint)(VirtualVoxel_PageIndexBuffer.Load((int)((_1994 + ((uint)(_1848 & 4194303))) + (((int)((_1996 * _1853) + _1995)) * _1851))))).x) != -1)));
                        _2023 = ((int)(_2014 % VirtualVoxel.PageCountResolution.x));
                        _2024 = ((int)(_2014 / (uint)(VirtualVoxel.PageCountResolution.x)));
                        _2025 = ((int)(((uint)(((uint)(VirtualVoxel_PageIndexBuffer.Load((int)((_1994 + ((uint)(_1848 & 4194303))) + (((int)((_1996 * _1853) + _1995)) * _1851))))).x)) / _2013));
                      } else {
                        _2019 = _1933;
                        _2020 = _1934;
                        _2021 = _1935;
                        _2022 = _1936;
                        _2023 = _1937;
                        _2024 = _1938;
                        _2025 = _1939;
                      }
                      if (_2022 == 0) {
                        _2062 = _1942;
                      } else {
                        _2040 = _1944 * (102.30000305175781f / _1861);
                        _2042 = uint(log2(_2040));
                        _2043 = _2042 & 31;
                        if ((int)(((uint)(VirtualVoxel_PageTexture.Load(int4(((uint)((_1990 - (_1994 << _1993)) + (_2023 << _1993)) >> _2043), ((uint)((_1991 - (_1995 << _1993)) + (_2024 << _1993)) >> _2043), ((uint)((_1992 - (_1996 << _1993)) + (_2025 << _1993)) >> _2043), _2042)))).x) > (int)-1) {
                          _2058 = (((VirtualVoxel.DensityScale_Shadow * 0.0010000000474974513f) * _2040) * float((uint)((uint)((((uint)(VirtualVoxel_PageTexture.Load(int4(((uint)((_1990 - (_1994 << _1993)) + (_2023 << _1993)) >> _2043), ((uint)((_1991 - (_1995 << _1993)) + (_2024 << _1993)) >> _2043), ((uint)((_1992 - (_1996 << _1993)) + (_2025 << _1993)) >> _2043), _2042)))).x) & 16777215))));
                        } else {
                          _2058 = 0.0f;
                        }
                        _2059 = _2058 + _1942;
                        if (!(_2059 > 1.0f)) {
                          _2062 = _2059;
                        } else {
                          _2069 = _2059;
                        }
                      }
                      _2065 = min(float((uint)(uint)(VirtualVoxel.PageResolution)), (_1940 * VirtualVoxel.SteppingScale_Shadow));
                      _2066 = _2065 + _1941;
                      if (_2066 < _1928) {
                        _1933 = _2019;
                        _1934 = _2020;
                        _1935 = _2021;
                        _1936 = _2022;
                        _1937 = _2023;
                        _1938 = _2024;
                        _1939 = _2025;
                        _1940 = _2065;
                        _1941 = _2066;
                        _1942 = _2062;
                        continue;
                      } else {
                        _2069 = _2062;
                      }
                      _2071 = _2069;
                      break;
                    }
                  } else {
                    _2071 = 0.0f;
                  }
                } else {
                  _2071 = 0.0f;
                }
              } else {
                _2071 = 0.0f;
              }
              _2074 = min(_1834, saturate(1.0f - _2071));
              _2075 = _1835 + 1u;
              if (!(_2075 == VirtualVoxel.NodeDescCount)) {
                _1834 = _2074;
                _1835 = _2075;
                continue;
              }
              _2079 = _2074;
              break;
            }
          } else {
            _2079 = _1617;
          }
        } else {
          _2079 = _1617;
        }
      } else {
        _2079 = 1.0f;
      }
      if ((int)(_2079 > 0.01666666753590107f) && (int)(_2079 < 1.0f)) {
        if (InjectionToggle(TOGGLE_USE_ISFAST_SHADOWS)) {
          float _isfast_scalar = ISFASTNoise.Load(int4(int2(uint(_62) % 128u, uint(_64) % 128u), uint(float(InjectionFrameIndex())) % 32u, 0)).x;
          _2105 = saturate(((_isfast_scalar + -0.5f) * 0.06666667014360428f) + _2079);
        } else {
          _2105 = saturate((((((float4)(BlueNoise_ScalarTexture.Load(int3((BlueNoise.ModuloMasks.x & _62), ((int)(((BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y) + ((uint)(BlueNoise.ModuloMasks.y & _64)))), 0)))).x) + -0.5f) * 0.06666667014360428f) + _2079);
        }
      } else {
        _2105 = _2079;
      }
      // Debug: amplified difference (second path)
      if ((InjectionEnum(ENUM_DEBUG_SHADOWS_SHIFT) > 0u)) {
        float _dbg_isfast_scalar = ISFASTNoise.Load(int4(int2(uint(_62) % 128u, uint(_64) % 128u), uint(float(InjectionFrameIndex())) % 32u, 0)).x;
        float _dbg_isfast_result = saturate(((_dbg_isfast_scalar + -0.5f) * 0.06666667014360428f) + _2079);
        float _dbg_bn_result = saturate((((((float4)(BlueNoise_ScalarTexture.Load(int3((BlueNoise.ModuloMasks.x & _62), ((int)(((BlueNoise.ModuloMasks.z & View.StateFrameIndex) * BlueNoise.Dimensions.y) + ((uint)(BlueNoise.ModuloMasks.y & _64)))), 0)))).x) + -0.5f) * 0.06666667014360428f) + _2079);
        float _dbg_diff = saturate(abs(_dbg_isfast_result - _dbg_bn_result) * 50.0f);
        OutShadowFactor[int2(_62, _64)] = float2(1.0f - _dbg_diff, 1.0f - _dbg_diff);
      } else {
        OutShadowFactor[int2(_62, _64)] = float2(_2105, _2105);
      }
    }
  }
}