;
; Note: shader requires additional functionality:
;       Wave level operations
;
;
; Input signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
;
; Output signature:
;
; Name                 Index   Mask Register SysValue  Format   Used
; -------------------- ----- ------ -------- -------- ------- ------
; no parameters
; shader hash: 639e69482934f2b2816ba6da7d318605
;
; Pipeline Runtime Information: 
;
;PSVRuntimeInfo:
; Compute Shader
; NumThreads=(8,8,1)
; MinimumExpectedWaveLaneCount: 0
; MaximumExpectedWaveLaneCount: 4294967295
; UsesViewID: false
; SigInputElements: 0
; SigOutputElements: 0
; SigPatchConstOrPrimElements: 0
; SigInputVectors: 0
; SigOutputVectors[0]: 0
; SigOutputVectors[1]: 0
; SigOutputVectors[2]: 0
; SigOutputVectors[3]: 0
;
;
; Buffer Definitions:
;
; cbuffer _RootShaderParameters
; {
;
;   struct _RootShaderParameters
;   {
;
;       float ScreenRayLength;                        ; Offset:  112
;       int SMRTRayCount;                             ; Offset:  116
;       int SMRTSamplesPerRay;                        ; Offset:  120
;       float SMRTCotMaxRayAngleFromLight;            ; Offset:  128
;       float SMRTTexelDitherScale;                   ; Offset:  132
;       float SMRTExtrapolateSlope;                   ; Offset:  136
;       float SMRTMaxSlopeBias;                       ; Offset:  140
;       uint SMRTAdaptiveRayCount;                    ; Offset:  144
;       int4 ProjectionRect;                          ; Offset:  160
;       float NormalBias;                             ; Offset:  176
;       uint InputType;                               ; Offset:  184
;       uint bCullBackfacingPixels;                   ; Offset:  188
;       int VisualizeVirtualShadowMapId;              ; Offset:  380
;   
;   } _RootShaderParameters;                          ; Offset:    0 Size:   384
;
; }
;
; cbuffer View
; {
;
;   struct hostlayout.View
;   {
;
;       struct hostlayout.struct.FViewConstants
;       {
;
;           row_major float4x4 TranslatedWorldToClip; ; Offset:    0
;           row_major float4x4 RelativeWorldToClip;   ; Offset:   64
;           row_major float4x4 ClipToRelativeWorld;   ; Offset:  128
;           row_major float4x4 TranslatedWorldToView; ; Offset:  192
;           row_major float4x4 ViewToTranslatedWorld; ; Offset:  256
;           row_major float4x4 TranslatedWorldToCameraView;; Offset:  320
;           row_major float4x4 CameraViewToTranslatedWorld;; Offset:  384
;           row_major float4x4 ViewToClip;            ; Offset:  448
;           row_major float4x4 ViewToClipNoAA;        ; Offset:  512
;           row_major float4x4 ClipToView;            ; Offset:  576
;           row_major float4x4 ClipToTranslatedWorld; ; Offset:  640
;           row_major float4x4 SVPositionToTranslatedWorld;; Offset:  704
;           row_major float4x4 ScreenToRelativeWorld; ; Offset:  768
;           row_major float4x4 ScreenToTranslatedWorld;; Offset:  832
;           row_major float4x4 MobileMultiviewShadowTransform;; Offset:  896
;           float3 ViewOriginHigh;                    ; Offset:  960
;           float Padding972;                         ; Offset:  972
;           float3 ViewForward;                       ; Offset:  976
;           float Padding988;                         ; Offset:  988
;           float3 ViewUp;                            ; Offset:  992
;           float Padding1004;                        ; Offset: 1004
;           float3 ViewRight;                         ; Offset: 1008
;           float Padding1020;                        ; Offset: 1020
;           float3 HMDViewNoRollUp;                   ; Offset: 1024
;           float Padding1036;                        ; Offset: 1036
;           float3 HMDViewNoRollRight;                ; Offset: 1040
;           float Padding1052;                        ; Offset: 1052
;           float4 InvDeviceZToWorldZTransform;       ; Offset: 1056
;           float4 ScreenPositionScaleBias;           ; Offset: 1072
;           float3 ViewOriginLow;                     ; Offset: 1088
;           float Padding1100;                        ; Offset: 1100
;           float3 TranslatedWorldCameraOrigin;       ; Offset: 1104
;           float Padding1116;                        ; Offset: 1116
;           float3 WorldViewOriginHigh;               ; Offset: 1120
;           float Padding1132;                        ; Offset: 1132
;           float3 WorldViewOriginLow;                ; Offset: 1136
;           float Padding1148;                        ; Offset: 1148
;           float3 PreViewTranslationHigh;            ; Offset: 1152
;           float Padding1164;                        ; Offset: 1164
;           float3 PreViewTranslationLow;             ; Offset: 1168
;           float Padding1180;                        ; Offset: 1180
;           row_major float4x4 PrevViewToClip;        ; Offset: 1184
;           row_major float4x4 PrevClipToView;        ; Offset: 1248
;           row_major float4x4 PrevTranslatedWorldToClip;; Offset: 1312
;           row_major float4x4 PrevTranslatedWorldToView;; Offset: 1376
;           row_major float4x4 PrevViewToTranslatedWorld;; Offset: 1440
;           row_major float4x4 PrevTranslatedWorldToCameraView;; Offset: 1504
;           row_major float4x4 PrevCameraViewToTranslatedWorld;; Offset: 1568
;           float3 PrevTranslatedWorldCameraOrigin;   ; Offset: 1632
;           float Padding1644;                        ; Offset: 1644
;           float3 PrevWorldCameraOriginHigh;         ; Offset: 1648
;           float Padding1660;                        ; Offset: 1660
;           float3 PrevWorldCameraOriginLow;          ; Offset: 1664
;           float Padding1676;                        ; Offset: 1676
;           float3 PrevWorldViewOriginHigh;           ; Offset: 1680
;           float Padding1692;                        ; Offset: 1692
;           float3 PrevWorldViewOriginLow;            ; Offset: 1696
;           float Padding1708;                        ; Offset: 1708
;           float3 PrevPreViewTranslationHigh;        ; Offset: 1712
;           float Padding1724;                        ; Offset: 1724
;           float3 PrevPreViewTranslationLow;         ; Offset: 1728
;           float Padding1740;                        ; Offset: 1740
;           float3 ViewTilePosition;                  ; Offset: 1744
;           float Padding1756;                        ; Offset: 1756
;           float3 RelativeWorldCameraOriginTO;       ; Offset: 1760
;           float Padding1772;                        ; Offset: 1772
;           float3 RelativeWorldViewOriginTO;         ; Offset: 1776
;           float Padding1788;                        ; Offset: 1788
;           float3 RelativePreViewTranslationTO;      ; Offset: 1792
;           float Padding1804;                        ; Offset: 1804
;           float3 PrevRelativeWorldCameraOriginTO;   ; Offset: 1808
;           float Padding1820;                        ; Offset: 1820
;           float3 PrevRelativeWorldViewOriginTO;     ; Offset: 1824
;           float Padding1836;                        ; Offset: 1836
;           float3 RelativePrevPreViewTranslationTO;  ; Offset: 1840
;           float Padding1852;                        ; Offset: 1852
;           row_major float4x4 PrevClipToRelativeWorld;; Offset: 1856
;           row_major float4x4 PrevScreenToTranslatedWorld;; Offset: 1920
;           row_major float4x4 ClipToPrevClip;        ; Offset: 1984
;           row_major float4x4 ClipToPrevClipWithAA;  ; Offset: 2048
;           float4 TemporalAAJitter;                  ; Offset: 2112
;           float4 GlobalClippingPlane;               ; Offset: 2128
;           float2 FieldOfViewWideAngles;             ; Offset: 2144
;           float2 PrevFieldOfViewWideAngles;         ; Offset: 2152
;           float4 ViewRectMin;                       ; Offset: 2160
;           float4 ViewSizeAndInvSize;                ; Offset: 2176
;           uint4 ViewRectMinAndSize;                 ; Offset: 2192
;           float4 LightProbeSizeRatioAndInvSizeRatio;; Offset: 2208
;           float4 BufferSizeAndInvSize;              ; Offset: 2224
;           float4 BufferBilinearUVMinMax;            ; Offset: 2240
;           float4 ScreenToViewSpace;                 ; Offset: 2256
;           float2 BufferToSceneTextureScale;         ; Offset: 2272
;           float2 ResolutionFractionAndInv;          ; Offset: 2280
;           int NumSceneColorMSAASamples;             ; Offset: 2288
;           float ProjectionDepthThicknessScale;      ; Offset: 2292
;           float PreExposure;                        ; Offset: 2296
;           float OneOverPreExposure;                 ; Offset: 2300
;           float4 DiffuseOverrideParameter;          ; Offset: 2304
;           float4 SpecularOverrideParameter;         ; Offset: 2320
;           float4 NormalOverrideParameter;           ; Offset: 2336
;           float2 RoughnessOverrideParameter;        ; Offset: 2352
;           float PrevFrameGameTime;                  ; Offset: 2360
;           float PrevFrameRealTime;                  ; Offset: 2364
;           float OutOfBoundsMask;                    ; Offset: 2368
;           float Padding2372;                        ; Offset: 2372
;           float Padding2376;                        ; Offset: 2376
;           float Padding2380;                        ; Offset: 2380
;           float3 WorldCameraMovementSinceLastFrame; ; Offset: 2384
;           float CullingSign;                        ; Offset: 2396
;           float NearPlane;                          ; Offset: 2400
;           float GameTime;                           ; Offset: 2404
;           float RealTime;                           ; Offset: 2408
;           float DeltaTime;                          ; Offset: 2412
;           float MaterialTextureMipBias;             ; Offset: 2416
;           float MaterialTextureDerivativeMultiply;  ; Offset: 2420
;           uint Random;                              ; Offset: 2424
;           uint FrameNumber;                         ; Offset: 2428
;           uint FrameCounter;                        ; Offset: 2432
;           uint StateFrameIndexMod8;                 ; Offset: 2436
;           uint StateFrameIndex;                     ; Offset: 2440
;           uint DebugViewModeMask;                   ; Offset: 2444
;           uint WorldIsPaused;                       ; Offset: 2448
;           float CameraCut;                          ; Offset: 2452
;           float UnlitViewmodeMask;                  ; Offset: 2456
;           float Padding2460;                        ; Offset: 2460
;           float4 DirectionalLightColor;             ; Offset: 2464
;           float3 DirectionalLightDirection;         ; Offset: 2480
;           float Padding2492;                        ; Offset: 2492
;           float4 TranslucencyLightingVolumeMin[2];  ; Offset: 2496
;           float4 TranslucencyLightingVolumeInvSize[2];; Offset: 2528
;           float4 TemporalAAParams;                  ; Offset: 2560
;           float4 CircleDOFParams;                   ; Offset: 2576
;           float DepthOfFieldSensorWidth;            ; Offset: 2592
;           float DepthOfFieldFocalDistance;          ; Offset: 2596
;           float DepthOfFieldScale;                  ; Offset: 2600
;           float DepthOfFieldFocalLength;            ; Offset: 2604
;           float DepthOfFieldFocalRegion;            ; Offset: 2608
;           float DepthOfFieldNearTransitionRegion;   ; Offset: 2612
;           float DepthOfFieldFarTransitionRegion;    ; Offset: 2616
;           float MotionBlurNormalizedToPixel;        ; Offset: 2620
;           float GeneralPurposeTweak;                ; Offset: 2624
;           float GeneralPurposeTweak2;               ; Offset: 2628
;           float DemosaicVposOffset;                 ; Offset: 2632
;           float DecalDepthBias;                     ; Offset: 2636
;           float3 IndirectLightingColorScale;        ; Offset: 2640
;           float Padding2652;                        ; Offset: 2652
;           float3 PrecomputedIndirectLightingColorScale;; Offset: 2656
;           float Padding2668;                        ; Offset: 2668
;           float3 PrecomputedIndirectSpecularColorScale;; Offset: 2672
;           float Padding2684;                        ; Offset: 2684
;           float4 AtmosphereLightDirection[2];       ; Offset: 2688
;           float4 AtmosphereLightIlluminanceOnGroundPostTransmittance[2];; Offset: 2720
;           float4 AtmosphereLightIlluminanceOuterSpace[2];; Offset: 2752
;           float4 AtmosphereLightDiscLuminance[2];   ; Offset: 2784
;           float4 AtmosphereLightDiscCosHalfApexAngle_PPTrans[2];; Offset: 2816
;           float4 SkyViewLutSizeAndInvSize;          ; Offset: 2848
;           float3 SkyCameraTranslatedWorldOrigin;    ; Offset: 2864
;           float Padding2876;                        ; Offset: 2876
;           float4 SkyPlanetTranslatedWorldCenterAndViewHeight;; Offset: 2880
;           row_major float4x4 SkyViewLutReferential; ; Offset: 2896
;           float4 SkyAtmosphereSkyLuminanceFactor;   ; Offset: 2960
;           float SkyAtmospherePresentInScene;        ; Offset: 2976
;           float SkyAtmosphereHeightFogContribution; ; Offset: 2980
;           float SkyAtmosphereBottomRadiusKm;        ; Offset: 2984
;           float SkyAtmosphereTopRadiusKm;           ; Offset: 2988
;           float4 SkyAtmosphereCameraAerialPerspectiveVolumeSizeAndInvSize;; Offset: 2992
;           float SkyAtmosphereAerialPerspectiveStartDepthKm;; Offset: 3008
;           float SkyAtmosphereCameraAerialPerspectiveVolumeDepthResolution;; Offset: 3012
;           float SkyAtmosphereCameraAerialPerspectiveVolumeDepthResolutionInv;; Offset: 3016
;           float SkyAtmosphereCameraAerialPerspectiveVolumeDepthSliceLengthKm;; Offset: 3020
;           float SkyAtmosphereCameraAerialPerspectiveVolumeDepthSliceLengthKmInv;; Offset: 3024
;           float SkyAtmosphereApplyCameraAerialPerspectiveVolume;; Offset: 3028
;           float Padding3032;                        ; Offset: 3032
;           float Padding3036;                        ; Offset: 3036
;           float3 NormalCurvatureToRoughnessScaleBias;; Offset: 3040
;           float RenderingReflectionCaptureMask;     ; Offset: 3052
;           float RealTimeReflectionCapture;          ; Offset: 3056
;           float RealTimeReflectionCapturePreExposure;; Offset: 3060
;           float Padding3064;                        ; Offset: 3064
;           float Padding3068;                        ; Offset: 3068
;           float4 AmbientCubemapTint;                ; Offset: 3072
;           float AmbientCubemapIntensity;            ; Offset: 3088
;           float SkyLightApplyPrecomputedBentNormalShadowingFlag;; Offset: 3092
;           float SkyLightAffectReflectionFlag;       ; Offset: 3096
;           float SkyLightAffectGlobalIlluminationFlag;; Offset: 3100
;           float4 SkyLightColor;                     ; Offset: 3104
;           float SkyLightVolumetricScatteringIntensity;; Offset: 3120
;           float Padding3124;                        ; Offset: 3124
;           float Padding3128;                        ; Offset: 3128
;           float Padding3132;                        ; Offset: 3132
;           float4 MobileSkyIrradianceEnvironmentMap[8];; Offset: 3136
;           float MobilePreviewMode;                  ; Offset: 3264
;           float HMDEyePaddingOffset;                ; Offset: 3268
;           float ReflectionCubemapMaxMip;            ; Offset: 3272
;           float ShowDecalsMask;                     ; Offset: 3276
;           uint DistanceFieldAOSpecularOcclusionMode;; Offset: 3280
;           float IndirectCapsuleSelfShadowingIntensity;; Offset: 3284
;           float Padding3288;                        ; Offset: 3288
;           float Padding3292;                        ; Offset: 3292
;           float3 ReflectionEnvironmentRoughnessMixingScaleBiasAndLargestWeight;; Offset: 3296
;           int StereoPassIndex;                      ; Offset: 3308
;           float4 GlobalVolumeTranslatedCenterAndExtent[6];; Offset: 3312
;           float4 GlobalVolumeTranslatedWorldToUVAddAndMul[6];; Offset: 3408
;           float4 GlobalDistanceFieldMipTranslatedWorldToUVScale[6];; Offset: 3504
;           float4 GlobalDistanceFieldMipTranslatedWorldToUVBias[6];; Offset: 3600
;           float GlobalDistanceFieldMipFactor;       ; Offset: 3696
;           float GlobalDistanceFieldMipTransition;   ; Offset: 3700
;           int GlobalDistanceFieldClipmapSizeInPages;; Offset: 3704
;           int Padding3708;                          ; Offset: 3708
;           float3 GlobalDistanceFieldInvPageAtlasSize;; Offset: 3712
;           float Padding3724;                        ; Offset: 3724
;           float3 GlobalDistanceFieldInvCoverageAtlasSize;; Offset: 3728
;           float GlobalVolumeDimension;              ; Offset: 3740
;           float GlobalVolumeTexelSize;              ; Offset: 3744
;           float MaxGlobalDFAOConeDistance;          ; Offset: 3748
;           uint NumGlobalSDFClipmaps;                ; Offset: 3752
;           float CoveredExpandSurfaceScale;          ; Offset: 3756
;           float NotCoveredExpandSurfaceScale;       ; Offset: 3760
;           float NotCoveredMinStepScale;             ; Offset: 3764
;           float DitheredTransparencyStepThreshold;  ; Offset: 3768
;           float DitheredTransparencyTraceThreshold; ; Offset: 3772
;           int2 CursorPosition;                      ; Offset: 3776
;           float bCheckerboardSubsurfaceProfileRendering;; Offset: 3784
;           float Padding3788;                        ; Offset: 3788
;           float3 VolumetricFogInvGridSize;          ; Offset: 3792
;           float Padding3804;                        ; Offset: 3804
;           float3 VolumetricFogGridZParams;          ; Offset: 3808
;           float Padding3820;                        ; Offset: 3820
;           float2 VolumetricFogSVPosToVolumeUV;      ; Offset: 3824
;           float2 VolumetricFogViewGridUVToPrevViewRectUV;; Offset: 3832
;           float2 VolumetricFogPrevViewGridRectUVToResourceUV;; Offset: 3840
;           float2 VolumetricFogPrevUVMax;            ; Offset: 3848
;           float2 VolumetricFogPrevUVMaxForTemporalBlend;; Offset: 3856
;           float2 VolumetricFogScreenToResourceUV;   ; Offset: 3864
;           float2 VolumetricFogUVMax;                ; Offset: 3872
;           float VolumetricFogMaxDistance;           ; Offset: 3880
;           float Padding3884;                        ; Offset: 3884
;           float3 VolumetricLightmapWorldToUVScale;  ; Offset: 3888
;           float Padding3900;                        ; Offset: 3900
;           float3 VolumetricLightmapWorldToUVAdd;    ; Offset: 3904
;           float Padding3916;                        ; Offset: 3916
;           float3 VolumetricLightmapIndirectionTextureSize;; Offset: 3920
;           float VolumetricLightmapBrickSize;        ; Offset: 3932
;           float3 VolumetricLightmapBrickTexelSize;  ; Offset: 3936
;           float IndirectLightingCacheShowFlag;      ; Offset: 3948
;           float EyeToPixelSpreadAngle;              ; Offset: 3952
;           float Padding3956;                        ; Offset: 3956
;           float Padding3960;                        ; Offset: 3960
;           float Padding3964;                        ; Offset: 3964
;           float4 XRPassthroughCameraUVs[2];         ; Offset: 3968
;           float GlobalVirtualTextureMipBias;        ; Offset: 4000
;           uint VirtualTextureFeedbackShift;         ; Offset: 4004
;           uint VirtualTextureFeedbackMask;          ; Offset: 4008
;           uint VirtualTextureFeedbackStride;        ; Offset: 4012
;           uint VirtualTextureFeedbackJitterOffset;  ; Offset: 4016
;           uint VirtualTextureFeedbackSampleOffset;  ; Offset: 4020
;           uint Padding4024;                         ; Offset: 4024
;           uint Padding4028;                         ; Offset: 4028
;           float4 RuntimeVirtualTextureMipLevel;     ; Offset: 4032
;           float2 RuntimeVirtualTexturePackHeight;   ; Offset: 4048
;           float Padding4056;                        ; Offset: 4056
;           float Padding4060;                        ; Offset: 4060
;           float4 RuntimeVirtualTextureDebugParams;  ; Offset: 4064
;           int FarShadowStaticMeshLODBias;           ; Offset: 4080
;           float MinRoughness;                       ; Offset: 4084
;           float Padding4088;                        ; Offset: 4088
;           float Padding4092;                        ; Offset: 4092
;           float4 HairRenderInfo;                    ; Offset: 4096
;           uint EnableSkyLight;                      ; Offset: 4112
;           uint HairRenderInfoBits;                  ; Offset: 4116
;           uint HairComponents;                      ; Offset: 4120
;           float bSubsurfacePostprocessEnabled;      ; Offset: 4124
;           float4 SSProfilesTextureSizeAndInvSize;   ; Offset: 4128
;           float4 SSProfilesPreIntegratedTextureSizeAndInvSize;; Offset: 4144
;           float4 SpecularProfileTextureSizeAndInvSize;; Offset: 4160
;           float3 PhysicsFieldClipmapCenter;         ; Offset: 4176
;           float PhysicsFieldClipmapDistance;        ; Offset: 4188
;           int PhysicsFieldClipmapResolution;        ; Offset: 4192
;           int PhysicsFieldClipmapExponent;          ; Offset: 4196
;           int PhysicsFieldClipmapCount;             ; Offset: 4200
;           int PhysicsFieldTargetCount;              ; Offset: 4204
;           int4 PhysicsFieldTargets[32];             ; Offset: 4208
;           uint GPUSceneViewId;                      ; Offset: 4720
;           float ViewResolutionFraction;             ; Offset: 4724
;           float SubSurfaceColorAsTransmittanceAtDistanceInMeters;; Offset: 4728
;           float Padding4732;                        ; Offset: 4732
;           float4 TanAndInvTanHalfFOV;               ; Offset: 4736
;           float4 PrevTanAndInvTanHalfFOV;           ; Offset: 4752
;           float2 WorldDepthToPixelWorldRadius;      ; Offset: 4768
;           float Padding4776;                        ; Offset: 4776
;           float Padding4780;                        ; Offset: 4780
;           float4 ScreenRayLengthMultiplier;         ; Offset: 4784
;           float4 GlintLUTParameters0;               ; Offset: 4800
;           float4 GlintLUTParameters1;               ; Offset: 4816
;           int4 EnvironmentComponentsFlags;          ; Offset: 4832
;           uint BindlessSampler_MaterialTextureBilinearWrapedSampler;; Offset: 4848
;           uint Padding4852;                         ; Offset: 4852
;           uint BindlessSampler_MaterialTextureBilinearClampedSampler;; Offset: 4856
;           uint Padding4860;                         ; Offset: 4860
;           uint BindlessSRV_VolumetricLightmapIndirectionTexture;; Offset: 4864
;           uint Padding4868;                         ; Offset: 4868
;           uint BindlessSRV_VolumetricLightmapBrickAmbientVector;; Offset: 4872
;           uint Padding4876;                         ; Offset: 4876
;           uint BindlessSRV_VolumetricLightmapBrickSHCoefficients0;; Offset: 4880
;           uint Padding4884;                         ; Offset: 4884
;           uint BindlessSRV_VolumetricLightmapBrickSHCoefficients1;; Offset: 4888
;           uint Padding4892;                         ; Offset: 4892
;           uint BindlessSRV_VolumetricLightmapBrickSHCoefficients2;; Offset: 4896
;           uint Padding4900;                         ; Offset: 4900
;           uint BindlessSRV_VolumetricLightmapBrickSHCoefficients3;; Offset: 4904
;           uint Padding4908;                         ; Offset: 4908
;           uint BindlessSRV_VolumetricLightmapBrickSHCoefficients4;; Offset: 4912
;           uint Padding4916;                         ; Offset: 4916
;           uint BindlessSRV_VolumetricLightmapBrickSHCoefficients5;; Offset: 4920
;           uint Padding4924;                         ; Offset: 4924
;           uint BindlessSRV_SkyBentNormalBrickTexture;; Offset: 4928
;           uint Padding4932;                         ; Offset: 4932
;           uint BindlessSRV_DirectionalLightShadowingBrickTexture;; Offset: 4936
;           uint Padding4940;                         ; Offset: 4940
;           uint BindlessSampler_VolumetricLightmapBrickAmbientVectorSampler;; Offset: 4944
;           uint Padding4948;                         ; Offset: 4948
;           uint BindlessSampler_VolumetricLightmapTextureSampler0;; Offset: 4952
;           uint Padding4956;                         ; Offset: 4956
;           uint BindlessSampler_VolumetricLightmapTextureSampler1;; Offset: 4960
;           uint Padding4964;                         ; Offset: 4964
;           uint BindlessSampler_VolumetricLightmapTextureSampler2;; Offset: 4968
;           uint Padding4972;                         ; Offset: 4972
;           uint BindlessSampler_VolumetricLightmapTextureSampler3;; Offset: 4976
;           uint Padding4980;                         ; Offset: 4980
;           uint BindlessSampler_VolumetricLightmapTextureSampler4;; Offset: 4984
;           uint Padding4988;                         ; Offset: 4988
;           uint BindlessSampler_VolumetricLightmapTextureSampler5;; Offset: 4992
;           uint Padding4996;                         ; Offset: 4996
;           uint BindlessSampler_SkyBentNormalTextureSampler;; Offset: 5000
;           uint Padding5004;                         ; Offset: 5004
;           uint BindlessSampler_DirectionalLightShadowingTextureSampler;; Offset: 5008
;           uint Padding5012;                         ; Offset: 5012
;           uint BindlessSRV_GlobalDistanceFieldPageAtlasTexture;; Offset: 5016
;           uint Padding5020;                         ; Offset: 5020
;           uint BindlessSRV_GlobalDistanceFieldCoverageAtlasTexture;; Offset: 5024
;           uint Padding5028;                         ; Offset: 5028
;           uint BindlessSRV_GlobalDistanceFieldPageTableTexture;; Offset: 5032
;           uint Padding5036;                         ; Offset: 5036
;           uint BindlessSRV_GlobalDistanceFieldMipTexture;; Offset: 5040
;           uint Padding5044;                         ; Offset: 5044
;           uint BindlessSampler_GlobalDistanceFieldPageAtlasTextureSampler;; Offset: 5048
;           uint Padding5052;                         ; Offset: 5052
;           uint BindlessSampler_GlobalDistanceFieldCoverageAtlasTextureSampler;; Offset: 5056
;           uint Padding5060;                         ; Offset: 5060
;           uint BindlessSampler_GlobalDistanceFieldMipTextureSampler;; Offset: 5064
;           uint Padding5068;                         ; Offset: 5068
;           uint BindlessSRV_AtmosphereTransmittanceTexture;; Offset: 5072
;           uint Padding5076;                         ; Offset: 5076
;           uint BindlessSampler_AtmosphereTransmittanceTextureSampler;; Offset: 5080
;           uint Padding5084;                         ; Offset: 5084
;           uint BindlessSRV_AtmosphereIrradianceTexture;; Offset: 5088
;           uint Padding5092;                         ; Offset: 5092
;           uint BindlessSampler_AtmosphereIrradianceTextureSampler;; Offset: 5096
;           uint Padding5100;                         ; Offset: 5100
;           uint BindlessSRV_AtmosphereInscatterTexture;; Offset: 5104
;           uint Padding5108;                         ; Offset: 5108
;           uint BindlessSampler_AtmosphereInscatterTextureSampler;; Offset: 5112
;           uint Padding5116;                         ; Offset: 5116
;           uint BindlessSRV_PerlinNoiseGradientTexture;; Offset: 5120
;           uint Padding5124;                         ; Offset: 5124
;           uint BindlessSampler_PerlinNoiseGradientTextureSampler;; Offset: 5128
;           uint Padding5132;                         ; Offset: 5132
;           uint BindlessSRV_PerlinNoise3DTexture;    ; Offset: 5136
;           uint Padding5140;                         ; Offset: 5140
;           uint BindlessSampler_PerlinNoise3DTextureSampler;; Offset: 5144
;           uint Padding5148;                         ; Offset: 5148
;           uint BindlessSRV_SobolSamplingTexture;    ; Offset: 5152
;           uint Padding5156;                         ; Offset: 5156
;           uint BindlessSampler_SharedPointWrappedSampler;; Offset: 5160
;           uint Padding5164;                         ; Offset: 5164
;           uint BindlessSampler_SharedPointClampedSampler;; Offset: 5168
;           uint Padding5172;                         ; Offset: 5172
;           uint BindlessSampler_SharedBilinearWrappedSampler;; Offset: 5176
;           uint Padding5180;                         ; Offset: 5180
;           uint BindlessSampler_SharedBilinearClampedSampler;; Offset: 5184
;           uint Padding5188;                         ; Offset: 5188
;           uint BindlessSampler_SharedBilinearAnisoClampedSampler;; Offset: 5192
;           uint Padding5196;                         ; Offset: 5196
;           uint BindlessSampler_SharedTrilinearWrappedSampler;; Offset: 5200
;           uint Padding5204;                         ; Offset: 5204
;           uint BindlessSampler_SharedTrilinearClampedSampler;; Offset: 5208
;           uint Padding5212;                         ; Offset: 5212
;           uint BindlessSRV_PreIntegratedBRDF;       ; Offset: 5216
;           uint Padding5220;                         ; Offset: 5220
;           uint BindlessSampler_PreIntegratedBRDFSampler;; Offset: 5224
;           uint Padding5228;                         ; Offset: 5228
;           uint BindlessSRV_SkyIrradianceEnvironmentMap;; Offset: 5232
;           uint Padding5236;                         ; Offset: 5236
;           uint BindlessSRV_TransmittanceLutTexture; ; Offset: 5240
;           uint Padding5244;                         ; Offset: 5244
;           uint BindlessSampler_TransmittanceLutTextureSampler;; Offset: 5248
;           uint Padding5252;                         ; Offset: 5252
;           uint BindlessSRV_SkyViewLutTexture;       ; Offset: 5256
;           uint Padding5260;                         ; Offset: 5260
;           uint BindlessSampler_SkyViewLutTextureSampler;; Offset: 5264
;           uint Padding5268;                         ; Offset: 5268
;           uint BindlessSRV_DistantSkyLightLutTexture;; Offset: 5272
;           uint Padding5276;                         ; Offset: 5276
;           uint BindlessSampler_DistantSkyLightLutTextureSampler;; Offset: 5280
;           uint Padding5284;                         ; Offset: 5284
;           uint BindlessSRV_CameraAerialPerspectiveVolume;; Offset: 5288
;           uint Padding5292;                         ; Offset: 5292
;           uint BindlessSampler_CameraAerialPerspectiveVolumeSampler;; Offset: 5296
;           uint Padding5300;                         ; Offset: 5300
;           uint BindlessSRV_CameraAerialPerspectiveVolumeMieOnly;; Offset: 5304
;           uint Padding5308;                         ; Offset: 5308
;           uint BindlessSampler_CameraAerialPerspectiveVolumeMieOnlySampler;; Offset: 5312
;           uint Padding5316;                         ; Offset: 5316
;           uint BindlessSRV_CameraAerialPerspectiveVolumeRayOnly;; Offset: 5320
;           uint Padding5324;                         ; Offset: 5324
;           uint BindlessSampler_CameraAerialPerspectiveVolumeRayOnlySampler;; Offset: 5328
;           uint Padding5332;                         ; Offset: 5332
;           uint BindlessSRV_HairScatteringLUTTexture;; Offset: 5336
;           uint Padding5340;                         ; Offset: 5340
;           uint BindlessSampler_HairScatteringLUTSampler;; Offset: 5344
;           uint Padding5348;                         ; Offset: 5348
;           uint BindlessSRV_GGXLTCMatTexture;        ; Offset: 5352
;           uint Padding5356;                         ; Offset: 5356
;           uint BindlessSampler_GGXLTCMatSampler;    ; Offset: 5360
;           uint Padding5364;                         ; Offset: 5364
;           uint BindlessSRV_GGXLTCAmpTexture;        ; Offset: 5368
;           uint Padding5372;                         ; Offset: 5372
;           uint BindlessSampler_GGXLTCAmpSampler;    ; Offset: 5376
;           uint Padding5380;                         ; Offset: 5380
;           uint BindlessSRV_SheenLTCTexture;         ; Offset: 5384
;           uint Padding5388;                         ; Offset: 5388
;           uint BindlessSampler_SheenLTCSampler;     ; Offset: 5392
;           uint Padding5396;                         ; Offset: 5396
;           uint bShadingEnergyConservation;          ; Offset: 5400
;           uint bShadingEnergyPreservation;          ; Offset: 5404
;           uint BindlessSRV_ShadingEnergyGGXSpecTexture;; Offset: 5408
;           uint Padding5412;                         ; Offset: 5412
;           uint BindlessSRV_ShadingEnergyGGXGlassTexture;; Offset: 5416
;           uint Padding5420;                         ; Offset: 5420
;           uint BindlessSRV_ShadingEnergyClothSpecTexture;; Offset: 5424
;           uint Padding5428;                         ; Offset: 5428
;           uint BindlessSRV_ShadingEnergyDiffuseTexture;; Offset: 5432
;           uint Padding5436;                         ; Offset: 5436
;           uint BindlessSampler_ShadingEnergySampler;; Offset: 5440
;           uint Padding5444;                         ; Offset: 5444
;           uint BindlessSRV_GlintTexture;            ; Offset: 5448
;           uint Padding5452;                         ; Offset: 5452
;           uint BindlessSampler_GlintSampler;        ; Offset: 5456
;           uint Padding5460;                         ; Offset: 5460
;           uint BindlessSRV_SimpleVolumeTexture;     ; Offset: 5464
;           uint Padding5468;                         ; Offset: 5468
;           uint BindlessSampler_SimpleVolumeTextureSampler;; Offset: 5472
;           uint Padding5476;                         ; Offset: 5476
;           uint BindlessSRV_SimpleVolumeEnvTexture;  ; Offset: 5480
;           uint Padding5484;                         ; Offset: 5484
;           uint BindlessSampler_SimpleVolumeEnvTextureSampler;; Offset: 5488
;           uint Padding5492;                         ; Offset: 5492
;           uint BindlessSRV_SSProfilesTexture;       ; Offset: 5496
;           uint Padding5500;                         ; Offset: 5500
;           uint BindlessSampler_SSProfilesSampler;   ; Offset: 5504
;           uint Padding5508;                         ; Offset: 5508
;           uint BindlessSampler_SSProfilesTransmissionSampler;; Offset: 5512
;           uint Padding5516;                         ; Offset: 5516
;           uint BindlessSRV_SSProfilesPreIntegratedTexture;; Offset: 5520
;           uint Padding5524;                         ; Offset: 5524
;           uint BindlessSampler_SSProfilesPreIntegratedSampler;; Offset: 5528
;           uint Padding5532;                         ; Offset: 5532
;           uint BindlessSRV_SpecularProfileTexture;  ; Offset: 5536
;           uint Padding5540;                         ; Offset: 5540
;           uint BindlessSampler_SpecularProfileSampler;; Offset: 5544
;           uint Padding5548;                         ; Offset: 5548
;           uint BindlessSRV_WaterIndirection;        ; Offset: 5552
;           uint Padding5556;                         ; Offset: 5556
;           uint BindlessSRV_WaterData;               ; Offset: 5560
;           uint Padding5564;                         ; Offset: 5564
;           float4 RectLightAtlasSizeAndInvSize;      ; Offset: 5568
;           float RectLightAtlasMaxMipLevel;          ; Offset: 5584
;           float Padding5588;                        ; Offset: 5588
;           uint BindlessSRV_RectLightAtlasTexture;   ; Offset: 5592
;           uint Padding5596;                         ; Offset: 5596
;           uint BindlessSampler_RectLightAtlasSampler;; Offset: 5600
;           uint Padding5604;                         ; Offset: 5604
;           uint Padding5608;                         ; Offset: 5608
;           uint Padding5612;                         ; Offset: 5612
;           float4 IESAtlasSizeAndInvSize;            ; Offset: 5616
;           uint BindlessSRV_IESAtlasTexture;         ; Offset: 5632
;           uint Padding5636;                         ; Offset: 5636
;           uint BindlessSampler_IESAtlasSampler;     ; Offset: 5640
;           uint Padding5644;                         ; Offset: 5644
;           uint BindlessSampler_LandscapeWeightmapSampler;; Offset: 5648
;           uint Padding5652;                         ; Offset: 5652
;           uint BindlessSRV_LandscapeIndirection;    ; Offset: 5656
;           uint Padding5660;                         ; Offset: 5660
;           uint BindlessSRV_LandscapePerComponentData;; Offset: 5664
;           uint Padding5668;                         ; Offset: 5668
;           uint BindlessUAV_VTFeedbackBuffer;        ; Offset: 5672
;           uint Padding5676;                         ; Offset: 5676
;           uint BindlessSRV_PhysicsFieldClipmapBuffer;; Offset: 5680
;           uint Padding5684;                         ; Offset: 5684
;           uint Padding5688;                         ; Offset: 5688
;           uint Padding5692;                         ; Offset: 5692
;           float3 TLASPreViewTranslationHigh;        ; Offset: 5696
;           float Padding5708;                        ; Offset: 5708
;           float3 TLASPreViewTranslationLow;         ; Offset: 5712
;       
;       } View;                                       ; Offset:    0
;
;   
;   } View;                                           ; Offset:    0 Size:  5724
;
; }
;
; cbuffer ForwardLightData
; {
;
;   struct hostlayout.ForwardLightData
;   {
;
;       struct hostlayout.struct.FForwardLightDataConstants
;       {
;
;           uint NumLocalLights;                      ; Offset:    0
;           uint NumReflectionCaptures;               ; Offset:    4
;           uint HasDirectionalLight;                 ; Offset:    8
;           uint NumGridCells;                        ; Offset:   12
;           int3 CulledGridSize;                      ; Offset:   16
;           uint MaxCulledLightsPerCell;              ; Offset:   28
;           uint LightGridPixelSizeShift;             ; Offset:   32
;           uint Padding36;                           ; Offset:   36
;           uint Padding40;                           ; Offset:   40
;           uint Padding44;                           ; Offset:   44
;           float3 LightGridZParams;                  ; Offset:   48
;           float Padding60;                          ; Offset:   60
;           float3 DirectionalLightDirection;         ; Offset:   64
;           float DirectionalLightSourceRadius;       ; Offset:   76
;           float DirectionalLightSoftSourceRadius;   ; Offset:   80
;           float Padding84;                          ; Offset:   84
;           float Padding88;                          ; Offset:   88
;           float Padding92;                          ; Offset:   92
;           float3 DirectionalLightColor;             ; Offset:   96
;           float DirectionalLightVolumetricScatteringIntensity;; Offset:  108
;           float DirectionalLightSpecularScale;      ; Offset:  112
;           uint DirectionalLightShadowMapChannelMask;; Offset:  116
;           float2 DirectionalLightDistanceFadeMAD;   ; Offset:  120
;           uint NumDirectionalLightCascades;         ; Offset:  128
;           int DirectionalLightVSM;                  ; Offset:  132
;           int Padding136;                           ; Offset:  136
;           int Padding140;                           ; Offset:  140
;           float4 CascadeEndDepths;                  ; Offset:  144
;           row_major float4x4 DirectionalLightTranslatedWorldToShadowMatrix[4];; Offset:  160
;           float4 DirectionalLightShadowmapMinMax[4];; Offset:  416
;           float4 DirectionalLightShadowmapAtlasBufferSize;; Offset:  480
;           float DirectionalLightDepthBias;          ; Offset:  496
;           uint DirectionalLightUseStaticShadowing;  ; Offset:  500
;           uint SimpleLightsEndIndex;                ; Offset:  504
;           uint ClusteredDeferredSupportedEndIndex;  ; Offset:  508
;           uint ManyLightsSupportedStartIndex;       ; Offset:  512
;           uint Padding516;                          ; Offset:  516
;           uint Padding520;                          ; Offset:  520
;           uint Padding524;                          ; Offset:  524
;           float4 DirectionalLightStaticShadowBufferSize;; Offset:  528
;           row_major float4x4 DirectionalLightTranslatedWorldToStaticShadow;; Offset:  544
;           uint DirectLightingShowFlag;              ; Offset:  608
;           uint LightFunctionAtlasLightIndex;        ; Offset:  612
;           float Padding616;                         ; Offset:  616
;           float Padding620;                         ; Offset:  620
;           float DirectionalLightSMRTSettings_ScreenRayLength;; Offset:  624
;           int DirectionalLightSMRTSettings_SMRTRayCount;; Offset:  628
;           int DirectionalLightSMRTSettings_SMRTSamplesPerRay;; Offset:  632
;           float DirectionalLightSMRTSettings_SMRTRayLengthScale;; Offset:  636
;           float DirectionalLightSMRTSettings_SMRTCotMaxRayAngleFromLight;; Offset:  640
;           float DirectionalLightSMRTSettings_SMRTTexelDitherScale;; Offset:  644
;           float DirectionalLightSMRTSettings_SMRTExtrapolateSlope;; Offset:  648
;           float DirectionalLightSMRTSettings_SMRTMaxSlopeBias;; Offset:  652
;           uint DirectionalLightSMRTSettings_SMRTAdaptiveRayCount;; Offset:  656
;           uint Padding660;                          ; Offset:  660
;           uint Padding664;                          ; Offset:  664
;           uint Padding668;                          ; Offset:  668
;           uint BindlessSRV_DirectionalLightShadowmapAtlas;; Offset:  672
;           uint Padding676;                          ; Offset:  676
;           uint BindlessSampler_ShadowmapSampler;    ; Offset:  680
;           uint Padding684;                          ; Offset:  684
;           uint BindlessSRV_DirectionalLightStaticShadowmap;; Offset:  688
;           uint Padding692;                          ; Offset:  692
;           uint BindlessSampler_StaticShadowmapSampler;; Offset:  696
;           uint Padding700;                          ; Offset:  700
;           uint BindlessSRV_ForwardLocalLightBuffer; ; Offset:  704
;           uint Padding708;                          ; Offset:  708
;           uint BindlessSRV_NumCulledLightsGrid;     ; Offset:  712
;           uint Padding716;                          ; Offset:  716
;           uint BindlessSRV_CulledLightDataGrid32Bit;; Offset:  720
;           uint Padding724;                          ; Offset:  724
;           uint BindlessSRV_CulledLightDataGrid16Bit;; Offset:  728
;       
;       } ForwardLightData;                           ; Offset:    0
;
;   
;   } ForwardLightData;                               ; Offset:    0 Size:   732
;
; }
;
; cbuffer VirtualShadowMap
; {
;
;   struct VirtualShadowMap
;   {
;
;       struct struct.FVirtualShadowMapConstants
;       {
;
;           uint NumFullShadowMaps;                   ; Offset:    0
;           uint NumSinglePageShadowMaps;             ; Offset:    4
;           uint MaxPhysicalPages;                    ; Offset:    8
;           uint NumShadowMapSlots;                   ; Offset:   12
;           uint StaticCachedArrayIndex;              ; Offset:   16
;           uint PhysicalPageRowMask;                 ; Offset:   20
;           uint PhysicalPageRowShift;                ; Offset:   24
;           uint PackedShadowMaskMaxLightCount;       ; Offset:   28
;           float4 RecPhysicalPoolSize;               ; Offset:   32
;           int2 PhysicalPoolSize;                    ; Offset:   48
;           int2 PhysicalPoolSizePages;               ; Offset:   56
;           uint bExcludeNonNaniteFromCoarsePages;    ; Offset:   64
;           float CoarsePagePixelThresholdDynamic;    ; Offset:   68
;           float CoarsePagePixelThresholdStatic;     ; Offset:   72
;           float CoarsePagePixelThresholdDynamicNanite;; Offset:   76
;           uint SceneFrameNumber;                    ; Offset:   80
;           uint bClipmapGreedyLevelSelection;        ; Offset:   84
;           float GlobalResolutionLodBias;            ; Offset:   88
;           float Padding92;                          ; Offset:   92
;           uint BindlessSRV_ProjectionData;          ; Offset:   96
;           uint Padding100;                          ; Offset:  100
;           uint BindlessSRV_PageTable;               ; Offset:  104
;           uint Padding108;                          ; Offset:  108
;           uint BindlessSRV_PageFlags;               ; Offset:  112
;           uint Padding116;                          ; Offset:  116
;           uint BindlessSRV_PageRectBounds;          ; Offset:  120
;           uint Padding124;                          ; Offset:  124
;           uint BindlessSRV_PhysicalPagePool;        ; Offset:  128
;           uint Padding132;                          ; Offset:  132
;           uint BindlessSRV_CachePrimitiveAsDynamic; ; Offset:  136
;           uint Padding140;                          ; Offset:  140
;           uint BindlessSRV_LightGridData;           ; Offset:  144
;           uint Padding148;                          ; Offset:  148
;           uint BindlessSRV_NumCulledLightsGrid;     ; Offset:  152
;       
;       } VirtualShadowMap;                           ; Offset:    0
;
;   
;   } VirtualShadowMap;                               ; Offset:    0 Size:   156
;
; }
;
; cbuffer BlueNoise
; {
;
;   struct BlueNoise
;   {
;
;       struct struct.FBlueNoiseConstants
;       {
;
;           int3 Dimensions;                          ; Offset:    0
;           int Padding12;                            ; Offset:   12
;           int3 ModuloMasks;                         ; Offset:   16
;           int Padding28;                            ; Offset:   28
;           uint BindlessSRV_ScalarTexture;           ; Offset:   32
;           uint Padding36;                           ; Offset:   36
;           uint BindlessSRV_Vec2Texture;             ; Offset:   40
;       
;       } BlueNoise;                                  ; Offset:    0
;
;   
;   } BlueNoise;                                      ; Offset:    0 Size:    44
;
; }
;
; cbuffer VirtualVoxel
; {
;
;   struct VirtualVoxel
;   {
;
;       struct struct.FVirtualVoxelConstants
;       {
;
;           int3 PageCountResolution;                 ; Offset:    0
;           float CPUMinVoxelWorldSize;               ; Offset:   12
;           int3 PageTextureResolution;               ; Offset:   16
;           uint PageCount;                           ; Offset:   28
;           uint PageResolution;                      ; Offset:   32
;           uint PageResolutionLog2;                  ; Offset:   36
;           uint PageIndexCount;                      ; Offset:   40
;           uint IndirectDispatchGroupSize;           ; Offset:   44
;           uint NodeDescCount;                       ; Offset:   48
;           uint JitterMode;                          ; Offset:   52
;           float DensityScale;                       ; Offset:   56
;           float DensityScale_AO;                    ; Offset:   60
;           float DensityScale_Shadow;                ; Offset:   64
;           float DensityScale_Transmittance;         ; Offset:   68
;           float DensityScale_Environment;           ; Offset:   72
;           float DensityScale_Raytracing;            ; Offset:   76
;           float DepthBiasScale_Shadow;              ; Offset:   80
;           float DepthBiasScale_Transmittance;       ; Offset:   84
;           float DepthBiasScale_Environment;         ; Offset:   88
;           float SteppingScale_Shadow;               ; Offset:   92
;           float SteppingScale_Transmittance;        ; Offset:   96
;           float SteppingScale_Environment;          ; Offset:  100
;           float SteppingScale_Raytracing;           ; Offset:  104
;           float HairCoveragePixelRadiusAtDepth1;    ; Offset:  108
;           float Raytracing_ShadowOcclusionThreshold;; Offset:  112
;           float Raytracing_SkyOcclusionThreshold;   ; Offset:  116
;           float Padding120;                         ; Offset:  120
;           float Padding124;                         ; Offset:  124
;           float3 TranslatedWorldOffset;             ; Offset:  128
;           float Padding140;                         ; Offset:  140
;           float3 TranslatedWorldOffsetStereoCorrection;; Offset:  144
;           uint AllocationFeedbackEnable;            ; Offset:  156
;           uint BindlessSRV_AllocatedPageCountBuffer;; Offset:  160
;           uint Padding164;                          ; Offset:  164
;           uint BindlessSRV_PageIndexBuffer;         ; Offset:  168
;           uint Padding172;                          ; Offset:  172
;           uint BindlessSRV_PageIndexCoordBuffer;    ; Offset:  176
;           uint Padding180;                          ; Offset:  180
;           uint BindlessSRV_NodeDescBuffer;          ; Offset:  184
;           uint Padding188;                          ; Offset:  188
;           uint BindlessSRV_CurrGPUMinVoxelSize;     ; Offset:  192
;           uint Padding196;                          ; Offset:  196
;           uint BindlessSRV_NextGPUMinVoxelSize;     ; Offset:  200
;           float Padding204;                         ; Offset:  204
;           uint BindlessSRV_PageTexture;             ; Offset:  208
;       
;       } VirtualVoxel;                               ; Offset:    0
;
;   
;   } VirtualVoxel;                                   ; Offset:    0 Size:   212
;
; }
;
; Resource bind info for ForwardLightData_ForwardLocalLightBuffer
; {
;
;   float4 $Element;                                  ; Offset:    0 Size:    16
;
; }
;
; Resource bind info for ForwardLightData_NumCulledLightsGrid
; {
;
;   uint $Element;                                    ; Offset:    0 Size:     4
;
; }
;
; Resource bind info for VirtualShadowMap_PageTable
; {
;
;   uint $Element;                                    ; Offset:    0 Size:     4
;
; }
;
; Resource bind info for VirtualShadowMap_LightGridData
; {
;
;   uint $Element;                                    ; Offset:    0 Size:     4
;
; }
;
; Resource bind info for VirtualShadowMap_NumCulledLightsGrid
; {
;
;   uint $Element;                                    ; Offset:    0 Size:     4
;
; }
;
; Resource bind info for VirtualVoxel_NodeDescBuffer
; {
;
;   struct struct.FPackedVirtualVoxelNodeDesc
;   {
;
;       float3 TranslatedWorldMinAABB;                ; Offset:    0
;       uint PackedPageIndexResolution;               ; Offset:   12
;       float3 TranslatedWorldMaxAABB;                ; Offset:   16
;       uint PageIndexOffset_VoxelWorldSize;          ; Offset:   28
;   
;   } $Element;                                       ; Offset:    0 Size:    32
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; _RootShaderParameters             cbuffer      NA          NA     CB0            cb0     1
; View                              cbuffer      NA          NA     CB1            cb1     1
; ForwardLightData                  cbuffer      NA          NA     CB2            cb2     1
; VirtualShadowMap                  cbuffer      NA          NA     CB3            cb3     1
; BlueNoise                         cbuffer      NA          NA     CB4            cb4     1
; VirtualVoxel                      cbuffer      NA          NA     CB5            cb5     1
; SceneTexturesStruct_PointClampSampler   sampler      NA          NA      S0             s0     1
; SceneTexturesStruct_SceneDepthTexture   texture     f32          2d      T0             t0     1
; SceneTexturesStruct_GBufferATexture   texture     f32          2d      T1             t1     1
; SceneTexturesStruct_GBufferBTexture   texture     f32          2d      T2             t2     1
; SceneTexturesStruct_GBufferDTexture   texture     f32          2d      T3             t3     1
; ForwardLightData_ForwardLocalLightBuffer   texture  struct         r/o      T4             t4     1
; ForwardLightData_NumCulledLightsGrid   texture  struct         r/o      T5             t5     1
; VirtualShadowMap_ProjectionData   texture    byte         r/o      T6             t6     1
; VirtualShadowMap_PageTable        texture  struct         r/o      T7             t7     1
; VirtualShadowMap_PhysicalPagePool   texture     u32     2darray      T8             t8     1
; VirtualShadowMap_LightGridData    texture  struct         r/o      T9             t9     1
; VirtualShadowMap_NumCulledLightsGrid   texture  struct         r/o     T10            t10     1
; BlueNoise_ScalarTexture           texture     f32          2d     T11            t11     1
; BlueNoise_Vec2Texture             texture     f32          2d     T12            t12     1
; HairStrands_HairOnlyDepthTexture   texture     f32          2d     T13            t13     1
; VirtualVoxel_PageIndexBuffer      texture     u32         buf     T14            t14     1
; VirtualVoxel_NodeDescBuffer       texture  struct         r/o     T15            t15     1
; VirtualVoxel_PageTexture          texture     u32          3d     T16            t16     1
; OutShadowMaskBits                     UAV     u32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.ResRet.i32 = type { i32, i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.StructuredBuffer<vector<float, 4> >" = type { <4 x float> }
%"class.StructuredBuffer<unsigned int>" = type { i32 }
%struct.ByteAddressBuffer = type { i32 }
%"class.Texture2DArray<unsigned int>" = type { i32, %"class.Texture2DArray<unsigned int>::mips_type" }
%"class.Texture2DArray<unsigned int>::mips_type" = type { i32 }
%"class.Buffer<unsigned int>" = type { i32 }
%"class.StructuredBuffer<FPackedVirtualVoxelNodeDesc>" = type { %struct.FPackedVirtualVoxelNodeDesc }
%struct.FPackedVirtualVoxelNodeDesc = type { <3 x float>, i32, <3 x float>, i32 }
%"class.Texture3D<unsigned int>" = type { i32, %"class.Texture3D<unsigned int>::mips_type" }
%"class.Texture3D<unsigned int>::mips_type" = type { i32 }
%"class.RWTexture2D<vector<unsigned int, 4> >" = type { <4 x i32> }
%_RootShaderParameters = type { float, i32, i32, float, float, float, float, i32, <4 x i32>, float, i32, i32, i32 }
%hostlayout.View = type { %hostlayout.struct.FViewConstants }
%hostlayout.struct.FViewConstants = type { [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <4 x float>, <4 x float>, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <4 x float>, <4 x float>, <2 x float>, <2 x float>, <4 x float>, <4 x float>, <4 x i32>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <2 x float>, <2 x float>, i32, float, float, float, <4 x float>, <4 x float>, <4 x float>, <2 x float>, float, float, float, float, float, float, <3 x float>, float, float, float, float, float, float, float, i32, i32, i32, i32, i32, i32, i32, float, float, float, <4 x float>, <3 x float>, float, [2 x <4 x float>], [2 x <4 x float>], <4 x float>, <4 x float>, float, float, float, float, float, float, float, float, float, float, float, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, [2 x <4 x float>], [2 x <4 x float>], [2 x <4 x float>], [2 x <4 x float>], [2 x <4 x float>], <4 x float>, <3 x float>, float, <4 x float>, [4 x <4 x float>], <4 x float>, float, float, float, float, <4 x float>, float, float, float, float, float, float, float, float, <3 x float>, float, float, float, float, float, <4 x float>, float, float, float, float, <4 x float>, float, float, float, float, [8 x <4 x float>], float, float, float, float, i32, float, float, float, <3 x float>, i32, [6 x <4 x float>], [6 x <4 x float>], [6 x <4 x float>], [6 x <4 x float>], float, float, i32, i32, <3 x float>, float, <3 x float>, float, float, float, i32, float, float, float, float, float, <2 x i32>, float, float, <3 x float>, float, <3 x float>, float, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, float, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, float, float, float, float, [2 x <4 x float>], float, i32, i32, i32, i32, i32, i32, i32, <4 x float>, <2 x float>, float, float, <4 x float>, i32, float, float, float, <4 x float>, i32, i32, i32, float, <4 x float>, <4 x float>, <4 x float>, <3 x float>, float, i32, i32, i32, i32, [32 x <4 x i32>], i32, float, float, float, <4 x float>, <4 x float>, <2 x float>, float, float, <4 x float>, <4 x float>, <4 x float>, <4 x i32>, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, <4 x float>, float, float, i32, i32, i32, i32, i32, i32, <4 x float>, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, <3 x float>, float, <3 x float> }
%hostlayout.ForwardLightData = type { %hostlayout.struct.FForwardLightDataConstants }
%hostlayout.struct.FForwardLightDataConstants = type { i32, i32, i32, i32, <3 x i32>, i32, i32, i32, i32, i32, <3 x float>, float, <3 x float>, float, float, float, float, float, <3 x float>, float, float, i32, <2 x float>, i32, i32, i32, i32, <4 x float>, [4 x [4 x <4 x float>]], [4 x <4 x float>], <4 x float>, float, i32, i32, i32, i32, i32, i32, i32, <4 x float>, [4 x <4 x float>], i32, i32, float, float, float, i32, i32, float, float, float, float, float, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%VirtualShadowMap = type { %struct.FVirtualShadowMapConstants }
%struct.FVirtualShadowMapConstants = type { i32, i32, i32, i32, i32, i32, i32, i32, <4 x float>, <2 x i32>, <2 x i32>, i32, float, float, float, i32, i32, float, float, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%BlueNoise = type { %struct.FBlueNoiseConstants }
%struct.FBlueNoiseConstants = type { <3 x i32>, i32, <3 x i32>, i32, i32, i32, i32 }
%VirtualVoxel = type { %struct.FVirtualVoxelConstants }
%struct.FVirtualVoxelConstants = type { <3 x i32>, float, <3 x i32>, i32, i32, i32, i32, i32, i32, i32, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, <3 x float>, float, <3 x float>, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, float, i32 }
%struct.SamplerState = type { i32 }

define void @VirtualShadowMapProjection() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 16, i32 16, i32 0, i8 0 }, i32 16, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 15, i32 15, i32 0, i8 0 }, i32 15, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 14, i32 14, i32 0, i8 0 }, i32 14, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 13, i32 13, i32 0, i8 0 }, i32 13, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 12, i32 12, i32 0, i8 0 }, i32 12, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 11, i32 11, i32 0, i8 0 }, i32 11, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 10, i32 10, i32 0, i8 0 }, i32 10, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 9, i32 9, i32 0, i8 0 }, i32 9, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 8, i32 8, i32 0, i8 0 }, i32 8, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 7, i32 7, i32 0, i8 0 }, i32 7, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 6, i32 6, i32 0, i8 0 }, i32 6, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 5, i32 5, i32 0, i8 0 }, i32 5, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %14 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 4, i32 4, i32 0, i8 0 }, i32 4, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %15 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 0, i8 0 }, i32 3, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %16 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 0 }, i32 2, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %17 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 0 }, i32 1, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %18 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %19 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 3 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %20 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 5, i32 5, i32 0, i8 2 }, i32 5, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %21 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 4, i32 4, i32 0, i8 2 }, i32 4, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %22 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 0, i8 2 }, i32 3, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %23 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 2 }, i32 2, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %24 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 2 }, i32 1, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %25 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 2 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %26 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %20, %dx.types.ResourceProperties { i32 13, i32 212 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %27 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %21, %dx.types.ResourceProperties { i32 13, i32 44 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %28 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %22, %dx.types.ResourceProperties { i32 13, i32 156 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %29 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %23, %dx.types.ResourceProperties { i32 13, i32 732 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %30 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %24, %dx.types.ResourceProperties { i32 13, i32 5724 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %31 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %25, %dx.types.ResourceProperties { i32 13, i32 384 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %32 = call i32 @dx.op.groupId.i32(i32 94, i32 0)  ; GroupId(component)
  %33 = call i32 @dx.op.groupId.i32(i32 94, i32 1)  ; GroupId(component)
  %34 = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()
  %35 = shl i32 %32, 3
  %36 = shl i32 %33, 3
  %37 = and i32 %34, 1431655765
  %38 = lshr i32 %37, 1
  %39 = or i32 %38, %37
  %40 = and i32 %39, 858993459
  %41 = lshr i32 %40, 2
  %42 = or i32 %41, %40
  %43 = and i32 %42, 252645135
  %44 = lshr i32 %43, 4
  %45 = or i32 %44, %43
  %46 = lshr i32 %45, 8
  %47 = and i32 %46, 65280
  %48 = and i32 %45, 255
  %49 = or i32 %47, %48
  %50 = lshr i32 %34, 1
  %51 = and i32 %50, 1431655765
  %52 = lshr i32 %51, 1
  %53 = or i32 %52, %51
  %54 = and i32 %53, 858993459
  %55 = lshr i32 %54, 2
  %56 = or i32 %55, %54
  %57 = and i32 %56, 252645135
  %58 = lshr i32 %57, 4
  %59 = or i32 %58, %57
  %60 = lshr i32 %59, 8
  %61 = and i32 %60, 65280
  %62 = and i32 %59, 255
  %63 = or i32 %61, %62
  %64 = add i32 %49, %35
  %65 = add i32 %63, %36
  %66 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %67 = extractvalue %dx.types.CBufRet.i32 %66, 0
  %68 = extractvalue %dx.types.CBufRet.i32 %66, 1
  %69 = add i32 %64, %67
  %70 = add i32 %65, %68
  %71 = extractvalue %dx.types.CBufRet.i32 %66, 2
  %72 = extractvalue %dx.types.CBufRet.i32 %66, 3
  %73 = icmp uge i32 %69, %71
  %74 = icmp uge i32 %70, %72
  %75 = or i1 %73, %74
  br i1 %75, label %2933, label %76

; <label>:76                                      ; preds = %0
  %77 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %18, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %78 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %77, i32 0, i32 %69, i32 %70, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %79 = extractvalue %dx.types.ResRet.f32 %78, 0
  %80 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %81 = extractvalue %dx.types.CBufRet.i32 %80, 2
  %82 = icmp eq i32 %81, 1
  br i1 %82, label %83, label %88

; <label>:83                                      ; preds = %76
  %84 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %5, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %85 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %84, i32 0, i32 %69, i32 %70, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %86 = extractvalue %dx.types.ResRet.f32 %85, 0
  %87 = fcmp oeq float %86, 0.000000e+00
  br i1 %87, label %2933, label %88

; <label>:88                                      ; preds = %83, %76
  %89 = phi float [ %86, %83 ], [ %79, %76 ]
  %90 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 66)  ; CBufferLoadLegacy(handle,regIndex)
  %91 = extractvalue %dx.types.CBufRet.f32 %90, 0
  %92 = fmul float %89, %91
  %93 = extractvalue %dx.types.CBufRet.f32 %90, 1
  %94 = fadd float %92, %93
  %95 = extractvalue %dx.types.CBufRet.f32 %90, 2
  %96 = fmul float %89, %95
  %97 = extractvalue %dx.types.CBufRet.f32 %90, 3
  %98 = fsub float %96, %97
  %99 = fdiv float 1.000000e+00, %98
  %100 = fadd float %94, %99
  %101 = uitofp i32 %69 to float
  %102 = uitofp i32 %70 to float
  %103 = fadd float %101, 5.000000e-01
  %104 = fadd float %102, 5.000000e-01
  %105 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 44)  ; CBufferLoadLegacy(handle,regIndex)
  %106 = extractvalue %dx.types.CBufRet.f32 %105, 0
  %107 = extractvalue %dx.types.CBufRet.f32 %105, 1
  %108 = extractvalue %dx.types.CBufRet.f32 %105, 2
  %109 = extractvalue %dx.types.CBufRet.f32 %105, 3
  %110 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 45)  ; CBufferLoadLegacy(handle,regIndex)
  %111 = extractvalue %dx.types.CBufRet.f32 %110, 0
  %112 = extractvalue %dx.types.CBufRet.f32 %110, 1
  %113 = extractvalue %dx.types.CBufRet.f32 %110, 2
  %114 = extractvalue %dx.types.CBufRet.f32 %110, 3
  %115 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 46)  ; CBufferLoadLegacy(handle,regIndex)
  %116 = extractvalue %dx.types.CBufRet.f32 %115, 0
  %117 = extractvalue %dx.types.CBufRet.f32 %115, 1
  %118 = extractvalue %dx.types.CBufRet.f32 %115, 2
  %119 = extractvalue %dx.types.CBufRet.f32 %115, 3
  %120 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 47)  ; CBufferLoadLegacy(handle,regIndex)
  %121 = extractvalue %dx.types.CBufRet.f32 %120, 0
  %122 = extractvalue %dx.types.CBufRet.f32 %120, 1
  %123 = extractvalue %dx.types.CBufRet.f32 %120, 2
  %124 = extractvalue %dx.types.CBufRet.f32 %120, 3
  %125 = fmul float %103, %106
  %126 = call float @dx.op.tertiary.f32(i32 46, float %104, float %111, float %125), !dx.precise !43  ; FMad(a,b,c)
  %127 = call float @dx.op.tertiary.f32(i32 46, float %89, float %116, float %126), !dx.precise !43  ; FMad(a,b,c)
  %128 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %121, float %127), !dx.precise !43  ; FMad(a,b,c)
  %129 = fmul float %103, %107
  %130 = call float @dx.op.tertiary.f32(i32 46, float %104, float %112, float %129), !dx.precise !43  ; FMad(a,b,c)
  %131 = call float @dx.op.tertiary.f32(i32 46, float %89, float %117, float %130), !dx.precise !43  ; FMad(a,b,c)
  %132 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %122, float %131), !dx.precise !43  ; FMad(a,b,c)
  %133 = fmul float %103, %108
  %134 = call float @dx.op.tertiary.f32(i32 46, float %104, float %113, float %133), !dx.precise !43  ; FMad(a,b,c)
  %135 = call float @dx.op.tertiary.f32(i32 46, float %89, float %118, float %134), !dx.precise !43  ; FMad(a,b,c)
  %136 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %123, float %135), !dx.precise !43  ; FMad(a,b,c)
  %137 = fmul float %103, %109
  %138 = call float @dx.op.tertiary.f32(i32 46, float %104, float %114, float %137), !dx.precise !43  ; FMad(a,b,c)
  %139 = call float @dx.op.tertiary.f32(i32 46, float %89, float %119, float %138), !dx.precise !43  ; FMad(a,b,c)
  %140 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %124, float %139), !dx.precise !43  ; FMad(a,b,c)
  %141 = fdiv float %128, %140
  %142 = fdiv float %132, %140
  %143 = fdiv float %136, %140
  %144 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %145 = extractvalue %dx.types.CBufRet.f32 %144, 0
  %146 = fmul fast float %145, %100
  %147 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 299)  ; CBufferLoadLegacy(handle,regIndex)
  %148 = extractvalue %dx.types.CBufRet.f32 %147, 1
  %149 = fmul fast float %146, %148
  %150 = extractvalue %dx.types.CBufRet.f32 %147, 3
  %151 = fadd fast float %149, %150
  %152 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %30, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %153 = extractvalue %dx.types.CBufRet.i32 %152, 1
  %154 = uitofp i32 %153 to float
  %155 = fmul fast float %154, 0x4040551EC0000000
  %156 = fmul fast float %154, 0x4027A147A0000000
  %157 = fadd fast float %155, %103
  %158 = fadd fast float %156, %104
  %159 = call float @dx.op.dot2.f32(i32 54, float %157, float %158, float 0x3FB12E2860000000, float 0x3F77E8B200000000)  ; Dot2(ax,ay,bx,by)
  %160 = call float @dx.op.unary.f32(i32 22, float %159)  ; Frc(value)
  %161 = fmul fast float %160, 0x404A7DD040000000
  %162 = call float @dx.op.unary.f32(i32 22, float %161)  ; Frc(value)
  %163 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %17, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %164 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %163, i32 0, i32 %69, i32 %70, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %165 = extractvalue %dx.types.ResRet.f32 %164, 0
  %166 = extractvalue %dx.types.ResRet.f32 %164, 1
  %167 = extractvalue %dx.types.ResRet.f32 %164, 2
  %168 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %16, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %169 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %168, i32 0, i32 %69, i32 %70, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %170 = extractvalue %dx.types.ResRet.f32 %169, 3
  %171 = fmul float %170, 2.550000e+02
  %172 = fadd float %171, 5.000000e-01
  %173 = fptoui float %172 to i32
  %174 = and i32 %173, 15
  %175 = fmul float %165, 2.000000e+00
  %176 = fmul float %166, 2.000000e+00
  %177 = fmul float %167, 2.000000e+00
  %178 = fadd float %175, -1.000000e+00
  %179 = fadd float %176, -1.000000e+00
  %180 = fadd float %177, -1.000000e+00
  %181 = call float @dx.op.dot3.f32(i32 55, float %178, float %179, float %180, float %178, float %179, float %180), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %182 = call float @dx.op.unary.f32(i32 25, float %181), !dx.precise !43  ; Rsqrt(value)
  %183 = fmul float %178, %182
  %184 = fmul float %179, %182
  %185 = fmul float %180, %182
  %186 = icmp ne i32 %174, 0
  %187 = icmp eq i32 %174, 7
  %188 = and i32 %173, 14
  %189 = icmp eq i32 %188, 2
  %190 = add nsw i32 %174, -5
  %191 = icmp ult i32 %190, 3
  %192 = or i1 %189, %191
  %193 = icmp eq i32 %174, 9
  %194 = or i1 %193, %192
  %195 = xor i1 %82, true
  %196 = icmp eq i32 %174, 6
  %197 = or i1 %189, %196
  %198 = and i1 %197, %195
  br i1 %198, label %199, label %215

; <label>:199                                     ; preds = %88
  %200 = icmp eq i32 %188, 8
  %201 = and i32 %173, 12
  %202 = icmp eq i32 %201, 4
  %203 = or i1 %202, %189
  %204 = or i1 %200, %203
  %205 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %15, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %206 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %205, i32 0, i32 %69, i32 %70, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %207 = extractvalue %dx.types.ResRet.f32 %206, 3
  %208 = select i1 %204, float %207, float 0.000000e+00
  %209 = call float @dx.op.binary.f32(i32 36, float %208, float 0x3FEFAE1480000000)  ; FMin(a,b)
  %210 = call float @dx.op.binary.f32(i32 36, float %209, float 0x3FEFAE1480000000)  ; FMin(a,b)
  %211 = fsub fast float 1.000000e+00, %210
  %212 = call float @dx.op.unary.f32(i32 23, float %211)  ; Log(value)
  %213 = fmul fast float %212, 0xBFA1BE9C00000000
  %214 = fmul float %213, 0xBFF7154760000000
  br label %215

; <label>:215                                     ; preds = %199, %88
  %216 = phi float [ -0.000000e+00, %88 ], [ %214, %199 ]
  %217 = phi float [ 1.000000e+00, %88 ], [ %209, %199 ]
  %218 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %29, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %219 = extractvalue %dx.types.CBufRet.i32 %218, 0
  %220 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %29, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %221 = extractvalue %dx.types.CBufRet.f32 %220, 0
  %222 = extractvalue %dx.types.CBufRet.f32 %220, 1
  %223 = extractvalue %dx.types.CBufRet.f32 %220, 2
  %224 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %29, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %225 = extractvalue %dx.types.CBufRet.i32 %224, 2
  %226 = fmul float %100, %221
  %227 = fadd float %222, %226
  %228 = call float @dx.op.unary.f32(i32 23, float %227), !dx.precise !43  ; Log(value)
  %229 = fmul float %223, %228
  %230 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %229), !dx.precise !43  ; FMax(a,b)
  %231 = fptoui float %230 to i32
  %232 = add nsw i32 %225, -1
  %233 = call i32 @dx.op.binary.i32(i32 40, i32 %231, i32 %232)  ; UMin(a,b)
  %234 = and i32 %219, 31
  %235 = lshr i32 %64, %234
  %236 = lshr i32 %65, %234
  %237 = extractvalue %dx.types.CBufRet.i32 %224, 0
  %238 = extractvalue %dx.types.CBufRet.i32 %224, 1
  %239 = mul i32 %238, %233
  %240 = add i32 %239, %236
  %241 = mul i32 %240, %237
  %242 = add i32 %241, %235
  %243 = shl i32 %242, 1
  %244 = or i32 %243, 1
  %245 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %13, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %246 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %245, i32 %244, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %247 = extractvalue %dx.types.ResRet.i32 %246, 0
  %248 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %249 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %248, i32 %242, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %250 = extractvalue %dx.types.ResRet.i32 %249, 0
  %251 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %28, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %252 = extractvalue %dx.types.CBufRet.i32 %251, 3
  %253 = call i32 @dx.op.binary.i32(i32 40, i32 %252, i32 %250)  ; UMin(a,b)
  %254 = icmp eq i32 %253, 0
  br i1 %254, label %2927, label %255

; <label>:255                                     ; preds = %215
  br label %256

; <label>:256                                     ; preds = %2919, %255
  %257 = phi i32 [ %2920, %2919 ], [ 0, %255 ]
  %258 = phi i32 [ %2921, %2919 ], [ 0, %255 ]
  %259 = phi i32 [ %2922, %2919 ], [ 0, %255 ]
  %260 = phi i32 [ %2923, %2919 ], [ 0, %255 ]
  %261 = phi i32 [ %2924, %2919 ], [ 0, %255 ]
  %262 = add i32 %261, %247
  %263 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %9, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %264 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %263, i32 %262, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %265 = extractvalue %dx.types.ResRet.i32 %264, 0
  %266 = mul i32 %265, 6
  %267 = add i32 %266, 5
  %268 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %14, %dx.types.ResourceProperties { i32 12, i32 16 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=16>
  %269 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %268, i32 %267, i32 0, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %270 = extractvalue %dx.types.ResRet.f32 %269, 3
  %271 = add i32 %266, 3
  %272 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %268, i32 %271, i32 0, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %273 = extractvalue %dx.types.ResRet.f32 %272, 0
  %274 = extractvalue %dx.types.ResRet.f32 %272, 1
  %275 = add i32 %266, 2
  %276 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %268, i32 %275, i32 0, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %277 = extractvalue %dx.types.ResRet.f32 %276, 0
  %278 = extractvalue %dx.types.ResRet.f32 %276, 1
  %279 = extractvalue %dx.types.ResRet.f32 %276, 2
  %280 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %268, i32 %266, i32 0, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %281 = extractvalue %dx.types.ResRet.f32 %280, 0
  %282 = extractvalue %dx.types.ResRet.f32 %280, 1
  %283 = extractvalue %dx.types.ResRet.f32 %280, 2
  %284 = extractvalue %dx.types.ResRet.f32 %280, 3
  %285 = fptosi float %270 to i32
  %286 = icmp eq i32 %285, -1
  br i1 %286, label %2919, label %287

; <label>:287                                     ; preds = %256
  %288 = extractvalue %dx.types.ResRet.f32 %272, 2
  %289 = extractvalue %dx.types.ResRet.f32 %269, 2
  %290 = bitcast float %288 to i32
  %291 = and i32 %290, 65535
  %292 = call float @dx.op.legacyF16ToF32(i32 131, i32 %291), !dx.precise !43  ; LegacyF16ToF32(value)
  %293 = bitcast float %289 to i32
  %294 = and i32 %293, 65535
  %295 = call float @dx.op.legacyF16ToF32(i32 131, i32 %294), !dx.precise !43  ; LegacyF16ToF32(value)
  %296 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %297 = extractvalue %dx.types.CBufRet.i32 %296, 2
  %298 = icmp eq i32 %297, 1
  %299 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 69)  ; CBufferLoadLegacy(handle,regIndex)
  %300 = extractvalue %dx.types.CBufRet.f32 %299, 0
  %301 = extractvalue %dx.types.CBufRet.f32 %299, 1
  %302 = extractvalue %dx.types.CBufRet.f32 %299, 2
  %303 = fsub float %141, %300
  %304 = fsub float %142, %301
  %305 = fsub float %143, %302
  %306 = fmul float %303, %303
  %307 = fmul float %304, %304
  %308 = fadd float %306, %307
  %309 = fmul float %305, %305
  %310 = fadd float %309, %308
  %311 = call float @dx.op.unary.f32(i32 24, float %310), !dx.precise !43  ; Sqrt(value)
  %312 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 31)  ; CBufferLoadLegacy(handle,regIndex)
  %313 = extractvalue %dx.types.CBufRet.f32 %312, 3
  %314 = fcmp ult float %313, 1.000000e+00
  br i1 %314, label %323, label %315

; <label>:315                                     ; preds = %287
  %316 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 61)  ; CBufferLoadLegacy(handle,regIndex)
  %317 = extractvalue %dx.types.CBufRet.f32 %316, 0
  %318 = extractvalue %dx.types.CBufRet.f32 %316, 1
  %319 = extractvalue %dx.types.CBufRet.f32 %316, 2
  %320 = call float @dx.op.dot3.f32(i32 55, float %303, float %304, float %305, float %317, float %318, float %319), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %321 = fdiv float %311, %320
  %322 = fmul float %311, %321
  br label %323

; <label>:323                                     ; preds = %315, %287
  %324 = phi float [ %322, %315 ], [ %311, %287 ]
  %325 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %326 = extractvalue %dx.types.CBufRet.f32 %325, 0
  %327 = fmul float %324, %326
  %328 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 296)  ; CBufferLoadLegacy(handle,regIndex)
  %329 = extractvalue %dx.types.CBufRet.f32 %328, 2
  %330 = fdiv float %327, %329
  %331 = call float @dx.op.binary.f32(i32 35, float 0x3F947AE140000000, float %330), !dx.precise !43  ; FMax(a,b)
  %332 = fsub float %281, %141
  %333 = fsub float %282, %142
  %334 = fsub float %283, %143
  %335 = call float @dx.op.dot3.f32(i32 55, float %332, float %333, float %334, float %332, float %333, float %334), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %336 = call float @dx.op.unary.f32(i32 25, float %335), !dx.precise !43  ; Rsqrt(value)
  %337 = fmul float %332, %336
  %338 = fmul float %333, %336
  %339 = fmul float %334, %336
  %340 = fcmp oge float %336, %284
  %341 = call float @dx.op.dot3.f32(i32 55, float %337, float %338, float %339, float %277, float %278, float %279), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %342 = fsub float %341, %273
  %343 = fmul float %274, %342
  %344 = call float @dx.op.unary.f32(i32 7, float %343), !dx.precise !43  ; Saturate(value)
  %345 = fcmp ogt float %344, 0.000000e+00
  %346 = and i1 %340, %345
  %347 = zext i1 %346 to i32
  %348 = fcmp ogt float %295, -2.000000e+00
  br i1 %348, label %349, label %353

; <label>:349                                     ; preds = %323
  %350 = call float @dx.op.dot3.f32(i32 55, float %332, float %333, float %334, float %277, float %278, float %279), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %351 = fcmp olt float %350, 0.000000e+00
  br i1 %351, label %352, label %353

; <label>:352                                     ; preds = %349
  br label %353

; <label>:353                                     ; preds = %352, %349, %323
  %354 = phi i32 [ 0, %352 ], [ %347, %349 ], [ %347, %323 ]
  %355 = or i1 %186, %298
  %356 = icmp ne i32 %354, 0
  %357 = and i1 %355, %356
  br i1 %357, label %358, label %2854

; <label>:358                                     ; preds = %353
  %359 = extractvalue %dx.types.CBufRet.i32 %296, 3
  %360 = icmp eq i32 %359, 0
  %361 = or i1 %298, %360
  %362 = or i1 %194, %361
  %363 = xor i1 %362, true
  %364 = or i1 %187, %298
  %365 = select i1 %364, float %337, float %183
  %366 = select i1 %364, float %338, float %184
  %367 = select i1 %364, float %339, float %185
  %368 = fmul float %331, %365
  %369 = fmul float %331, %366
  %370 = fmul float %331, %367
  %371 = fadd float %141, %368
  %372 = fadd float %142, %369
  %373 = fadd float %143, %370
  %374 = xor i1 %298, true
  %375 = fcmp fast ogt float %151, 0.000000e+00
  %376 = and i1 %375, %374
  br i1 %376, label %377, label %515

; <label>:377                                     ; preds = %358
  %378 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %379 = extractvalue %dx.types.CBufRet.f32 %378, 0
  %380 = extractvalue %dx.types.CBufRet.f32 %378, 1
  %381 = extractvalue %dx.types.CBufRet.f32 %378, 2
  %382 = extractvalue %dx.types.CBufRet.f32 %378, 3
  %383 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %384 = extractvalue %dx.types.CBufRet.f32 %383, 0
  %385 = extractvalue %dx.types.CBufRet.f32 %383, 1
  %386 = extractvalue %dx.types.CBufRet.f32 %383, 2
  %387 = extractvalue %dx.types.CBufRet.f32 %383, 3
  %388 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %389 = extractvalue %dx.types.CBufRet.f32 %388, 0
  %390 = extractvalue %dx.types.CBufRet.f32 %388, 1
  %391 = extractvalue %dx.types.CBufRet.f32 %388, 2
  %392 = extractvalue %dx.types.CBufRet.f32 %388, 3
  %393 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %394 = extractvalue %dx.types.CBufRet.f32 %393, 0
  %395 = extractvalue %dx.types.CBufRet.f32 %393, 1
  %396 = extractvalue %dx.types.CBufRet.f32 %393, 2
  %397 = extractvalue %dx.types.CBufRet.f32 %393, 3
  %398 = fmul fast float %379, %371
  %399 = call float @dx.op.tertiary.f32(i32 46, float %372, float %384, float %398)  ; FMad(a,b,c)
  %400 = call float @dx.op.tertiary.f32(i32 46, float %373, float %389, float %399)  ; FMad(a,b,c)
  %401 = fadd fast float %400, %394
  %402 = fmul fast float %380, %371
  %403 = call float @dx.op.tertiary.f32(i32 46, float %372, float %385, float %402)  ; FMad(a,b,c)
  %404 = call float @dx.op.tertiary.f32(i32 46, float %373, float %390, float %403)  ; FMad(a,b,c)
  %405 = fadd fast float %404, %395
  %406 = fmul fast float %381, %371
  %407 = call float @dx.op.tertiary.f32(i32 46, float %372, float %386, float %406)  ; FMad(a,b,c)
  %408 = call float @dx.op.tertiary.f32(i32 46, float %373, float %391, float %407)  ; FMad(a,b,c)
  %409 = fadd fast float %408, %396
  %410 = fmul fast float %382, %371
  %411 = call float @dx.op.tertiary.f32(i32 46, float %372, float %387, float %410)  ; FMad(a,b,c)
  %412 = call float @dx.op.tertiary.f32(i32 46, float %373, float %392, float %411)  ; FMad(a,b,c)
  %413 = fadd fast float %412, %397
  %414 = fmul fast float %337, %151
  %415 = fmul fast float %338, %151
  %416 = fmul fast float %339, %151
  %417 = fmul fast float %379, %414
  %418 = call float @dx.op.tertiary.f32(i32 46, float %415, float %384, float %417)  ; FMad(a,b,c)
  %419 = call float @dx.op.tertiary.f32(i32 46, float %416, float %389, float %418)  ; FMad(a,b,c)
  %420 = fmul fast float %380, %414
  %421 = call float @dx.op.tertiary.f32(i32 46, float %415, float %385, float %420)  ; FMad(a,b,c)
  %422 = call float @dx.op.tertiary.f32(i32 46, float %416, float %390, float %421)  ; FMad(a,b,c)
  %423 = fmul fast float %381, %414
  %424 = call float @dx.op.tertiary.f32(i32 46, float %415, float %386, float %423)  ; FMad(a,b,c)
  %425 = call float @dx.op.tertiary.f32(i32 46, float %416, float %391, float %424)  ; FMad(a,b,c)
  %426 = fmul fast float %382, %414
  %427 = call float @dx.op.tertiary.f32(i32 46, float %415, float %387, float %426)  ; FMad(a,b,c)
  %428 = call float @dx.op.tertiary.f32(i32 46, float %416, float %392, float %427)  ; FMad(a,b,c)
  %429 = fadd fast float %419, %401
  %430 = fadd fast float %422, %405
  %431 = fadd fast float %425, %409
  %432 = fadd fast float %428, %413
  %433 = fdiv fast float %401, %413
  %434 = fdiv fast float %405, %413
  %435 = fdiv fast float %409, %413
  %436 = fdiv fast float %429, %432
  %437 = fdiv fast float %430, %432
  %438 = fdiv fast float %431, %432
  %439 = fsub fast float %436, %433
  %440 = fsub fast float %437, %434
  %441 = fsub fast float %438, %435
  %442 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 67)  ; CBufferLoadLegacy(handle,regIndex)
  %443 = extractvalue %dx.types.CBufRet.f32 %442, 0
  %444 = extractvalue %dx.types.CBufRet.f32 %442, 1
  %445 = fmul fast float %443, %433
  %446 = fmul fast float %444, %434
  %447 = extractvalue %dx.types.CBufRet.f32 %442, 2
  %448 = extractvalue %dx.types.CBufRet.f32 %442, 3
  %449 = fadd fast float %445, %448
  %450 = fadd fast float %446, %447
  %451 = fmul fast float %443, %439
  %452 = fmul fast float %444, %440
  %453 = fadd fast float %162, -5.000000e-01
  %454 = fmul fast float %453, 2.500000e-01
  %455 = fadd fast float %454, 2.500000e-01
  %456 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %18, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %457 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %19, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %458 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %456, %dx.types.Handle %457, float %449, float %450, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %459 = extractvalue %dx.types.ResRet.f32 %458, 0
  %460 = fmul fast float %451, %455
  %461 = fmul fast float %452, %455
  %462 = fmul fast float %441, %455
  %463 = fadd fast float %460, %449
  %464 = fadd fast float %461, %450
  %465 = fadd fast float %462, %435
  %466 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %456, %dx.types.Handle %457, float %463, float %464, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %467 = extractvalue %dx.types.ResRet.f32 %466, 0
  %468 = fcmp fast une float %467, %459
  %469 = fcmp fast olt float %465, %467
  %470 = and i1 %468, %469
  br i1 %470, label %510, label %471

; <label>:471                                     ; preds = %377
  %472 = fadd fast float %454, 5.000000e-01
  %473 = fmul fast float %451, %472
  %474 = fmul fast float %452, %472
  %475 = fmul fast float %441, %472
  %476 = fadd fast float %473, %449
  %477 = fadd fast float %474, %450
  %478 = fadd fast float %475, %435
  %479 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %456, %dx.types.Handle %457, float %476, float %477, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %480 = extractvalue %dx.types.ResRet.f32 %479, 0
  %481 = fcmp fast une float %480, %459
  %482 = fcmp fast olt float %478, %480
  %483 = and i1 %481, %482
  br i1 %483, label %510, label %484

; <label>:484                                     ; preds = %471
  %485 = fadd fast float %454, 7.500000e-01
  %486 = fmul fast float %451, %485
  %487 = fmul fast float %452, %485
  %488 = fmul fast float %441, %485
  %489 = fadd fast float %486, %449
  %490 = fadd fast float %487, %450
  %491 = fadd fast float %488, %435
  %492 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %456, %dx.types.Handle %457, float %489, float %490, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %493 = extractvalue %dx.types.ResRet.f32 %492, 0
  %494 = fcmp fast une float %493, %459
  %495 = fcmp fast olt float %491, %493
  %496 = and i1 %494, %495
  br i1 %496, label %510, label %497

; <label>:497                                     ; preds = %484
  %498 = fadd fast float %454, 1.000000e+00
  %499 = fmul fast float %451, %498
  %500 = fmul fast float %452, %498
  %501 = fmul fast float %441, %498
  %502 = fadd fast float %499, %449
  %503 = fadd fast float %500, %450
  %504 = fadd fast float %501, %435
  %505 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %456, %dx.types.Handle %457, float %502, float %503, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %506 = extractvalue %dx.types.ResRet.f32 %505, 0
  %507 = fcmp fast une float %506, %459
  %508 = fcmp fast olt float %504, %506
  %509 = and i1 %507, %508
  br i1 %509, label %510, label %515

; <label>:510                                     ; preds = %497, %484, %471, %377
  %511 = phi float [ %455, %377 ], [ %472, %471 ], [ %485, %484 ], [ %498, %497 ]
  %512 = fadd fast float %511, -3.750000e-01
  %513 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %512)  ; FMax(a,b)
  %514 = fmul fast float %513, %151
  br label %515

; <label>:515                                     ; preds = %510, %497, %358
  %516 = phi float [ %151, %358 ], [ %514, %510 ], [ %151, %497 ]
  %517 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %518 = extractvalue %dx.types.CBufRet.i32 %517, 1
  %519 = icmp sgt i32 %518, 0
  br i1 %519, label %520, label %1993

; <label>:520                                     ; preds = %515
  %521 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %522 = extractvalue %dx.types.CBufRet.i32 %521, 0
  %523 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %524 = extractvalue %dx.types.CBufRet.f32 %523, 3
  %525 = extractvalue %dx.types.CBufRet.f32 %523, 2
  %526 = extractvalue %dx.types.CBufRet.f32 %523, 1
  %527 = extractvalue %dx.types.CBufRet.f32 %523, 0
  %528 = extractvalue %dx.types.CBufRet.i32 %517, 2
  %529 = fcmp ogt float %273, -2.000000e+00
  %530 = fsub float %281, %371
  %531 = fsub float %282, %372
  %532 = fsub float %283, %373
  %533 = call float @dx.op.dot3.f32(i32 55, float %530, float %531, float %532, float %530, float %531, float %532), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %534 = fadd float %533, 1.000000e+00
  %535 = fdiv float 1.000000e+00, %534
  %536 = call float @dx.op.unary.f32(i32 25, float %533), !dx.precise !43  ; Rsqrt(value)
  %537 = fmul float %530, %536
  %538 = fmul float %531, %536
  %539 = fmul float %532, %536
  %540 = fmul fast float %536, %292
  %541 = fmul float %292, %292
  %542 = fmul float %541, %535
  %543 = call float @dx.op.unary.f32(i32 7, float %542), !dx.precise !43  ; Saturate(value)
  %544 = call float @dx.op.unary.f32(i32 24, float %543), !dx.precise !43  ; Sqrt(value)
  %545 = call float @dx.op.dot3.f32(i32 55, float %365, float %366, float %367, float %537, float %538, float %539), !dx.precise !43  ; Dot3(ax,ay,az,bx,by,bz)
  %546 = fsub float -0.000000e+00, %544
  %547 = fcmp olt float %545, %546
  %548 = and i1 %547, %363
  br i1 %548, label %2325, label %549

; <label>:549                                     ; preds = %520
  %550 = and i32 %69, 65535
  %551 = shl nuw nsw i32 %550, 8
  %552 = or i32 %551, %550
  %553 = and i32 %552, 16711935
  %554 = shl nuw nsw i32 %553, 4
  %555 = or i32 %554, %553
  %556 = and i32 %555, 252645135
  %557 = shl nuw nsw i32 %556, 2
  %558 = or i32 %557, %556
  %559 = and i32 %558, 858993459
  %560 = shl nuw nsw i32 %559, 1
  %561 = or i32 %560, %559
  %562 = and i32 %561, 1431655765
  %563 = and i32 %70, 65535
  %564 = shl nuw nsw i32 %563, 8
  %565 = or i32 %564, %563
  %566 = and i32 %565, 16711935
  %567 = shl nuw nsw i32 %566, 4
  %568 = or i32 %567, %566
  %569 = and i32 %568, 252645135
  %570 = shl nuw nsw i32 %569, 2
  %571 = or i32 %570, %569
  %572 = and i32 %571, 858993459
  %573 = shl nuw nsw i32 %572, 1
  %574 = or i32 %573, %572
  %575 = shl nuw i32 %574, 1
  %576 = and i32 %575, -1431655766
  %577 = or i32 %576, %562
  br i1 %529, label %602, label %578

; <label>:578                                     ; preds = %549
  %579 = fsub float -0.000000e+00, %530
  %580 = fsub float -0.000000e+00, %531
  %581 = fsub float -0.000000e+00, %532
  %582 = call float @dx.op.unary.f32(i32 6, float %579), !dx.precise !43  ; FAbs(value)
  %583 = call float @dx.op.unary.f32(i32 6, float %580), !dx.precise !43  ; FAbs(value)
  %584 = fcmp ult float %582, %583
  %585 = call float @dx.op.unary.f32(i32 6, float %581), !dx.precise !43  ; FAbs(value)
  %586 = fcmp ult float %582, %585
  %587 = or i1 %584, %586
  br i1 %587, label %591, label %588

; <label>:588                                     ; preds = %578
  %589 = fcmp uge float %530, -0.000000e+00
  %590 = zext i1 %589 to i32
  br label %599

; <label>:591                                     ; preds = %578
  %592 = fcmp ogt float %583, %585
  br i1 %592, label %593, label %596

; <label>:593                                     ; preds = %591
  %594 = fcmp olt float %531, -0.000000e+00
  %595 = select i1 %594, i32 2, i32 3
  br label %599

; <label>:596                                     ; preds = %591
  %597 = fcmp olt float %532, -0.000000e+00
  %598 = select i1 %597, i32 4, i32 5
  br label %599

; <label>:599                                     ; preds = %596, %593, %588
  %600 = phi i32 [ %590, %588 ], [ %595, %593 ], [ %598, %596 ]
  %601 = add i32 %600, %285
  br label %602

; <label>:602                                     ; preds = %599, %549
  %603 = phi i32 [ %601, %599 ], [ %285, %549 ]
  %604 = mul i32 %603, 288
  %605 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %606 = add i32 %604, 48
  %607 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %606, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %608 = extractvalue %dx.types.ResRet.i32 %607, 2
  %609 = bitcast i32 %608 to float
  %610 = add i32 %604, 64
  %611 = add i32 %604, 80
  %612 = add i32 %604, 96
  %613 = add i32 %604, 112
  %614 = add i32 %604, 128
  %615 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %614, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %616 = extractvalue %dx.types.ResRet.i32 %615, 0
  %617 = extractvalue %dx.types.ResRet.i32 %615, 1
  %618 = extractvalue %dx.types.ResRet.i32 %615, 2
  %619 = bitcast i32 %616 to float
  %620 = bitcast i32 %617 to float
  %621 = bitcast i32 %618 to float
  %622 = add i32 %604, 144
  %623 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %622, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %624 = extractvalue %dx.types.ResRet.i32 %623, 0
  %625 = extractvalue %dx.types.ResRet.i32 %623, 1
  %626 = extractvalue %dx.types.ResRet.i32 %623, 2
  %627 = bitcast i32 %624 to float
  %628 = bitcast i32 %625 to float
  %629 = bitcast i32 %626 to float
  %630 = add i32 %604, 160
  %631 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %630, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %632 = extractvalue %dx.types.ResRet.i32 %631, 0
  %633 = extractvalue %dx.types.ResRet.i32 %631, 1
  %634 = extractvalue %dx.types.ResRet.i32 %631, 2
  %635 = bitcast i32 %632 to float
  %636 = bitcast i32 %633 to float
  %637 = bitcast i32 %634 to float
  %638 = add i32 %604, 176
  %639 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %638, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %640 = extractvalue %dx.types.ResRet.i32 %639, 0
  %641 = extractvalue %dx.types.ResRet.i32 %639, 1
  %642 = extractvalue %dx.types.ResRet.i32 %639, 2
  %643 = bitcast i32 %640 to float
  %644 = bitcast i32 %641 to float
  %645 = bitcast i32 %642 to float
  %646 = add i32 %604, 208
  %647 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %646, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %648 = extractvalue %dx.types.ResRet.i32 %647, 0
  %649 = extractvalue %dx.types.ResRet.i32 %647, 1
  %650 = extractvalue %dx.types.ResRet.i32 %647, 2
  %651 = bitcast i32 %648 to float
  %652 = bitcast i32 %649 to float
  %653 = bitcast i32 %650 to float
  %654 = add i32 %604, 224
  %655 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %654, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %656 = extractvalue %dx.types.ResRet.i32 %655, 0
  %657 = extractvalue %dx.types.ResRet.i32 %655, 1
  %658 = extractvalue %dx.types.ResRet.i32 %655, 2
  %659 = bitcast i32 %656 to float
  %660 = bitcast i32 %657 to float
  %661 = bitcast i32 %658 to float
  %662 = add i32 %604, 236
  %663 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %662, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %664 = extractvalue %dx.types.ResRet.i32 %663, 0
  %665 = bitcast i32 %664 to float
  %666 = add i32 %604, 280
  %667 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %605, i32 %666, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %668 = extractvalue %dx.types.ResRet.i32 %667, 0
  %669 = bitcast i32 %668 to float
  %670 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 72)  ; CBufferLoadLegacy(handle,regIndex)
  %671 = extractvalue %dx.types.CBufRet.f32 %670, 0
  %672 = extractvalue %dx.types.CBufRet.f32 %670, 1
  %673 = extractvalue %dx.types.CBufRet.f32 %670, 2
  %674 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 73)  ; CBufferLoadLegacy(handle,regIndex)
  %675 = extractvalue %dx.types.CBufRet.f32 %674, 0
  %676 = extractvalue %dx.types.CBufRet.f32 %674, 1
  %677 = extractvalue %dx.types.CBufRet.f32 %674, 2
  %678 = fsub float %651, %671
  %679 = fsub float %652, %672
  %680 = fsub float %653, %673
  %681 = fsub float %659, %675
  %682 = fsub float %660, %676
  %683 = fsub float %661, %677
  %684 = fadd float %678, %681
  %685 = fadd float %679, %682
  %686 = fadd float %680, %683
  %687 = fadd fast float %684, %371
  %688 = fadd fast float %685, %372
  %689 = fadd fast float %686, %373
  %690 = call float @dx.op.dot3.f32(i32 55, float %365, float %366, float %367, float %687, float %688, float %689)  ; Dot3(ax,ay,az,bx,by,bz)
  %691 = fsub fast float -0.000000e+00, %690
  %692 = fmul fast float %619, %365
  %693 = call float @dx.op.tertiary.f32(i32 46, float %366, float %627, float %692)  ; FMad(a,b,c)
  %694 = call float @dx.op.tertiary.f32(i32 46, float %367, float %635, float %693)  ; FMad(a,b,c)
  %695 = call float @dx.op.tertiary.f32(i32 46, float %691, float %643, float %694)  ; FMad(a,b,c)
  %696 = fmul fast float %620, %365
  %697 = call float @dx.op.tertiary.f32(i32 46, float %366, float %628, float %696)  ; FMad(a,b,c)
  %698 = call float @dx.op.tertiary.f32(i32 46, float %367, float %636, float %697)  ; FMad(a,b,c)
  %699 = call float @dx.op.tertiary.f32(i32 46, float %691, float %644, float %698)  ; FMad(a,b,c)
  %700 = fmul fast float %621, %365
  %701 = call float @dx.op.tertiary.f32(i32 46, float %366, float %629, float %700)  ; FMad(a,b,c)
  %702 = call float @dx.op.tertiary.f32(i32 46, float %367, float %637, float %701)  ; FMad(a,b,c)
  %703 = call float @dx.op.tertiary.f32(i32 46, float %691, float %645, float %702)  ; FMad(a,b,c)
  %704 = fsub fast float -0.000000e+00, %695
  %705 = fsub fast float -0.000000e+00, %699
  %706 = fdiv fast float %704, %703
  %707 = fdiv fast float %705, %703
  %708 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 136)  ; CBufferLoadLegacy(handle,regIndex)
  %709 = extractvalue %dx.types.CBufRet.f32 %708, 0
  %710 = extractvalue %dx.types.CBufRet.f32 %708, 1
  %711 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 28)  ; CBufferLoadLegacy(handle,regIndex)
  %712 = extractvalue %dx.types.CBufRet.f32 %711, 0
  %713 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 29)  ; CBufferLoadLegacy(handle,regIndex)
  %714 = extractvalue %dx.types.CBufRet.f32 %713, 1
  %715 = fmul fast float %712, %709
  %716 = fmul fast float %714, %710
  %717 = fdiv fast float 1.000000e+00, %715
  %718 = fdiv fast float 1.000000e+00, %716
  %719 = call float @dx.op.binary.f32(i32 36, float %717, float %718)  ; FMin(a,b)
  %720 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 30)  ; CBufferLoadLegacy(handle,regIndex)
  %721 = extractvalue %dx.types.CBufRet.f32 %720, 3
  %722 = fmul fast float %721, %100
  %723 = fadd fast float %722, %313
  %724 = call float @dx.op.unary.f32(i32 21, float %665)  ; Exp(value)
  %725 = fmul fast float %724, %719
  %726 = fmul fast float %725, %723
  %727 = call float @dx.op.binary.f32(i32 35, float 0x3FB99999A0000000, float %726)  ; FMax(a,b)
  %728 = fdiv fast float %609, %533
  %729 = call float @dx.op.unary.f32(i32 6, float %728)  ; FAbs(value)
  %730 = fmul fast float %669, %526
  %731 = fmul fast float %730, %727
  %732 = fmul fast float %731, %524
  %733 = fmul fast float %727, %524
  %734 = fmul fast float %733, %729
  %735 = fcmp fast oge float %539, 0.000000e+00
  %736 = select i1 %735, float 1.000000e+00, float -1.000000e+00
  %737 = fadd fast float %736, %539
  %738 = fdiv fast float 1.000000e+00, %737
  %739 = fsub fast float -0.000000e+00, %738
  %740 = fmul fast float %537, %538
  %741 = fmul fast float %740, %739
  %742 = fmul fast float %537, %537
  %743 = fmul fast float %742, %736
  %744 = fmul fast float %743, %739
  %745 = fadd fast float %744, 1.000000e+00
  %746 = fmul fast float %741, %736
  %747 = fmul fast float %537, %736
  %748 = fsub fast float -0.000000e+00, %747
  %749 = fmul fast float %538, %538
  %750 = fmul fast float %749, %739
  %751 = fadd fast float %750, %736
  %752 = fsub fast float -0.000000e+00, %538
  %753 = fmul fast float %745, %365
  %754 = call float @dx.op.tertiary.f32(i32 46, float %746, float %366, float %753)  ; FMad(a,b,c)
  %755 = call float @dx.op.tertiary.f32(i32 46, float %748, float %367, float %754)  ; FMad(a,b,c)
  %756 = fmul fast float %741, %365
  %757 = call float @dx.op.tertiary.f32(i32 46, float %751, float %366, float %756)  ; FMad(a,b,c)
  %758 = call float @dx.op.tertiary.f32(i32 46, float %752, float %367, float %757)  ; FMad(a,b,c)
  %759 = fmul fast float %537, %365
  %760 = call float @dx.op.tertiary.f32(i32 46, float %538, float %366, float %759)  ; FMad(a,b,c)
  %761 = call float @dx.op.tertiary.f32(i32 46, float %539, float %367, float %760)  ; FMad(a,b,c)
  %762 = fsub fast float -0.000000e+00, %755
  %763 = fsub fast float -0.000000e+00, %758
  %764 = fdiv fast float %762, %761
  %765 = fdiv fast float %763, %761
  %766 = fcmp fast oeq float %540, 0.000000e+00
  %767 = select i1 %766, i32 0, i32 %528
  br label %768

; <label>:768                                     ; preds = %1971, %602
  %769 = phi float [ %1979, %1971 ], [ %162, %602 ]
  %770 = phi i32 [ %1980, %1971 ], [ 0, %602 ]
  %771 = phi i32 [ %1959, %1971 ], [ 0, %602 ]
  %772 = phi float [ %1960, %1971 ], [ 0.000000e+00, %602 ]
  %773 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %30, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %774 = extractvalue %dx.types.CBufRet.i32 %773, 2
  %775 = shl i32 %774, 16
  %776 = add i32 %775, %577
  %777 = mul i32 %776, %518
  %778 = add i32 %777, %770
  %779 = call i32 @dx.op.unary.i32(i32 30, i32 %778)  ; Bfrev(value)
  %780 = add i32 %779, 1216234700
  %781 = mul i32 %780, -1676577210
  %782 = xor i32 %781, %780
  %783 = mul i32 %782, -529506958
  %784 = xor i32 %783, %782
  %785 = call i32 @dx.op.unary.i32(i32 30, i32 %784)  ; Bfrev(value)
  %786 = and i32 %785, 255
  %787 = and i32 %785, 1
  %788 = lshr i32 %785, 1
  %789 = and i32 %788, 1
  %790 = sub nsw i32 0, %789
  %791 = and i32 %790, 3
  %792 = xor i32 %791, %787
  %793 = lshr i32 %785, 2
  %794 = and i32 %793, 1
  %795 = sub nsw i32 0, %794
  %796 = and i32 %795, 5
  %797 = xor i32 %792, %796
  %798 = and i32 %795, 6
  %799 = shl nuw nsw i32 %794, 2
  %800 = xor i32 %792, %798
  %801 = or i32 %792, %799
  %802 = lshr i32 %785, 3
  %803 = and i32 %802, 1
  %804 = sub nsw i32 0, %803
  %805 = and i32 %804, 15
  %806 = xor i32 %797, %805
  %807 = and i32 %804, 9
  %808 = and i32 %804, 10
  %809 = xor i32 %800, %807
  %810 = lshr i32 %785, 4
  %811 = and i32 %810, 1
  %812 = sub nsw i32 0, %811
  %813 = and i32 %812, 17
  %814 = xor i32 %806, %813
  %815 = and i32 %812, 23
  %816 = and i32 %812, 31
  %817 = xor i32 %809, %815
  %818 = lshr i32 %785, 5
  %819 = and i32 %818, 1
  %820 = sub nsw i32 0, %819
  %821 = and i32 %820, 51
  %822 = xor i32 %814, %821
  %823 = and i32 %820, 58
  %824 = and i32 %820, 46
  %825 = xor i32 %817, %823
  %826 = lshr i32 %785, 6
  %827 = and i32 %826, 1
  %828 = sub nsw i32 0, %827
  %829 = and i32 %828, 85
  %830 = xor i32 %822, %829
  %831 = and i32 %828, 113
  %832 = and i32 %828, 69
  %833 = xor i32 %825, %831
  %834 = lshr i32 %785, 7
  %835 = and i32 %834, 1
  %836 = sub nsw i32 0, %835
  %837 = and i32 %836, 255
  %838 = xor i32 %830, %837
  %839 = and i32 %836, 163
  %840 = and i32 %836, 201
  %841 = xor i32 %833, %839
  %842 = xor i32 %816, %808
  %843 = xor i32 %842, %824
  %844 = xor i32 %843, %832
  %845 = xor i32 %844, %840
  %846 = xor i32 %845, %801
  %847 = add nsw i32 %786, -1862497895
  %848 = mul i32 %847, -1676577210
  %849 = xor i32 %848, %847
  %850 = mul i32 %849, -529506958
  %851 = xor i32 %850, %849
  %852 = call i32 @dx.op.unary.i32(i32 30, i32 %851)  ; Bfrev(value)
  %853 = add i32 %838, -646066581
  %854 = mul i32 %853, -1676577210
  %855 = xor i32 %854, %853
  %856 = mul i32 %855, -529506958
  %857 = xor i32 %856, %855
  %858 = call i32 @dx.op.unary.i32(i32 30, i32 %857)  ; Bfrev(value)
  %859 = add i32 %841, 570102578
  %860 = mul i32 %859, -1676577210
  %861 = xor i32 %860, %859
  %862 = add nuw i32 %846, 1786441729
  %863 = mul i32 %862, -1676577210
  %864 = xor i32 %863, %862
  %865 = lshr i32 %852, 8
  %866 = lshr i32 %858, 8
  %867 = uitofp i32 %865 to float
  %868 = uitofp i32 %866 to float
  %869 = fmul fast float %867, 0x3E76A09E60000000
  %870 = fmul fast float %868, 0x3E76A09E60000000
  %871 = fadd fast float %869, 0xBFE6A09E60000000
  %872 = fadd fast float %870, 0xBFE6A09E60000000
  %873 = fmul fast float %871, %871
  %874 = fmul fast float %872, %872
  %875 = call float @dx.op.binary.f32(i32 35, float %873, float %874)  ; FMax(a,b)
  %876 = fmul fast float %875, 2.000000e+00
  %877 = call float @dx.op.binary.f32(i32 36, float %873, float %874)  ; FMin(a,b)
  %878 = fsub fast float %876, %877
  %879 = call float @dx.op.unary.f32(i32 24, float %878)  ; Sqrt(value)
  %880 = fcmp fast ogt float %873, %874
  %881 = fsub fast float -0.000000e+00, %879
  %882 = fcmp fast ogt float %871, 0.000000e+00
  %883 = select i1 %882, float %879, float %881
  %884 = fcmp fast ogt float %872, 0.000000e+00
  %885 = select i1 %884, float %879, float %881
  %886 = select i1 %880, float %883, float %871
  %887 = select i1 %880, float %872, float %885
  %888 = fmul fast float %886, %540
  %889 = fmul fast float %887, %540
  %890 = call float @dx.op.dot2.f32(i32 54, float %888, float %889, float %888, float %889)  ; Dot2(ax,ay,bx,by)
  %891 = call float @dx.op.unary.f32(i32 24, float %890)  ; Sqrt(value)
  %892 = fsub fast float 1.000000e+00, %890
  %893 = call float @dx.op.unary.f32(i32 24, float %892)  ; Sqrt(value)
  %894 = fmul fast float %888, %745
  %895 = call float @dx.op.tertiary.f32(i32 46, float %889, float %741, float %894)  ; FMad(a,b,c)
  %896 = call float @dx.op.tertiary.f32(i32 46, float %893, float %537, float %895)  ; FMad(a,b,c)
  %897 = fmul fast float %888, %746
  %898 = call float @dx.op.tertiary.f32(i32 46, float %889, float %751, float %897)  ; FMad(a,b,c)
  %899 = call float @dx.op.tertiary.f32(i32 46, float %893, float %538, float %898)  ; FMad(a,b,c)
  %900 = fmul fast float %888, %748
  %901 = call float @dx.op.tertiary.f32(i32 46, float %889, float %752, float %900)  ; FMad(a,b,c)
  %902 = call float @dx.op.tertiary.f32(i32 46, float %893, float %539, float %901)  ; FMad(a,b,c)
  %903 = fcmp fast ogt float %731, 0.000000e+00
  br i1 %903, label %904, label %937

; <label>:904                                     ; preds = %768
  %905 = mul i32 %864, -529506958
  %906 = xor i32 %905, %864
  %907 = call i32 @dx.op.unary.i32(i32 30, i32 %906)  ; Bfrev(value)
  %908 = lshr i32 %907, 8
  %909 = uitofp i32 %908 to float
  %910 = fmul fast float %909, 0x3E70000000000000
  %911 = mul i32 %861, -529506958
  %912 = xor i32 %911, %861
  %913 = call i32 @dx.op.unary.i32(i32 30, i32 %912)  ; Bfrev(value)
  %914 = lshr i32 %913, 8
  %915 = uitofp i32 %914 to float
  %916 = fmul fast float %915, 0x3E70000000000000
  %917 = fadd fast float %916, -5.000000e-01
  %918 = fadd fast float %910, -5.000000e-01
  %919 = fmul fast float %917, %731
  %920 = fmul fast float %918, %731
  %921 = call float @dx.op.dot2.f32(i32 54, float %764, float %765, float %919, float %920)  ; Dot2(ax,ay,bx,by)
  %922 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %921)  ; FMax(a,b)
  %923 = fmul fast float %922, 2.000000e+00
  %924 = call float @dx.op.binary.f32(i32 36, float %732, float %923)  ; FMin(a,b)
  %925 = fmul fast float %919, %745
  %926 = call float @dx.op.tertiary.f32(i32 46, float %920, float %741, float %925)  ; FMad(a,b,c)
  %927 = call float @dx.op.tertiary.f32(i32 46, float %924, float %537, float %926)  ; FMad(a,b,c)
  %928 = fmul fast float %919, %746
  %929 = call float @dx.op.tertiary.f32(i32 46, float %920, float %751, float %928)  ; FMad(a,b,c)
  %930 = call float @dx.op.tertiary.f32(i32 46, float %924, float %538, float %929)  ; FMad(a,b,c)
  %931 = fmul fast float %919, %748
  %932 = call float @dx.op.tertiary.f32(i32 46, float %920, float %752, float %931)  ; FMad(a,b,c)
  %933 = call float @dx.op.tertiary.f32(i32 46, float %924, float %539, float %932)  ; FMad(a,b,c)
  %934 = fadd fast float %927, %687
  %935 = fadd fast float %930, %688
  %936 = fadd fast float %933, %689
  br label %937

; <label>:937                                     ; preds = %904, %768
  %938 = phi float [ %934, %904 ], [ %687, %768 ]
  %939 = phi float [ %935, %904 ], [ %688, %768 ]
  %940 = phi float [ %936, %904 ], [ %689, %768 ]
  %941 = fmul fast float %891, %527
  %942 = fadd fast float %893, %941
  %943 = fdiv fast float 1.500000e+00, %942
  %944 = call float @dx.op.unary.f32(i32 7, float %943)  ; Saturate(value)
  %945 = fmul fast float %533, 7.500000e-01
  %946 = fmul fast float %945, %536
  %947 = fmul fast float %946, %944
  %948 = fadd fast float %947, 0xBEB0C6F7A0000000
  %949 = call float @dx.op.binary.f32(i32 36, float %516, float %948)  ; FMin(a,b)
  %950 = fmul fast float %949, %896
  %951 = fmul fast float %949, %899
  %952 = fmul fast float %949, %902
  %953 = fadd fast float %950, %938
  %954 = fadd fast float %951, %939
  %955 = fadd fast float %952, %940
  %956 = fmul fast float %947, %896
  %957 = fmul fast float %947, %899
  %958 = fmul fast float %947, %902
  %959 = fadd fast float %956, %938
  %960 = fadd fast float %957, %939
  %961 = fadd fast float %958, %940
  %962 = fcmp fast ogt float %734, 0.000000e+00
  br i1 %962, label %963, label %1044

; <label>:963                                     ; preds = %937
  %964 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %965 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %964, i32 %610, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %966 = extractvalue %dx.types.ResRet.i32 %965, 0
  %967 = extractvalue %dx.types.ResRet.i32 %965, 1
  %968 = extractvalue %dx.types.ResRet.i32 %965, 3
  %969 = bitcast i32 %966 to float
  %970 = bitcast i32 %967 to float
  %971 = bitcast i32 %968 to float
  %972 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %964, i32 %611, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %973 = extractvalue %dx.types.ResRet.i32 %972, 0
  %974 = extractvalue %dx.types.ResRet.i32 %972, 1
  %975 = extractvalue %dx.types.ResRet.i32 %972, 3
  %976 = bitcast i32 %973 to float
  %977 = bitcast i32 %974 to float
  %978 = bitcast i32 %975 to float
  %979 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %964, i32 %612, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %980 = extractvalue %dx.types.ResRet.i32 %979, 0
  %981 = extractvalue %dx.types.ResRet.i32 %979, 1
  %982 = extractvalue %dx.types.ResRet.i32 %979, 3
  %983 = bitcast i32 %980 to float
  %984 = bitcast i32 %981 to float
  %985 = bitcast i32 %982 to float
  %986 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %964, i32 %613, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %987 = extractvalue %dx.types.ResRet.i32 %986, 0
  %988 = extractvalue %dx.types.ResRet.i32 %986, 1
  %989 = extractvalue %dx.types.ResRet.i32 %986, 3
  %990 = bitcast i32 %987 to float
  %991 = bitcast i32 %988 to float
  %992 = bitcast i32 %989 to float
  %993 = fmul fast float %969, %953
  %994 = call float @dx.op.tertiary.f32(i32 46, float %954, float %976, float %993)  ; FMad(a,b,c)
  %995 = call float @dx.op.tertiary.f32(i32 46, float %955, float %983, float %994)  ; FMad(a,b,c)
  %996 = fadd fast float %995, %990
  %997 = fmul fast float %970, %953
  %998 = call float @dx.op.tertiary.f32(i32 46, float %954, float %977, float %997)  ; FMad(a,b,c)
  %999 = call float @dx.op.tertiary.f32(i32 46, float %955, float %984, float %998)  ; FMad(a,b,c)
  %1000 = fadd fast float %999, %991
  %1001 = fmul fast float %971, %953
  %1002 = call float @dx.op.tertiary.f32(i32 46, float %954, float %978, float %1001)  ; FMad(a,b,c)
  %1003 = call float @dx.op.tertiary.f32(i32 46, float %955, float %985, float %1002)  ; FMad(a,b,c)
  %1004 = fadd fast float %1003, %992
  %1005 = fdiv fast float %996, %1004
  %1006 = fdiv fast float %1000, %1004
  %1007 = icmp ult i32 %603, 8192
  br i1 %1007, label %1018, label %1008

; <label>:1008                                    ; preds = %963
  %1009 = fmul fast float %1006, 1.280000e+02
  %1010 = fptoui float %1009 to i32
  %1011 = fmul fast float %1005, 1.280000e+02
  %1012 = fptoui float %1011 to i32
  %1013 = mul i32 %603, 21845
  %1014 = shl i32 %1010, 7
  %1015 = add i32 %1013, -178946048
  %1016 = add i32 %1015, %1012
  %1017 = add i32 %1016, %1014
  br label %1018

; <label>:1018                                    ; preds = %1008, %963
  %1019 = phi i32 [ %1017, %1008 ], [ %603, %963 ]
  %1020 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1021 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1020, i32 %1019, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1022 = extractvalue %dx.types.ResRet.i32 %1021, 0
  %1023 = lshr i32 %1022, 20
  %1024 = and i32 %1023, 31
  %1025 = lshr i32 16384, %1024
  %1026 = uitofp i32 %1025 to float
  %1027 = select i1 %1007, float 1.280000e+02, float %1026
  %1028 = fmul fast float %1027, %1005
  %1029 = fmul fast float %1027, %1006
  %1030 = fptoui float %1028 to i32
  %1031 = fptoui float %1029 to i32
  %1032 = uitofp i32 %1030 to float
  %1033 = uitofp i32 %1031 to float
  %1034 = fsub fast float 5.000000e-01, %1028
  %1035 = fadd fast float %1034, %1032
  %1036 = fsub fast float 5.000000e-01, %1029
  %1037 = fadd fast float %1036, %1033
  %1038 = fdiv fast float %1035, %1027
  %1039 = fdiv fast float %1037, %1027
  %1040 = call float @dx.op.dot2.f32(i32 54, float %706, float %707, float %1038, float %1039)  ; Dot2(ax,ay,bx,by)
  %1041 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %1040)  ; FMax(a,b)
  %1042 = fmul fast float %1041, 2.000000e+00
  %1043 = call float @dx.op.binary.f32(i32 36, float %734, float %1042)  ; FMin(a,b)
  br label %1044

; <label>:1044                                    ; preds = %1018, %937
  %1045 = phi float [ %1043, %1018 ], [ 0.000000e+00, %937 ]
  br i1 %529, label %1046, label %1253

; <label>:1046                                    ; preds = %1044
  %1047 = mul i32 %285, 288
  %1048 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1049 = add i32 %1047, 64
  %1050 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1048, i32 %1049, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1051 = extractvalue %dx.types.ResRet.i32 %1050, 0
  %1052 = extractvalue %dx.types.ResRet.i32 %1050, 1
  %1053 = extractvalue %dx.types.ResRet.i32 %1050, 2
  %1054 = extractvalue %dx.types.ResRet.i32 %1050, 3
  %1055 = bitcast i32 %1051 to float
  %1056 = bitcast i32 %1052 to float
  %1057 = bitcast i32 %1053 to float
  %1058 = bitcast i32 %1054 to float
  %1059 = add i32 %1047, 80
  %1060 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1048, i32 %1059, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1061 = extractvalue %dx.types.ResRet.i32 %1060, 0
  %1062 = extractvalue %dx.types.ResRet.i32 %1060, 1
  %1063 = extractvalue %dx.types.ResRet.i32 %1060, 2
  %1064 = extractvalue %dx.types.ResRet.i32 %1060, 3
  %1065 = bitcast i32 %1061 to float
  %1066 = bitcast i32 %1062 to float
  %1067 = bitcast i32 %1063 to float
  %1068 = bitcast i32 %1064 to float
  %1069 = add i32 %1047, 96
  %1070 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1048, i32 %1069, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1071 = extractvalue %dx.types.ResRet.i32 %1070, 0
  %1072 = extractvalue %dx.types.ResRet.i32 %1070, 1
  %1073 = extractvalue %dx.types.ResRet.i32 %1070, 2
  %1074 = extractvalue %dx.types.ResRet.i32 %1070, 3
  %1075 = bitcast i32 %1071 to float
  %1076 = bitcast i32 %1072 to float
  %1077 = bitcast i32 %1073 to float
  %1078 = bitcast i32 %1074 to float
  %1079 = add i32 %1047, 112
  %1080 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1048, i32 %1079, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1081 = extractvalue %dx.types.ResRet.i32 %1080, 0
  %1082 = extractvalue %dx.types.ResRet.i32 %1080, 1
  %1083 = extractvalue %dx.types.ResRet.i32 %1080, 2
  %1084 = extractvalue %dx.types.ResRet.i32 %1080, 3
  %1085 = bitcast i32 %1081 to float
  %1086 = bitcast i32 %1082 to float
  %1087 = bitcast i32 %1083 to float
  %1088 = bitcast i32 %1084 to float
  %1089 = fmul fast float %1055, %953
  %1090 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1065, float %1089)  ; FMad(a,b,c)
  %1091 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1075, float %1090)  ; FMad(a,b,c)
  %1092 = fadd fast float %1091, %1085
  %1093 = fmul fast float %1056, %953
  %1094 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1066, float %1093)  ; FMad(a,b,c)
  %1095 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1076, float %1094)  ; FMad(a,b,c)
  %1096 = fadd fast float %1095, %1086
  %1097 = fmul fast float %1057, %953
  %1098 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1067, float %1097)  ; FMad(a,b,c)
  %1099 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1077, float %1098)  ; FMad(a,b,c)
  %1100 = fadd fast float %1099, %1087
  %1101 = fmul fast float %1058, %953
  %1102 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1068, float %1101)  ; FMad(a,b,c)
  %1103 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1078, float %1102)  ; FMad(a,b,c)
  %1104 = fadd fast float %1103, %1088
  %1105 = fmul fast float %1055, %959
  %1106 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1065, float %1105)  ; FMad(a,b,c)
  %1107 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1075, float %1106)  ; FMad(a,b,c)
  %1108 = fadd fast float %1107, %1085
  %1109 = fmul fast float %1056, %959
  %1110 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1066, float %1109)  ; FMad(a,b,c)
  %1111 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1076, float %1110)  ; FMad(a,b,c)
  %1112 = fadd fast float %1111, %1086
  %1113 = fmul fast float %1057, %959
  %1114 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1067, float %1113)  ; FMad(a,b,c)
  %1115 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1077, float %1114)  ; FMad(a,b,c)
  %1116 = fadd fast float %1115, %1087
  %1117 = fmul fast float %1058, %959
  %1118 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1068, float %1117)  ; FMad(a,b,c)
  %1119 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1078, float %1118)  ; FMad(a,b,c)
  %1120 = fadd fast float %1119, %1088
  %1121 = fdiv fast float %1092, %1104
  %1122 = fdiv fast float %1096, %1104
  %1123 = fdiv fast float %1100, %1104
  %1124 = fdiv fast float %1108, %1120
  %1125 = fdiv fast float %1112, %1120
  %1126 = fdiv fast float %1116, %1120
  %1127 = fsub fast float %1124, %1121
  %1128 = fsub fast float %1125, %1122
  %1129 = fsub fast float %1126, %1123
  %1130 = call float @dx.op.unary.f32(i32 7, float %1121)  ; Saturate(value)
  %1131 = call float @dx.op.unary.f32(i32 7, float %1122)  ; Saturate(value)
  %1132 = fadd fast float %1123, %1045
  %1133 = fmul fast float %1129, %525
  %1134 = sitofp i32 %767 to float
  %1135 = fdiv fast float -1.000000e+00, %1134
  %1136 = fsub fast float 1.000000e+00, %769
  %1137 = icmp sgt i32 %767, -1
  br i1 %1137, label %1138, label %1923

; <label>:1138                                    ; preds = %1046
  br label %1139

; <label>:1139                                    ; preds = %1246, %1138
  %1140 = phi float [ %1247, %1246 ], [ -1.000000e+04, %1138 ]
  %1141 = phi float [ %1248, %1246 ], [ -1.000000e+00, %1138 ]
  %1142 = phi float [ %1249, %1246 ], [ 0.000000e+00, %1138 ]
  %1143 = phi float [ %1250, %1246 ], [ -1.000000e+00, %1138 ]
  %1144 = phi i32 [ %1251, %1246 ], [ 0, %1138 ]
  %1145 = icmp eq i32 %1144, %767
  br i1 %1145, label %1152, label %1146

; <label>:1146                                    ; preds = %1139
  %1147 = sitofp i32 %1144 to float
  %1148 = fadd fast float %1147, %1136
  %1149 = fmul fast float %1148, %1135
  %1150 = fadd fast float %1149, 1.000000e+00
  %1151 = fmul fast float %1150, %1150
  br label %1152

; <label>:1152                                    ; preds = %1146, %1139
  %1153 = phi float [ %1151, %1146 ], [ 0.000000e+00, %1139 ]
  %1154 = fmul fast float %1153, %1127
  %1155 = fmul fast float %1153, %1128
  %1156 = fmul fast float %1153, %1129
  %1157 = fadd fast float %1154, %1130
  %1158 = fadd fast float %1155, %1131
  %1159 = fadd fast float %1156, %1132
  %1160 = call float @dx.op.unary.f32(i32 7, float %1157)  ; Saturate(value)
  %1161 = call float @dx.op.unary.f32(i32 7, float %1158)  ; Saturate(value)
  %1162 = fcmp fast oeq float %1157, %1160
  %1163 = fcmp fast oeq float %1158, %1161
  %1164 = and i1 %1162, %1163
  br i1 %1164, label %1165, label %1210

; <label>:1165                                    ; preds = %1152
  %1166 = icmp ult i32 %285, 8192
  br i1 %1166, label %1177, label %1167

; <label>:1167                                    ; preds = %1165
  %1168 = fmul fast float %1158, 1.280000e+02
  %1169 = fptoui float %1168 to i32
  %1170 = fmul fast float %1157, 1.280000e+02
  %1171 = fptoui float %1170 to i32
  %1172 = mul i32 %285, 21845
  %1173 = shl i32 %1169, 7
  %1174 = add i32 %1172, -178946048
  %1175 = add i32 %1174, %1171
  %1176 = add i32 %1175, %1173
  br label %1177

; <label>:1177                                    ; preds = %1167, %1165
  %1178 = phi i32 [ %1176, %1167 ], [ %285, %1165 ]
  %1179 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1180 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1179, i32 %1178, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1181 = extractvalue %dx.types.ResRet.i32 %1180, 0
  %1182 = lshr i32 %1181, 20
  %1183 = icmp slt i32 %1181, 0
  %1184 = and i32 %1182, 31
  %1185 = lshr i32 16384, %1184
  %1186 = uitofp i32 %1185 to float
  %1187 = select i1 %1166, float 1.280000e+02, float %1186
  br i1 %1183, label %1188, label %1205

; <label>:1188                                    ; preds = %1177
  %1189 = fmul fast float %1187, %1158
  %1190 = fptoui float %1189 to i32
  %1191 = and i32 %1190, 127
  %1192 = lshr i32 %1181, 3
  %1193 = and i32 %1192, 130944
  %1194 = or i32 %1191, %1193
  %1195 = fmul fast float %1187, %1157
  %1196 = fptoui float %1195 to i32
  %1197 = and i32 %1196, 127
  %1198 = shl i32 %1181, 7
  %1199 = and i32 %1198, 130944
  %1200 = or i32 %1197, %1199
  %1201 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1202 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1201, i32 0, i32 %1200, i32 %1194, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1203 = extractvalue %dx.types.ResRet.i32 %1202, 0
  %1204 = bitcast i32 %1203 to float
  br label %1205

; <label>:1205                                    ; preds = %1188, %1177
  %1206 = phi i1 [ true, %1188 ], [ false, %1177 ]
  %1207 = phi float [ %1204, %1188 ], [ 0.000000e+00, %1177 ]
  %1208 = select i1 %1206, float %1207, float 0.000000e+00
  %1209 = zext i1 %1206 to i32
  br label %1210

; <label>:1210                                    ; preds = %1205, %1152
  %1211 = phi float [ 0.000000e+00, %1152 ], [ %1208, %1205 ]
  %1212 = phi i32 [ 0, %1152 ], [ %1209, %1205 ]
  %1213 = icmp eq i32 %1212, 0
  br i1 %1213, label %1246, label %1214

; <label>:1214                                    ; preds = %1210
  %1215 = fcmp fast oeq float %1140, -1.000000e+04
  br i1 %1215, label %1216, label %1218

; <label>:1216                                    ; preds = %1214
  %1217 = fcmp fast ogt float %1211, %1159
  br i1 %1217, label %1917, label %1246

; <label>:1218                                    ; preds = %1214
  %1219 = fsub fast float %1159, %1143
  %1220 = call float @dx.op.unary.f32(i32 6, float %1219)  ; FAbs(value)
  %1221 = fmul fast float %1220, 0x3FF0CCCCC0000000
  %1222 = fsub fast float %1211, %1159
  %1223 = fcmp fast ogt float %1222, %1221
  %1224 = fsub fast float %1153, %1141
  br i1 %1223, label %1225, label %1228

; <label>:1225                                    ; preds = %1218
  %1226 = fmul fast float %1224, %1142
  %1227 = fadd fast float %1226, %1140
  br label %1236

; <label>:1228                                    ; preds = %1218
  %1229 = fcmp fast une float %1211, %1140
  br i1 %1229, label %1230, label %1236

; <label>:1230                                    ; preds = %1228
  %1231 = fsub fast float %1211, %1140
  %1232 = fdiv fast float %1231, %1224
  %1233 = fsub fast float -0.000000e+00, %1133
  %1234 = call float @dx.op.binary.f32(i32 35, float %1232, float %1233)  ; FMax(a,b)
  %1235 = call float @dx.op.binary.f32(i32 36, float %1234, float %1133)  ; FMin(a,b)
  br label %1236

; <label>:1236                                    ; preds = %1230, %1228, %1225
  %1237 = phi float [ %1140, %1225 ], [ %1211, %1230 ], [ %1140, %1228 ]
  %1238 = phi float [ %1141, %1225 ], [ %1153, %1230 ], [ %1141, %1228 ]
  %1239 = phi float [ %1142, %1225 ], [ %1235, %1230 ], [ %1142, %1228 ]
  %1240 = phi float [ %1227, %1225 ], [ %1211, %1230 ], [ %1211, %1228 ]
  %1241 = fmul fast float %1220, 0x3FE0CCCCC0000000
  %1242 = fadd fast float %1241, %1159
  %1243 = fsub fast float %1242, %1240
  %1244 = call float @dx.op.unary.f32(i32 6, float %1243)  ; FAbs(value)
  %1245 = fcmp fast olt float %1244, %1241
  br i1 %1245, label %1917, label %1246

; <label>:1246                                    ; preds = %1236, %1216, %1210
  %1247 = phi float [ %1140, %1210 ], [ %1211, %1216 ], [ %1237, %1236 ]
  %1248 = phi float [ %1141, %1210 ], [ %1153, %1216 ], [ %1238, %1236 ]
  %1249 = phi float [ %1142, %1210 ], [ %1142, %1216 ], [ %1239, %1236 ]
  %1250 = phi float [ %1143, %1210 ], [ %1159, %1216 ], [ %1159, %1236 ]
  %1251 = add nuw nsw i32 %1144, 1
  %1252 = icmp slt i32 %1144, %767
  br i1 %1252, label %1139, label %1917

; <label>:1253                                    ; preds = %1044
  %1254 = call float @dx.op.unary.f32(i32 6, float %953)  ; FAbs(value)
  %1255 = call float @dx.op.unary.f32(i32 6, float %954)  ; FAbs(value)
  %1256 = fcmp fast ult float %1254, %1255
  %1257 = call float @dx.op.unary.f32(i32 6, float %955)  ; FAbs(value)
  %1258 = fcmp fast ult float %1254, %1257
  %1259 = or i1 %1256, %1258
  br i1 %1259, label %1263, label %1260

; <label>:1260                                    ; preds = %1253
  %1261 = fcmp ule float %953, 0.000000e+00
  %1262 = zext i1 %1261 to i32
  br label %1271

; <label>:1263                                    ; preds = %1253
  %1264 = fcmp fast ogt float %1255, %1257
  br i1 %1264, label %1265, label %1268

; <label>:1265                                    ; preds = %1263
  %1266 = fcmp fast ogt float %954, 0.000000e+00
  %1267 = select i1 %1266, i32 2, i32 3
  br label %1271

; <label>:1268                                    ; preds = %1263
  %1269 = fcmp fast ogt float %955, 0.000000e+00
  %1270 = select i1 %1269, i32 4, i32 5
  br label %1271

; <label>:1271                                    ; preds = %1268, %1265, %1260
  %1272 = phi i32 [ %1262, %1260 ], [ %1267, %1265 ], [ %1270, %1268 ]
  %1273 = call float @dx.op.unary.f32(i32 6, float %959)  ; FAbs(value)
  %1274 = call float @dx.op.unary.f32(i32 6, float %960)  ; FAbs(value)
  %1275 = fcmp fast ult float %1273, %1274
  %1276 = call float @dx.op.unary.f32(i32 6, float %961)  ; FAbs(value)
  %1277 = fcmp fast ult float %1273, %1276
  %1278 = or i1 %1275, %1277
  br i1 %1278, label %1282, label %1279

; <label>:1279                                    ; preds = %1271
  %1280 = fcmp ule float %959, 0.000000e+00
  %1281 = zext i1 %1280 to i32
  br label %1290

; <label>:1282                                    ; preds = %1271
  %1283 = fcmp fast ogt float %1274, %1276
  br i1 %1283, label %1284, label %1287

; <label>:1284                                    ; preds = %1282
  %1285 = fcmp fast ogt float %960, 0.000000e+00
  %1286 = select i1 %1285, i32 2, i32 3
  br label %1290

; <label>:1287                                    ; preds = %1282
  %1288 = fcmp fast ogt float %961, 0.000000e+00
  %1289 = select i1 %1288, i32 4, i32 5
  br label %1290

; <label>:1290                                    ; preds = %1287, %1284, %1279
  %1291 = phi i32 [ %1281, %1279 ], [ %1286, %1284 ], [ %1289, %1287 ]
  %1292 = icmp ne i32 %1272, %1291
  %1293 = call i1 @dx.op.waveAnyTrue(i32 113, i1 %1292)  ; WaveAnyTrue(cond)
  %1294 = add i32 %1291, %285
  %1295 = mul i32 %1294, 288
  %1296 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1297 = add i32 %1295, 64
  %1298 = add i32 %1295, 80
  %1299 = add i32 %1295, 96
  %1300 = add i32 %1295, 112
  %1301 = sitofp i32 %767 to float
  %1302 = fdiv fast float -1.000000e+00, %1301
  %1303 = fsub fast float 1.000000e+00, %769
  %1304 = icmp sgt i32 %767, -1
  br i1 %1293, label %1305, label %1720

; <label>:1305                                    ; preds = %1290
  %1306 = add i32 %1272, %285
  %1307 = mul i32 %1306, 288
  %1308 = add i32 %1307, 64
  %1309 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1308, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1310 = extractvalue %dx.types.ResRet.i32 %1309, 0
  %1311 = extractvalue %dx.types.ResRet.i32 %1309, 1
  %1312 = extractvalue %dx.types.ResRet.i32 %1309, 2
  %1313 = extractvalue %dx.types.ResRet.i32 %1309, 3
  %1314 = bitcast i32 %1310 to float
  %1315 = bitcast i32 %1311 to float
  %1316 = bitcast i32 %1312 to float
  %1317 = bitcast i32 %1313 to float
  %1318 = add i32 %1307, 80
  %1319 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1318, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1320 = extractvalue %dx.types.ResRet.i32 %1319, 0
  %1321 = extractvalue %dx.types.ResRet.i32 %1319, 1
  %1322 = extractvalue %dx.types.ResRet.i32 %1319, 2
  %1323 = extractvalue %dx.types.ResRet.i32 %1319, 3
  %1324 = bitcast i32 %1320 to float
  %1325 = bitcast i32 %1321 to float
  %1326 = bitcast i32 %1322 to float
  %1327 = bitcast i32 %1323 to float
  %1328 = add i32 %1307, 96
  %1329 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1328, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1330 = extractvalue %dx.types.ResRet.i32 %1329, 0
  %1331 = extractvalue %dx.types.ResRet.i32 %1329, 1
  %1332 = extractvalue %dx.types.ResRet.i32 %1329, 2
  %1333 = extractvalue %dx.types.ResRet.i32 %1329, 3
  %1334 = bitcast i32 %1330 to float
  %1335 = bitcast i32 %1331 to float
  %1336 = bitcast i32 %1332 to float
  %1337 = bitcast i32 %1333 to float
  %1338 = add i32 %1307, 112
  %1339 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1338, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1340 = extractvalue %dx.types.ResRet.i32 %1339, 0
  %1341 = extractvalue %dx.types.ResRet.i32 %1339, 1
  %1342 = extractvalue %dx.types.ResRet.i32 %1339, 2
  %1343 = extractvalue %dx.types.ResRet.i32 %1339, 3
  %1344 = bitcast i32 %1340 to float
  %1345 = bitcast i32 %1341 to float
  %1346 = bitcast i32 %1342 to float
  %1347 = bitcast i32 %1343 to float
  %1348 = fmul fast float %1314, %953
  %1349 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1324, float %1348)  ; FMad(a,b,c)
  %1350 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1334, float %1349)  ; FMad(a,b,c)
  %1351 = fadd fast float %1350, %1344
  %1352 = fmul fast float %1315, %953
  %1353 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1325, float %1352)  ; FMad(a,b,c)
  %1354 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1335, float %1353)  ; FMad(a,b,c)
  %1355 = fadd fast float %1354, %1345
  %1356 = fmul fast float %1316, %953
  %1357 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1326, float %1356)  ; FMad(a,b,c)
  %1358 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1336, float %1357)  ; FMad(a,b,c)
  %1359 = fadd fast float %1358, %1346
  %1360 = fmul fast float %1317, %953
  %1361 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1327, float %1360)  ; FMad(a,b,c)
  %1362 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1337, float %1361)  ; FMad(a,b,c)
  %1363 = fadd fast float %1362, %1347
  %1364 = fmul fast float %1314, %959
  %1365 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1324, float %1364)  ; FMad(a,b,c)
  %1366 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1334, float %1365)  ; FMad(a,b,c)
  %1367 = fadd fast float %1366, %1344
  %1368 = fmul fast float %1315, %959
  %1369 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1325, float %1368)  ; FMad(a,b,c)
  %1370 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1335, float %1369)  ; FMad(a,b,c)
  %1371 = fadd fast float %1370, %1345
  %1372 = fmul fast float %1316, %959
  %1373 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1326, float %1372)  ; FMad(a,b,c)
  %1374 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1336, float %1373)  ; FMad(a,b,c)
  %1375 = fadd fast float %1374, %1346
  %1376 = fmul fast float %1317, %959
  %1377 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1327, float %1376)  ; FMad(a,b,c)
  %1378 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1337, float %1377)  ; FMad(a,b,c)
  %1379 = fadd fast float %1378, %1347
  %1380 = fdiv fast float %1351, %1363
  %1381 = fdiv fast float %1355, %1363
  %1382 = fdiv fast float %1359, %1363
  %1383 = fdiv fast float %1367, %1379
  %1384 = fdiv fast float %1371, %1379
  %1385 = fdiv fast float %1375, %1379
  %1386 = fsub fast float %1383, %1380
  %1387 = fsub fast float %1384, %1381
  %1388 = fsub fast float %1385, %1382
  %1389 = fadd fast float %1382, %1045
  %1390 = fmul fast float %1388, %525
  %1391 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1297, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1392 = extractvalue %dx.types.ResRet.i32 %1391, 0
  %1393 = extractvalue %dx.types.ResRet.i32 %1391, 1
  %1394 = extractvalue %dx.types.ResRet.i32 %1391, 2
  %1395 = extractvalue %dx.types.ResRet.i32 %1391, 3
  %1396 = bitcast i32 %1392 to float
  %1397 = bitcast i32 %1393 to float
  %1398 = bitcast i32 %1394 to float
  %1399 = bitcast i32 %1395 to float
  %1400 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1298, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1401 = extractvalue %dx.types.ResRet.i32 %1400, 0
  %1402 = extractvalue %dx.types.ResRet.i32 %1400, 1
  %1403 = extractvalue %dx.types.ResRet.i32 %1400, 2
  %1404 = extractvalue %dx.types.ResRet.i32 %1400, 3
  %1405 = bitcast i32 %1401 to float
  %1406 = bitcast i32 %1402 to float
  %1407 = bitcast i32 %1403 to float
  %1408 = bitcast i32 %1404 to float
  %1409 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1299, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1410 = extractvalue %dx.types.ResRet.i32 %1409, 0
  %1411 = extractvalue %dx.types.ResRet.i32 %1409, 1
  %1412 = extractvalue %dx.types.ResRet.i32 %1409, 2
  %1413 = extractvalue %dx.types.ResRet.i32 %1409, 3
  %1414 = bitcast i32 %1410 to float
  %1415 = bitcast i32 %1411 to float
  %1416 = bitcast i32 %1412 to float
  %1417 = bitcast i32 %1413 to float
  %1418 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1300, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1419 = extractvalue %dx.types.ResRet.i32 %1418, 0
  %1420 = extractvalue %dx.types.ResRet.i32 %1418, 1
  %1421 = extractvalue %dx.types.ResRet.i32 %1418, 2
  %1422 = extractvalue %dx.types.ResRet.i32 %1418, 3
  %1423 = bitcast i32 %1419 to float
  %1424 = bitcast i32 %1420 to float
  %1425 = bitcast i32 %1421 to float
  %1426 = bitcast i32 %1422 to float
  %1427 = fmul fast float %1396, %953
  %1428 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1405, float %1427)  ; FMad(a,b,c)
  %1429 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1414, float %1428)  ; FMad(a,b,c)
  %1430 = fadd fast float %1429, %1423
  %1431 = fmul fast float %1397, %953
  %1432 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1406, float %1431)  ; FMad(a,b,c)
  %1433 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1415, float %1432)  ; FMad(a,b,c)
  %1434 = fadd fast float %1433, %1424
  %1435 = fmul fast float %1398, %953
  %1436 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1407, float %1435)  ; FMad(a,b,c)
  %1437 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1416, float %1436)  ; FMad(a,b,c)
  %1438 = fadd fast float %1437, %1425
  %1439 = fmul fast float %1399, %953
  %1440 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1408, float %1439)  ; FMad(a,b,c)
  %1441 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1417, float %1440)  ; FMad(a,b,c)
  %1442 = fadd fast float %1441, %1426
  %1443 = fmul fast float %1396, %959
  %1444 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1405, float %1443)  ; FMad(a,b,c)
  %1445 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1414, float %1444)  ; FMad(a,b,c)
  %1446 = fadd fast float %1445, %1423
  %1447 = fmul fast float %1397, %959
  %1448 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1406, float %1447)  ; FMad(a,b,c)
  %1449 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1415, float %1448)  ; FMad(a,b,c)
  %1450 = fadd fast float %1449, %1424
  %1451 = fmul fast float %1398, %959
  %1452 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1407, float %1451)  ; FMad(a,b,c)
  %1453 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1416, float %1452)  ; FMad(a,b,c)
  %1454 = fadd fast float %1453, %1425
  %1455 = fmul fast float %1399, %959
  %1456 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1408, float %1455)  ; FMad(a,b,c)
  %1457 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1417, float %1456)  ; FMad(a,b,c)
  %1458 = fadd fast float %1457, %1426
  %1459 = fdiv fast float %1430, %1442
  %1460 = fdiv fast float %1434, %1442
  %1461 = fdiv fast float %1438, %1442
  %1462 = fdiv fast float %1446, %1458
  %1463 = fdiv fast float %1450, %1458
  %1464 = fdiv fast float %1454, %1458
  %1465 = fsub fast float %1462, %1459
  %1466 = fsub fast float %1463, %1460
  %1467 = fsub fast float %1464, %1461
  %1468 = fadd fast float %1461, %1045
  %1469 = fmul fast float %1467, %525
  br i1 %1304, label %1470, label %1713

; <label>:1470                                    ; preds = %1305
  br label %1471

; <label>:1471                                    ; preds = %1703, %1470
  %1472 = phi i32 [ %1664, %1703 ], [ 1, %1470 ]
  %1473 = phi float [ %1704, %1703 ], [ -1.000000e+04, %1470 ]
  %1474 = phi float [ %1705, %1703 ], [ -1.000000e+00, %1470 ]
  %1475 = phi float [ %1706, %1703 ], [ 0.000000e+00, %1470 ]
  %1476 = phi float [ %1707, %1703 ], [ -1.000000e+00, %1470 ]
  %1477 = phi i32 [ %1708, %1703 ], [ 0, %1470 ]
  %1478 = icmp eq i32 %1477, %767
  br i1 %1478, label %1485, label %1479

; <label>:1479                                    ; preds = %1471
  %1480 = sitofp i32 %1477 to float
  %1481 = fadd fast float %1480, %1303
  %1482 = fmul fast float %1481, %1302
  %1483 = fadd fast float %1482, 1.000000e+00
  %1484 = fmul fast float %1483, %1483
  br label %1485

; <label>:1485                                    ; preds = %1479, %1471
  %1486 = phi float [ %1484, %1479 ], [ 0.000000e+00, %1471 ]
  %1487 = icmp eq i32 %1472, 0
  br i1 %1487, label %1606, label %1488

; <label>:1488                                    ; preds = %1485
  %1489 = fmul fast float %1486, %1465
  %1490 = fmul fast float %1486, %1466
  %1491 = fmul fast float %1486, %1467
  %1492 = fadd fast float %1489, %1459
  %1493 = fadd fast float %1490, %1460
  %1494 = fadd fast float %1491, %1468
  %1495 = call float @dx.op.unary.f32(i32 7, float %1492)  ; Saturate(value)
  %1496 = call float @dx.op.unary.f32(i32 7, float %1493)  ; Saturate(value)
  %1497 = fcmp fast oeq float %1492, %1495
  %1498 = fcmp fast oeq float %1493, %1496
  %1499 = and i1 %1497, %1498
  br i1 %1499, label %1500, label %1545

; <label>:1500                                    ; preds = %1488
  %1501 = icmp ult i32 %1294, 8192
  br i1 %1501, label %1512, label %1502

; <label>:1502                                    ; preds = %1500
  %1503 = fmul fast float %1493, 1.280000e+02
  %1504 = fptoui float %1503 to i32
  %1505 = fmul fast float %1492, 1.280000e+02
  %1506 = fptoui float %1505 to i32
  %1507 = mul i32 %1294, 21845
  %1508 = shl i32 %1504, 7
  %1509 = add i32 %1507, -178946048
  %1510 = add i32 %1509, %1506
  %1511 = add i32 %1510, %1508
  br label %1512

; <label>:1512                                    ; preds = %1502, %1500
  %1513 = phi i32 [ %1511, %1502 ], [ %1294, %1500 ]
  %1514 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1515 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1514, i32 %1513, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1516 = extractvalue %dx.types.ResRet.i32 %1515, 0
  %1517 = lshr i32 %1516, 20
  %1518 = icmp slt i32 %1516, 0
  %1519 = and i32 %1517, 31
  %1520 = lshr i32 16384, %1519
  %1521 = uitofp i32 %1520 to float
  %1522 = select i1 %1501, float 1.280000e+02, float %1521
  br i1 %1518, label %1523, label %1540

; <label>:1523                                    ; preds = %1512
  %1524 = fmul fast float %1522, %1493
  %1525 = fptoui float %1524 to i32
  %1526 = and i32 %1525, 127
  %1527 = lshr i32 %1516, 3
  %1528 = and i32 %1527, 130944
  %1529 = or i32 %1526, %1528
  %1530 = fmul fast float %1522, %1492
  %1531 = fptoui float %1530 to i32
  %1532 = and i32 %1531, 127
  %1533 = shl i32 %1516, 7
  %1534 = and i32 %1533, 130944
  %1535 = or i32 %1532, %1534
  %1536 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1537 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1536, i32 0, i32 %1535, i32 %1529, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1538 = extractvalue %dx.types.ResRet.i32 %1537, 0
  %1539 = bitcast i32 %1538 to float
  br label %1540

; <label>:1540                                    ; preds = %1523, %1512
  %1541 = phi i1 [ true, %1523 ], [ false, %1512 ]
  %1542 = phi float [ %1539, %1523 ], [ 0.000000e+00, %1512 ]
  %1543 = select i1 %1541, float %1542, float 0.000000e+00
  %1544 = zext i1 %1541 to i32
  br label %1545

; <label>:1545                                    ; preds = %1540, %1488
  %1546 = phi float [ 0.000000e+00, %1488 ], [ %1543, %1540 ]
  %1547 = phi i32 [ 0, %1488 ], [ %1544, %1540 ]
  %1548 = icmp eq i32 %1547, 0
  br i1 %1548, label %1549, label %1663

; <label>:1549                                    ; preds = %1545
  %1550 = fmul fast float %1486, %1386
  %1551 = fmul fast float %1486, %1387
  %1552 = fmul fast float %1486, %1388
  %1553 = fadd fast float %1550, %1380
  %1554 = fadd fast float %1551, %1381
  %1555 = fadd fast float %1552, %1389
  %1556 = call float @dx.op.unary.f32(i32 7, float %1553)  ; Saturate(value)
  %1557 = call float @dx.op.unary.f32(i32 7, float %1554)  ; Saturate(value)
  %1558 = fcmp fast oeq float %1553, %1556
  %1559 = fcmp fast oeq float %1554, %1557
  %1560 = and i1 %1558, %1559
  br i1 %1560, label %1561, label %1663

; <label>:1561                                    ; preds = %1549
  %1562 = icmp ult i32 %1306, 8192
  br i1 %1562, label %1573, label %1563

; <label>:1563                                    ; preds = %1561
  %1564 = fmul fast float %1554, 1.280000e+02
  %1565 = fptoui float %1564 to i32
  %1566 = fmul fast float %1553, 1.280000e+02
  %1567 = fptoui float %1566 to i32
  %1568 = mul i32 %1306, 21845
  %1569 = shl i32 %1565, 7
  %1570 = add i32 %1568, -178946048
  %1571 = add i32 %1570, %1567
  %1572 = add i32 %1571, %1569
  br label %1573

; <label>:1573                                    ; preds = %1563, %1561
  %1574 = phi i32 [ %1572, %1563 ], [ %1306, %1561 ]
  %1575 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1576 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1575, i32 %1574, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1577 = extractvalue %dx.types.ResRet.i32 %1576, 0
  %1578 = lshr i32 %1577, 20
  %1579 = icmp slt i32 %1577, 0
  %1580 = and i32 %1578, 31
  %1581 = lshr i32 16384, %1580
  %1582 = uitofp i32 %1581 to float
  %1583 = select i1 %1562, float 1.280000e+02, float %1582
  br i1 %1579, label %1584, label %1601

; <label>:1584                                    ; preds = %1573
  %1585 = fmul fast float %1583, %1554
  %1586 = fptoui float %1585 to i32
  %1587 = and i32 %1586, 127
  %1588 = lshr i32 %1577, 3
  %1589 = and i32 %1588, 130944
  %1590 = or i32 %1587, %1589
  %1591 = fmul fast float %1583, %1553
  %1592 = fptoui float %1591 to i32
  %1593 = and i32 %1592, 127
  %1594 = shl i32 %1577, 7
  %1595 = and i32 %1594, 130944
  %1596 = or i32 %1593, %1595
  %1597 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1598 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1597, i32 0, i32 %1596, i32 %1590, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1599 = extractvalue %dx.types.ResRet.i32 %1598, 0
  %1600 = bitcast i32 %1599 to float
  br label %1601

; <label>:1601                                    ; preds = %1584, %1573
  %1602 = phi float [ %1600, %1584 ], [ 0.000000e+00, %1573 ]
  %1603 = phi i1 [ true, %1584 ], [ false, %1573 ]
  %1604 = select i1 %1603, float %1602, float 0.000000e+00
  %1605 = zext i1 %1603 to i32
  br label %1663

; <label>:1606                                    ; preds = %1485
  %1607 = fmul fast float %1486, %1386
  %1608 = fmul fast float %1486, %1387
  %1609 = fmul fast float %1486, %1388
  %1610 = fadd fast float %1607, %1380
  %1611 = fadd fast float %1608, %1381
  %1612 = fadd fast float %1609, %1389
  %1613 = call float @dx.op.unary.f32(i32 7, float %1610)  ; Saturate(value)
  %1614 = call float @dx.op.unary.f32(i32 7, float %1611)  ; Saturate(value)
  %1615 = fcmp fast oeq float %1610, %1613
  %1616 = fcmp fast oeq float %1611, %1614
  %1617 = and i1 %1615, %1616
  br i1 %1617, label %1618, label %1663

; <label>:1618                                    ; preds = %1606
  %1619 = icmp ult i32 %1306, 8192
  br i1 %1619, label %1630, label %1620

; <label>:1620                                    ; preds = %1618
  %1621 = fmul fast float %1611, 1.280000e+02
  %1622 = fptoui float %1621 to i32
  %1623 = fmul fast float %1610, 1.280000e+02
  %1624 = fptoui float %1623 to i32
  %1625 = mul i32 %1306, 21845
  %1626 = shl i32 %1622, 7
  %1627 = add i32 %1625, -178946048
  %1628 = add i32 %1627, %1624
  %1629 = add i32 %1628, %1626
  br label %1630

; <label>:1630                                    ; preds = %1620, %1618
  %1631 = phi i32 [ %1629, %1620 ], [ %1306, %1618 ]
  %1632 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1633 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1632, i32 %1631, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1634 = extractvalue %dx.types.ResRet.i32 %1633, 0
  %1635 = lshr i32 %1634, 20
  %1636 = icmp slt i32 %1634, 0
  %1637 = and i32 %1635, 31
  %1638 = lshr i32 16384, %1637
  %1639 = uitofp i32 %1638 to float
  %1640 = select i1 %1619, float 1.280000e+02, float %1639
  br i1 %1636, label %1641, label %1658

; <label>:1641                                    ; preds = %1630
  %1642 = fmul fast float %1640, %1611
  %1643 = fptoui float %1642 to i32
  %1644 = and i32 %1643, 127
  %1645 = lshr i32 %1634, 3
  %1646 = and i32 %1645, 130944
  %1647 = or i32 %1644, %1646
  %1648 = fmul fast float %1640, %1610
  %1649 = fptoui float %1648 to i32
  %1650 = and i32 %1649, 127
  %1651 = shl i32 %1634, 7
  %1652 = and i32 %1651, 130944
  %1653 = or i32 %1650, %1652
  %1654 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1655 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1654, i32 0, i32 %1653, i32 %1647, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1656 = extractvalue %dx.types.ResRet.i32 %1655, 0
  %1657 = bitcast i32 %1656 to float
  br label %1658

; <label>:1658                                    ; preds = %1641, %1630
  %1659 = phi i1 [ true, %1641 ], [ false, %1630 ]
  %1660 = phi float [ %1657, %1641 ], [ 0.000000e+00, %1630 ]
  %1661 = select i1 %1659, float %1660, float 0.000000e+00
  %1662 = zext i1 %1659 to i32
  br label %1663

; <label>:1663                                    ; preds = %1658, %1606, %1601, %1549, %1545
  %1664 = phi i32 [ %1472, %1545 ], [ 0, %1601 ], [ 0, %1549 ], [ 0, %1658 ], [ 0, %1606 ]
  %1665 = phi float [ %1475, %1545 ], [ %1390, %1601 ], [ %1390, %1549 ], [ %1475, %1658 ], [ %1475, %1606 ]
  %1666 = phi float [ %1469, %1545 ], [ %1390, %1601 ], [ %1390, %1549 ], [ %1390, %1658 ], [ %1390, %1606 ]
  %1667 = phi float [ %1494, %1545 ], [ %1555, %1601 ], [ %1555, %1549 ], [ %1612, %1658 ], [ %1612, %1606 ]
  %1668 = phi float [ %1546, %1545 ], [ %1604, %1601 ], [ 0.000000e+00, %1549 ], [ %1661, %1658 ], [ 0.000000e+00, %1606 ]
  %1669 = phi i32 [ %1547, %1545 ], [ %1605, %1601 ], [ 0, %1549 ], [ %1662, %1658 ], [ 0, %1606 ]
  %1670 = icmp eq i32 %1669, 0
  br i1 %1670, label %1703, label %1671

; <label>:1671                                    ; preds = %1663
  %1672 = fcmp fast oeq float %1473, -1.000000e+04
  br i1 %1672, label %1673, label %1675

; <label>:1673                                    ; preds = %1671
  %1674 = fcmp fast ogt float %1668, %1667
  br i1 %1674, label %1710, label %1703

; <label>:1675                                    ; preds = %1671
  %1676 = fsub fast float %1667, %1476
  %1677 = call float @dx.op.unary.f32(i32 6, float %1676)  ; FAbs(value)
  %1678 = fmul fast float %1677, 0x3FF0CCCCC0000000
  %1679 = fsub fast float %1668, %1667
  %1680 = fcmp fast ogt float %1679, %1678
  %1681 = fsub fast float %1486, %1474
  br i1 %1680, label %1682, label %1685

; <label>:1682                                    ; preds = %1675
  %1683 = fmul fast float %1665, %1681
  %1684 = fadd fast float %1683, %1473
  br label %1693

; <label>:1685                                    ; preds = %1675
  %1686 = fcmp fast une float %1668, %1473
  br i1 %1686, label %1687, label %1693

; <label>:1687                                    ; preds = %1685
  %1688 = fsub fast float %1668, %1473
  %1689 = fdiv fast float %1688, %1681
  %1690 = fsub fast float -0.000000e+00, %1666
  %1691 = call float @dx.op.binary.f32(i32 35, float %1689, float %1690)  ; FMax(a,b)
  %1692 = call float @dx.op.binary.f32(i32 36, float %1691, float %1666)  ; FMin(a,b)
  br label %1693

; <label>:1693                                    ; preds = %1687, %1685, %1682
  %1694 = phi float [ %1473, %1682 ], [ %1668, %1687 ], [ %1473, %1685 ]
  %1695 = phi float [ %1474, %1682 ], [ %1486, %1687 ], [ %1474, %1685 ]
  %1696 = phi float [ %1665, %1682 ], [ %1692, %1687 ], [ %1665, %1685 ]
  %1697 = phi float [ %1684, %1682 ], [ %1668, %1687 ], [ %1668, %1685 ]
  %1698 = fmul fast float %1677, 0x3FE0CCCCC0000000
  %1699 = fadd fast float %1698, %1667
  %1700 = fsub fast float %1699, %1697
  %1701 = call float @dx.op.unary.f32(i32 6, float %1700)  ; FAbs(value)
  %1702 = fcmp fast olt float %1701, %1698
  br i1 %1702, label %1710, label %1703

; <label>:1703                                    ; preds = %1693, %1673, %1663
  %1704 = phi float [ %1473, %1663 ], [ %1668, %1673 ], [ %1694, %1693 ]
  %1705 = phi float [ %1474, %1663 ], [ %1486, %1673 ], [ %1695, %1693 ]
  %1706 = phi float [ %1665, %1663 ], [ %1665, %1673 ], [ %1696, %1693 ]
  %1707 = phi float [ %1476, %1663 ], [ %1667, %1673 ], [ %1667, %1693 ]
  %1708 = add nuw nsw i32 %1477, 1
  %1709 = icmp slt i32 %1477, %767
  br i1 %1709, label %1471, label %1710

; <label>:1710                                    ; preds = %1703, %1693, %1673
  %1711 = phi float [ -1.000000e+00, %1703 ], [ %1697, %1693 ], [ %1668, %1673 ]
  %1712 = phi i32 [ 0, %1703 ], [ 1, %1693 ], [ 1, %1673 ]
  br label %1713

; <label>:1713                                    ; preds = %1710, %1305
  %1714 = phi i32 [ 1, %1305 ], [ %1664, %1710 ]
  %1715 = phi float [ -1.000000e+00, %1305 ], [ %1711, %1710 ]
  %1716 = phi i32 [ 0, %1305 ], [ %1712, %1710 ]
  %1717 = icmp ne i32 %1714, 0
  %1718 = select i1 %1717, i32 %1294, i32 %1306
  %1719 = select i1 %1717, float %1468, float %1389
  br label %1923

; <label>:1720                                    ; preds = %1290
  %1721 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1297, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1722 = extractvalue %dx.types.ResRet.i32 %1721, 0
  %1723 = extractvalue %dx.types.ResRet.i32 %1721, 1
  %1724 = extractvalue %dx.types.ResRet.i32 %1721, 2
  %1725 = extractvalue %dx.types.ResRet.i32 %1721, 3
  %1726 = bitcast i32 %1722 to float
  %1727 = bitcast i32 %1723 to float
  %1728 = bitcast i32 %1724 to float
  %1729 = bitcast i32 %1725 to float
  %1730 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1298, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1731 = extractvalue %dx.types.ResRet.i32 %1730, 0
  %1732 = extractvalue %dx.types.ResRet.i32 %1730, 1
  %1733 = extractvalue %dx.types.ResRet.i32 %1730, 2
  %1734 = extractvalue %dx.types.ResRet.i32 %1730, 3
  %1735 = bitcast i32 %1731 to float
  %1736 = bitcast i32 %1732 to float
  %1737 = bitcast i32 %1733 to float
  %1738 = bitcast i32 %1734 to float
  %1739 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1299, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1740 = extractvalue %dx.types.ResRet.i32 %1739, 0
  %1741 = extractvalue %dx.types.ResRet.i32 %1739, 1
  %1742 = extractvalue %dx.types.ResRet.i32 %1739, 2
  %1743 = extractvalue %dx.types.ResRet.i32 %1739, 3
  %1744 = bitcast i32 %1740 to float
  %1745 = bitcast i32 %1741 to float
  %1746 = bitcast i32 %1742 to float
  %1747 = bitcast i32 %1743 to float
  %1748 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1296, i32 %1300, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1749 = extractvalue %dx.types.ResRet.i32 %1748, 0
  %1750 = extractvalue %dx.types.ResRet.i32 %1748, 1
  %1751 = extractvalue %dx.types.ResRet.i32 %1748, 2
  %1752 = extractvalue %dx.types.ResRet.i32 %1748, 3
  %1753 = bitcast i32 %1749 to float
  %1754 = bitcast i32 %1750 to float
  %1755 = bitcast i32 %1751 to float
  %1756 = bitcast i32 %1752 to float
  %1757 = fmul fast float %1726, %953
  %1758 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1735, float %1757)  ; FMad(a,b,c)
  %1759 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1744, float %1758)  ; FMad(a,b,c)
  %1760 = fadd fast float %1759, %1753
  %1761 = fmul fast float %1727, %953
  %1762 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1736, float %1761)  ; FMad(a,b,c)
  %1763 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1745, float %1762)  ; FMad(a,b,c)
  %1764 = fadd fast float %1763, %1754
  %1765 = fmul fast float %1728, %953
  %1766 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1737, float %1765)  ; FMad(a,b,c)
  %1767 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1746, float %1766)  ; FMad(a,b,c)
  %1768 = fadd fast float %1767, %1755
  %1769 = fmul fast float %1729, %953
  %1770 = call float @dx.op.tertiary.f32(i32 46, float %954, float %1738, float %1769)  ; FMad(a,b,c)
  %1771 = call float @dx.op.tertiary.f32(i32 46, float %955, float %1747, float %1770)  ; FMad(a,b,c)
  %1772 = fadd fast float %1771, %1756
  %1773 = fmul fast float %1726, %959
  %1774 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1735, float %1773)  ; FMad(a,b,c)
  %1775 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1744, float %1774)  ; FMad(a,b,c)
  %1776 = fadd fast float %1775, %1753
  %1777 = fmul fast float %1727, %959
  %1778 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1736, float %1777)  ; FMad(a,b,c)
  %1779 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1745, float %1778)  ; FMad(a,b,c)
  %1780 = fadd fast float %1779, %1754
  %1781 = fmul fast float %1728, %959
  %1782 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1737, float %1781)  ; FMad(a,b,c)
  %1783 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1746, float %1782)  ; FMad(a,b,c)
  %1784 = fadd fast float %1783, %1755
  %1785 = fmul fast float %1729, %959
  %1786 = call float @dx.op.tertiary.f32(i32 46, float %960, float %1738, float %1785)  ; FMad(a,b,c)
  %1787 = call float @dx.op.tertiary.f32(i32 46, float %961, float %1747, float %1786)  ; FMad(a,b,c)
  %1788 = fadd fast float %1787, %1756
  %1789 = fdiv fast float %1760, %1772
  %1790 = fdiv fast float %1764, %1772
  %1791 = fdiv fast float %1768, %1772
  %1792 = fdiv fast float %1776, %1788
  %1793 = fdiv fast float %1780, %1788
  %1794 = fdiv fast float %1784, %1788
  %1795 = fsub fast float %1792, %1789
  %1796 = fsub fast float %1793, %1790
  %1797 = fsub fast float %1794, %1791
  %1798 = call float @dx.op.unary.f32(i32 7, float %1789)  ; Saturate(value)
  %1799 = call float @dx.op.unary.f32(i32 7, float %1790)  ; Saturate(value)
  %1800 = fadd fast float %1791, %1045
  %1801 = fmul fast float %1797, %525
  br i1 %1304, label %1802, label %1923

; <label>:1802                                    ; preds = %1720
  br label %1803

; <label>:1803                                    ; preds = %1910, %1802
  %1804 = phi float [ %1911, %1910 ], [ -1.000000e+04, %1802 ]
  %1805 = phi float [ %1912, %1910 ], [ -1.000000e+00, %1802 ]
  %1806 = phi float [ %1913, %1910 ], [ 0.000000e+00, %1802 ]
  %1807 = phi float [ %1914, %1910 ], [ -1.000000e+00, %1802 ]
  %1808 = phi i32 [ %1915, %1910 ], [ 0, %1802 ]
  %1809 = icmp eq i32 %1808, %767
  br i1 %1809, label %1816, label %1810

; <label>:1810                                    ; preds = %1803
  %1811 = sitofp i32 %1808 to float
  %1812 = fadd fast float %1811, %1303
  %1813 = fmul fast float %1812, %1302
  %1814 = fadd fast float %1813, 1.000000e+00
  %1815 = fmul fast float %1814, %1814
  br label %1816

; <label>:1816                                    ; preds = %1810, %1803
  %1817 = phi float [ %1815, %1810 ], [ 0.000000e+00, %1803 ]
  %1818 = fmul fast float %1817, %1795
  %1819 = fmul fast float %1817, %1796
  %1820 = fmul fast float %1817, %1797
  %1821 = fadd fast float %1818, %1798
  %1822 = fadd fast float %1819, %1799
  %1823 = fadd fast float %1820, %1800
  %1824 = call float @dx.op.unary.f32(i32 7, float %1821)  ; Saturate(value)
  %1825 = call float @dx.op.unary.f32(i32 7, float %1822)  ; Saturate(value)
  %1826 = fcmp fast oeq float %1821, %1824
  %1827 = fcmp fast oeq float %1822, %1825
  %1828 = and i1 %1826, %1827
  br i1 %1828, label %1829, label %1874

; <label>:1829                                    ; preds = %1816
  %1830 = icmp ult i32 %1294, 8192
  br i1 %1830, label %1841, label %1831

; <label>:1831                                    ; preds = %1829
  %1832 = fmul fast float %1822, 1.280000e+02
  %1833 = fptoui float %1832 to i32
  %1834 = fmul fast float %1821, 1.280000e+02
  %1835 = fptoui float %1834 to i32
  %1836 = mul i32 %1294, 21845
  %1837 = shl i32 %1833, 7
  %1838 = add i32 %1836, -178946048
  %1839 = add i32 %1838, %1835
  %1840 = add i32 %1839, %1837
  br label %1841

; <label>:1841                                    ; preds = %1831, %1829
  %1842 = phi i32 [ %1840, %1831 ], [ %1294, %1829 ]
  %1843 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1844 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1843, i32 %1842, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1845 = extractvalue %dx.types.ResRet.i32 %1844, 0
  %1846 = lshr i32 %1845, 20
  %1847 = icmp slt i32 %1845, 0
  %1848 = and i32 %1846, 31
  %1849 = lshr i32 16384, %1848
  %1850 = uitofp i32 %1849 to float
  %1851 = select i1 %1830, float 1.280000e+02, float %1850
  br i1 %1847, label %1852, label %1869

; <label>:1852                                    ; preds = %1841
  %1853 = fmul fast float %1851, %1822
  %1854 = fptoui float %1853 to i32
  %1855 = and i32 %1854, 127
  %1856 = lshr i32 %1845, 3
  %1857 = and i32 %1856, 130944
  %1858 = or i32 %1855, %1857
  %1859 = fmul fast float %1851, %1821
  %1860 = fptoui float %1859 to i32
  %1861 = and i32 %1860, 127
  %1862 = shl i32 %1845, 7
  %1863 = and i32 %1862, 130944
  %1864 = or i32 %1861, %1863
  %1865 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1866 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1865, i32 0, i32 %1864, i32 %1858, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1867 = extractvalue %dx.types.ResRet.i32 %1866, 0
  %1868 = bitcast i32 %1867 to float
  br label %1869

; <label>:1869                                    ; preds = %1852, %1841
  %1870 = phi float [ %1868, %1852 ], [ 0.000000e+00, %1841 ]
  %1871 = phi i1 [ true, %1852 ], [ false, %1841 ]
  %1872 = select i1 %1871, float %1870, float 0.000000e+00
  %1873 = zext i1 %1871 to i32
  br label %1874

; <label>:1874                                    ; preds = %1869, %1816
  %1875 = phi float [ 0.000000e+00, %1816 ], [ %1872, %1869 ]
  %1876 = phi i32 [ 0, %1816 ], [ %1873, %1869 ]
  %1877 = icmp eq i32 %1876, 0
  br i1 %1877, label %1910, label %1878

; <label>:1878                                    ; preds = %1874
  %1879 = fcmp fast oeq float %1804, -1.000000e+04
  br i1 %1879, label %1880, label %1882

; <label>:1880                                    ; preds = %1878
  %1881 = fcmp fast ogt float %1875, %1823
  br i1 %1881, label %1920, label %1910

; <label>:1882                                    ; preds = %1878
  %1883 = fsub fast float %1823, %1807
  %1884 = call float @dx.op.unary.f32(i32 6, float %1883)  ; FAbs(value)
  %1885 = fmul fast float %1884, 0x3FF0CCCCC0000000
  %1886 = fsub fast float %1875, %1823
  %1887 = fcmp fast ogt float %1886, %1885
  %1888 = fsub fast float %1817, %1805
  br i1 %1887, label %1889, label %1892

; <label>:1889                                    ; preds = %1882
  %1890 = fmul fast float %1888, %1806
  %1891 = fadd fast float %1890, %1804
  br label %1900

; <label>:1892                                    ; preds = %1882
  %1893 = fcmp fast une float %1875, %1804
  br i1 %1893, label %1894, label %1900

; <label>:1894                                    ; preds = %1892
  %1895 = fsub fast float %1875, %1804
  %1896 = fdiv fast float %1895, %1888
  %1897 = fsub fast float -0.000000e+00, %1801
  %1898 = call float @dx.op.binary.f32(i32 35, float %1896, float %1897)  ; FMax(a,b)
  %1899 = call float @dx.op.binary.f32(i32 36, float %1898, float %1801)  ; FMin(a,b)
  br label %1900

; <label>:1900                                    ; preds = %1894, %1892, %1889
  %1901 = phi float [ %1804, %1889 ], [ %1875, %1894 ], [ %1804, %1892 ]
  %1902 = phi float [ %1805, %1889 ], [ %1817, %1894 ], [ %1805, %1892 ]
  %1903 = phi float [ %1806, %1889 ], [ %1899, %1894 ], [ %1806, %1892 ]
  %1904 = phi float [ %1891, %1889 ], [ %1875, %1894 ], [ %1875, %1892 ]
  %1905 = fmul fast float %1884, 0x3FE0CCCCC0000000
  %1906 = fadd fast float %1905, %1823
  %1907 = fsub fast float %1906, %1904
  %1908 = call float @dx.op.unary.f32(i32 6, float %1907)  ; FAbs(value)
  %1909 = fcmp fast olt float %1908, %1905
  br i1 %1909, label %1920, label %1910

; <label>:1910                                    ; preds = %1900, %1880, %1874
  %1911 = phi float [ %1804, %1874 ], [ %1875, %1880 ], [ %1901, %1900 ]
  %1912 = phi float [ %1805, %1874 ], [ %1817, %1880 ], [ %1902, %1900 ]
  %1913 = phi float [ %1806, %1874 ], [ %1806, %1880 ], [ %1903, %1900 ]
  %1914 = phi float [ %1807, %1874 ], [ %1823, %1880 ], [ %1823, %1900 ]
  %1915 = add nuw nsw i32 %1808, 1
  %1916 = icmp slt i32 %1808, %767
  br i1 %1916, label %1803, label %1920

; <label>:1917                                    ; preds = %1246, %1236, %1216
  %1918 = phi float [ %1211, %1216 ], [ %1240, %1236 ], [ -1.000000e+00, %1246 ]
  %1919 = phi i32 [ 1, %1216 ], [ 1, %1236 ], [ 0, %1246 ]
  br label %1923

; <label>:1920                                    ; preds = %1910, %1900, %1880
  %1921 = phi float [ -1.000000e+00, %1910 ], [ %1904, %1900 ], [ %1875, %1880 ]
  %1922 = phi i32 [ 0, %1910 ], [ 1, %1900 ], [ 1, %1880 ]
  br label %1923

; <label>:1923                                    ; preds = %1920, %1917, %1720, %1713, %1046
  %1924 = phi i32 [ %285, %1046 ], [ %1294, %1720 ], [ %1718, %1713 ], [ %285, %1917 ], [ %1294, %1920 ]
  %1925 = phi float [ %1132, %1046 ], [ %1800, %1720 ], [ %1719, %1713 ], [ %1132, %1917 ], [ %1800, %1920 ]
  %1926 = phi float [ -1.000000e+00, %1046 ], [ -1.000000e+00, %1720 ], [ %1715, %1713 ], [ %1918, %1917 ], [ %1921, %1920 ]
  %1927 = phi i32 [ 0, %1046 ], [ 0, %1720 ], [ %1716, %1713 ], [ %1919, %1917 ], [ %1922, %1920 ]
  %1928 = icmp eq i32 %1927, 0
  br i1 %1928, label %1956, label %1929

; <label>:1929                                    ; preds = %1923
  %1930 = fmul fast float %953, %953
  %1931 = fmul fast float %954, %954
  %1932 = fadd fast float %1930, %1931
  %1933 = fmul fast float %955, %955
  %1934 = fadd fast float %1932, %1933
  %1935 = call float @dx.op.unary.f32(i32 24, float %1934)  ; Sqrt(value)
  %1936 = call float @dx.op.unary.f32(i32 7, float %1925)  ; Saturate(value)
  %1937 = mul i32 %1924, 288
  %1938 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1939 = add i32 %1937, 32
  %1940 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1938, i32 %1939, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1941 = extractvalue %dx.types.ResRet.i32 %1940, 2
  %1942 = bitcast i32 %1941 to float
  %1943 = add i32 %1937, 48
  %1944 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1938, i32 %1943, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1945 = extractvalue %dx.types.ResRet.i32 %1944, 2
  %1946 = bitcast i32 %1945 to float
  %1947 = fsub fast float %1926, %1942
  %1948 = fdiv fast float %1946, %1947
  %1949 = fsub fast float %1936, %1942
  %1950 = fmul fast float %1949, %1935
  %1951 = fdiv fast float %1950, %1946
  %1952 = fmul fast float %1951, %1948
  %1953 = fsub fast float %1935, %1952
  %1954 = call float @dx.op.binary.f32(i32 35, float 0x3EB0C6F7A0000000, float %1953)  ; FMax(a,b)
  %1955 = fadd fast float %1954, %772
  br label %1958

; <label>:1956                                    ; preds = %1923
  %1957 = add i32 %771, 1
  br label %1958

; <label>:1958                                    ; preds = %1956, %1929
  %1959 = phi i32 [ %771, %1929 ], [ %1957, %1956 ]
  %1960 = phi float [ %1955, %1929 ], [ %772, %1956 ]
  %1961 = icmp eq i32 %522, 0
  br i1 %1961, label %1971, label %1962

; <label>:1962                                    ; preds = %1958
  %1963 = icmp eq i32 %770, 0
  br i1 %1963, label %1964, label %1966

; <label>:1964                                    ; preds = %1962
  %1965 = call i1 @dx.op.waveAllTrue(i32 114, i1 %1928)  ; WaveAllTrue(cond)
  br i1 %1965, label %1982, label %1971

; <label>:1966                                    ; preds = %1962
  %1967 = icmp ult i32 %770, %522
  br i1 %1967, label %1971, label %1968

; <label>:1968                                    ; preds = %1966
  %1969 = icmp eq i32 %1959, 0
  %1970 = call i1 @dx.op.waveAllTrue(i32 114, i1 %1969)  ; WaveAllTrue(cond)
  br i1 %1970, label %1982, label %1971

; <label>:1971                                    ; preds = %1968, %1966, %1964, %1958
  %1972 = bitcast float %769 to i32
  %1973 = mul i32 %1972, -1835707051
  %1974 = add i32 %1973, 1216271409
  %1975 = lshr i32 %1974, 15
  %1976 = xor i32 %1975, %1974
  %1977 = lshr i32 %1976, 8
  %1978 = uitofp i32 %1977 to float
  %1979 = fmul fast float %1978, 0x3E70000000000000
  %1980 = add nuw i32 %770, 1
  %1981 = icmp ult i32 %1980, %518
  br i1 %1981, label %768, label %1982

; <label>:1982                                    ; preds = %1971, %1968, %1964
  %1983 = phi i32 [ %1980, %1971 ], [ 0, %1964 ], [ %770, %1968 ]
  %1984 = add i32 %1983, 1
  %1985 = call i32 @dx.op.binary.i32(i32 40, i32 %1984, i32 %518)  ; UMin(a,b)
  %1986 = sub i32 %1985, %1959
  %1987 = call i32 @dx.op.binary.i32(i32 39, i32 1, i32 %1986)  ; UMax(a,b)
  %1988 = uitofp i32 %1987 to float
  %1989 = fdiv fast float %1960, %1988
  %1990 = uitofp i32 %1959 to float
  %1991 = uitofp i32 %1985 to float
  %1992 = fdiv fast float %1990, %1991
  br label %2325

; <label>:1993                                    ; preds = %515
  %1994 = call float @dx.op.binary.f32(i32 35, float %516, float 0.000000e+00)  ; FMax(a,b)
  %1995 = mul i32 %285, 288
  %1996 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1997 = add i32 %1995, 32
  %1998 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %1997, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1999 = extractvalue %dx.types.ResRet.i32 %1998, 2
  %2000 = add i32 %1995, 48
  %2001 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2000, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2002 = extractvalue %dx.types.ResRet.i32 %2001, 2
  %2003 = add i32 %1995, 64
  %2004 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2003, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2005 = extractvalue %dx.types.ResRet.i32 %2004, 0
  %2006 = extractvalue %dx.types.ResRet.i32 %2004, 1
  %2007 = extractvalue %dx.types.ResRet.i32 %2004, 2
  %2008 = extractvalue %dx.types.ResRet.i32 %2004, 3
  %2009 = add i32 %1995, 80
  %2010 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2009, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2011 = extractvalue %dx.types.ResRet.i32 %2010, 0
  %2012 = extractvalue %dx.types.ResRet.i32 %2010, 1
  %2013 = extractvalue %dx.types.ResRet.i32 %2010, 2
  %2014 = extractvalue %dx.types.ResRet.i32 %2010, 3
  %2015 = add i32 %1995, 96
  %2016 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2015, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2017 = extractvalue %dx.types.ResRet.i32 %2016, 0
  %2018 = extractvalue %dx.types.ResRet.i32 %2016, 1
  %2019 = extractvalue %dx.types.ResRet.i32 %2016, 2
  %2020 = extractvalue %dx.types.ResRet.i32 %2016, 3
  %2021 = add i32 %1995, 112
  %2022 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2021, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2023 = extractvalue %dx.types.ResRet.i32 %2022, 0
  %2024 = extractvalue %dx.types.ResRet.i32 %2022, 1
  %2025 = extractvalue %dx.types.ResRet.i32 %2022, 2
  %2026 = extractvalue %dx.types.ResRet.i32 %2022, 3
  %2027 = add i32 %1995, 204
  %2028 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2027, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2029 = extractvalue %dx.types.ResRet.i32 %2028, 0
  %2030 = add i32 %1995, 208
  %2031 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2030, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2032 = extractvalue %dx.types.ResRet.i32 %2031, 0
  %2033 = extractvalue %dx.types.ResRet.i32 %2031, 1
  %2034 = extractvalue %dx.types.ResRet.i32 %2031, 2
  %2035 = bitcast i32 %2032 to float
  %2036 = bitcast i32 %2033 to float
  %2037 = bitcast i32 %2034 to float
  %2038 = add i32 %1995, 224
  %2039 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2038, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2040 = extractvalue %dx.types.ResRet.i32 %2039, 0
  %2041 = extractvalue %dx.types.ResRet.i32 %2039, 1
  %2042 = extractvalue %dx.types.ResRet.i32 %2039, 2
  %2043 = bitcast i32 %2040 to float
  %2044 = bitcast i32 %2041 to float
  %2045 = bitcast i32 %2042 to float
  %2046 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 72)  ; CBufferLoadLegacy(handle,regIndex)
  %2047 = extractvalue %dx.types.CBufRet.f32 %2046, 0
  %2048 = extractvalue %dx.types.CBufRet.f32 %2046, 1
  %2049 = extractvalue %dx.types.CBufRet.f32 %2046, 2
  %2050 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %30, i32 73)  ; CBufferLoadLegacy(handle,regIndex)
  %2051 = extractvalue %dx.types.CBufRet.f32 %2050, 0
  %2052 = extractvalue %dx.types.CBufRet.f32 %2050, 1
  %2053 = extractvalue %dx.types.CBufRet.f32 %2050, 2
  %2054 = fsub float %2035, %2047
  %2055 = fsub float %2036, %2048
  %2056 = fsub float %2037, %2049
  %2057 = fsub float %2043, %2051
  %2058 = fsub float %2044, %2052
  %2059 = fsub float %2045, %2053
  %2060 = fadd float %2054, %2057
  %2061 = fadd float %2055, %2058
  %2062 = fadd float %2056, %2059
  %2063 = fadd fast float %2060, %371
  %2064 = fadd fast float %2061, %372
  %2065 = fadd fast float %2062, %373
  %2066 = icmp eq i32 %2029, 2
  br i1 %2066, label %2119, label %2067

; <label>:2067                                    ; preds = %1993
  %2068 = call float @dx.op.unary.f32(i32 6, float %2063)  ; FAbs(value)
  %2069 = call float @dx.op.unary.f32(i32 6, float %2064)  ; FAbs(value)
  %2070 = fcmp fast ult float %2068, %2069
  %2071 = call float @dx.op.unary.f32(i32 6, float %2065)  ; FAbs(value)
  %2072 = fcmp fast ult float %2068, %2071
  %2073 = or i1 %2070, %2072
  br i1 %2073, label %2077, label %2074

; <label>:2074                                    ; preds = %2067
  %2075 = fcmp ule float %2063, 0.000000e+00
  %2076 = zext i1 %2075 to i32
  br label %2085

; <label>:2077                                    ; preds = %2067
  %2078 = fcmp fast ogt float %2069, %2071
  br i1 %2078, label %2079, label %2082

; <label>:2079                                    ; preds = %2077
  %2080 = fcmp fast ogt float %2064, 0.000000e+00
  %2081 = select i1 %2080, i32 2, i32 3
  br label %2085

; <label>:2082                                    ; preds = %2077
  %2083 = fcmp fast ogt float %2065, 0.000000e+00
  %2084 = select i1 %2083, i32 4, i32 5
  br label %2085

; <label>:2085                                    ; preds = %2082, %2079, %2074
  %2086 = phi i32 [ %2076, %2074 ], [ %2081, %2079 ], [ %2084, %2082 ]
  %2087 = add i32 %2086, %285
  %2088 = mul i32 %2087, 288
  %2089 = add i32 %2088, 32
  %2090 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2089, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2091 = extractvalue %dx.types.ResRet.i32 %2090, 2
  %2092 = add i32 %2088, 48
  %2093 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2092, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2094 = extractvalue %dx.types.ResRet.i32 %2093, 2
  %2095 = add i32 %2088, 64
  %2096 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2095, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2097 = extractvalue %dx.types.ResRet.i32 %2096, 0
  %2098 = extractvalue %dx.types.ResRet.i32 %2096, 1
  %2099 = extractvalue %dx.types.ResRet.i32 %2096, 2
  %2100 = extractvalue %dx.types.ResRet.i32 %2096, 3
  %2101 = add i32 %2088, 80
  %2102 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2101, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2103 = extractvalue %dx.types.ResRet.i32 %2102, 0
  %2104 = extractvalue %dx.types.ResRet.i32 %2102, 1
  %2105 = extractvalue %dx.types.ResRet.i32 %2102, 2
  %2106 = extractvalue %dx.types.ResRet.i32 %2102, 3
  %2107 = add i32 %2088, 96
  %2108 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2107, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2109 = extractvalue %dx.types.ResRet.i32 %2108, 0
  %2110 = extractvalue %dx.types.ResRet.i32 %2108, 1
  %2111 = extractvalue %dx.types.ResRet.i32 %2108, 2
  %2112 = extractvalue %dx.types.ResRet.i32 %2108, 3
  %2113 = add i32 %2088, 112
  %2114 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2113, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2115 = extractvalue %dx.types.ResRet.i32 %2114, 0
  %2116 = extractvalue %dx.types.ResRet.i32 %2114, 1
  %2117 = extractvalue %dx.types.ResRet.i32 %2114, 2
  %2118 = extractvalue %dx.types.ResRet.i32 %2114, 3
  br label %2119

; <label>:2119                                    ; preds = %2085, %1993
  %2120 = phi i32 [ %2091, %2085 ], [ %1999, %1993 ]
  %2121 = phi i32 [ %2094, %2085 ], [ %2002, %1993 ]
  %2122 = phi i32 [ %2097, %2085 ], [ %2005, %1993 ]
  %2123 = phi i32 [ %2098, %2085 ], [ %2006, %1993 ]
  %2124 = phi i32 [ %2099, %2085 ], [ %2007, %1993 ]
  %2125 = phi i32 [ %2100, %2085 ], [ %2008, %1993 ]
  %2126 = phi i32 [ %2103, %2085 ], [ %2011, %1993 ]
  %2127 = phi i32 [ %2104, %2085 ], [ %2012, %1993 ]
  %2128 = phi i32 [ %2105, %2085 ], [ %2013, %1993 ]
  %2129 = phi i32 [ %2106, %2085 ], [ %2014, %1993 ]
  %2130 = phi i32 [ %2109, %2085 ], [ %2017, %1993 ]
  %2131 = phi i32 [ %2110, %2085 ], [ %2018, %1993 ]
  %2132 = phi i32 [ %2111, %2085 ], [ %2019, %1993 ]
  %2133 = phi i32 [ %2112, %2085 ], [ %2020, %1993 ]
  %2134 = phi i32 [ %2115, %2085 ], [ %2023, %1993 ]
  %2135 = phi i32 [ %2116, %2085 ], [ %2024, %1993 ]
  %2136 = phi i32 [ %2117, %2085 ], [ %2025, %1993 ]
  %2137 = phi i32 [ %2118, %2085 ], [ %2026, %1993 ]
  %2138 = phi i32 [ %2087, %2085 ], [ %285, %1993 ]
  %2139 = bitcast i32 %2137 to float
  %2140 = bitcast i32 %2136 to float
  %2141 = bitcast i32 %2135 to float
  %2142 = bitcast i32 %2134 to float
  %2143 = bitcast i32 %2133 to float
  %2144 = bitcast i32 %2132 to float
  %2145 = bitcast i32 %2131 to float
  %2146 = bitcast i32 %2130 to float
  %2147 = bitcast i32 %2129 to float
  %2148 = bitcast i32 %2128 to float
  %2149 = bitcast i32 %2127 to float
  %2150 = bitcast i32 %2126 to float
  %2151 = bitcast i32 %2125 to float
  %2152 = bitcast i32 %2124 to float
  %2153 = bitcast i32 %2123 to float
  %2154 = bitcast i32 %2122 to float
  %2155 = bitcast i32 %2121 to float
  %2156 = bitcast i32 %2120 to float
  %2157 = fmul fast float %2154, %2063
  %2158 = call float @dx.op.tertiary.f32(i32 46, float %2064, float %2150, float %2157)  ; FMad(a,b,c)
  %2159 = call float @dx.op.tertiary.f32(i32 46, float %2065, float %2146, float %2158)  ; FMad(a,b,c)
  %2160 = fadd fast float %2159, %2142
  %2161 = fmul fast float %2153, %2063
  %2162 = call float @dx.op.tertiary.f32(i32 46, float %2064, float %2149, float %2161)  ; FMad(a,b,c)
  %2163 = call float @dx.op.tertiary.f32(i32 46, float %2065, float %2145, float %2162)  ; FMad(a,b,c)
  %2164 = fadd fast float %2163, %2141
  %2165 = fmul fast float %2152, %2063
  %2166 = call float @dx.op.tertiary.f32(i32 46, float %2064, float %2148, float %2165)  ; FMad(a,b,c)
  %2167 = call float @dx.op.tertiary.f32(i32 46, float %2065, float %2144, float %2166)  ; FMad(a,b,c)
  %2168 = fadd fast float %2167, %2140
  %2169 = fmul fast float %2151, %2063
  %2170 = call float @dx.op.tertiary.f32(i32 46, float %2064, float %2147, float %2169)  ; FMad(a,b,c)
  %2171 = call float @dx.op.tertiary.f32(i32 46, float %2065, float %2143, float %2170)  ; FMad(a,b,c)
  %2172 = fadd fast float %2171, %2139
  %2173 = fdiv fast float %2160, %2172
  %2174 = fdiv fast float %2164, %2172
  %2175 = fdiv fast float %2168, %2172
  %2176 = icmp ult i32 %2138, 8192
  br i1 %2176, label %2187, label %2177

; <label>:2177                                    ; preds = %2119
  %2178 = fmul fast float %2174, 1.280000e+02
  %2179 = fptoui float %2178 to i32
  %2180 = fmul fast float %2173, 1.280000e+02
  %2181 = fptoui float %2180 to i32
  %2182 = mul i32 %2138, 21845
  %2183 = shl i32 %2179, 7
  %2184 = add i32 %2182, -178946048
  %2185 = add i32 %2184, %2181
  %2186 = add i32 %2185, %2183
  br label %2187

; <label>:2187                                    ; preds = %2177, %2119
  %2188 = phi i32 [ %2186, %2177 ], [ %2138, %2119 ]
  %2189 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %2190 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %2189, i32 %2188, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2191 = extractvalue %dx.types.ResRet.i32 %2190, 0
  %2192 = lshr i32 %2191, 20
  %2193 = and i32 %2192, 63
  %2194 = icmp slt i32 %2191, 0
  %2195 = select i1 %2176, i32 7, i32 %2193
  %2196 = and i32 %2195, 31
  %2197 = lshr i32 16384, %2196
  %2198 = uitofp i32 %2197 to float
  %2199 = fmul fast float %2198, %2173
  %2200 = fmul fast float %2198, %2174
  %2201 = fptoui float %2199 to i32
  %2202 = fptoui float %2200 to i32
  br i1 %2194, label %2203, label %2216

; <label>:2203                                    ; preds = %2187
  %2204 = lshr i32 %2191, 3
  %2205 = and i32 %2204, 130944
  %2206 = and i32 %2202, 127
  %2207 = or i32 %2206, %2205
  %2208 = shl i32 %2191, 7
  %2209 = and i32 %2208, 130944
  %2210 = and i32 %2201, 127
  %2211 = or i32 %2210, %2209
  %2212 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %2213 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %2212, i32 0, i32 %2211, i32 %2207, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2214 = extractvalue %dx.types.ResRet.i32 %2213, 0
  %2215 = bitcast i32 %2214 to float
  br label %2216

; <label>:2216                                    ; preds = %2203, %2187
  %2217 = phi float [ %2199, %2203 ], [ 0.000000e+00, %2187 ]
  %2218 = phi float [ %2200, %2203 ], [ 0.000000e+00, %2187 ]
  %2219 = phi i32 [ %2201, %2203 ], [ 0, %2187 ]
  %2220 = phi i32 [ %2202, %2203 ], [ 0, %2187 ]
  %2221 = phi i1 [ true, %2203 ], [ false, %2187 ]
  %2222 = phi i32 [ %2138, %2203 ], [ -1, %2187 ]
  %2223 = phi i32 [ %2195, %2203 ], [ 0, %2187 ]
  %2224 = phi float [ %2215, %2203 ], [ 0.000000e+00, %2187 ]
  br i1 %2221, label %2225, label %2325

; <label>:2225                                    ; preds = %2216
  %2226 = mul i32 %2222, 288
  %2227 = add i32 %2226, 32
  %2228 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2227, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2229 = extractvalue %dx.types.ResRet.i32 %2228, 2
  %2230 = bitcast i32 %2229 to float
  %2231 = add i32 %2226, 128
  %2232 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2231, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2233 = extractvalue %dx.types.ResRet.i32 %2232, 0
  %2234 = extractvalue %dx.types.ResRet.i32 %2232, 1
  %2235 = extractvalue %dx.types.ResRet.i32 %2232, 2
  %2236 = bitcast i32 %2233 to float
  %2237 = bitcast i32 %2234 to float
  %2238 = bitcast i32 %2235 to float
  %2239 = add i32 %2226, 144
  %2240 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2239, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2241 = extractvalue %dx.types.ResRet.i32 %2240, 0
  %2242 = extractvalue %dx.types.ResRet.i32 %2240, 1
  %2243 = extractvalue %dx.types.ResRet.i32 %2240, 2
  %2244 = bitcast i32 %2241 to float
  %2245 = bitcast i32 %2242 to float
  %2246 = bitcast i32 %2243 to float
  %2247 = add i32 %2226, 160
  %2248 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2247, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2249 = extractvalue %dx.types.ResRet.i32 %2248, 0
  %2250 = extractvalue %dx.types.ResRet.i32 %2248, 1
  %2251 = extractvalue %dx.types.ResRet.i32 %2248, 2
  %2252 = bitcast i32 %2249 to float
  %2253 = bitcast i32 %2250 to float
  %2254 = bitcast i32 %2251 to float
  %2255 = add i32 %2226, 176
  %2256 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1996, i32 %2255, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2257 = extractvalue %dx.types.ResRet.i32 %2256, 0
  %2258 = extractvalue %dx.types.ResRet.i32 %2256, 1
  %2259 = extractvalue %dx.types.ResRet.i32 %2256, 2
  %2260 = bitcast i32 %2257 to float
  %2261 = bitcast i32 %2258 to float
  %2262 = bitcast i32 %2259 to float
  %2263 = call float @dx.op.dot3.f32(i32 55, float %365, float %366, float %367, float %2063, float %2064, float %2065)  ; Dot3(ax,ay,az,bx,by,bz)
  %2264 = fsub fast float -0.000000e+00, %2263
  %2265 = fmul fast float %2236, %365
  %2266 = call float @dx.op.tertiary.f32(i32 46, float %366, float %2244, float %2265)  ; FMad(a,b,c)
  %2267 = call float @dx.op.tertiary.f32(i32 46, float %367, float %2252, float %2266)  ; FMad(a,b,c)
  %2268 = call float @dx.op.tertiary.f32(i32 46, float %2264, float %2260, float %2267)  ; FMad(a,b,c)
  %2269 = fmul fast float %2237, %365
  %2270 = call float @dx.op.tertiary.f32(i32 46, float %366, float %2245, float %2269)  ; FMad(a,b,c)
  %2271 = call float @dx.op.tertiary.f32(i32 46, float %367, float %2253, float %2270)  ; FMad(a,b,c)
  %2272 = call float @dx.op.tertiary.f32(i32 46, float %2264, float %2261, float %2271)  ; FMad(a,b,c)
  %2273 = fmul fast float %2238, %365
  %2274 = call float @dx.op.tertiary.f32(i32 46, float %366, float %2246, float %2273)  ; FMad(a,b,c)
  %2275 = call float @dx.op.tertiary.f32(i32 46, float %367, float %2254, float %2274)  ; FMad(a,b,c)
  %2276 = call float @dx.op.tertiary.f32(i32 46, float %2264, float %2262, float %2275)  ; FMad(a,b,c)
  %2277 = fsub fast float -0.000000e+00, %2268
  %2278 = fsub fast float -0.000000e+00, %2272
  %2279 = fdiv fast float %2277, %2276
  %2280 = fdiv fast float %2278, %2276
  %2281 = and i32 %2223, 31
  %2282 = lshr i32 16384, %2281
  %2283 = uitofp i32 %2282 to float
  %2284 = uitofp i32 %2219 to float
  %2285 = uitofp i32 %2220 to float
  %2286 = fsub fast float 5.000000e-01, %2217
  %2287 = fadd fast float %2286, %2284
  %2288 = fsub fast float 5.000000e-01, %2218
  %2289 = fadd fast float %2288, %2285
  %2290 = fdiv fast float %2287, %2283
  %2291 = fdiv fast float %2289, %2283
  %2292 = call float @dx.op.dot2.f32(i32 54, float %2279, float %2280, float %2290, float %2291)  ; Dot2(ax,ay,bx,by)
  %2293 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %2292)  ; FMax(a,b)
  %2294 = fmul fast float %2293, 2.000000e+00
  %2295 = fmul fast float %2230, 1.000000e+02
  %2296 = call float @dx.op.unary.f32(i32 6, float %2295)  ; FAbs(value)
  %2297 = call float @dx.op.binary.f32(i32 36, float %2294, float %2296)  ; FMin(a,b)
  %2298 = sub nsw i32 %2222, %2138
  %2299 = and i32 %2298, 31
  %2300 = shl i32 1, %2299
  %2301 = uitofp i32 %2300 to float
  %2302 = fmul fast float %2297, %2301
  %2303 = fmul fast float %1994, %2156
  %2304 = fsub fast float -0.000000e+00, %2303
  %2305 = fdiv fast float %2304, %2172
  %2306 = fsub fast float %2224, %2302
  %2307 = fsub fast float %2306, %2305
  %2308 = fcmp fast ogt float %2307, %2175
  br i1 %2308, label %2309, label %2325

; <label>:2309                                    ; preds = %2225
  %2310 = fmul fast float %2063, %2063
  %2311 = fmul fast float %2064, %2064
  %2312 = fadd fast float %2310, %2311
  %2313 = fmul fast float %2065, %2065
  %2314 = fadd fast float %2312, %2313
  %2315 = call float @dx.op.unary.f32(i32 24, float %2314)  ; Sqrt(value)
  %2316 = fsub fast float %2224, %2156
  %2317 = fdiv fast float %2155, %2316
  %2318 = fsub fast float %2175, %2156
  %2319 = fmul fast float %2315, %2318
  %2320 = fdiv fast float %2319, %2155
  %2321 = fmul fast float %2320, %2317
  %2322 = fsub fast float %2315, %2321
  %2323 = call float @dx.op.binary.f32(i32 35, float 0x3EB0C6F7A0000000, float %2322)  ; FMax(a,b)
  %2324 = fadd fast float %2323, %1994
  br label %2325

; <label>:2325                                    ; preds = %2309, %2225, %2216, %1982, %520
  %2326 = phi float [ 0.000000e+00, %520 ], [ %1992, %1982 ], [ 0.000000e+00, %2309 ], [ 1.000000e+00, %2225 ], [ 1.000000e+00, %2216 ]
  %2327 = phi float [ -1.000000e+00, %520 ], [ %1989, %1982 ], [ %2324, %2309 ], [ -1.000000e+00, %2225 ], [ -1.000000e+00, %2216 ]
  %2328 = fcmp fast olt float %217, 1.000000e+00
  %2329 = fcmp fast olt float %2326, 1.000000e+00
  %2330 = and i1 %2328, %2329
  br i1 %2330, label %2331, label %2339

; <label>:2331                                    ; preds = %2325
  %2332 = fmul fast float %216, %2327
  %2333 = call float @dx.op.unary.f32(i32 21, float %2332)  ; Exp(value)
  %2334 = call float @dx.op.unary.f32(i32 7, float %2333)  ; Saturate(value)
  %2335 = fsub fast float 1.000000e+00, %2334
  %2336 = fmul fast float %2335, %2326
  %2337 = fadd fast float %2336, %2334
  %2338 = fmul fast float %2337, %2337
  br label %2339

; <label>:2339                                    ; preds = %2331, %2325
  %2340 = phi float [ %2326, %2325 ], [ %2338, %2331 ]
  br i1 %298, label %2854, label %2341

; <label>:2341                                    ; preds = %2339
  %2342 = fcmp fast ogt float %2340, 0.000000e+00
  br i1 %2342, label %2343, label %2572

; <label>:2343                                    ; preds = %2341
  %2344 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %30, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %2345 = extractvalue %dx.types.CBufRet.i32 %2344, 1
  %2346 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %27, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %2347 = extractvalue %dx.types.CBufRet.i32 %2346, 0
  %2348 = extractvalue %dx.types.CBufRet.i32 %2346, 1
  %2349 = sitofp i32 %2347 to float
  %2350 = sitofp i32 %2348 to float
  %2351 = fmul fast float %2349, 0x3FE827F520000000
  %2352 = fmul fast float %2350, 0x3FE23C21A0000000
  %2353 = fptosi float %2351 to i32
  %2354 = fptosi float %2352 to i32
  %2355 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %27, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %2356 = extractvalue %dx.types.CBufRet.i32 %2355, 0
  %2357 = extractvalue %dx.types.CBufRet.i32 %2355, 1
  %2358 = extractvalue %dx.types.CBufRet.i32 %2355, 2
  %2359 = and i32 %2356, %69
  %2360 = and i32 %2357, %70
  %2361 = and i32 %2358, %2345
  %2362 = mul i32 %2361, %2348
  %2363 = add i32 %2362, %2360
  %2364 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %2365 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2364, i32 0, i32 %2359, i32 %2363, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2366 = extractvalue %dx.types.ResRet.f32 %2365, 0
  %2367 = extractvalue %dx.types.ResRet.f32 %2365, 1
  %2368 = add i32 %2353, %69
  %2369 = add i32 %2354, %70
  %2370 = and i32 %2356, %2368
  %2371 = and i32 %2357, %2369
  %2372 = add i32 %2362, %2371
  %2373 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2364, i32 0, i32 %2370, i32 %2372, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2374 = extractvalue %dx.types.ResRet.f32 %2373, 0
  %2375 = fsub fast float %281, %371
  %2376 = fsub fast float %282, %372
  %2377 = fsub fast float %283, %373
  %2378 = call float @dx.op.dot3.f32(i32 55, float %2375, float %2376, float %2377, float %2375, float %2376, float %2377)  ; Dot3(ax,ay,az,bx,by,bz)
  %2379 = fadd fast float %2378, 1.000000e+00
  %2380 = fdiv fast float 1.000000e+00, %2379
  %2381 = call float @dx.op.unary.f32(i32 25, float %2378)  ; Rsqrt(value)
  %2382 = fmul fast float %2381, %2375
  %2383 = fmul fast float %2381, %2376
  %2384 = fmul fast float %2381, %2377
  %2385 = fmul fast float %2381, %292
  %2386 = fmul fast float %292, %292
  %2387 = fmul fast float %2386, %2380
  %2388 = call float @dx.op.unary.f32(i32 7, float %2387)  ; Saturate(value)
  %2389 = call float @dx.op.unary.f32(i32 24, float %2388)  ; Sqrt(value)
  %2390 = call float @dx.op.dot3.f32(i32 55, float %365, float %366, float %367, float %2382, float %2383, float %2384)  ; Dot3(ax,ay,az,bx,by,bz)
  %2391 = fsub fast float -0.000000e+00, %2389
  %2392 = fcmp fast olt float %2390, %2391
  br i1 %2392, label %2563, label %2393

; <label>:2393                                    ; preds = %2343
  %2394 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %31, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %2395 = extractvalue %dx.types.CBufRet.f32 %2394, 0
  %2396 = extractvalue %dx.types.ResRet.f32 %2373, 1
  %2397 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %31, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %2398 = extractvalue %dx.types.CBufRet.i32 %2397, 1
  %2399 = sitofp i32 %2398 to float
  %2400 = fmul fast float %2399, %2396
  %2401 = add nsw i32 %2398, -1
  %2402 = sitofp i32 %2401 to float
  %2403 = call float @dx.op.binary.f32(i32 36, float %2400, float %2402)  ; FMin(a,b)
  %2404 = fptoui float %2403 to i32
  %2405 = and i32 %69, 65535
  %2406 = shl nuw nsw i32 %2405, 8
  %2407 = or i32 %2406, %2405
  %2408 = and i32 %2407, 16711935
  %2409 = shl nuw nsw i32 %2408, 4
  %2410 = or i32 %2409, %2408
  %2411 = and i32 %2410, 252645135
  %2412 = shl nuw nsw i32 %2411, 2
  %2413 = or i32 %2412, %2411
  %2414 = and i32 %2413, 858993459
  %2415 = shl nuw nsw i32 %2414, 1
  %2416 = or i32 %2415, %2414
  %2417 = and i32 %2416, 1431655765
  %2418 = and i32 %70, 65535
  %2419 = shl nuw nsw i32 %2418, 8
  %2420 = or i32 %2419, %2418
  %2421 = and i32 %2420, 16711935
  %2422 = shl nuw nsw i32 %2421, 4
  %2423 = or i32 %2422, %2421
  %2424 = and i32 %2423, 252645135
  %2425 = shl nuw nsw i32 %2424, 2
  %2426 = or i32 %2425, %2424
  %2427 = and i32 %2426, 858993459
  %2428 = shl nuw nsw i32 %2427, 1
  %2429 = or i32 %2428, %2427
  %2430 = shl nuw i32 %2429, 1
  %2431 = and i32 %2430, -1431655766
  %2432 = or i32 %2431, %2417
  %2433 = extractvalue %dx.types.CBufRet.i32 %2344, 2
  %2434 = shl i32 %2433, 16
  %2435 = add i32 %2434, %2432
  %2436 = mul i32 %2435, %2398
  %2437 = add i32 %2436, %2404
  %2438 = call i32 @dx.op.unary.i32(i32 30, i32 %2437)  ; Bfrev(value)
  %2439 = add i32 %2438, 1216234700
  %2440 = mul i32 %2439, -1676577210
  %2441 = xor i32 %2440, %2439
  %2442 = mul i32 %2441, -529506958
  %2443 = xor i32 %2442, %2441
  %2444 = call i32 @dx.op.unary.i32(i32 30, i32 %2443)  ; Bfrev(value)
  %2445 = and i32 %2444, 255
  %2446 = and i32 %2444, 1
  %2447 = lshr i32 %2444, 1
  %2448 = and i32 %2447, 1
  %2449 = sub nsw i32 0, %2448
  %2450 = and i32 %2449, 3
  %2451 = xor i32 %2450, %2446
  %2452 = lshr i32 %2444, 2
  %2453 = and i32 %2452, 1
  %2454 = sub nsw i32 0, %2453
  %2455 = and i32 %2454, 5
  %2456 = xor i32 %2451, %2455
  %2457 = lshr i32 %2444, 3
  %2458 = and i32 %2457, 1
  %2459 = sub nsw i32 0, %2458
  %2460 = and i32 %2459, 15
  %2461 = xor i32 %2456, %2460
  %2462 = lshr i32 %2444, 4
  %2463 = and i32 %2462, 1
  %2464 = sub nsw i32 0, %2463
  %2465 = and i32 %2464, 17
  %2466 = xor i32 %2461, %2465
  %2467 = lshr i32 %2444, 5
  %2468 = and i32 %2467, 1
  %2469 = sub nsw i32 0, %2468
  %2470 = and i32 %2469, 51
  %2471 = xor i32 %2466, %2470
  %2472 = lshr i32 %2444, 6
  %2473 = and i32 %2472, 1
  %2474 = sub nsw i32 0, %2473
  %2475 = and i32 %2474, 85
  %2476 = xor i32 %2471, %2475
  %2477 = lshr i32 %2444, 7
  %2478 = and i32 %2477, 1
  %2479 = sub nsw i32 0, %2478
  %2480 = and i32 %2479, 255
  %2481 = xor i32 %2476, %2480
  %2482 = add nsw i32 %2445, -1862497895
  %2483 = mul i32 %2482, -1676577210
  %2484 = xor i32 %2483, %2482
  %2485 = mul i32 %2484, -529506958
  %2486 = xor i32 %2485, %2484
  %2487 = call i32 @dx.op.unary.i32(i32 30, i32 %2486)  ; Bfrev(value)
  %2488 = add i32 %2481, -646066581
  %2489 = mul i32 %2488, -1676577210
  %2490 = xor i32 %2489, %2488
  %2491 = mul i32 %2490, -529506958
  %2492 = xor i32 %2491, %2490
  %2493 = call i32 @dx.op.unary.i32(i32 30, i32 %2492)  ; Bfrev(value)
  %2494 = lshr i32 %2487, 8
  %2495 = lshr i32 %2493, 8
  %2496 = uitofp i32 %2494 to float
  %2497 = uitofp i32 %2495 to float
  %2498 = fmul fast float %2496, 0x3E76A09E60000000
  %2499 = fmul fast float %2497, 0x3E76A09E60000000
  %2500 = fadd fast float %2498, 0xBFE6A09E60000000
  %2501 = fadd fast float %2499, 0xBFE6A09E60000000
  %2502 = fmul fast float %2500, %2500
  %2503 = fmul fast float %2501, %2501
  %2504 = call float @dx.op.binary.f32(i32 35, float %2502, float %2503)  ; FMax(a,b)
  %2505 = fmul fast float %2504, 2.000000e+00
  %2506 = call float @dx.op.binary.f32(i32 36, float %2502, float %2503)  ; FMin(a,b)
  %2507 = fsub fast float %2505, %2506
  %2508 = call float @dx.op.unary.f32(i32 24, float %2507)  ; Sqrt(value)
  %2509 = fcmp fast ogt float %2502, %2503
  %2510 = fsub fast float -0.000000e+00, %2508
  %2511 = fcmp fast ogt float %2500, 0.000000e+00
  %2512 = select i1 %2511, float %2508, float %2510
  %2513 = fcmp fast ogt float %2501, 0.000000e+00
  %2514 = select i1 %2513, float %2508, float %2510
  %2515 = select i1 %2509, float %2512, float %2500
  %2516 = select i1 %2509, float %2501, float %2514
  %2517 = fmul fast float %2515, %2385
  %2518 = fmul fast float %2516, %2385
  %2519 = call float @dx.op.dot2.f32(i32 54, float %2517, float %2518, float %2517, float %2518)  ; Dot2(ax,ay,bx,by)
  %2520 = call float @dx.op.unary.f32(i32 24, float %2519)  ; Sqrt(value)
  %2521 = fsub fast float 1.000000e+00, %2519
  %2522 = call float @dx.op.unary.f32(i32 24, float %2521)  ; Sqrt(value)
  %2523 = fcmp fast oge float %2384, 0.000000e+00
  %2524 = select i1 %2523, float 1.000000e+00, float -1.000000e+00
  %2525 = fadd fast float %2524, %2384
  %2526 = fdiv fast float 1.000000e+00, %2525
  %2527 = fsub fast float -0.000000e+00, %2526
  %2528 = fmul fast float %2382, %2383
  %2529 = fmul fast float %2528, %2527
  %2530 = fmul fast float %2382, %2382
  %2531 = fmul fast float %2530, %2524
  %2532 = fmul fast float %2531, %2527
  %2533 = fadd fast float %2532, 1.000000e+00
  %2534 = fmul fast float %2383, %2383
  %2535 = fmul fast float %2534, %2527
  %2536 = fadd fast float %2535, %2524
  %2537 = fsub fast float -0.000000e+00, %2383
  %2538 = fmul fast float %2533, %2517
  %2539 = call float @dx.op.tertiary.f32(i32 46, float %2518, float %2529, float %2538)  ; FMad(a,b,c)
  %2540 = call float @dx.op.tertiary.f32(i32 46, float %2522, float %2382, float %2539)  ; FMad(a,b,c)
  %2541 = fmul fast float %2517, %2524
  %2542 = fmul fast float %2541, %2529
  %2543 = call float @dx.op.tertiary.f32(i32 46, float %2518, float %2536, float %2542)  ; FMad(a,b,c)
  %2544 = call float @dx.op.tertiary.f32(i32 46, float %2522, float %2383, float %2543)  ; FMad(a,b,c)
  %2545 = fmul fast float %2382, %2524
  %2546 = fmul fast float %2545, %2517
  %2547 = fsub fast float -0.000000e+00, %2546
  %2548 = call float @dx.op.tertiary.f32(i32 46, float %2518, float %2537, float %2547)  ; FMad(a,b,c)
  %2549 = call float @dx.op.tertiary.f32(i32 46, float %2522, float %2384, float %2548)  ; FMad(a,b,c)
  %2550 = fmul fast float %2520, %2395
  %2551 = fadd fast float %2522, %2550
  %2552 = fdiv fast float 1.500000e+00, %2551
  %2553 = call float @dx.op.unary.f32(i32 7, float %2552)  ; Saturate(value)
  %2554 = fmul fast float %2378, 7.500000e-01
  %2555 = fmul fast float %2554, %2381
  %2556 = fmul fast float %2555, %2553
  %2557 = fmul fast float %2556, %2540
  %2558 = fmul fast float %2556, %2544
  %2559 = fmul fast float %2556, %2549
  %2560 = fadd fast float %2557, %371
  %2561 = fadd fast float %2558, %372
  %2562 = fadd fast float %2559, %373
  br label %2563

; <label>:2563                                    ; preds = %2393, %2343
  %2564 = phi float [ 0.000000e+00, %2343 ], [ %371, %2393 ]
  %2565 = phi float [ 0.000000e+00, %2343 ], [ %372, %2393 ]
  %2566 = phi float [ 0.000000e+00, %2343 ], [ %373, %2393 ]
  %2567 = phi float [ 0.000000e+00, %2343 ], [ %2560, %2393 ]
  %2568 = phi float [ 0.000000e+00, %2343 ], [ %2561, %2393 ]
  %2569 = phi float [ 0.000000e+00, %2343 ], [ %2562, %2393 ]
  %2570 = zext i1 %2392 to i32
  %2571 = xor i32 %2570, 1
  br label %2572

; <label>:2572                                    ; preds = %2563, %2341
  %2573 = phi float [ %2564, %2563 ], [ 0.000000e+00, %2341 ]
  %2574 = phi float [ %2565, %2563 ], [ 0.000000e+00, %2341 ]
  %2575 = phi float [ %2566, %2563 ], [ 0.000000e+00, %2341 ]
  %2576 = phi float [ %2567, %2563 ], [ 0.000000e+00, %2341 ]
  %2577 = phi float [ %2568, %2563 ], [ 0.000000e+00, %2341 ]
  %2578 = phi float [ %2569, %2563 ], [ 0.000000e+00, %2341 ]
  %2579 = phi i32 [ %2571, %2563 ], [ 0, %2341 ]
  %2580 = phi float [ %2366, %2563 ], [ 0.000000e+00, %2341 ]
  %2581 = phi float [ %2367, %2563 ], [ 0.000000e+00, %2341 ]
  %2582 = phi float [ %2374, %2563 ], [ 0.000000e+00, %2341 ]
  %2583 = icmp eq i32 %2579, 0
  br i1 %2583, label %2854, label %2584

; <label>:2584                                    ; preds = %2572
  %2585 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %26, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %2586 = extractvalue %dx.types.CBufRet.f32 %2585, 0
  %2587 = fmul fast float %2586, %337
  %2588 = fmul fast float %2586, %338
  %2589 = fmul fast float %2586, %339
  %2590 = fadd fast float %2580, -5.000000e-01
  %2591 = fadd fast float %2581, -5.000000e-01
  %2592 = fadd fast float %2582, -5.000000e-01
  %2593 = fadd fast float %2590, %2587
  %2594 = fadd fast float %2591, %2588
  %2595 = fadd fast float %2592, %2589
  %2596 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %26, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %2597 = extractvalue %dx.types.CBufRet.i32 %2596, 0
  %2598 = extractvalue %dx.types.CBufRet.i32 %2596, 1
  %2599 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %26, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %2600 = extractvalue %dx.types.CBufRet.i32 %2599, 0
  %2601 = extractvalue %dx.types.CBufRet.i32 %2599, 1
  %2602 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %26, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %2603 = extractvalue %dx.types.CBufRet.f32 %2602, 0
  %2604 = extractvalue %dx.types.CBufRet.f32 %2585, 3
  %2605 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %26, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %2606 = extractvalue %dx.types.CBufRet.i32 %2605, 0
  %2607 = icmp eq i32 %2606, 0
  br i1 %2607, label %2854, label %2608

; <label>:2608                                    ; preds = %2584
  br label %2609

; <label>:2609                                    ; preds = %2846, %2608
  %2610 = phi float [ %2850, %2846 ], [ %2340, %2608 ]
  %2611 = phi i32 [ %2851, %2846 ], [ 0, %2608 ]
  %2612 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 524, i32 32 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=32>
  %2613 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %2612, i32 %2611, i32 0, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2614 = extractvalue %dx.types.ResRet.f32 %2613, 0
  %2615 = extractvalue %dx.types.ResRet.f32 %2613, 1
  %2616 = extractvalue %dx.types.ResRet.f32 %2613, 2
  %2617 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %2612, i32 %2611, i32 12, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2618 = extractvalue %dx.types.ResRet.i32 %2617, 0
  %2619 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %2612, i32 %2611, i32 16, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2620 = extractvalue %dx.types.ResRet.f32 %2619, 0
  %2621 = extractvalue %dx.types.ResRet.f32 %2619, 1
  %2622 = extractvalue %dx.types.ResRet.f32 %2619, 2
  %2623 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %2612, i32 %2611, i32 28, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %2624 = extractvalue %dx.types.ResRet.i32 %2623, 0
  %2625 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %26, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %2626 = extractvalue %dx.types.CBufRet.i32 %2625, 0
  %2627 = and i32 %2618, 255
  %2628 = lshr i32 %2618, 8
  %2629 = and i32 %2628, 255
  %2630 = lshr i32 %2618, 16
  %2631 = and i32 %2630, 255
  %2632 = mul i32 %2626, %2627
  %2633 = mul i32 %2626, %2629
  %2634 = mul i32 %2626, %2631
  %2635 = and i32 %2624, 4194303
  %2636 = lshr i32 %2624, 22
  %2637 = uitofp i32 %2636 to float
  %2638 = fmul fast float %2637, 0x3F84050140000000
  %2639 = icmp ne i32 %2627, 0
  %2640 = icmp ne i32 %2629, 0
  %2641 = icmp ne i32 %2631, 0
  %2642 = and i1 %2639, %2640
  %2643 = and i1 %2641, %2642
  %2644 = fmul fast float %2638, %2593
  %2645 = fmul fast float %2638, %2594
  %2646 = fmul fast float %2638, %2595
  %2647 = fadd fast float %2644, %2573
  %2648 = fadd fast float %2645, %2574
  %2649 = fadd fast float %2646, %2575
  %2650 = fdiv fast float 0x4059933340000000, %2637
  br i1 %2643, label %2651, label %2846

; <label>:2651                                    ; preds = %2609
  %2652 = fsub fast float %2576, %2647
  %2653 = fsub fast float %2577, %2648
  %2654 = fsub fast float %2578, %2649
  %2655 = fdiv fast float 1.000000e+00, %2652
  %2656 = fdiv fast float 1.000000e+00, %2653
  %2657 = fdiv fast float 1.000000e+00, %2654
  %2658 = fsub fast float %2614, %2647
  %2659 = fsub fast float %2615, %2648
  %2660 = fsub fast float %2616, %2649
  %2661 = fmul fast float %2655, %2658
  %2662 = fmul fast float %2656, %2659
  %2663 = fmul fast float %2657, %2660
  %2664 = fsub fast float %2620, %2647
  %2665 = fsub fast float %2621, %2648
  %2666 = fsub fast float %2622, %2649
  %2667 = fmul fast float %2655, %2664
  %2668 = fmul fast float %2656, %2665
  %2669 = fmul fast float %2657, %2666
  %2670 = call float @dx.op.binary.f32(i32 36, float %2661, float %2667)  ; FMin(a,b)
  %2671 = call float @dx.op.binary.f32(i32 36, float %2662, float %2668)  ; FMin(a,b)
  %2672 = call float @dx.op.binary.f32(i32 36, float %2663, float %2669)  ; FMin(a,b)
  %2673 = call float @dx.op.binary.f32(i32 35, float %2661, float %2667)  ; FMax(a,b)
  %2674 = call float @dx.op.binary.f32(i32 35, float %2662, float %2668)  ; FMax(a,b)
  %2675 = call float @dx.op.binary.f32(i32 35, float %2663, float %2669)  ; FMax(a,b)
  %2676 = call float @dx.op.binary.f32(i32 35, float %2671, float %2672)  ; FMax(a,b)
  %2677 = call float @dx.op.binary.f32(i32 35, float %2670, float %2676)  ; FMax(a,b)
  %2678 = call float @dx.op.binary.f32(i32 36, float %2674, float %2675)  ; FMin(a,b)
  %2679 = call float @dx.op.binary.f32(i32 36, float %2673, float %2678)  ; FMin(a,b)
  %2680 = call float @dx.op.unary.f32(i32 7, float %2677)  ; Saturate(value)
  %2681 = call float @dx.op.unary.f32(i32 7, float %2679)  ; Saturate(value)
  %2682 = fcmp fast olt float %2680, %2681
  br i1 %2682, label %2683, label %2846

; <label>:2683                                    ; preds = %2651
  %2684 = fmul fast float %2680, %2652
  %2685 = fmul fast float %2680, %2653
  %2686 = fmul fast float %2680, %2654
  %2687 = fsub fast float %2681, %2680
  %2688 = fmul fast float %2652, %2687
  %2689 = fsub fast float %2681, %2680
  %2690 = fmul fast float %2653, %2689
  %2691 = fsub fast float %2681, %2680
  %2692 = fmul fast float %2654, %2691
  %2693 = fmul fast float %2688, %2688
  %2694 = fmul fast float %2690, %2690
  %2695 = fadd fast float %2693, %2694
  %2696 = fmul fast float %2692, %2692
  %2697 = fadd fast float %2695, %2696
  %2698 = call float @dx.op.unary.f32(i32 24, float %2697)  ; Sqrt(value)
  %2699 = call float @dx.op.binary.f32(i32 36, float %2698, float 1.000000e+05)  ; FMin(a,b)
  %2700 = call float @dx.op.dot3.f32(i32 55, float %2688, float %2690, float %2692, float %2688, float %2690, float %2692)  ; Dot3(ax,ay,az,bx,by,bz)
  %2701 = call float @dx.op.unary.f32(i32 25, float %2700)  ; Rsqrt(value)
  %2702 = fdiv fast float %2699, %2638
  %2703 = call float @dx.op.unary.f32(i32 28, float %2702)  ; Round_pi(value)
  %2704 = call float @dx.op.binary.f32(i32 36, float %2703, float 1.024000e+03)  ; FMin(a,b)
  %2705 = fdiv fast float %2699, %2704
  %2706 = fcmp fast ogt float %2704, 0.000000e+00
  br i1 %2706, label %2707, label %2846

; <label>:2707                                    ; preds = %2683
  br label %2708

; <label>:2708                                    ; preds = %2837, %2707
  %2709 = phi i32 [ %2795, %2837 ], [ 9999, %2707 ]
  %2710 = phi i32 [ %2796, %2837 ], [ 9999, %2707 ]
  %2711 = phi i32 [ %2797, %2837 ], [ 9999, %2707 ]
  %2712 = phi i32 [ %2798, %2837 ], [ 0, %2707 ]
  %2713 = phi i32 [ %2799, %2837 ], [ 0, %2707 ]
  %2714 = phi i32 [ %2800, %2837 ], [ 0, %2707 ]
  %2715 = phi i32 [ %2801, %2837 ], [ 0, %2707 ]
  %2716 = phi float [ %2841, %2837 ], [ 1.000000e+00, %2707 ]
  %2717 = phi float [ %2842, %2837 ], [ 0.000000e+00, %2707 ]
  %2718 = phi float [ %2838, %2837 ], [ 0.000000e+00, %2707 ]
  %2719 = fmul fast float %2716, %2705
  %2720 = call float @dx.op.binary.f32(i32 35, float %2719, float 0.000000e+00)  ; FMax(a,b)
  %2721 = fmul fast float %2688, %2638
  %2722 = fmul fast float %2721, %2701
  %2723 = fmul fast float %2722, %2717
  %2724 = fmul fast float %2690, %2638
  %2725 = fmul fast float %2724, %2701
  %2726 = fmul fast float %2725, %2717
  %2727 = fmul fast float %2692, %2638
  %2728 = fmul fast float %2727, %2701
  %2729 = fmul fast float %2728, %2717
  %2730 = fmul fast float %2590, %2720
  %2731 = fmul fast float %2591, %2720
  %2732 = fmul fast float %2592, %2720
  %2733 = add i32 %2632, -1
  %2734 = add i32 %2633, -1
  %2735 = add i32 %2634, -1
  %2736 = fsub fast float %2647, %2614
  %2737 = fadd fast float %2736, %2684
  %2738 = fadd fast float %2737, %2723
  %2739 = fadd fast float %2738, %2730
  %2740 = fsub fast float %2648, %2615
  %2741 = fadd fast float %2740, %2685
  %2742 = fadd fast float %2741, %2726
  %2743 = fadd fast float %2742, %2731
  %2744 = fsub fast float %2649, %2616
  %2745 = fadd fast float %2744, %2686
  %2746 = fadd fast float %2745, %2729
  %2747 = fadd fast float %2746, %2732
  %2748 = fsub fast float %2620, %2614
  %2749 = fsub fast float %2621, %2615
  %2750 = fsub fast float %2622, %2616
  %2751 = fdiv fast float %2739, %2748
  %2752 = fdiv fast float %2743, %2749
  %2753 = fdiv fast float %2747, %2750
  %2754 = call float @dx.op.unary.f32(i32 7, float %2751)  ; Saturate(value)
  %2755 = call float @dx.op.unary.f32(i32 7, float %2752)  ; Saturate(value)
  %2756 = call float @dx.op.unary.f32(i32 7, float %2753)  ; Saturate(value)
  %2757 = uitofp i32 %2632 to float
  %2758 = uitofp i32 %2633 to float
  %2759 = uitofp i32 %2634 to float
  %2760 = fmul fast float %2754, %2757
  %2761 = fmul fast float %2755, %2758
  %2762 = fmul fast float %2756, %2759
  %2763 = fptoui float %2760 to i32
  %2764 = fptoui float %2761 to i32
  %2765 = fptoui float %2762 to i32
  %2766 = call i32 @dx.op.binary.i32(i32 40, i32 %2763, i32 %2733)  ; UMin(a,b)
  %2767 = call i32 @dx.op.binary.i32(i32 40, i32 %2764, i32 %2734)  ; UMin(a,b)
  %2768 = call i32 @dx.op.binary.i32(i32 40, i32 %2765, i32 %2735)  ; UMin(a,b)
  %2769 = and i32 %2601, 31
  %2770 = lshr i32 %2766, %2769
  %2771 = lshr i32 %2767, %2769
  %2772 = lshr i32 %2768, %2769
  %2773 = icmp ne i32 %2770, %2709
  %2774 = icmp ne i32 %2771, %2710
  %2775 = icmp ne i32 %2772, %2711
  %2776 = or i1 %2773, %2774
  %2777 = or i1 %2776, %2775
  br i1 %2777, label %2778, label %2794

; <label>:2778                                    ; preds = %2708
  %2779 = mul i32 %2772, %2629
  %2780 = add i32 %2779, %2771
  %2781 = mul i32 %2780, %2627
  %2782 = add i32 %2770, %2635
  %2783 = add i32 %2782, %2781
  %2784 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %4, %dx.types.ResourceProperties { i32 10, i32 261 })  ; AnnotateHandle(res,props)  resource: TypedBuffer<U32>
  %2785 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %2784, i32 %2783, i32 undef)  ; BufferLoad(srv,index,wot)
  %2786 = extractvalue %dx.types.ResRet.i32 %2785, 0
  %2787 = icmp ne i32 %2786, -1
  %2788 = zext i1 %2787 to i32
  %2789 = mul i32 %2597, %2598
  %2790 = urem i32 %2786, %2789
  %2791 = urem i32 %2790, %2597
  %2792 = udiv i32 %2790, %2597
  %2793 = udiv i32 %2786, %2789
  br label %2794

; <label>:2794                                    ; preds = %2778, %2708
  %2795 = phi i32 [ %2770, %2778 ], [ %2709, %2708 ]
  %2796 = phi i32 [ %2771, %2778 ], [ %2710, %2708 ]
  %2797 = phi i32 [ %2772, %2778 ], [ %2711, %2708 ]
  %2798 = phi i32 [ %2788, %2778 ], [ %2712, %2708 ]
  %2799 = phi i32 [ %2791, %2778 ], [ %2713, %2708 ]
  %2800 = phi i32 [ %2792, %2778 ], [ %2714, %2708 ]
  %2801 = phi i32 [ %2793, %2778 ], [ %2715, %2708 ]
  %2802 = icmp eq i32 %2798, 0
  br i1 %2802, label %2837, label %2803

; <label>:2803                                    ; preds = %2794
  %2804 = shl i32 %2799, %2769
  %2805 = shl i32 %2800, %2769
  %2806 = shl i32 %2801, %2769
  %2807 = shl i32 %2770, %2769
  %2808 = shl i32 %2771, %2769
  %2809 = shl i32 %2772, %2769
  %2810 = sub i32 %2766, %2807
  %2811 = sub i32 %2767, %2808
  %2812 = sub i32 %2768, %2809
  %2813 = add i32 %2810, %2804
  %2814 = add i32 %2811, %2805
  %2815 = add i32 %2812, %2806
  %2816 = fmul fast float %2720, %2650
  %2817 = call float @dx.op.unary.f32(i32 23, float %2816)  ; Log(value)
  %2818 = fptoui float %2817 to i32
  %2819 = and i32 %2818, 31
  %2820 = lshr i32 %2813, %2819
  %2821 = lshr i32 %2814, %2819
  %2822 = lshr i32 %2815, %2819
  %2823 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture3D<U32>
  %2824 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %2823, i32 %2818, i32 %2820, i32 %2821, i32 %2822, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2825 = extractvalue %dx.types.ResRet.i32 %2824, 0
  %2826 = icmp sgt i32 %2825, -1
  br i1 %2826, label %2827, label %2833

; <label>:2827                                    ; preds = %2803
  %2828 = and i32 %2825, 16777215
  %2829 = uitofp i32 %2828 to float
  %2830 = fmul fast float %2603, 0x3F50624DE0000000
  %2831 = fmul fast float %2830, %2816
  %2832 = fmul fast float %2831, %2829
  br label %2833

; <label>:2833                                    ; preds = %2827, %2803
  %2834 = phi float [ %2832, %2827 ], [ 0.000000e+00, %2803 ]
  %2835 = fadd fast float %2834, %2718
  %2836 = fcmp fast ogt float %2835, 1.000000e+00
  br i1 %2836, label %2844, label %2837

; <label>:2837                                    ; preds = %2833, %2794
  %2838 = phi float [ %2835, %2833 ], [ %2718, %2794 ]
  %2839 = fmul fast float %2716, %2604
  %2840 = uitofp i32 %2600 to float
  %2841 = call float @dx.op.binary.f32(i32 36, float %2840, float %2839)  ; FMin(a,b)
  %2842 = fadd fast float %2841, %2717
  %2843 = fcmp fast olt float %2842, %2704
  br i1 %2843, label %2708, label %2844

; <label>:2844                                    ; preds = %2837, %2833
  %2845 = phi float [ %2835, %2833 ], [ %2838, %2837 ]
  br label %2846

; <label>:2846                                    ; preds = %2844, %2683, %2651, %2609
  %2847 = phi float [ 0.000000e+00, %2651 ], [ 0.000000e+00, %2609 ], [ 0.000000e+00, %2683 ], [ %2845, %2844 ]
  %2848 = fsub fast float 1.000000e+00, %2847
  %2849 = call float @dx.op.unary.f32(i32 7, float %2848)  ; Saturate(value)
  %2850 = call float @dx.op.binary.f32(i32 36, float %2610, float %2849)  ; FMin(a,b)
  %2851 = add nuw i32 %2611, 1
  %2852 = icmp eq i32 %2851, %2606
  br i1 %2852, label %2853, label %2609

; <label>:2853                                    ; preds = %2846
  br label %2854

; <label>:2854                                    ; preds = %2853, %2584, %2572, %2339, %353
  %2855 = phi float [ %2340, %2339 ], [ %2340, %2572 ], [ 1.000000e+00, %353 ], [ %2340, %2584 ], [ %2850, %2853 ]
  %2856 = fcmp fast ogt float %2855, 0x3F91111120000000
  %2857 = fcmp fast olt float %2855, 1.000000e+00
  %2858 = and i1 %2856, %2857
  br i1 %2858, label %2859, label %2880

; <label>:2859                                    ; preds = %2854
  %2860 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %30, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %2861 = extractvalue %dx.types.CBufRet.i32 %2860, 2
  %2862 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %27, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %2863 = extractvalue %dx.types.CBufRet.i32 %2862, 0
  %2864 = extractvalue %dx.types.CBufRet.i32 %2862, 1
  %2865 = extractvalue %dx.types.CBufRet.i32 %2862, 2
  %2866 = and i32 %2863, %69
  %2867 = and i32 %2864, %70
  %2868 = and i32 %2865, %2861
  %2869 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %27, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %2870 = extractvalue %dx.types.CBufRet.i32 %2869, 1
  %2871 = mul i32 %2868, %2870
  %2872 = add i32 %2871, %2867
  %2873 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %2874 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2873, i32 0, i32 %2866, i32 %2872, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2875 = extractvalue %dx.types.ResRet.f32 %2874, 0
  %2876 = fadd fast float %2875, -5.000000e-01
  %2877 = fmul fast float %2876, 0x3FB1111120000000
  %2878 = fadd fast float %2877, %2855
  %2879 = call float @dx.op.unary.f32(i32 7, float %2878)  ; Saturate(value)
  br label %2880

; <label>:2880                                    ; preds = %2859, %2854
  %2881 = phi float [ %2879, %2859 ], [ %2855, %2854 ]
  %2882 = fmul fast float %2881, 1.500000e+01
  %2883 = call float @dx.op.unary.f32(i32 26, float %2882)  ; Round_ne(value)
  %2884 = fptoui float %2883 to i32
  %2885 = and i32 %2884, 15
  %2886 = lshr i32 %261, 3
  %2887 = icmp eq i32 %2886, 0
  br i1 %2887, label %2888, label %2892

; <label>:2888                                    ; preds = %2880
  %2889 = shl i32 %261, 2
  %2890 = and i32 %2889, 28
  %2891 = shl i32 %2885, %2890
  br label %2892

; <label>:2892                                    ; preds = %2888, %2880
  %2893 = phi i32 [ %2891, %2888 ], [ 0, %2880 ]
  %2894 = xor i32 %2893, %257
  %2895 = icmp eq i32 %2886, 1
  br i1 %2895, label %2896, label %2900

; <label>:2896                                    ; preds = %2892
  %2897 = shl i32 %261, 2
  %2898 = and i32 %2897, 28
  %2899 = shl i32 %2885, %2898
  br label %2900

; <label>:2900                                    ; preds = %2896, %2892
  %2901 = phi i32 [ %2899, %2896 ], [ 0, %2892 ]
  %2902 = xor i32 %2901, %258
  %2903 = icmp eq i32 %2886, 2
  br i1 %2903, label %2904, label %2908

; <label>:2904                                    ; preds = %2900
  %2905 = shl i32 %261, 2
  %2906 = and i32 %2905, 28
  %2907 = shl i32 %2885, %2906
  br label %2908

; <label>:2908                                    ; preds = %2904, %2900
  %2909 = phi i32 [ %2907, %2904 ], [ 0, %2900 ]
  %2910 = xor i32 %2909, %259
  %2911 = icmp eq i32 %2886, 3
  br i1 %2911, label %2912, label %2916

; <label>:2912                                    ; preds = %2908
  %2913 = shl i32 %261, 2
  %2914 = and i32 %2913, 28
  %2915 = shl i32 %2885, %2914
  br label %2916

; <label>:2916                                    ; preds = %2912, %2908
  %2917 = phi i32 [ %2915, %2912 ], [ 0, %2908 ]
  %2918 = xor i32 %2917, %260
  br label %2919

; <label>:2919                                    ; preds = %2916, %256
  %2920 = phi i32 [ %257, %256 ], [ %2894, %2916 ]
  %2921 = phi i32 [ %258, %256 ], [ %2902, %2916 ]
  %2922 = phi i32 [ %259, %256 ], [ %2910, %2916 ]
  %2923 = phi i32 [ %260, %256 ], [ %2918, %2916 ]
  %2924 = add nuw i32 %261, 1
  %2925 = icmp eq i32 %2924, %253
  br i1 %2925, label %2926, label %256, !llvm.loop !44

; <label>:2926                                    ; preds = %2919
  br label %2927

; <label>:2927                                    ; preds = %2926, %215
  %2928 = phi i32 [ 0, %215 ], [ %2920, %2926 ]
  %2929 = phi i32 [ 0, %215 ], [ %2921, %2926 ]
  %2930 = phi i32 [ 0, %215 ], [ %2922, %2926 ]
  %2931 = phi i32 [ 0, %215 ], [ %2923, %2926 ]
  %2932 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4098, i32 1029 })  ; AnnotateHandle(res,props)  resource: RWTexture2D<4xU32>
  call void @dx.op.textureStore.i32(i32 67, %dx.types.Handle %2932, i32 %69, i32 %70, i32 undef, i32 %2928, i32 %2929, i32 %2930, i32 %2931, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %2933

; <label>:2933                                    ; preds = %2927, %83, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.groupId.i32(i32, i32) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot2.f32(i32, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.legacyF16ToF32(i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind readnone
declare i32 @dx.op.unary.i32(i32, i32) #0

; Function Attrs: nounwind
declare i1 @dx.op.waveAllTrue(i32, i1) #2

; Function Attrs: nounwind
declare i1 @dx.op.waveAnyTrue(i32, i1) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #0

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind readonly }
attributes #2 = { nounwind }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.entryPoints = !{!39}

!0 = !{!"dxc(private) 1.7.0.0 (private, 00000000)"}
!1 = !{i32 1, i32 6}
!2 = !{i32 1, i32 7}
!3 = !{!"cs", i32 6, i32 6}
!4 = !{!5, !28, !30, !37}
!5 = !{!6, !8, !9, !10, !11, !13, !15, !16, !17, !19, !20, !21, !22, !23, !24, !25, !27}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i32 0, !7}
!9 = !{i32 2, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 2, i32 1, i32 2, i32 0, !7}
!10 = !{i32 3, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 3, i32 1, i32 2, i32 0, !7}
!11 = !{i32 4, %"class.StructuredBuffer<vector<float, 4> >"* undef, !"", i32 0, i32 4, i32 1, i32 12, i32 0, !12}
!12 = !{i32 1, i32 16}
!13 = !{i32 5, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 5, i32 1, i32 12, i32 0, !14}
!14 = !{i32 1, i32 4}
!15 = !{i32 6, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 6, i32 1, i32 11, i32 0, null}
!16 = !{i32 7, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 7, i32 1, i32 12, i32 0, !14}
!17 = !{i32 8, %"class.Texture2DArray<unsigned int>"* undef, !"", i32 0, i32 8, i32 1, i32 7, i32 0, !18}
!18 = !{i32 0, i32 5}
!19 = !{i32 9, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 9, i32 1, i32 12, i32 0, !14}
!20 = !{i32 10, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 10, i32 1, i32 12, i32 0, !14}
!21 = !{i32 11, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 11, i32 1, i32 2, i32 0, !7}
!22 = !{i32 12, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 12, i32 1, i32 2, i32 0, !7}
!23 = !{i32 13, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 13, i32 1, i32 2, i32 0, !7}
!24 = !{i32 14, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 14, i32 1, i32 10, i32 0, !18}
!25 = !{i32 15, %"class.StructuredBuffer<FPackedVirtualVoxelNodeDesc>"* undef, !"", i32 0, i32 15, i32 1, i32 12, i32 0, !26}
!26 = !{i32 1, i32 32}
!27 = !{i32 16, %"class.Texture3D<unsigned int>"* undef, !"", i32 0, i32 16, i32 1, i32 4, i32 0, !18}
!28 = !{!29}
!29 = !{i32 0, %"class.RWTexture2D<vector<unsigned int, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !18}
!30 = !{!31, !32, !33, !34, !35, !36}
!31 = !{i32 0, %_RootShaderParameters* undef, !"", i32 0, i32 0, i32 1, i32 384, null}
!32 = !{i32 1, %hostlayout.View* undef, !"", i32 0, i32 1, i32 1, i32 5724, null}
!33 = !{i32 2, %hostlayout.ForwardLightData* undef, !"", i32 0, i32 2, i32 1, i32 732, null}
!34 = !{i32 3, %VirtualShadowMap* undef, !"", i32 0, i32 3, i32 1, i32 156, null}
!35 = !{i32 4, %BlueNoise* undef, !"", i32 0, i32 4, i32 1, i32 44, null}
!36 = !{i32 5, %VirtualVoxel* undef, !"", i32 0, i32 5, i32 1, i32 212, null}
!37 = !{!38}
!38 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 0, i32 1, i32 0, null}
!39 = !{void ()* @VirtualShadowMapProjection, !"VirtualShadowMapProjection", null, !4, !40}
!40 = !{i32 0, i64 524304, i32 4, !41, i32 5, !42}
!41 = !{i32 8, i32 8, i32 1}
!42 = !{i32 0}
!43 = !{i32 1}
!44 = distinct !{!44, !45}
!45 = !{!"llvm.loop.unroll.disable"}

