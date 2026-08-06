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
; shader hash: f0f5b102aa327c7d4625414d76ec1285
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
;       float SMRTRayLengthScale;                     ; Offset:  124
;       float SMRTTexelDitherScale;                   ; Offset:  132
;       float SMRTExtrapolateSlope;                   ; Offset:  136
;       uint SMRTAdaptiveRayCount;                    ; Offset:  144
;       int4 ProjectionRect;                          ; Offset:  160
;       float NormalBias;                             ; Offset:  176
;       float SubsurfaceMinSourceRadius;              ; Offset:  180
;       uint InputType;                               ; Offset:  184
;       uint bCullBackfacingPixels;                   ; Offset:  188
;       float3 Light_TranslatedWorldPosition;         ; Offset:  224
;       float Light_InvRadius;                        ; Offset:  236
;       float3 Light_Color;                           ; Offset:  240
;       float Light_FalloffExponent;                  ; Offset:  252
;       float3 Light_Direction;                       ; Offset:  256
;       float Light_SpecularScale;                    ; Offset:  268
;       float3 Light_Tangent;                         ; Offset:  272
;       float Light_SourceRadius;                     ; Offset:  284
;       float2 Light_SpotAngles;                      ; Offset:  288
;       float Light_SoftSourceRadius;                 ; Offset:  296
;       float Light_SourceLength;                     ; Offset:  300
;       float Light_RectLightBarnCosAngle;            ; Offset:  304
;       float Light_RectLightBarnLength;              ; Offset:  308
;       float2 Light_RectLightAtlasUVOffset;          ; Offset:  312
;       float2 Light_RectLightAtlasUVScale;           ; Offset:  320
;       float Light_RectLightAtlasMaxLevel;           ; Offset:  328
;       int LightUniformVirtualShadowMapId;           ; Offset:  352
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
; Resource bind info for VirtualShadowMap_PageTable
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
; VirtualShadowMap                  cbuffer      NA          NA     CB2            cb2     1
; BlueNoise                         cbuffer      NA          NA     CB3            cb3     1
; VirtualVoxel                      cbuffer      NA          NA     CB4            cb4     1
; SceneTexturesStruct_PointClampSampler   sampler      NA          NA      S0             s0     1
; SceneTexturesStruct_SceneDepthTexture   texture     f32          2d      T0             t0     1
; SceneTexturesStruct_GBufferATexture   texture     f32          2d      T1             t1     1
; SceneTexturesStruct_GBufferBTexture   texture     f32          2d      T2             t2     1
; SceneTexturesStruct_GBufferDTexture   texture     f32          2d      T3             t3     1
; VirtualShadowMap_ProjectionData   texture    byte         r/o      T4             t4     1
; VirtualShadowMap_PageTable        texture  struct         r/o      T5             t5     1
; VirtualShadowMap_PhysicalPagePool   texture     u32     2darray      T6             t6     1
; BlueNoise_ScalarTexture           texture     f32          2d      T7             t7     1
; BlueNoise_Vec2Texture             texture     f32          2d      T8             t8     1
; HairStrands_HairOnlyDepthTexture   texture     f32          2d      T9             t9     1
; VirtualVoxel_PageIndexBuffer      texture     u32         buf     T10            t10     1
; VirtualVoxel_NodeDescBuffer       texture  struct         r/o     T11            t11     1
; VirtualVoxel_PageTexture          texture     u32          3d     T12            t12     1
; OutShadowFactor                       UAV     f32          2d      U0             u0     1
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
%struct.ByteAddressBuffer = type { i32 }
%"class.StructuredBuffer<unsigned int>" = type { i32 }
%"class.Texture2DArray<unsigned int>" = type { i32, %"class.Texture2DArray<unsigned int>::mips_type" }
%"class.Texture2DArray<unsigned int>::mips_type" = type { i32 }
%"class.Buffer<unsigned int>" = type { i32 }
%"class.StructuredBuffer<FPackedVirtualVoxelNodeDesc>" = type { %struct.FPackedVirtualVoxelNodeDesc }
%struct.FPackedVirtualVoxelNodeDesc = type { <3 x float>, i32, <3 x float>, i32 }
%"class.Texture3D<unsigned int>" = type { i32, %"class.Texture3D<unsigned int>::mips_type" }
%"class.Texture3D<unsigned int>::mips_type" = type { i32 }
%"class.RWTexture2D<vector<float, 2> >" = type { <2 x float> }
%_RootShaderParameters = type { float, i32, i32, float, float, float, i32, <4 x i32>, float, float, i32, i32, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <2 x float>, float, float, float, float, <2 x float>, <2 x float>, float, i32, i32 }
%hostlayout.View = type { %hostlayout.struct.FViewConstants }
%hostlayout.struct.FViewConstants = type { [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <4 x float>, <4 x float>, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], [4 x <4 x float>], <4 x float>, <4 x float>, <2 x float>, <2 x float>, <4 x float>, <4 x float>, <4 x i32>, <4 x float>, <4 x float>, <4 x float>, <4 x float>, <2 x float>, <2 x float>, i32, float, float, float, <4 x float>, <4 x float>, <4 x float>, <2 x float>, float, float, float, float, float, float, <3 x float>, float, float, float, float, float, float, float, i32, i32, i32, i32, i32, i32, i32, float, float, float, <4 x float>, <3 x float>, float, [2 x <4 x float>], [2 x <4 x float>], <4 x float>, <4 x float>, float, float, float, float, float, float, float, float, float, float, float, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, [2 x <4 x float>], [2 x <4 x float>], [2 x <4 x float>], [2 x <4 x float>], [2 x <4 x float>], <4 x float>, <3 x float>, float, <4 x float>, [4 x <4 x float>], <4 x float>, float, float, float, float, <4 x float>, float, float, float, float, float, float, float, float, <3 x float>, float, float, float, float, float, <4 x float>, float, float, float, float, <4 x float>, float, float, float, float, [8 x <4 x float>], float, float, float, float, i32, float, float, float, <3 x float>, i32, [6 x <4 x float>], [6 x <4 x float>], [6 x <4 x float>], [6 x <4 x float>], float, float, i32, i32, <3 x float>, float, <3 x float>, float, float, float, i32, float, float, float, float, float, <2 x i32>, float, float, <3 x float>, float, <3 x float>, float, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, float, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, <3 x float>, float, float, float, float, float, [2 x <4 x float>], float, i32, i32, i32, i32, i32, i32, i32, <4 x float>, <2 x float>, float, float, <4 x float>, i32, float, float, float, <4 x float>, i32, i32, i32, float, <4 x float>, <4 x float>, <4 x float>, <3 x float>, float, i32, i32, i32, i32, [32 x <4 x i32>], i32, float, float, float, <4 x float>, <4 x float>, <2 x float>, float, float, <4 x float>, <4 x float>, <4 x float>, <4 x i32>, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, <4 x float>, float, float, i32, i32, i32, i32, i32, i32, <4 x float>, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, <3 x float>, float, <3 x float> }
%VirtualShadowMap = type { %struct.FVirtualShadowMapConstants }
%struct.FVirtualShadowMapConstants = type { i32, i32, i32, i32, i32, i32, i32, i32, <4 x float>, <2 x i32>, <2 x i32>, i32, float, float, float, i32, i32, float, float, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32 }
%BlueNoise = type { %struct.FBlueNoiseConstants }
%struct.FBlueNoiseConstants = type { <3 x i32>, i32, <3 x i32>, i32, i32, i32, i32 }
%VirtualVoxel = type { %struct.FVirtualVoxelConstants }
%struct.FVirtualVoxelConstants = type { <3 x i32>, float, <3 x i32>, i32, i32, i32, i32, i32, i32, i32, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, <3 x float>, float, <3 x float>, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, float, i32 }
%struct.SamplerState = type { i32 }

define void @VirtualShadowMapProjection() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 12, i32 12, i32 0, i8 0 }, i32 12, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 11, i32 11, i32 0, i8 0 }, i32 11, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 10, i32 10, i32 0, i8 0 }, i32 10, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 9, i32 9, i32 0, i8 0 }, i32 9, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 8, i32 8, i32 0, i8 0 }, i32 8, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 7, i32 7, i32 0, i8 0 }, i32 7, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %8 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 6, i32 6, i32 0, i8 0 }, i32 6, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %9 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 5, i32 5, i32 0, i8 0 }, i32 5, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %10 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 4, i32 4, i32 0, i8 0 }, i32 4, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %11 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 0, i8 0 }, i32 3, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %12 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 0 }, i32 2, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %13 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 0 }, i32 1, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %14 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %15 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 3 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %16 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 4, i32 4, i32 0, i8 2 }, i32 4, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %17 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 0, i8 2 }, i32 3, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %18 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 2, i32 2, i32 0, i8 2 }, i32 2, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %19 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 2 }, i32 1, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %20 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 2 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %21 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %16, %dx.types.ResourceProperties { i32 13, i32 212 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %22 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %17, %dx.types.ResourceProperties { i32 13, i32 44 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %23 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %18, %dx.types.ResourceProperties { i32 13, i32 156 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %24 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %19, %dx.types.ResourceProperties { i32 13, i32 5724 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %25 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %20, %dx.types.ResourceProperties { i32 13, i32 384 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %26 = call i32 @dx.op.groupId.i32(i32 94, i32 0)  ; GroupId(component)
  %27 = call i32 @dx.op.groupId.i32(i32 94, i32 1)  ; GroupId(component)
  %28 = call i32 @dx.op.flattenedThreadIdInGroup.i32(i32 96)  ; FlattenedThreadIdInGroup()
  %29 = shl i32 %26, 3
  %30 = shl i32 %27, 3
  %31 = and i32 %28, 1431655765
  %32 = lshr i32 %31, 1
  %33 = or i32 %32, %31
  %34 = and i32 %33, 858993459
  %35 = lshr i32 %34, 2
  %36 = or i32 %35, %34
  %37 = and i32 %36, 252645135
  %38 = lshr i32 %37, 4
  %39 = or i32 %38, %37
  %40 = lshr i32 %39, 8
  %41 = and i32 %40, 65280
  %42 = and i32 %39, 255
  %43 = or i32 %41, %42
  %44 = lshr i32 %28, 1
  %45 = and i32 %44, 1431655765
  %46 = lshr i32 %45, 1
  %47 = or i32 %46, %45
  %48 = and i32 %47, 858993459
  %49 = lshr i32 %48, 2
  %50 = or i32 %49, %48
  %51 = and i32 %50, 252645135
  %52 = lshr i32 %51, 4
  %53 = or i32 %52, %51
  %54 = lshr i32 %53, 8
  %55 = and i32 %54, 65280
  %56 = and i32 %53, 255
  %57 = or i32 %55, %56
  %58 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %25, i32 10)  ; CBufferLoadLegacy(handle,regIndex)
  %59 = extractvalue %dx.types.CBufRet.i32 %58, 0
  %60 = extractvalue %dx.types.CBufRet.i32 %58, 1
  %61 = add i32 %59, %29
  %62 = add i32 %61, %43
  %63 = add i32 %60, %30
  %64 = add i32 %63, %57
  %65 = extractvalue %dx.types.CBufRet.i32 %58, 2
  %66 = extractvalue %dx.types.CBufRet.i32 %58, 3
  %67 = icmp uge i32 %62, %65
  %68 = icmp uge i32 %64, %66
  %69 = or i1 %67, %68
  br i1 %69, label %2107, label %70

; <label>:70                                      ; preds = %0
  %71 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %14, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %72 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %71, i32 0, i32 %62, i32 %64, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %73 = extractvalue %dx.types.ResRet.f32 %72, 0
  %74 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %25, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %75 = extractvalue %dx.types.CBufRet.i32 %74, 2
  %76 = icmp eq i32 %75, 1
  br i1 %76, label %77, label %82

; <label>:77                                      ; preds = %70
  %78 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %5, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %79 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %78, i32 0, i32 %62, i32 %64, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %80 = extractvalue %dx.types.ResRet.f32 %79, 0
  %81 = fcmp oeq float %80, 0.000000e+00
  br i1 %81, label %2107, label %82

; <label>:82                                      ; preds = %77, %70
  %83 = phi float [ %80, %77 ], [ %73, %70 ]
  %84 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 66)  ; CBufferLoadLegacy(handle,regIndex)
  %85 = extractvalue %dx.types.CBufRet.f32 %84, 0
  %86 = fmul fast float %85, %83
  %87 = extractvalue %dx.types.CBufRet.f32 %84, 1
  %88 = fadd fast float %86, %87
  %89 = extractvalue %dx.types.CBufRet.f32 %84, 2
  %90 = fmul fast float %89, %83
  %91 = extractvalue %dx.types.CBufRet.f32 %84, 3
  %92 = fsub fast float %90, %91
  %93 = fdiv fast float 1.000000e+00, %92
  %94 = fadd fast float %88, %93
  %95 = uitofp i32 %62 to float
  %96 = uitofp i32 %64 to float
  %97 = fadd float %95, 5.000000e-01
  %98 = fadd float %96, 5.000000e-01
  %99 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 44)  ; CBufferLoadLegacy(handle,regIndex)
  %100 = extractvalue %dx.types.CBufRet.f32 %99, 0
  %101 = extractvalue %dx.types.CBufRet.f32 %99, 1
  %102 = extractvalue %dx.types.CBufRet.f32 %99, 2
  %103 = extractvalue %dx.types.CBufRet.f32 %99, 3
  %104 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 45)  ; CBufferLoadLegacy(handle,regIndex)
  %105 = extractvalue %dx.types.CBufRet.f32 %104, 0
  %106 = extractvalue %dx.types.CBufRet.f32 %104, 1
  %107 = extractvalue %dx.types.CBufRet.f32 %104, 2
  %108 = extractvalue %dx.types.CBufRet.f32 %104, 3
  %109 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 46)  ; CBufferLoadLegacy(handle,regIndex)
  %110 = extractvalue %dx.types.CBufRet.f32 %109, 0
  %111 = extractvalue %dx.types.CBufRet.f32 %109, 1
  %112 = extractvalue %dx.types.CBufRet.f32 %109, 2
  %113 = extractvalue %dx.types.CBufRet.f32 %109, 3
  %114 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 47)  ; CBufferLoadLegacy(handle,regIndex)
  %115 = extractvalue %dx.types.CBufRet.f32 %114, 0
  %116 = extractvalue %dx.types.CBufRet.f32 %114, 1
  %117 = extractvalue %dx.types.CBufRet.f32 %114, 2
  %118 = extractvalue %dx.types.CBufRet.f32 %114, 3
  %119 = fmul float %97, %100
  %120 = call float @dx.op.tertiary.f32(i32 46, float %98, float %105, float %119), !dx.precise !37  ; FMad(a,b,c)
  %121 = call float @dx.op.tertiary.f32(i32 46, float %83, float %110, float %120), !dx.precise !37  ; FMad(a,b,c)
  %122 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %115, float %121), !dx.precise !37  ; FMad(a,b,c)
  %123 = fmul float %97, %101
  %124 = call float @dx.op.tertiary.f32(i32 46, float %98, float %106, float %123), !dx.precise !37  ; FMad(a,b,c)
  %125 = call float @dx.op.tertiary.f32(i32 46, float %83, float %111, float %124), !dx.precise !37  ; FMad(a,b,c)
  %126 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %116, float %125), !dx.precise !37  ; FMad(a,b,c)
  %127 = fmul float %97, %102
  %128 = call float @dx.op.tertiary.f32(i32 46, float %98, float %107, float %127), !dx.precise !37  ; FMad(a,b,c)
  %129 = call float @dx.op.tertiary.f32(i32 46, float %83, float %112, float %128), !dx.precise !37  ; FMad(a,b,c)
  %130 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %117, float %129), !dx.precise !37  ; FMad(a,b,c)
  %131 = fmul float %97, %103
  %132 = call float @dx.op.tertiary.f32(i32 46, float %98, float %108, float %131), !dx.precise !37  ; FMad(a,b,c)
  %133 = call float @dx.op.tertiary.f32(i32 46, float %83, float %113, float %132), !dx.precise !37  ; FMad(a,b,c)
  %134 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %118, float %133), !dx.precise !37  ; FMad(a,b,c)
  %135 = fdiv float %122, %134
  %136 = fdiv float %126, %134
  %137 = fdiv float %130, %134
  %138 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %139 = extractvalue %dx.types.CBufRet.f32 %138, 0
  %140 = fmul fast float %139, %94
  %141 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 299)  ; CBufferLoadLegacy(handle,regIndex)
  %142 = extractvalue %dx.types.CBufRet.f32 %141, 1
  %143 = fmul fast float %140, %142
  %144 = extractvalue %dx.types.CBufRet.f32 %141, 3
  %145 = fadd fast float %143, %144
  %146 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %24, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %147 = extractvalue %dx.types.CBufRet.i32 %146, 1
  %148 = uitofp i32 %147 to float
  %149 = fmul fast float %148, 0x4040551EC0000000
  %150 = fmul fast float %148, 0x4027A147A0000000
  %151 = fadd fast float %149, %97
  %152 = fadd fast float %150, %98
  %153 = call float @dx.op.dot2.f32(i32 54, float %151, float %152, float 0x3FB12E2860000000, float 0x3F77E8B200000000)  ; Dot2(ax,ay,bx,by)
  %154 = call float @dx.op.unary.f32(i32 22, float %153)  ; Frc(value)
  %155 = fmul fast float %154, 0x404A7DD040000000
  %156 = call float @dx.op.unary.f32(i32 22, float %155)  ; Frc(value)
  %157 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %13, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %158 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %157, i32 0, i32 %62, i32 %64, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %159 = extractvalue %dx.types.ResRet.f32 %158, 0
  %160 = extractvalue %dx.types.ResRet.f32 %158, 1
  %161 = extractvalue %dx.types.ResRet.f32 %158, 2
  %162 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %12, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %163 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %162, i32 0, i32 %62, i32 %64, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %164 = extractvalue %dx.types.ResRet.f32 %163, 3
  %165 = fmul float %164, 2.550000e+02
  %166 = fadd float %165, 5.000000e-01
  %167 = fptoui float %166 to i32
  %168 = and i32 %167, 15
  %169 = fmul float %159, 2.000000e+00
  %170 = fmul float %160, 2.000000e+00
  %171 = fmul float %161, 2.000000e+00
  %172 = fadd float %169, -1.000000e+00
  %173 = fadd float %170, -1.000000e+00
  %174 = fadd float %171, -1.000000e+00
  %175 = call float @dx.op.dot3.f32(i32 55, float %172, float %173, float %174, float %172, float %173, float %174), !dx.precise !37  ; Dot3(ax,ay,az,bx,by,bz)
  %176 = call float @dx.op.unary.f32(i32 25, float %175), !dx.precise !37  ; Rsqrt(value)
  %177 = fmul float %172, %176
  %178 = fmul float %173, %176
  %179 = fmul float %174, %176
  %180 = icmp ne i32 %168, 0
  %181 = icmp eq i32 %168, 7
  %182 = and i32 %167, 14
  %183 = icmp eq i32 %182, 2
  %184 = add nsw i32 %168, -5
  %185 = icmp ult i32 %184, 3
  %186 = or i1 %183, %185
  %187 = icmp eq i32 %168, 9
  %188 = or i1 %187, %186
  %189 = xor i1 %76, true
  %190 = icmp eq i32 %168, 6
  %191 = or i1 %183, %190
  %192 = and i1 %191, %189
  br i1 %192, label %193, label %209

; <label>:193                                     ; preds = %82
  %194 = icmp eq i32 %182, 8
  %195 = and i32 %167, 12
  %196 = icmp eq i32 %195, 4
  %197 = or i1 %196, %183
  %198 = or i1 %194, %197
  %199 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %11, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %200 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %199, i32 0, i32 %62, i32 %64, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %201 = extractvalue %dx.types.ResRet.f32 %200, 3
  %202 = select i1 %198, float %201, float 0.000000e+00
  %203 = call float @dx.op.binary.f32(i32 36, float %202, float 0x3FEFAE1480000000), !dx.precise !37  ; FMin(a,b)
  %204 = call float @dx.op.binary.f32(i32 36, float %203, float 0x3FEFAE1480000000)  ; FMin(a,b)
  %205 = fsub fast float 1.000000e+00, %204
  %206 = call float @dx.op.unary.f32(i32 23, float %205)  ; Log(value)
  %207 = fmul fast float %206, 0xBFA1BE9C00000000
  %208 = fmul float %207, 0xBFF7154760000000
  br label %209

; <label>:209                                     ; preds = %193, %82
  %210 = phi float [ -0.000000e+00, %82 ], [ %208, %193 ]
  %211 = phi float [ 1.000000e+00, %82 ], [ %203, %193 ]
  %212 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %25, i32 22)  ; CBufferLoadLegacy(handle,regIndex)
  %213 = extractvalue %dx.types.CBufRet.i32 %212, 0
  %214 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 16)  ; CBufferLoadLegacy(handle,regIndex)
  %215 = extractvalue %dx.types.CBufRet.f32 %214, 0
  %216 = extractvalue %dx.types.CBufRet.f32 %214, 1
  %217 = extractvalue %dx.types.CBufRet.f32 %214, 2
  %218 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 17)  ; CBufferLoadLegacy(handle,regIndex)
  %219 = extractvalue %dx.types.CBufRet.f32 %218, 3
  %220 = fcmp olt float %211, 1.000000e+00
  br i1 %220, label %221, label %227

; <label>:221                                     ; preds = %209
  %222 = fsub float 1.000000e+00, %211
  %223 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %224 = extractvalue %dx.types.CBufRet.f32 %223, 1
  %225 = fmul float %222, %224
  %226 = call float @dx.op.binary.f32(i32 35, float %219, float %225), !dx.precise !37  ; FMax(a,b)
  br label %227

; <label>:227                                     ; preds = %221, %209
  %228 = phi float [ %226, %221 ], [ %219, %209 ]
  %229 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 69)  ; CBufferLoadLegacy(handle,regIndex)
  %230 = extractvalue %dx.types.CBufRet.f32 %229, 0
  %231 = extractvalue %dx.types.CBufRet.f32 %229, 1
  %232 = extractvalue %dx.types.CBufRet.f32 %229, 2
  %233 = fsub float %135, %230
  %234 = fsub float %136, %231
  %235 = fsub float %137, %232
  %236 = fmul float %233, %233
  %237 = fmul float %234, %234
  %238 = fadd float %236, %237
  %239 = fmul float %235, %235
  %240 = fadd float %239, %238
  %241 = call float @dx.op.unary.f32(i32 24, float %240), !dx.precise !37  ; Sqrt(value)
  %242 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 31)  ; CBufferLoadLegacy(handle,regIndex)
  %243 = extractvalue %dx.types.CBufRet.f32 %242, 3
  %244 = fcmp ult float %243, 1.000000e+00
  br i1 %244, label %253, label %245

; <label>:245                                     ; preds = %227
  %246 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 61)  ; CBufferLoadLegacy(handle,regIndex)
  %247 = extractvalue %dx.types.CBufRet.f32 %246, 0
  %248 = extractvalue %dx.types.CBufRet.f32 %246, 1
  %249 = extractvalue %dx.types.CBufRet.f32 %246, 2
  %250 = call float @dx.op.dot3.f32(i32 55, float %233, float %234, float %235, float %247, float %248, float %249), !dx.precise !37  ; Dot3(ax,ay,az,bx,by,bz)
  %251 = fdiv float %241, %250
  %252 = fmul float %241, %251
  br label %253

; <label>:253                                     ; preds = %245, %227
  %254 = phi float [ %252, %245 ], [ %241, %227 ]
  %255 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 11)  ; CBufferLoadLegacy(handle,regIndex)
  %256 = extractvalue %dx.types.CBufRet.f32 %255, 0
  %257 = fmul float %254, %256
  %258 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 296)  ; CBufferLoadLegacy(handle,regIndex)
  %259 = extractvalue %dx.types.CBufRet.f32 %258, 2
  %260 = fdiv float %257, %259
  %261 = call float @dx.op.binary.f32(i32 35, float 0x3F947AE140000000, float %260), !dx.precise !37  ; FMax(a,b)
  %262 = or i1 %180, %76
  br i1 %262, label %263, label %2078

; <label>:263                                     ; preds = %253
  %264 = extractvalue %dx.types.CBufRet.i32 %74, 3
  %265 = icmp eq i32 %264, 0
  %266 = or i1 %76, %265
  %267 = or i1 %188, %266
  %268 = or i1 %181, %76
  %269 = select i1 %268, float %215, float %177
  %270 = select i1 %268, float %216, float %178
  %271 = select i1 %268, float %217, float %179
  %272 = fmul float %269, %261
  %273 = fmul float %270, %261
  %274 = fmul float %271, %261
  %275 = fadd float %135, %272
  %276 = fadd float %136, %273
  %277 = fadd float %137, %274
  %278 = xor i1 %76, true
  %279 = fcmp fast ogt float %145, 0.000000e+00
  %280 = and i1 %279, %278
  br i1 %280, label %281, label %418

; <label>:281                                     ; preds = %263
  %282 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %283 = extractvalue %dx.types.CBufRet.f32 %282, 0
  %284 = extractvalue %dx.types.CBufRet.f32 %282, 1
  %285 = extractvalue %dx.types.CBufRet.f32 %282, 2
  %286 = extractvalue %dx.types.CBufRet.f32 %282, 3
  %287 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %288 = extractvalue %dx.types.CBufRet.f32 %287, 0
  %289 = extractvalue %dx.types.CBufRet.f32 %287, 1
  %290 = extractvalue %dx.types.CBufRet.f32 %287, 2
  %291 = extractvalue %dx.types.CBufRet.f32 %287, 3
  %292 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %293 = extractvalue %dx.types.CBufRet.f32 %292, 0
  %294 = extractvalue %dx.types.CBufRet.f32 %292, 1
  %295 = extractvalue %dx.types.CBufRet.f32 %292, 2
  %296 = extractvalue %dx.types.CBufRet.f32 %292, 3
  %297 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %298 = extractvalue %dx.types.CBufRet.f32 %297, 0
  %299 = extractvalue %dx.types.CBufRet.f32 %297, 1
  %300 = extractvalue %dx.types.CBufRet.f32 %297, 2
  %301 = extractvalue %dx.types.CBufRet.f32 %297, 3
  %302 = fmul fast float %283, %275
  %303 = call float @dx.op.tertiary.f32(i32 46, float %276, float %288, float %302)  ; FMad(a,b,c)
  %304 = call float @dx.op.tertiary.f32(i32 46, float %277, float %293, float %303)  ; FMad(a,b,c)
  %305 = fadd fast float %304, %298
  %306 = fmul fast float %284, %275
  %307 = call float @dx.op.tertiary.f32(i32 46, float %276, float %289, float %306)  ; FMad(a,b,c)
  %308 = call float @dx.op.tertiary.f32(i32 46, float %277, float %294, float %307)  ; FMad(a,b,c)
  %309 = fadd fast float %308, %299
  %310 = fmul fast float %285, %275
  %311 = call float @dx.op.tertiary.f32(i32 46, float %276, float %290, float %310)  ; FMad(a,b,c)
  %312 = call float @dx.op.tertiary.f32(i32 46, float %277, float %295, float %311)  ; FMad(a,b,c)
  %313 = fadd fast float %312, %300
  %314 = fmul fast float %286, %275
  %315 = call float @dx.op.tertiary.f32(i32 46, float %276, float %291, float %314)  ; FMad(a,b,c)
  %316 = call float @dx.op.tertiary.f32(i32 46, float %277, float %296, float %315)  ; FMad(a,b,c)
  %317 = fadd fast float %316, %301
  %318 = fmul fast float %215, %145
  %319 = fmul fast float %216, %145
  %320 = fmul fast float %217, %145
  %321 = fmul fast float %283, %318
  %322 = call float @dx.op.tertiary.f32(i32 46, float %319, float %288, float %321)  ; FMad(a,b,c)
  %323 = call float @dx.op.tertiary.f32(i32 46, float %320, float %293, float %322)  ; FMad(a,b,c)
  %324 = fmul fast float %284, %318
  %325 = call float @dx.op.tertiary.f32(i32 46, float %319, float %289, float %324)  ; FMad(a,b,c)
  %326 = call float @dx.op.tertiary.f32(i32 46, float %320, float %294, float %325)  ; FMad(a,b,c)
  %327 = fmul fast float %285, %318
  %328 = call float @dx.op.tertiary.f32(i32 46, float %319, float %290, float %327)  ; FMad(a,b,c)
  %329 = call float @dx.op.tertiary.f32(i32 46, float %320, float %295, float %328)  ; FMad(a,b,c)
  %330 = fmul fast float %286, %318
  %331 = call float @dx.op.tertiary.f32(i32 46, float %319, float %291, float %330)  ; FMad(a,b,c)
  %332 = call float @dx.op.tertiary.f32(i32 46, float %320, float %296, float %331)  ; FMad(a,b,c)
  %333 = fadd fast float %323, %305
  %334 = fadd fast float %326, %309
  %335 = fadd fast float %329, %313
  %336 = fadd fast float %332, %317
  %337 = fdiv fast float %305, %317
  %338 = fdiv fast float %309, %317
  %339 = fdiv fast float %313, %317
  %340 = fdiv fast float %333, %336
  %341 = fdiv fast float %334, %336
  %342 = fdiv fast float %335, %336
  %343 = fsub fast float %340, %337
  %344 = fsub fast float %341, %338
  %345 = fsub fast float %342, %339
  %346 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 67)  ; CBufferLoadLegacy(handle,regIndex)
  %347 = extractvalue %dx.types.CBufRet.f32 %346, 0
  %348 = extractvalue %dx.types.CBufRet.f32 %346, 1
  %349 = fmul fast float %347, %337
  %350 = fmul fast float %348, %338
  %351 = extractvalue %dx.types.CBufRet.f32 %346, 2
  %352 = extractvalue %dx.types.CBufRet.f32 %346, 3
  %353 = fadd fast float %349, %352
  %354 = fadd fast float %350, %351
  %355 = fmul fast float %347, %343
  %356 = fmul fast float %348, %344
  %357 = fadd fast float %156, -5.000000e-01
  %358 = fmul fast float %357, 2.500000e-01
  %359 = fadd fast float %358, 2.500000e-01
  %360 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %15, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %361 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %71, %dx.types.Handle %360, float %353, float %354, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %362 = extractvalue %dx.types.ResRet.f32 %361, 0
  %363 = fmul fast float %355, %359
  %364 = fmul fast float %356, %359
  %365 = fmul fast float %345, %359
  %366 = fadd fast float %363, %353
  %367 = fadd fast float %364, %354
  %368 = fadd fast float %365, %339
  %369 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %71, %dx.types.Handle %360, float %366, float %367, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %370 = extractvalue %dx.types.ResRet.f32 %369, 0
  %371 = fcmp fast une float %370, %362
  %372 = fcmp fast olt float %368, %370
  %373 = and i1 %371, %372
  br i1 %373, label %413, label %374

; <label>:374                                     ; preds = %281
  %375 = fadd fast float %358, 5.000000e-01
  %376 = fmul fast float %355, %375
  %377 = fmul fast float %356, %375
  %378 = fmul fast float %345, %375
  %379 = fadd fast float %376, %353
  %380 = fadd fast float %377, %354
  %381 = fadd fast float %378, %339
  %382 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %71, %dx.types.Handle %360, float %379, float %380, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %383 = extractvalue %dx.types.ResRet.f32 %382, 0
  %384 = fcmp fast une float %383, %362
  %385 = fcmp fast olt float %381, %383
  %386 = and i1 %384, %385
  br i1 %386, label %413, label %387

; <label>:387                                     ; preds = %374
  %388 = fadd fast float %358, 7.500000e-01
  %389 = fmul fast float %355, %388
  %390 = fmul fast float %356, %388
  %391 = fmul fast float %345, %388
  %392 = fadd fast float %389, %353
  %393 = fadd fast float %390, %354
  %394 = fadd fast float %391, %339
  %395 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %71, %dx.types.Handle %360, float %392, float %393, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %396 = extractvalue %dx.types.ResRet.f32 %395, 0
  %397 = fcmp fast une float %396, %362
  %398 = fcmp fast olt float %394, %396
  %399 = and i1 %397, %398
  br i1 %399, label %413, label %400

; <label>:400                                     ; preds = %387
  %401 = fadd fast float %358, 1.000000e+00
  %402 = fmul fast float %355, %401
  %403 = fmul fast float %356, %401
  %404 = fmul fast float %345, %401
  %405 = fadd fast float %402, %353
  %406 = fadd fast float %403, %354
  %407 = fadd fast float %404, %339
  %408 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %71, %dx.types.Handle %360, float %405, float %406, float undef, float undef, i32 0, i32 0, i32 undef, float 0.000000e+00)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %409 = extractvalue %dx.types.ResRet.f32 %408, 0
  %410 = fcmp fast une float %409, %362
  %411 = fcmp fast olt float %407, %409
  %412 = and i1 %410, %411
  br i1 %412, label %413, label %418

; <label>:413                                     ; preds = %400, %387, %374, %281
  %414 = phi float [ %359, %281 ], [ %375, %374 ], [ %388, %387 ], [ %401, %400 ]
  %415 = fadd fast float %414, -3.750000e-01
  %416 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %415)  ; FMax(a,b)
  %417 = fmul fast float %416, %145
  br label %418

; <label>:418                                     ; preds = %413, %400, %263
  %419 = phi float [ %145, %263 ], [ %417, %413 ], [ %145, %400 ]
  %420 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %25, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %421 = extractvalue %dx.types.CBufRet.i32 %420, 1
  %422 = icmp sgt i32 %421, 0
  br i1 %422, label %423, label %1228

; <label>:423                                     ; preds = %418
  %424 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %25, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %425 = extractvalue %dx.types.CBufRet.i32 %424, 0
  %426 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %427 = extractvalue %dx.types.CBufRet.f32 %426, 2
  %428 = extractvalue %dx.types.CBufRet.f32 %426, 1
  %429 = extractvalue %dx.types.CBufRet.f32 %138, 3
  %430 = extractvalue %dx.types.CBufRet.i32 %420, 2
  %431 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %432 = extractvalue %dx.types.CBufRet.f32 %431, 0
  %433 = extractvalue %dx.types.CBufRet.f32 %431, 1
  %434 = extractvalue %dx.types.CBufRet.f32 %431, 2
  %435 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %436 = extractvalue %dx.types.CBufRet.f32 %435, 0
  %437 = extractvalue %dx.types.CBufRet.f32 %435, 1
  %438 = extractvalue %dx.types.CBufRet.f32 %435, 2
  %439 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %440 = extractvalue %dx.types.CBufRet.f32 %439, 0
  %441 = extractvalue %dx.types.CBufRet.f32 %439, 1
  %442 = extractvalue %dx.types.CBufRet.f32 %439, 2
  %443 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %444 = extractvalue %dx.types.CBufRet.f32 %443, 0
  %445 = extractvalue %dx.types.CBufRet.f32 %443, 1
  %446 = extractvalue %dx.types.CBufRet.f32 %443, 2
  %447 = fmul fast float %432, %275
  %448 = call float @dx.op.tertiary.f32(i32 46, float %276, float %436, float %447)  ; FMad(a,b,c)
  %449 = call float @dx.op.tertiary.f32(i32 46, float %277, float %440, float %448)  ; FMad(a,b,c)
  %450 = fadd fast float %449, %444
  %451 = fmul fast float %433, %275
  %452 = call float @dx.op.tertiary.f32(i32 46, float %276, float %437, float %451)  ; FMad(a,b,c)
  %453 = call float @dx.op.tertiary.f32(i32 46, float %277, float %441, float %452)  ; FMad(a,b,c)
  %454 = fadd fast float %453, %445
  %455 = fmul fast float %434, %275
  %456 = call float @dx.op.tertiary.f32(i32 46, float %276, float %438, float %455)  ; FMad(a,b,c)
  %457 = call float @dx.op.tertiary.f32(i32 46, float %277, float %442, float %456)  ; FMad(a,b,c)
  %458 = fadd fast float %457, %446
  %459 = fmul fast float %450, %450
  %460 = fmul fast float %454, %454
  %461 = fadd fast float %460, %459
  %462 = fmul fast float %458, %458
  %463 = fadd fast float %461, %462
  %464 = call float @dx.op.unary.f32(i32 24, float %463)  ; Sqrt(value)
  br i1 %267, label %471, label %465

; <label>:465                                     ; preds = %423
  %466 = call float @dx.op.unary.f32(i32 6, float %228), !dx.precise !37  ; FAbs(value)
  %467 = call float @dx.op.binary.f32(i32 35, float %466, float 0x3FB99999A0000000), !dx.precise !37  ; FMax(a,b)
  %468 = call float @dx.op.dot3.f32(i32 55, float %269, float %270, float %271, float %215, float %216, float %217), !dx.precise !37  ; Dot3(ax,ay,az,bx,by,bz)
  %469 = fsub float -0.000000e+00, %467
  %470 = fcmp olt float %468, %469
  br i1 %470, label %1603, label %471

; <label>:471                                     ; preds = %465, %423
  %472 = mul i32 %213, 288
  %473 = add i32 %472, 208
  %474 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %475 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %473, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %476 = extractvalue %dx.types.ResRet.i32 %475, 0
  %477 = extractvalue %dx.types.ResRet.i32 %475, 1
  %478 = extractvalue %dx.types.ResRet.i32 %475, 2
  %479 = bitcast i32 %476 to float
  %480 = bitcast i32 %477 to float
  %481 = bitcast i32 %478 to float
  %482 = add i32 %472, 224
  %483 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %482, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %484 = extractvalue %dx.types.ResRet.i32 %483, 0
  %485 = extractvalue %dx.types.ResRet.i32 %483, 1
  %486 = extractvalue %dx.types.ResRet.i32 %483, 2
  %487 = bitcast i32 %484 to float
  %488 = bitcast i32 %485 to float
  %489 = bitcast i32 %486 to float
  %490 = add i32 %472, 236
  %491 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %490, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %492 = extractvalue %dx.types.ResRet.i32 %491, 0
  %493 = bitcast i32 %492 to float
  %494 = add i32 %472, 240
  %495 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %494, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %496 = extractvalue %dx.types.ResRet.i32 %495, 0
  %497 = extractvalue %dx.types.ResRet.i32 %495, 1
  %498 = extractvalue %dx.types.ResRet.i32 %495, 2
  %499 = bitcast i32 %496 to float
  %500 = bitcast i32 %497 to float
  %501 = bitcast i32 %498 to float
  %502 = add i32 %472, 264
  %503 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %502, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %504 = extractvalue %dx.types.ResRet.i32 %503, 0
  %505 = add i32 %472, 268
  %506 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %505, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %507 = extractvalue %dx.types.ResRet.i32 %506, 0
  %508 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 72)  ; CBufferLoadLegacy(handle,regIndex)
  %509 = extractvalue %dx.types.CBufRet.f32 %508, 0
  %510 = extractvalue %dx.types.CBufRet.f32 %508, 1
  %511 = extractvalue %dx.types.CBufRet.f32 %508, 2
  %512 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 73)  ; CBufferLoadLegacy(handle,regIndex)
  %513 = extractvalue %dx.types.CBufRet.f32 %512, 0
  %514 = extractvalue %dx.types.CBufRet.f32 %512, 1
  %515 = extractvalue %dx.types.CBufRet.f32 %512, 2
  %516 = fsub float %479, %509
  %517 = fsub float %480, %510
  %518 = fsub float %481, %511
  %519 = fsub float %487, %513
  %520 = fsub float %488, %514
  %521 = fsub float %489, %515
  %522 = fadd float %516, %519
  %523 = fadd float %517, %520
  %524 = fadd float %518, %521
  %525 = fadd float %499, %522
  %526 = fadd float %500, %523
  %527 = fadd float %501, %524
  %528 = fadd float %275, %525
  %529 = fadd float %276, %526
  %530 = fadd float %277, %527
  %531 = fmul float %528, %528
  %532 = fmul float %529, %529
  %533 = fadd float %531, %532
  %534 = fmul float %530, %530
  %535 = fadd float %534, %533
  %536 = call float @dx.op.unary.f32(i32 24, float %535), !dx.precise !37  ; Sqrt(value)
  %537 = call float @dx.op.unary.f32(i32 23, float %536), !dx.precise !37  ; Log(value)
  %538 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %23, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %539 = extractvalue %dx.types.CBufRet.i32 %538, 1
  %540 = icmp ne i32 %539, 0
  %541 = select i1 %540, float 0.000000e+00, float %493
  %542 = fadd float %537, %541
  %543 = call float @dx.op.unary.f32(i32 27, float %542), !dx.precise !37  ; Round_ni(value)
  %544 = fptosi float %543 to i32
  %545 = sub nsw i32 %544, %504
  %546 = call i32 @dx.op.binary.i32(i32 37, i32 0, i32 %545)  ; IMax(a,b)
  %547 = icmp slt i32 %546, %507
  br i1 %547, label %548, label %682

; <label>:548                                     ; preds = %471
  %549 = add i32 %546, %213
  %550 = mul i32 %549, 288
  %551 = add i32 %550, 64
  %552 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %551, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %553 = extractvalue %dx.types.ResRet.i32 %552, 0
  %554 = extractvalue %dx.types.ResRet.i32 %552, 1
  %555 = bitcast i32 %553 to float
  %556 = bitcast i32 %554 to float
  %557 = add i32 %550, 80
  %558 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %557, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %559 = extractvalue %dx.types.ResRet.i32 %558, 0
  %560 = extractvalue %dx.types.ResRet.i32 %558, 1
  %561 = bitcast i32 %559 to float
  %562 = bitcast i32 %560 to float
  %563 = add i32 %550, 96
  %564 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %563, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %565 = extractvalue %dx.types.ResRet.i32 %564, 0
  %566 = extractvalue %dx.types.ResRet.i32 %564, 1
  %567 = bitcast i32 %565 to float
  %568 = bitcast i32 %566 to float
  %569 = add i32 %550, 112
  %570 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %569, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %571 = extractvalue %dx.types.ResRet.i32 %570, 0
  %572 = extractvalue %dx.types.ResRet.i32 %570, 1
  %573 = bitcast i32 %571 to float
  %574 = bitcast i32 %572 to float
  %575 = add i32 %550, 208
  %576 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %575, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %577 = extractvalue %dx.types.ResRet.i32 %576, 0
  %578 = extractvalue %dx.types.ResRet.i32 %576, 1
  %579 = extractvalue %dx.types.ResRet.i32 %576, 2
  %580 = bitcast i32 %577 to float
  %581 = bitcast i32 %578 to float
  %582 = bitcast i32 %579 to float
  %583 = add i32 %550, 224
  %584 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %583, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %585 = extractvalue %dx.types.ResRet.i32 %584, 0
  %586 = extractvalue %dx.types.ResRet.i32 %584, 1
  %587 = extractvalue %dx.types.ResRet.i32 %584, 2
  %588 = bitcast i32 %585 to float
  %589 = bitcast i32 %586 to float
  %590 = bitcast i32 %587 to float
  %591 = fsub float %580, %509
  %592 = fsub float %581, %510
  %593 = fsub float %582, %511
  %594 = fsub float %588, %513
  %595 = fsub float %589, %514
  %596 = fsub float %590, %515
  %597 = fadd float %591, %594
  %598 = fadd float %592, %595
  %599 = fadd float %593, %596
  %600 = fadd float %275, %597
  %601 = fadd float %276, %598
  %602 = fadd float %277, %599
  %603 = fmul float %555, %600
  %604 = call float @dx.op.tertiary.f32(i32 46, float %601, float %561, float %603), !dx.precise !37  ; FMad(a,b,c)
  %605 = call float @dx.op.tertiary.f32(i32 46, float %602, float %567, float %604), !dx.precise !37  ; FMad(a,b,c)
  %606 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %573, float %605), !dx.precise !37  ; FMad(a,b,c)
  %607 = fmul float %556, %600
  %608 = call float @dx.op.tertiary.f32(i32 46, float %601, float %562, float %607), !dx.precise !37  ; FMad(a,b,c)
  %609 = call float @dx.op.tertiary.f32(i32 46, float %602, float %568, float %608), !dx.precise !37  ; FMad(a,b,c)
  %610 = call float @dx.op.tertiary.f32(i32 46, float 1.000000e+00, float %574, float %609), !dx.precise !37  ; FMad(a,b,c)
  %611 = fmul float %606, 1.280000e+02
  %612 = fmul float %610, 1.280000e+02
  %613 = fptoui float %611 to i32
  %614 = fptoui float %612 to i32
  %615 = icmp ult i32 %549, 8192
  br i1 %615, label %622, label %616

; <label>:616                                     ; preds = %548
  %617 = mul i32 %549, 21845
  %618 = shl i32 %614, 7
  %619 = add i32 %617, -178946048
  %620 = add i32 %619, %613
  %621 = add i32 %620, %618
  br label %622

; <label>:622                                     ; preds = %616, %548
  %623 = phi i32 [ %621, %616 ], [ %549, %548 ]
  %624 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %9, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %625 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %624, i32 %623, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %626 = extractvalue %dx.types.ResRet.i32 %625, 0
  %627 = lshr i32 %626, 20
  %628 = and i32 %627, 63
  %629 = icmp slt i32 %626, 0
  br i1 %629, label %630, label %675

; <label>:630                                     ; preds = %622
  %631 = icmp eq i32 %628, 0
  %632 = zext i1 %631 to i32
  %633 = add i32 %628, %549
  br i1 %631, label %671, label %634

; <label>:634                                     ; preds = %630
  %635 = add i32 %550, 256
  %636 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %635, i32 undef, i8 3, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %637 = mul i32 %633, 288
  %638 = add i32 %637, 256
  %639 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %638, i32 undef, i8 3, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %640 = icmp ult i32 %633, 8192
  br i1 %640, label %664, label %641

; <label>:641                                     ; preds = %634
  %642 = extractvalue %dx.types.ResRet.i32 %636, 1
  %643 = shl i32 %642, 5
  %644 = sub i32 %614, %643
  %645 = extractvalue %dx.types.ResRet.i32 %639, 1
  %646 = shl i32 %645, 5
  %647 = and i32 %627, 31
  %648 = shl i32 %646, %647
  %649 = add i32 %644, %648
  %650 = lshr i32 %649, %647
  %651 = extractvalue %dx.types.ResRet.i32 %636, 0
  %652 = shl i32 %651, 5
  %653 = sub i32 %613, %652
  %654 = extractvalue %dx.types.ResRet.i32 %639, 0
  %655 = shl i32 %654, 5
  %656 = shl i32 %655, %647
  %657 = add i32 %653, %656
  %658 = lshr i32 %657, %647
  %659 = mul i32 %633, 21845
  %660 = shl i32 %650, 7
  %661 = add i32 %659, -178946048
  %662 = add i32 %661, %658
  %663 = add i32 %662, %660
  br label %664

; <label>:664                                     ; preds = %641, %634
  %665 = phi i32 [ %663, %641 ], [ %633, %634 ]
  %666 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %624, i32 %665, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %667 = extractvalue %dx.types.ResRet.i32 %666, 0
  %668 = and i32 %667, -2081423360
  %669 = icmp eq i32 %668, -2147483648
  %670 = zext i1 %669 to i32
  br label %671

; <label>:671                                     ; preds = %664, %630
  %672 = phi i32 [ %670, %664 ], [ %632, %630 ]
  %673 = icmp ne i32 %672, 0
  %674 = select i1 %673, i32 %633, i32 -1
  br label %675

; <label>:675                                     ; preds = %671, %622
  %676 = phi i32 [ 0, %622 ], [ %672, %671 ]
  %677 = phi i32 [ -1, %622 ], [ %674, %671 ]
  %678 = icmp ne i32 %676, 0
  %679 = icmp sgt i32 %677, %549
  %680 = and i1 %678, %679
  %681 = select i1 %680, i32 %677, i32 %549
  br label %682

; <label>:682                                     ; preds = %675, %471
  %683 = phi i32 [ %681, %675 ], [ -1, %471 ]
  %684 = icmp slt i32 %683, 0
  br i1 %684, label %1603, label %685

; <label>:685                                     ; preds = %682
  %686 = mul i32 %683, 288
  %687 = add i32 %686, 32
  %688 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %687, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %689 = extractvalue %dx.types.ResRet.i32 %688, 2
  %690 = bitcast i32 %689 to float
  %691 = add i32 %686, 64
  %692 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %691, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %693 = extractvalue %dx.types.ResRet.i32 %692, 0
  %694 = extractvalue %dx.types.ResRet.i32 %692, 1
  %695 = extractvalue %dx.types.ResRet.i32 %692, 2
  %696 = bitcast i32 %693 to float
  %697 = bitcast i32 %694 to float
  %698 = bitcast i32 %695 to float
  %699 = add i32 %686, 80
  %700 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %699, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %701 = extractvalue %dx.types.ResRet.i32 %700, 0
  %702 = extractvalue %dx.types.ResRet.i32 %700, 1
  %703 = extractvalue %dx.types.ResRet.i32 %700, 2
  %704 = bitcast i32 %701 to float
  %705 = bitcast i32 %702 to float
  %706 = bitcast i32 %703 to float
  %707 = add i32 %686, 96
  %708 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %707, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %709 = extractvalue %dx.types.ResRet.i32 %708, 0
  %710 = extractvalue %dx.types.ResRet.i32 %708, 1
  %711 = extractvalue %dx.types.ResRet.i32 %708, 2
  %712 = bitcast i32 %709 to float
  %713 = bitcast i32 %710 to float
  %714 = bitcast i32 %711 to float
  %715 = add i32 %686, 112
  %716 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %715, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %717 = extractvalue %dx.types.ResRet.i32 %716, 0
  %718 = extractvalue %dx.types.ResRet.i32 %716, 1
  %719 = extractvalue %dx.types.ResRet.i32 %716, 2
  %720 = bitcast i32 %717 to float
  %721 = bitcast i32 %718 to float
  %722 = bitcast i32 %719 to float
  %723 = add i32 %686, 128
  %724 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %723, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %725 = extractvalue %dx.types.ResRet.i32 %724, 0
  %726 = extractvalue %dx.types.ResRet.i32 %724, 1
  %727 = extractvalue %dx.types.ResRet.i32 %724, 2
  %728 = bitcast i32 %725 to float
  %729 = bitcast i32 %726 to float
  %730 = bitcast i32 %727 to float
  %731 = add i32 %686, 144
  %732 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %731, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %733 = extractvalue %dx.types.ResRet.i32 %732, 0
  %734 = extractvalue %dx.types.ResRet.i32 %732, 1
  %735 = extractvalue %dx.types.ResRet.i32 %732, 2
  %736 = bitcast i32 %733 to float
  %737 = bitcast i32 %734 to float
  %738 = bitcast i32 %735 to float
  %739 = add i32 %686, 160
  %740 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %739, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %741 = extractvalue %dx.types.ResRet.i32 %740, 0
  %742 = extractvalue %dx.types.ResRet.i32 %740, 1
  %743 = extractvalue %dx.types.ResRet.i32 %740, 2
  %744 = bitcast i32 %741 to float
  %745 = bitcast i32 %742 to float
  %746 = bitcast i32 %743 to float
  %747 = add i32 %686, 208
  %748 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %747, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %749 = extractvalue %dx.types.ResRet.i32 %748, 0
  %750 = extractvalue %dx.types.ResRet.i32 %748, 1
  %751 = extractvalue %dx.types.ResRet.i32 %748, 2
  %752 = bitcast i32 %749 to float
  %753 = bitcast i32 %750 to float
  %754 = bitcast i32 %751 to float
  %755 = add i32 %686, 224
  %756 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %755, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %757 = extractvalue %dx.types.ResRet.i32 %756, 0
  %758 = extractvalue %dx.types.ResRet.i32 %756, 1
  %759 = extractvalue %dx.types.ResRet.i32 %756, 2
  %760 = bitcast i32 %757 to float
  %761 = bitcast i32 %758 to float
  %762 = bitcast i32 %759 to float
  %763 = add i32 %686, 280
  %764 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %763, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %765 = extractvalue %dx.types.ResRet.i32 %764, 0
  %766 = bitcast i32 %765 to float
  %767 = fmul fast float %766, %428
  %768 = fcmp fast ogt float %767, 0.000000e+00
  br i1 %768, label %769, label %783

; <label>:769                                     ; preds = %685
  %770 = add i32 %686, 264
  %771 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %770, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %772 = extractvalue %dx.types.ResRet.i32 %771, 0
  %773 = add i32 %686, 236
  %774 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %474, i32 %773, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %775 = extractvalue %dx.types.ResRet.i32 %774, 0
  %776 = bitcast i32 %775 to float
  %777 = fmul fast float %464, 0x3F00000000000000
  %778 = fmul fast float %777, %767
  %779 = sitofp i32 %772 to float
  %780 = fsub fast float %779, %776
  %781 = call float @dx.op.unary.f32(i32 21, float %780)  ; Exp(value)
  %782 = fdiv fast float %778, %781
  br label %783

; <label>:783                                     ; preds = %769, %685
  %784 = phi float [ %782, %769 ], [ 0.000000e+00, %685 ]
  %785 = fmul fast float %728, %269
  %786 = call float @dx.op.tertiary.f32(i32 46, float %270, float %736, float %785)  ; FMad(a,b,c)
  %787 = call float @dx.op.tertiary.f32(i32 46, float %271, float %744, float %786)  ; FMad(a,b,c)
  %788 = fmul fast float %729, %269
  %789 = call float @dx.op.tertiary.f32(i32 46, float %270, float %737, float %788)  ; FMad(a,b,c)
  %790 = call float @dx.op.tertiary.f32(i32 46, float %271, float %745, float %789)  ; FMad(a,b,c)
  %791 = fmul fast float %730, %269
  %792 = call float @dx.op.tertiary.f32(i32 46, float %270, float %738, float %791)  ; FMad(a,b,c)
  %793 = call float @dx.op.tertiary.f32(i32 46, float %271, float %746, float %792)  ; FMad(a,b,c)
  %794 = fsub fast float -0.000000e+00, %787
  %795 = fsub fast float -0.000000e+00, %790
  %796 = fdiv fast float %794, %793
  %797 = fdiv fast float %795, %793
  %798 = call float @dx.op.binary.f32(i32 35, float %796, float 0xBFA99999A0000000)  ; FMax(a,b)
  %799 = call float @dx.op.binary.f32(i32 35, float %797, float 0xBFA99999A0000000)  ; FMax(a,b)
  %800 = call float @dx.op.binary.f32(i32 36, float %798, float 0x3FA99999A0000000)  ; FMin(a,b)
  %801 = call float @dx.op.binary.f32(i32 36, float %799, float 0x3FA99999A0000000)  ; FMin(a,b)
  %802 = fmul fast float %464, %429
  %803 = fsub float %752, %509
  %804 = fsub float %753, %510
  %805 = fsub float %754, %511
  %806 = fsub float %760, %513
  %807 = fsub float %761, %514
  %808 = fsub float %762, %515
  %809 = fadd float %803, %806
  %810 = fadd float %804, %807
  %811 = fadd float %805, %808
  %812 = fadd fast float %809, %275
  %813 = fadd fast float %810, %276
  %814 = fadd fast float %811, %277
  %815 = icmp eq i32 %421, 0
  br i1 %815, label %1215, label %816

; <label>:816                                     ; preds = %783
  br label %817

; <label>:817                                     ; preds = %1210, %816
  %818 = phi i32 [ %1197, %1210 ], [ 0, %816 ]
  %819 = phi i32 [ %1211, %1210 ], [ 0, %816 ]
  %820 = phi float [ %1198, %1210 ], [ 0.000000e+00, %816 ]
  %821 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %24, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %822 = extractvalue %dx.types.CBufRet.i32 %821, 2
  %823 = uitofp i32 %819 to float
  %824 = fmul fast float %823, 0x3FE827F520000000
  %825 = fmul fast float %823, 0x3FE23C21A0000000
  %826 = call float @dx.op.unary.f32(i32 22, float %824)  ; Frc(value)
  %827 = call float @dx.op.unary.f32(i32 22, float %825)  ; Frc(value)
  %828 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %22, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %829 = extractvalue %dx.types.CBufRet.i32 %828, 0
  %830 = extractvalue %dx.types.CBufRet.i32 %828, 1
  %831 = sitofp i32 %829 to float
  %832 = sitofp i32 %830 to float
  %833 = fmul fast float %831, %826
  %834 = fmul fast float %832, %827
  %835 = fptosi float %833 to i32
  %836 = fptosi float %834 to i32
  %837 = add i32 %819, %421
  %838 = uitofp i32 %837 to float
  %839 = fmul fast float %838, 0x3FE827F520000000
  %840 = fmul fast float %838, 0x3FE23C21A0000000
  %841 = call float @dx.op.unary.f32(i32 22, float %839)  ; Frc(value)
  %842 = call float @dx.op.unary.f32(i32 22, float %840)  ; Frc(value)
  %843 = fmul fast float %831, %841
  %844 = fmul fast float %832, %842
  %845 = fptosi float %843 to i32
  %846 = fptosi float %844 to i32
  %847 = add i32 %835, %62
  %848 = add i32 %836, %64
  %849 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %22, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %850 = extractvalue %dx.types.CBufRet.i32 %849, 0
  %851 = extractvalue %dx.types.CBufRet.i32 %849, 1
  %852 = extractvalue %dx.types.CBufRet.i32 %849, 2
  %853 = and i32 %847, %850
  %854 = and i32 %848, %851
  %855 = and i32 %852, %822
  %856 = mul i32 %855, %830
  %857 = add i32 %856, %854
  %858 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %859 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %858, i32 0, i32 %853, i32 %857, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %860 = extractvalue %dx.types.ResRet.f32 %859, 0
  %861 = extractvalue %dx.types.ResRet.f32 %859, 1
  %862 = add i32 %845, %62
  %863 = add i32 %846, %64
  %864 = and i32 %850, %862
  %865 = and i32 %851, %863
  %866 = add i32 %856, %865
  %867 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %858, i32 0, i32 %864, i32 %866, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %868 = extractvalue %dx.types.ResRet.f32 %867, 0
  %869 = extractvalue %dx.types.ResRet.f32 %867, 1
  %870 = fmul fast float %860, 2.000000e+00
  %871 = fmul fast float %861, 2.000000e+00
  %872 = fadd fast float %870, 0xBFEFFFFFE0000000
  %873 = fadd fast float %871, 0xBFEFFFFFE0000000
  %874 = call float @dx.op.unary.f32(i32 6, float %872)  ; FAbs(value)
  %875 = call float @dx.op.unary.f32(i32 6, float %873)  ; FAbs(value)
  %876 = call float @dx.op.binary.f32(i32 36, float %874, float %875)  ; FMin(a,b)
  %877 = call float @dx.op.binary.f32(i32 35, float %874, float %875)  ; FMax(a,b)
  %878 = fadd fast float %877, 0x3BF0000000000000
  %879 = fdiv fast float %876, %878
  %880 = fcmp fast oge float %875, %874
  %881 = uitofp i1 %880 to float
  %882 = fmul fast float %881, 2.000000e+00
  %883 = fadd fast float %879, %882
  %884 = fmul fast float %883, 0x3FE921FB60000000
  %885 = call float @dx.op.unary.f32(i32 12, float %884)  ; Cos(value)
  %886 = call float @dx.op.unary.f32(i32 13, float %884)  ; Sin(value)
  %887 = bitcast float %885 to i32
  %888 = bitcast float %886 to i32
  %889 = and i32 %887, 2147483647
  %890 = and i32 %888, 2147483647
  %891 = bitcast float %872 to i32
  %892 = bitcast float %873 to i32
  %893 = and i32 %891, -2147483648
  %894 = and i32 %892, -2147483648
  %895 = or i32 %889, %893
  %896 = or i32 %890, %894
  %897 = bitcast i32 %895 to float
  %898 = bitcast i32 %896 to float
  %899 = fmul fast float %877, %228
  %900 = fmul fast float %899, %897
  %901 = fmul fast float %899, %898
  %902 = call float @dx.op.unary.f32(i32 6, float %215)  ; FAbs(value)
  %903 = fcmp fast ogt float %902, 0x3EB0C6F7A0000000
  %904 = select i1 %903, float 1.000000e+00, float 0.000000e+00
  %905 = select i1 %903, float 0.000000e+00, float 1.000000e+00
  %906 = fmul fast float %217, %905
  %907 = fsub fast float -0.000000e+00, %906
  %908 = fmul fast float %904, %217
  %909 = fmul fast float %905, %215
  %910 = fmul fast float %904, %216
  %911 = fsub fast float %909, %910
  %912 = fmul fast float %908, %217
  %913 = fmul fast float %911, %216
  %914 = fsub fast float %912, %913
  %915 = fmul fast float %911, %215
  %916 = fmul fast float %217, %907
  %917 = fsub fast float %915, %916
  %918 = fmul fast float %216, %907
  %919 = fmul fast float %908, %215
  %920 = fsub fast float %918, %919
  %921 = fmul fast float %900, %907
  %922 = fmul fast float %900, %908
  %923 = fmul fast float %911, %900
  %924 = fmul fast float %914, %901
  %925 = fmul fast float %917, %901
  %926 = fmul fast float %920, %901
  %927 = fadd fast float %921, %215
  %928 = fadd fast float %927, %924
  %929 = fadd fast float %922, %216
  %930 = fadd fast float %929, %925
  %931 = fadd fast float %923, %217
  %932 = fadd fast float %931, %926
  %933 = call float @dx.op.dot3.f32(i32 55, float %928, float %930, float %932, float %928, float %930, float %932)  ; Dot3(ax,ay,az,bx,by,bz)
  %934 = call float @dx.op.unary.f32(i32 25, float %933)  ; Rsqrt(value)
  %935 = fmul fast float %928, %934
  %936 = fmul fast float %930, %934
  %937 = fmul fast float %932, %934
  %938 = fadd fast float %868, -5.000000e-01
  %939 = fadd fast float %869, -5.000000e-01
  %940 = fmul fast float %938, %784
  %941 = fmul fast float %939, %784
  %942 = fmul fast float %935, %419
  %943 = fmul fast float %936, %419
  %944 = fmul fast float %937, %419
  %945 = fadd fast float %812, %942
  %946 = fadd fast float %813, %943
  %947 = fadd fast float %814, %944
  %948 = fmul fast float %935, %802
  %949 = fmul fast float %936, %802
  %950 = fmul fast float %937, %802
  %951 = fmul fast float %945, %696
  %952 = call float @dx.op.tertiary.f32(i32 46, float %946, float %704, float %951)  ; FMad(a,b,c)
  %953 = call float @dx.op.tertiary.f32(i32 46, float %947, float %712, float %952)  ; FMad(a,b,c)
  %954 = fmul fast float %945, %697
  %955 = call float @dx.op.tertiary.f32(i32 46, float %946, float %705, float %954)  ; FMad(a,b,c)
  %956 = call float @dx.op.tertiary.f32(i32 46, float %947, float %713, float %955)  ; FMad(a,b,c)
  %957 = fmul fast float %945, %698
  %958 = call float @dx.op.tertiary.f32(i32 46, float %946, float %706, float %957)  ; FMad(a,b,c)
  %959 = call float @dx.op.tertiary.f32(i32 46, float %947, float %714, float %958)  ; FMad(a,b,c)
  %960 = fadd fast float %959, %722
  %961 = fmul fast float %948, %696
  %962 = call float @dx.op.tertiary.f32(i32 46, float %949, float %704, float %961)  ; FMad(a,b,c)
  %963 = call float @dx.op.tertiary.f32(i32 46, float %950, float %712, float %962)  ; FMad(a,b,c)
  %964 = fmul fast float %948, %697
  %965 = call float @dx.op.tertiary.f32(i32 46, float %949, float %705, float %964)  ; FMad(a,b,c)
  %966 = call float @dx.op.tertiary.f32(i32 46, float %950, float %713, float %965)  ; FMad(a,b,c)
  %967 = fmul fast float %948, %698
  %968 = call float @dx.op.tertiary.f32(i32 46, float %949, float %706, float %967)  ; FMad(a,b,c)
  %969 = call float @dx.op.tertiary.f32(i32 46, float %950, float %714, float %968)  ; FMad(a,b,c)
  %970 = call float @dx.op.dot2.f32(i32 54, float %800, float %801, float %940, float %941)  ; Dot2(ax,ay,bx,by)
  %971 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %970)  ; FMax(a,b)
  %972 = fmul fast float %971, 2.000000e+00
  %973 = fmul fast float %690, %419
  %974 = call float @dx.op.unary.f32(i32 6, float %973)  ; FAbs(value)
  %975 = fsub fast float %972, %974
  %976 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %975)  ; FMax(a,b)
  %977 = fadd fast float %960, %976
  %978 = fmul fast float %690, %427
  %979 = call float @dx.op.unary.f32(i32 6, float %978)  ; FAbs(value)
  %980 = sitofp i32 %430 to float
  %981 = fdiv fast float -1.000000e+00, %980
  %982 = fsub fast float 1.000000e+00, %156
  %983 = icmp sgt i32 %430, -1
  br i1 %983, label %984, label %1182

; <label>:984                                     ; preds = %817
  br label %985

; <label>:985                                     ; preds = %1172, %984
  %986 = phi float [ %1173, %1172 ], [ -1.000000e+04, %984 ]
  %987 = phi float [ %1174, %1172 ], [ -1.000000e+00, %984 ]
  %988 = phi float [ %1175, %1172 ], [ 0.000000e+00, %984 ]
  %989 = phi float [ %1176, %1172 ], [ -1.000000e+00, %984 ]
  %990 = phi i32 [ %1177, %1172 ], [ 0, %984 ]
  %991 = icmp eq i32 %990, %430
  br i1 %991, label %998, label %992

; <label>:992                                     ; preds = %985
  %993 = sitofp i32 %990 to float
  %994 = fadd fast float %993, %982
  %995 = fmul fast float %994, %981
  %996 = fadd fast float %995, 1.000000e+00
  %997 = fmul fast float %996, %996
  br label %998

; <label>:998                                     ; preds = %992, %985
  %999 = phi float [ %997, %992 ], [ 0.000000e+00, %985 ]
  %1000 = fmul fast float %999, %963
  %1001 = fmul fast float %999, %966
  %1002 = fmul fast float %999, %969
  %1003 = fadd fast float %940, %720
  %1004 = fadd fast float %1003, %953
  %1005 = fadd fast float %1004, %1000
  %1006 = fadd fast float %941, %721
  %1007 = fadd fast float %1006, %956
  %1008 = fadd fast float %1007, %1001
  %1009 = fadd fast float %1002, %977
  %1010 = fmul fast float %1005, 1.280000e+02
  %1011 = fmul fast float %1008, 1.280000e+02
  %1012 = fptoui float %1010 to i32
  %1013 = fptoui float %1011 to i32
  %1014 = icmp ult i32 %683, 8192
  br i1 %1014, label %1021, label %1015

; <label>:1015                                    ; preds = %998
  %1016 = mul i32 %683, 21845
  %1017 = shl i32 %1013, 7
  %1018 = add i32 %1016, -178946048
  %1019 = add i32 %1018, %1012
  %1020 = add i32 %1019, %1017
  br label %1021

; <label>:1021                                    ; preds = %1015, %998
  %1022 = phi i32 [ %1020, %1015 ], [ %683, %998 ]
  %1023 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %9, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1024 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1023, i32 %1022, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1025 = extractvalue %dx.types.ResRet.i32 %1024, 0
  %1026 = lshr i32 %1025, 20
  %1027 = and i32 %1026, 63
  %1028 = icmp slt i32 %1025, 0
  br i1 %1028, label %1029, label %1136

; <label>:1029                                    ; preds = %1021
  %1030 = icmp eq i32 %1027, 0
  %1031 = zext i1 %1030 to i32
  %1032 = add i32 %1027, %683
  %1033 = fmul fast float %1005, 1.638400e+04
  %1034 = fmul fast float %1008, 1.638400e+04
  %1035 = fptoui float %1033 to i32
  %1036 = fptoui float %1034 to i32
  br i1 %1030, label %1113, label %1037

; <label>:1037                                    ; preds = %1029
  %1038 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1039 = add i32 %686, 48
  %1040 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1038, i32 %1039, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1041 = extractvalue %dx.types.ResRet.i32 %1040, 2
  %1042 = bitcast i32 %1041 to float
  %1043 = add i32 %686, 256
  %1044 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1038, i32 %1043, i32 undef, i8 3, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1045 = extractvalue %dx.types.ResRet.i32 %1044, 0
  %1046 = extractvalue %dx.types.ResRet.i32 %1044, 1
  %1047 = mul i32 %1032, 288
  %1048 = add i32 %1047, 48
  %1049 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1038, i32 %1048, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1050 = extractvalue %dx.types.ResRet.i32 %1049, 2
  %1051 = bitcast i32 %1050 to float
  %1052 = add i32 %1047, 256
  %1053 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1038, i32 %1052, i32 undef, i8 3, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1054 = extractvalue %dx.types.ResRet.i32 %1053, 0
  %1055 = extractvalue %dx.types.ResRet.i32 %1053, 1
  %1056 = shl i32 %1045, 5
  %1057 = shl i32 %1046, 5
  %1058 = shl i32 %1054, 5
  %1059 = shl i32 %1055, 5
  %1060 = sub i32 %1012, %1056
  %1061 = sub i32 %1013, %1057
  %1062 = and i32 %1026, 31
  %1063 = shl i32 %1058, %1062
  %1064 = shl i32 %1059, %1062
  %1065 = add i32 %1060, %1063
  %1066 = add i32 %1061, %1064
  %1067 = lshr i32 %1065, %1062
  %1068 = lshr i32 %1066, %1062
  %1069 = shl i32 %1067, 7
  %1070 = shl i32 %1068, 7
  %1071 = or i32 %1069, 127
  %1072 = or i32 %1070, 127
  %1073 = sitofp i32 %1045 to float
  %1074 = sitofp i32 %1046 to float
  %1075 = sitofp i32 %1054 to float
  %1076 = sitofp i32 %1055 to float
  %1077 = shl i32 1, %1062
  %1078 = uitofp i32 %1077 to float
  %1079 = fdiv fast float 1.000000e+00, %1078
  %1080 = fmul fast float %1079, %1073
  %1081 = fmul fast float %1079, %1074
  %1082 = fsub fast float %1075, %1080
  %1083 = fsub fast float %1076, %1081
  %1084 = fmul fast float %1082, 2.500000e-01
  %1085 = fmul fast float %1083, 2.500000e-01
  %1086 = fmul fast float %1079, %1042
  %1087 = fsub fast float %1051, %1086
  %1088 = fmul fast float %1079, %1005
  %1089 = fmul fast float %1079, %1008
  %1090 = fadd fast float %1084, %1088
  %1091 = fadd fast float %1085, %1089
  %1092 = fmul fast float %1090, 1.638400e+04
  %1093 = fmul fast float %1091, 1.638400e+04
  %1094 = fptoui float %1092 to i32
  %1095 = fptoui float %1093 to i32
  %1096 = call i32 @dx.op.binary.i32(i32 39, i32 %1094, i32 %1069)  ; UMax(a,b)
  %1097 = call i32 @dx.op.binary.i32(i32 39, i32 %1095, i32 %1070)  ; UMax(a,b)
  %1098 = call i32 @dx.op.binary.i32(i32 40, i32 %1096, i32 %1071)  ; UMin(a,b)
  %1099 = call i32 @dx.op.binary.i32(i32 40, i32 %1097, i32 %1072)  ; UMin(a,b)
  %1100 = icmp ult i32 %1032, 8192
  br i1 %1100, label %1106, label %1101

; <label>:1101                                    ; preds = %1037
  %1102 = mul i32 %1032, 21845
  %1103 = add i32 %1102, -178946048
  %1104 = add i32 %1103, %1067
  %1105 = add i32 %1104, %1070
  br label %1106

; <label>:1106                                    ; preds = %1101, %1037
  %1107 = phi i32 [ %1105, %1101 ], [ %1032, %1037 ]
  %1108 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1023, i32 %1107, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1109 = extractvalue %dx.types.ResRet.i32 %1108, 0
  %1110 = and i32 %1109, -2081423360
  %1111 = icmp eq i32 %1110, -2147483648
  %1112 = zext i1 %1111 to i32
  br label %1113

; <label>:1113                                    ; preds = %1106, %1029
  %1114 = phi i32 [ %1098, %1106 ], [ %1035, %1029 ]
  %1115 = phi i32 [ %1099, %1106 ], [ %1036, %1029 ]
  %1116 = phi float [ %1079, %1106 ], [ 1.000000e+00, %1029 ]
  %1117 = phi float [ %1087, %1106 ], [ 0.000000e+00, %1029 ]
  %1118 = phi i32 [ %1112, %1106 ], [ %1031, %1029 ]
  %1119 = phi i32 [ %1109, %1106 ], [ %1025, %1029 ]
  %1120 = icmp eq i32 %1118, 0
  br i1 %1120, label %1136, label %1121

; <label>:1121                                    ; preds = %1113
  %1122 = shl i32 %1119, 7
  %1123 = and i32 %1122, 130944
  %1124 = lshr i32 %1119, 3
  %1125 = and i32 %1124, 130944
  %1126 = and i32 %1114, 127
  %1127 = and i32 %1115, 127
  %1128 = or i32 %1123, %1126
  %1129 = or i32 %1125, %1127
  %1130 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1131 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1130, i32 0, i32 %1128, i32 %1129, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1132 = extractvalue %dx.types.ResRet.i32 %1131, 0
  %1133 = bitcast i32 %1132 to float
  %1134 = fsub fast float %1133, %1117
  %1135 = fdiv fast float %1134, %1116
  br label %1136

; <label>:1136                                    ; preds = %1121, %1113, %1021
  %1137 = phi i1 [ true, %1121 ], [ false, %1113 ], [ false, %1021 ]
  %1138 = phi float [ %1135, %1121 ], [ 0.000000e+00, %1113 ], [ 0.000000e+00, %1021 ]
  %1139 = select i1 %1137, float %1138, float 0.000000e+00
  br i1 %1137, label %1140, label %1172

; <label>:1140                                    ; preds = %1136
  %1141 = fcmp fast oeq float %986, -1.000000e+04
  br i1 %1141, label %1142, label %1144

; <label>:1142                                    ; preds = %1140
  %1143 = fcmp fast ogt float %1139, %1009
  br i1 %1143, label %1179, label %1172

; <label>:1144                                    ; preds = %1140
  %1145 = fsub fast float %1009, %989
  %1146 = call float @dx.op.unary.f32(i32 6, float %1145)  ; FAbs(value)
  %1147 = fmul fast float %1146, 0x3FF0CCCCC0000000
  %1148 = fsub fast float %1139, %1009
  %1149 = fcmp fast ogt float %1148, %1147
  %1150 = fsub fast float %999, %987
  br i1 %1149, label %1151, label %1154

; <label>:1151                                    ; preds = %1144
  %1152 = fmul fast float %1150, %988
  %1153 = fadd fast float %1152, %986
  br label %1162

; <label>:1154                                    ; preds = %1144
  %1155 = fcmp fast une float %1139, %986
  br i1 %1155, label %1156, label %1162

; <label>:1156                                    ; preds = %1154
  %1157 = fsub fast float %1139, %986
  %1158 = fdiv fast float %1157, %1150
  %1159 = fsub fast float -0.000000e+00, %979
  %1160 = call float @dx.op.binary.f32(i32 35, float %1158, float %1159)  ; FMax(a,b)
  %1161 = call float @dx.op.binary.f32(i32 36, float %1160, float %979)  ; FMin(a,b)
  br label %1162

; <label>:1162                                    ; preds = %1156, %1154, %1151
  %1163 = phi float [ %986, %1151 ], [ %1138, %1156 ], [ %986, %1154 ]
  %1164 = phi float [ %987, %1151 ], [ %999, %1156 ], [ %987, %1154 ]
  %1165 = phi float [ %988, %1151 ], [ %1161, %1156 ], [ %988, %1154 ]
  %1166 = phi float [ %1153, %1151 ], [ %1138, %1156 ], [ %1138, %1154 ]
  %1167 = fmul fast float %1146, 0x3FE0CCCCC0000000
  %1168 = fadd fast float %1167, %1009
  %1169 = fsub fast float %1168, %1166
  %1170 = call float @dx.op.unary.f32(i32 6, float %1169)  ; FAbs(value)
  %1171 = fcmp fast olt float %1170, %1167
  br i1 %1171, label %1179, label %1172

; <label>:1172                                    ; preds = %1162, %1142, %1136
  %1173 = phi float [ %986, %1136 ], [ %1138, %1142 ], [ %1163, %1162 ]
  %1174 = phi float [ %987, %1136 ], [ %999, %1142 ], [ %1164, %1162 ]
  %1175 = phi float [ %988, %1136 ], [ %988, %1142 ], [ %1165, %1162 ]
  %1176 = phi float [ %989, %1136 ], [ %1009, %1142 ], [ %1009, %1162 ]
  %1177 = add nuw nsw i32 %990, 1
  %1178 = icmp slt i32 %990, %430
  br i1 %1178, label %985, label %1179

; <label>:1179                                    ; preds = %1172, %1162, %1142
  %1180 = phi float [ %1138, %1142 ], [ %1166, %1162 ], [ -1.000000e+00, %1172 ]
  %1181 = phi i1 [ true, %1142 ], [ true, %1162 ], [ false, %1172 ]
  br label %1182

; <label>:1182                                    ; preds = %1179, %817
  %1183 = phi float [ -1.000000e+00, %817 ], [ %1180, %1179 ]
  %1184 = phi i1 [ false, %817 ], [ %1181, %1179 ]
  br i1 %1184, label %1185, label %1194

; <label>:1185                                    ; preds = %1182
  %1186 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1187 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1186, i32 %687, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1188 = extractvalue %dx.types.ResRet.i32 %1187, 2
  %1189 = bitcast i32 %1188 to float
  %1190 = fsub fast float %977, %1183
  %1191 = fdiv fast float %1190, %1189
  %1192 = call float @dx.op.binary.f32(i32 35, float 0x3EB0C6F7A0000000, float %1191)  ; FMax(a,b)
  %1193 = fadd fast float %1192, %820
  br label %1196

; <label>:1194                                    ; preds = %1182
  %1195 = add i32 %818, 1
  br label %1196

; <label>:1196                                    ; preds = %1194, %1185
  %1197 = phi i32 [ %818, %1185 ], [ %1195, %1194 ]
  %1198 = phi float [ %1193, %1185 ], [ %820, %1194 ]
  %1199 = icmp eq i32 %425, 0
  br i1 %1199, label %1210, label %1200

; <label>:1200                                    ; preds = %1196
  %1201 = icmp eq i32 %819, 0
  br i1 %1201, label %1202, label %1205

; <label>:1202                                    ; preds = %1200
  %1203 = xor i1 %1184, true
  %1204 = call i1 @dx.op.waveAllTrue(i32 114, i1 %1203)  ; WaveAllTrue(cond)
  br i1 %1204, label %1213, label %1210

; <label>:1205                                    ; preds = %1200
  %1206 = icmp ult i32 %819, %425
  br i1 %1206, label %1210, label %1207

; <label>:1207                                    ; preds = %1205
  %1208 = icmp eq i32 %1197, 0
  %1209 = call i1 @dx.op.waveAllTrue(i32 114, i1 %1208)  ; WaveAllTrue(cond)
  br i1 %1209, label %1213, label %1210

; <label>:1210                                    ; preds = %1207, %1205, %1202, %1196
  %1211 = add nuw i32 %819, 1
  %1212 = icmp ult i32 %1211, %421
  br i1 %1212, label %817, label %1213

; <label>:1213                                    ; preds = %1210, %1207, %1202
  %1214 = phi i32 [ %1211, %1210 ], [ 0, %1202 ], [ %819, %1207 ]
  br label %1215

; <label>:1215                                    ; preds = %1213, %783
  %1216 = phi i32 [ 0, %783 ], [ %1197, %1213 ]
  %1217 = phi i32 [ 0, %783 ], [ %1214, %1213 ]
  %1218 = phi float [ 0.000000e+00, %783 ], [ %1198, %1213 ]
  %1219 = add i32 %1217, 1
  %1220 = call i32 @dx.op.binary.i32(i32 40, i32 %1219, i32 %421)  ; UMin(a,b)
  %1221 = sub i32 %1220, %1216
  %1222 = call i32 @dx.op.binary.i32(i32 39, i32 1, i32 %1221)  ; UMax(a,b)
  %1223 = uitofp i32 %1222 to float
  %1224 = fdiv fast float %1218, %1223
  %1225 = uitofp i32 %1216 to float
  %1226 = uitofp i32 %1220 to float
  %1227 = fdiv fast float %1225, %1226
  br label %1603

; <label>:1228                                    ; preds = %418
  %1229 = call float @dx.op.binary.f32(i32 35, float %419, float 0.000000e+00)  ; FMax(a,b)
  %1230 = mul i32 %213, 288
  %1231 = add i32 %1230, 208
  %1232 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %10, %dx.types.ResourceProperties { i32 11, i32 0 })  ; AnnotateHandle(res,props)  resource: ByteAddressBuffer
  %1233 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1231, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1234 = extractvalue %dx.types.ResRet.i32 %1233, 0
  %1235 = extractvalue %dx.types.ResRet.i32 %1233, 1
  %1236 = extractvalue %dx.types.ResRet.i32 %1233, 2
  %1237 = bitcast i32 %1234 to float
  %1238 = bitcast i32 %1235 to float
  %1239 = bitcast i32 %1236 to float
  %1240 = add i32 %1230, 224
  %1241 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1240, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1242 = extractvalue %dx.types.ResRet.i32 %1241, 0
  %1243 = extractvalue %dx.types.ResRet.i32 %1241, 1
  %1244 = extractvalue %dx.types.ResRet.i32 %1241, 2
  %1245 = bitcast i32 %1242 to float
  %1246 = bitcast i32 %1243 to float
  %1247 = bitcast i32 %1244 to float
  %1248 = add i32 %1230, 236
  %1249 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1248, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1250 = extractvalue %dx.types.ResRet.i32 %1249, 0
  %1251 = bitcast i32 %1250 to float
  %1252 = add i32 %1230, 240
  %1253 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1252, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1254 = extractvalue %dx.types.ResRet.i32 %1253, 0
  %1255 = extractvalue %dx.types.ResRet.i32 %1253, 1
  %1256 = extractvalue %dx.types.ResRet.i32 %1253, 2
  %1257 = bitcast i32 %1254 to float
  %1258 = bitcast i32 %1255 to float
  %1259 = bitcast i32 %1256 to float
  %1260 = add i32 %1230, 264
  %1261 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1260, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1262 = extractvalue %dx.types.ResRet.i32 %1261, 0
  %1263 = add i32 %1230, 268
  %1264 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1263, i32 undef, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1265 = extractvalue %dx.types.ResRet.i32 %1264, 0
  %1266 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 72)  ; CBufferLoadLegacy(handle,regIndex)
  %1267 = extractvalue %dx.types.CBufRet.f32 %1266, 0
  %1268 = extractvalue %dx.types.CBufRet.f32 %1266, 1
  %1269 = extractvalue %dx.types.CBufRet.f32 %1266, 2
  %1270 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 73)  ; CBufferLoadLegacy(handle,regIndex)
  %1271 = extractvalue %dx.types.CBufRet.f32 %1270, 0
  %1272 = extractvalue %dx.types.CBufRet.f32 %1270, 1
  %1273 = extractvalue %dx.types.CBufRet.f32 %1270, 2
  %1274 = fsub float %1237, %1267
  %1275 = fsub float %1238, %1268
  %1276 = fsub float %1239, %1269
  %1277 = fsub float %1245, %1271
  %1278 = fsub float %1246, %1272
  %1279 = fsub float %1247, %1273
  %1280 = fadd float %1274, %1277
  %1281 = fadd float %1275, %1278
  %1282 = fadd float %1276, %1279
  %1283 = fadd float %1257, %1280
  %1284 = fadd float %1258, %1281
  %1285 = fadd float %1259, %1282
  %1286 = fadd float %275, %1283
  %1287 = fadd float %276, %1284
  %1288 = fadd float %277, %1285
  %1289 = fmul float %1286, %1286
  %1290 = fmul float %1287, %1287
  %1291 = fadd float %1289, %1290
  %1292 = fmul float %1288, %1288
  %1293 = fadd float %1292, %1291
  %1294 = call float @dx.op.unary.f32(i32 24, float %1293), !dx.precise !37  ; Sqrt(value)
  %1295 = call float @dx.op.unary.f32(i32 23, float %1294), !dx.precise !37  ; Log(value)
  %1296 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %23, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %1297 = extractvalue %dx.types.CBufRet.i32 %1296, 1
  %1298 = icmp ne i32 %1297, 0
  %1299 = select i1 %1298, float 0.000000e+00, float %1251
  %1300 = fadd float %1295, %1299
  %1301 = call float @dx.op.unary.f32(i32 27, float %1300), !dx.precise !37  ; Round_ni(value)
  %1302 = fptosi float %1301 to i32
  %1303 = sub nsw i32 %1302, %1262
  %1304 = call i32 @dx.op.binary.i32(i32 37, i32 0, i32 %1303)  ; IMax(a,b)
  %1305 = icmp slt i32 %1304, %1265
  br i1 %1305, label %1306, label %1603

; <label>:1306                                    ; preds = %1228
  %1307 = add nsw i32 %1304, %213
  %1308 = mul i32 %1307, 288
  %1309 = add i32 %1308, 32
  %1310 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1309, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1311 = extractvalue %dx.types.ResRet.i32 %1310, 2
  %1312 = bitcast i32 %1311 to float
  %1313 = add i32 %1308, 64
  %1314 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1313, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1315 = extractvalue %dx.types.ResRet.i32 %1314, 0
  %1316 = extractvalue %dx.types.ResRet.i32 %1314, 1
  %1317 = extractvalue %dx.types.ResRet.i32 %1314, 2
  %1318 = bitcast i32 %1315 to float
  %1319 = bitcast i32 %1316 to float
  %1320 = bitcast i32 %1317 to float
  %1321 = add i32 %1308, 80
  %1322 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1321, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1323 = extractvalue %dx.types.ResRet.i32 %1322, 0
  %1324 = extractvalue %dx.types.ResRet.i32 %1322, 1
  %1325 = extractvalue %dx.types.ResRet.i32 %1322, 2
  %1326 = bitcast i32 %1323 to float
  %1327 = bitcast i32 %1324 to float
  %1328 = bitcast i32 %1325 to float
  %1329 = add i32 %1308, 96
  %1330 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1329, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1331 = extractvalue %dx.types.ResRet.i32 %1330, 0
  %1332 = extractvalue %dx.types.ResRet.i32 %1330, 1
  %1333 = extractvalue %dx.types.ResRet.i32 %1330, 2
  %1334 = bitcast i32 %1331 to float
  %1335 = bitcast i32 %1332 to float
  %1336 = bitcast i32 %1333 to float
  %1337 = add i32 %1308, 112
  %1338 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1337, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1339 = extractvalue %dx.types.ResRet.i32 %1338, 0
  %1340 = extractvalue %dx.types.ResRet.i32 %1338, 1
  %1341 = extractvalue %dx.types.ResRet.i32 %1338, 2
  %1342 = bitcast i32 %1339 to float
  %1343 = bitcast i32 %1340 to float
  %1344 = bitcast i32 %1341 to float
  %1345 = add i32 %1308, 208
  %1346 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1345, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1347 = extractvalue %dx.types.ResRet.i32 %1346, 0
  %1348 = extractvalue %dx.types.ResRet.i32 %1346, 1
  %1349 = extractvalue %dx.types.ResRet.i32 %1346, 2
  %1350 = bitcast i32 %1347 to float
  %1351 = bitcast i32 %1348 to float
  %1352 = bitcast i32 %1349 to float
  %1353 = add i32 %1308, 224
  %1354 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1353, i32 undef, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1355 = extractvalue %dx.types.ResRet.i32 %1354, 0
  %1356 = extractvalue %dx.types.ResRet.i32 %1354, 1
  %1357 = extractvalue %dx.types.ResRet.i32 %1354, 2
  %1358 = bitcast i32 %1355 to float
  %1359 = bitcast i32 %1356 to float
  %1360 = bitcast i32 %1357 to float
  %1361 = fsub float %1350, %1267
  %1362 = fsub float %1351, %1268
  %1363 = fsub float %1352, %1269
  %1364 = fsub float %1358, %1271
  %1365 = fsub float %1359, %1272
  %1366 = fsub float %1360, %1273
  %1367 = fadd float %1361, %1364
  %1368 = fadd float %1362, %1365
  %1369 = fadd float %1363, %1366
  %1370 = fadd fast float %1367, %275
  %1371 = fadd fast float %1368, %276
  %1372 = fadd fast float %1369, %277
  %1373 = fmul fast float %1370, %1318
  %1374 = call float @dx.op.tertiary.f32(i32 46, float %1371, float %1326, float %1373)  ; FMad(a,b,c)
  %1375 = call float @dx.op.tertiary.f32(i32 46, float %1372, float %1334, float %1374)  ; FMad(a,b,c)
  %1376 = fadd fast float %1375, %1342
  %1377 = fmul fast float %1370, %1319
  %1378 = call float @dx.op.tertiary.f32(i32 46, float %1371, float %1327, float %1377)  ; FMad(a,b,c)
  %1379 = call float @dx.op.tertiary.f32(i32 46, float %1372, float %1335, float %1378)  ; FMad(a,b,c)
  %1380 = fadd fast float %1379, %1343
  %1381 = fmul fast float %1370, %1320
  %1382 = call float @dx.op.tertiary.f32(i32 46, float %1371, float %1328, float %1381)  ; FMad(a,b,c)
  %1383 = call float @dx.op.tertiary.f32(i32 46, float %1372, float %1336, float %1382)  ; FMad(a,b,c)
  %1384 = fadd fast float %1383, %1344
  %1385 = fmul fast float %1376, 1.280000e+02
  %1386 = fmul fast float %1380, 1.280000e+02
  %1387 = fptoui float %1385 to i32
  %1388 = fptoui float %1386 to i32
  %1389 = icmp ult i32 %1307, 8192
  br i1 %1389, label %1396, label %1390

; <label>:1390                                    ; preds = %1306
  %1391 = mul i32 %1307, 21845
  %1392 = shl i32 %1388, 7
  %1393 = add i32 %1391, -178946048
  %1394 = add i32 %1393, %1387
  %1395 = add i32 %1394, %1392
  br label %1396

; <label>:1396                                    ; preds = %1390, %1306
  %1397 = phi i32 [ %1395, %1390 ], [ %1307, %1306 ]
  %1398 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %9, %dx.types.ResourceProperties { i32 12, i32 4 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=4>
  %1399 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1398, i32 %1397, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1400 = extractvalue %dx.types.ResRet.i32 %1399, 0
  %1401 = lshr i32 %1400, 20
  %1402 = and i32 %1401, 63
  %1403 = icmp slt i32 %1400, 0
  br i1 %1403, label %1404, label %1512

; <label>:1404                                    ; preds = %1396
  %1405 = icmp eq i32 %1402, 0
  %1406 = zext i1 %1405 to i32
  %1407 = add i32 %1402, %1307
  %1408 = fmul fast float %1376, 1.638400e+04
  %1409 = fmul fast float %1380, 1.638400e+04
  %1410 = fptoui float %1408 to i32
  %1411 = fptoui float %1409 to i32
  br i1 %1405, label %1487, label %1412

; <label>:1412                                    ; preds = %1404
  %1413 = add i32 %1308, 256
  %1414 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1413, i32 undef, i8 3, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1415 = extractvalue %dx.types.ResRet.i32 %1414, 0
  %1416 = extractvalue %dx.types.ResRet.i32 %1414, 1
  %1417 = mul i32 %1407, 288
  %1418 = add i32 %1417, 256
  %1419 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1418, i32 undef, i8 3, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1420 = extractvalue %dx.types.ResRet.i32 %1419, 0
  %1421 = extractvalue %dx.types.ResRet.i32 %1419, 1
  %1422 = shl i32 %1415, 5
  %1423 = shl i32 %1416, 5
  %1424 = shl i32 %1420, 5
  %1425 = shl i32 %1421, 5
  %1426 = sub i32 %1387, %1422
  %1427 = sub i32 %1388, %1423
  %1428 = and i32 %1401, 31
  %1429 = shl i32 %1424, %1428
  %1430 = shl i32 %1425, %1428
  %1431 = add i32 %1426, %1429
  %1432 = add i32 %1427, %1430
  %1433 = lshr i32 %1431, %1428
  %1434 = lshr i32 %1432, %1428
  %1435 = shl i32 %1433, 7
  %1436 = shl i32 %1434, 7
  %1437 = or i32 %1435, 127
  %1438 = or i32 %1436, 127
  %1439 = add i32 %1308, 48
  %1440 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1439, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1441 = extractvalue %dx.types.ResRet.i32 %1440, 2
  %1442 = bitcast i32 %1441 to float
  %1443 = add i32 %1417, 48
  %1444 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1443, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1445 = extractvalue %dx.types.ResRet.i32 %1444, 2
  %1446 = bitcast i32 %1445 to float
  %1447 = sitofp i32 %1415 to float
  %1448 = sitofp i32 %1416 to float
  %1449 = sitofp i32 %1420 to float
  %1450 = sitofp i32 %1421 to float
  %1451 = shl i32 1, %1428
  %1452 = uitofp i32 %1451 to float
  %1453 = fdiv fast float 1.000000e+00, %1452
  %1454 = fmul fast float %1453, %1447
  %1455 = fmul fast float %1453, %1448
  %1456 = fsub fast float %1449, %1454
  %1457 = fsub fast float %1450, %1455
  %1458 = fmul fast float %1456, 2.500000e-01
  %1459 = fmul fast float %1457, 2.500000e-01
  %1460 = fmul fast float %1453, %1442
  %1461 = fsub fast float %1446, %1460
  %1462 = fmul fast float %1453, %1376
  %1463 = fmul fast float %1453, %1380
  %1464 = fadd fast float %1458, %1462
  %1465 = fadd fast float %1459, %1463
  %1466 = fmul fast float %1464, 1.638400e+04
  %1467 = fmul fast float %1465, 1.638400e+04
  %1468 = fptoui float %1466 to i32
  %1469 = fptoui float %1467 to i32
  %1470 = call i32 @dx.op.binary.i32(i32 39, i32 %1468, i32 %1435)  ; UMax(a,b)
  %1471 = call i32 @dx.op.binary.i32(i32 39, i32 %1469, i32 %1436)  ; UMax(a,b)
  %1472 = call i32 @dx.op.binary.i32(i32 40, i32 %1470, i32 %1437)  ; UMin(a,b)
  %1473 = call i32 @dx.op.binary.i32(i32 40, i32 %1471, i32 %1438)  ; UMin(a,b)
  %1474 = icmp ult i32 %1407, 8192
  br i1 %1474, label %1480, label %1475

; <label>:1475                                    ; preds = %1412
  %1476 = mul i32 %1407, 21845
  %1477 = add i32 %1476, -178946048
  %1478 = add i32 %1477, %1433
  %1479 = add i32 %1478, %1436
  br label %1480

; <label>:1480                                    ; preds = %1475, %1412
  %1481 = phi i32 [ %1479, %1475 ], [ %1407, %1412 ]
  %1482 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1398, i32 %1481, i32 0, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1483 = extractvalue %dx.types.ResRet.i32 %1482, 0
  %1484 = and i32 %1483, -2081423360
  %1485 = icmp eq i32 %1484, -2147483648
  %1486 = zext i1 %1485 to i32
  br label %1487

; <label>:1487                                    ; preds = %1480, %1404
  %1488 = phi float [ %1466, %1480 ], [ %1408, %1404 ]
  %1489 = phi float [ %1467, %1480 ], [ %1409, %1404 ]
  %1490 = phi i32 [ %1472, %1480 ], [ %1410, %1404 ]
  %1491 = phi i32 [ %1473, %1480 ], [ %1411, %1404 ]
  %1492 = phi float [ %1453, %1480 ], [ 1.000000e+00, %1404 ]
  %1493 = phi float [ %1461, %1480 ], [ 0.000000e+00, %1404 ]
  %1494 = phi i32 [ %1486, %1480 ], [ %1406, %1404 ]
  %1495 = phi i32 [ %1483, %1480 ], [ %1400, %1404 ]
  %1496 = icmp eq i32 %1494, 0
  br i1 %1496, label %1512, label %1497

; <label>:1497                                    ; preds = %1487
  %1498 = shl i32 %1495, 7
  %1499 = and i32 %1498, 130944
  %1500 = lshr i32 %1495, 3
  %1501 = and i32 %1500, 130944
  %1502 = and i32 %1490, 127
  %1503 = and i32 %1491, 127
  %1504 = or i32 %1499, %1502
  %1505 = or i32 %1501, %1503
  %1506 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %8, %dx.types.ResourceProperties { i32 7, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture2DArray<U32>
  %1507 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %1506, i32 0, i32 %1504, i32 %1505, i32 0, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1508 = extractvalue %dx.types.ResRet.i32 %1507, 0
  %1509 = bitcast i32 %1508 to float
  %1510 = fsub fast float %1509, %1493
  %1511 = fdiv fast float %1510, %1492
  br label %1512

; <label>:1512                                    ; preds = %1497, %1487, %1396
  %1513 = phi float [ %1488, %1497 ], [ %1488, %1487 ], [ 0.000000e+00, %1396 ]
  %1514 = phi float [ %1489, %1497 ], [ %1489, %1487 ], [ 0.000000e+00, %1396 ]
  %1515 = phi i32 [ %1490, %1497 ], [ %1490, %1487 ], [ 0, %1396 ]
  %1516 = phi i32 [ %1491, %1497 ], [ %1491, %1487 ], [ 0, %1396 ]
  %1517 = phi i1 [ true, %1497 ], [ false, %1487 ], [ false, %1396 ]
  %1518 = phi i32 [ %1407, %1497 ], [ -1, %1487 ], [ -1, %1396 ]
  %1519 = phi float [ %1511, %1497 ], [ 0.000000e+00, %1487 ], [ 0.000000e+00, %1396 ]
  br i1 %1517, label %1520, label %1603

; <label>:1520                                    ; preds = %1512
  %1521 = mul i32 %1518, 288
  %1522 = add i32 %1521, 32
  %1523 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1522, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1524 = extractvalue %dx.types.ResRet.i32 %1523, 2
  %1525 = bitcast i32 %1524 to float
  %1526 = add i32 %1521, 128
  %1527 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1526, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1528 = extractvalue %dx.types.ResRet.i32 %1527, 0
  %1529 = extractvalue %dx.types.ResRet.i32 %1527, 1
  %1530 = extractvalue %dx.types.ResRet.i32 %1527, 2
  %1531 = bitcast i32 %1528 to float
  %1532 = bitcast i32 %1529 to float
  %1533 = bitcast i32 %1530 to float
  %1534 = add i32 %1521, 144
  %1535 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1534, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1536 = extractvalue %dx.types.ResRet.i32 %1535, 0
  %1537 = extractvalue %dx.types.ResRet.i32 %1535, 1
  %1538 = extractvalue %dx.types.ResRet.i32 %1535, 2
  %1539 = bitcast i32 %1536 to float
  %1540 = bitcast i32 %1537 to float
  %1541 = bitcast i32 %1538 to float
  %1542 = add i32 %1521, 160
  %1543 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1542, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1544 = extractvalue %dx.types.ResRet.i32 %1543, 0
  %1545 = extractvalue %dx.types.ResRet.i32 %1543, 1
  %1546 = extractvalue %dx.types.ResRet.i32 %1543, 2
  %1547 = bitcast i32 %1544 to float
  %1548 = bitcast i32 %1545 to float
  %1549 = bitcast i32 %1546 to float
  %1550 = add i32 %1521, 176
  %1551 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1232, i32 %1550, i32 undef, i8 15, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1552 = extractvalue %dx.types.ResRet.i32 %1551, 0
  %1553 = extractvalue %dx.types.ResRet.i32 %1551, 1
  %1554 = extractvalue %dx.types.ResRet.i32 %1551, 2
  %1555 = bitcast i32 %1552 to float
  %1556 = bitcast i32 %1553 to float
  %1557 = bitcast i32 %1554 to float
  %1558 = call float @dx.op.dot3.f32(i32 55, float %269, float %270, float %271, float %1370, float %1371, float %1372)  ; Dot3(ax,ay,az,bx,by,bz)
  %1559 = fsub fast float -0.000000e+00, %1558
  %1560 = fmul fast float %1531, %269
  %1561 = call float @dx.op.tertiary.f32(i32 46, float %270, float %1539, float %1560)  ; FMad(a,b,c)
  %1562 = call float @dx.op.tertiary.f32(i32 46, float %271, float %1547, float %1561)  ; FMad(a,b,c)
  %1563 = call float @dx.op.tertiary.f32(i32 46, float %1559, float %1555, float %1562)  ; FMad(a,b,c)
  %1564 = fmul fast float %1532, %269
  %1565 = call float @dx.op.tertiary.f32(i32 46, float %270, float %1540, float %1564)  ; FMad(a,b,c)
  %1566 = call float @dx.op.tertiary.f32(i32 46, float %271, float %1548, float %1565)  ; FMad(a,b,c)
  %1567 = call float @dx.op.tertiary.f32(i32 46, float %1559, float %1556, float %1566)  ; FMad(a,b,c)
  %1568 = fmul fast float %1533, %269
  %1569 = call float @dx.op.tertiary.f32(i32 46, float %270, float %1541, float %1568)  ; FMad(a,b,c)
  %1570 = call float @dx.op.tertiary.f32(i32 46, float %271, float %1549, float %1569)  ; FMad(a,b,c)
  %1571 = call float @dx.op.tertiary.f32(i32 46, float %1559, float %1557, float %1570)  ; FMad(a,b,c)
  %1572 = fsub fast float -0.000000e+00, %1563
  %1573 = fsub fast float -0.000000e+00, %1567
  %1574 = fdiv fast float %1572, %1571
  %1575 = fdiv fast float %1573, %1571
  %1576 = uitofp i32 %1515 to float
  %1577 = uitofp i32 %1516 to float
  %1578 = fsub fast float 5.000000e-01, %1513
  %1579 = fadd fast float %1578, %1576
  %1580 = fsub fast float 5.000000e-01, %1514
  %1581 = fadd fast float %1580, %1577
  %1582 = fmul fast float %1579, 0x3F10000000000000
  %1583 = fmul fast float %1581, 0x3F10000000000000
  %1584 = call float @dx.op.dot2.f32(i32 54, float %1574, float %1575, float %1582, float %1583)  ; Dot2(ax,ay,bx,by)
  %1585 = call float @dx.op.binary.f32(i32 35, float 0.000000e+00, float %1584)  ; FMax(a,b)
  %1586 = fmul fast float %1585, 2.000000e+00
  %1587 = fmul fast float %1525, 1.000000e+02
  %1588 = call float @dx.op.unary.f32(i32 6, float %1587)  ; FAbs(value)
  %1589 = call float @dx.op.binary.f32(i32 36, float %1586, float %1588)  ; FMin(a,b)
  %1590 = sub nsw i32 %1518, %1307
  %1591 = and i32 %1590, 31
  %1592 = shl i32 1, %1591
  %1593 = uitofp i32 %1592 to float
  %1594 = fmul fast float %1589, %1593
  %1595 = fmul fast float %1312, %1229
  %1596 = fadd fast float %1519, %1595
  %1597 = fsub fast float %1596, %1594
  %1598 = fcmp fast ogt float %1597, %1384
  br i1 %1598, label %1599, label %1603

; <label>:1599                                    ; preds = %1520
  %1600 = fsub fast float %1384, %1519
  %1601 = fdiv fast float %1600, %1312
  %1602 = call float @dx.op.binary.f32(i32 35, float 0x3EB0C6F7A0000000, float %1601)  ; FMax(a,b)
  br label %1603

; <label>:1603                                    ; preds = %1599, %1520, %1512, %1228, %1215, %682, %465
  %1604 = phi float [ %1227, %1215 ], [ 0.000000e+00, %465 ], [ 1.000000e+00, %682 ], [ 0.000000e+00, %1599 ], [ 1.000000e+00, %1520 ], [ 1.000000e+00, %1512 ], [ 1.000000e+00, %1228 ]
  %1605 = phi float [ %1224, %1215 ], [ -1.000000e+00, %465 ], [ -1.000000e+00, %682 ], [ %1602, %1599 ], [ -1.000000e+00, %1520 ], [ -1.000000e+00, %1512 ], [ -1.000000e+00, %1228 ]
  %1606 = fcmp fast olt float %1604, 1.000000e+00
  %1607 = and i1 %220, %1606
  br i1 %1607, label %1608, label %1616

; <label>:1608                                    ; preds = %1603
  %1609 = fmul fast float %210, %1605
  %1610 = call float @dx.op.unary.f32(i32 21, float %1609)  ; Exp(value)
  %1611 = call float @dx.op.unary.f32(i32 7, float %1610)  ; Saturate(value)
  %1612 = fsub fast float 1.000000e+00, %1611
  %1613 = fmul fast float %1612, %1604
  %1614 = fadd fast float %1613, %1611
  %1615 = fmul fast float %1614, %1614
  br label %1616

; <label>:1616                                    ; preds = %1608, %1603
  %1617 = phi float [ %1604, %1603 ], [ %1615, %1608 ]
  br i1 %76, label %2078, label %1618

; <label>:1618                                    ; preds = %1616
  %1619 = fcmp fast ogt float %1617, 0.000000e+00
  br i1 %1619, label %1620, label %1800

; <label>:1620                                    ; preds = %1618
  %1621 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %24, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %1622 = extractvalue %dx.types.CBufRet.i32 %1621, 1
  %1623 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %22, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %1624 = extractvalue %dx.types.CBufRet.i32 %1623, 0
  %1625 = extractvalue %dx.types.CBufRet.i32 %1623, 1
  %1626 = sitofp i32 %1624 to float
  %1627 = sitofp i32 %1625 to float
  %1628 = fmul fast float %1626, 0x3FE827F520000000
  %1629 = fmul fast float %1627, 0x3FE23C21A0000000
  %1630 = fptosi float %1628 to i32
  %1631 = fptosi float %1629 to i32
  %1632 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %22, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %1633 = extractvalue %dx.types.CBufRet.i32 %1632, 0
  %1634 = extractvalue %dx.types.CBufRet.i32 %1632, 1
  %1635 = extractvalue %dx.types.CBufRet.i32 %1632, 2
  %1636 = and i32 %1633, %62
  %1637 = and i32 %1634, %64
  %1638 = and i32 %1635, %1622
  %1639 = mul i32 %1638, %1625
  %1640 = add i32 %1639, %1637
  %1641 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %1642 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %1641, i32 0, i32 %1636, i32 %1640, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1643 = extractvalue %dx.types.ResRet.f32 %1642, 0
  %1644 = extractvalue %dx.types.ResRet.f32 %1642, 1
  %1645 = add i32 %1630, %62
  %1646 = add i32 %1631, %64
  %1647 = and i32 %1633, %1645
  %1648 = and i32 %1634, %1646
  %1649 = add i32 %1639, %1648
  %1650 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %1641, i32 0, i32 %1647, i32 %1649, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1651 = extractvalue %dx.types.ResRet.f32 %1650, 0
  %1652 = extractvalue %dx.types.ResRet.f32 %1650, 1
  %1653 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %25, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %1654 = extractvalue %dx.types.CBufRet.i32 %1653, 1
  %1655 = add nsw i32 %1654, -1
  %1656 = sitofp i32 %1655 to float
  %1657 = sitofp i32 %1654 to float
  %1658 = fmul fast float %1657, %1652
  %1659 = call float @dx.op.binary.f32(i32 36, float %1658, float %1656)  ; FMin(a,b)
  %1660 = fptoui float %1659 to i32
  %1661 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %25, i32 7)  ; CBufferLoadLegacy(handle,regIndex)
  %1662 = extractvalue %dx.types.CBufRet.f32 %1661, 3
  %1663 = fmul fast float %1662, 1.000000e+02
  %1664 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 12)  ; CBufferLoadLegacy(handle,regIndex)
  %1665 = extractvalue %dx.types.CBufRet.f32 %1664, 0
  %1666 = extractvalue %dx.types.CBufRet.f32 %1664, 1
  %1667 = extractvalue %dx.types.CBufRet.f32 %1664, 2
  %1668 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 13)  ; CBufferLoadLegacy(handle,regIndex)
  %1669 = extractvalue %dx.types.CBufRet.f32 %1668, 0
  %1670 = extractvalue %dx.types.CBufRet.f32 %1668, 1
  %1671 = extractvalue %dx.types.CBufRet.f32 %1668, 2
  %1672 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 14)  ; CBufferLoadLegacy(handle,regIndex)
  %1673 = extractvalue %dx.types.CBufRet.f32 %1672, 0
  %1674 = extractvalue %dx.types.CBufRet.f32 %1672, 1
  %1675 = extractvalue %dx.types.CBufRet.f32 %1672, 2
  %1676 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %24, i32 15)  ; CBufferLoadLegacy(handle,regIndex)
  %1677 = extractvalue %dx.types.CBufRet.f32 %1676, 0
  %1678 = extractvalue %dx.types.CBufRet.f32 %1676, 1
  %1679 = extractvalue %dx.types.CBufRet.f32 %1676, 2
  %1680 = fmul fast float %1665, %275
  %1681 = call float @dx.op.tertiary.f32(i32 46, float %276, float %1669, float %1680)  ; FMad(a,b,c)
  %1682 = call float @dx.op.tertiary.f32(i32 46, float %277, float %1673, float %1681)  ; FMad(a,b,c)
  %1683 = fadd fast float %1682, %1677
  %1684 = fmul fast float %1666, %275
  %1685 = call float @dx.op.tertiary.f32(i32 46, float %276, float %1670, float %1684)  ; FMad(a,b,c)
  %1686 = call float @dx.op.tertiary.f32(i32 46, float %277, float %1674, float %1685)  ; FMad(a,b,c)
  %1687 = fadd fast float %1686, %1678
  %1688 = fmul fast float %1667, %275
  %1689 = call float @dx.op.tertiary.f32(i32 46, float %276, float %1671, float %1688)  ; FMad(a,b,c)
  %1690 = call float @dx.op.tertiary.f32(i32 46, float %277, float %1675, float %1689)  ; FMad(a,b,c)
  %1691 = fadd fast float %1690, %1679
  %1692 = fmul fast float %1683, %1683
  %1693 = fmul fast float %1687, %1687
  %1694 = fadd fast float %1693, %1692
  %1695 = fmul fast float %1691, %1691
  %1696 = fadd fast float %1694, %1695
  %1697 = call float @dx.op.unary.f32(i32 24, float %1696)  ; Sqrt(value)
  %1698 = fmul fast float %1663, %1697
  %1699 = extractvalue %dx.types.CBufRet.i32 %1621, 2
  %1700 = uitofp i32 %1660 to float
  %1701 = fmul fast float %1700, 0x3FE827F520000000
  %1702 = fmul fast float %1700, 0x3FE23C21A0000000
  %1703 = call float @dx.op.unary.f32(i32 22, float %1701)  ; Frc(value)
  %1704 = call float @dx.op.unary.f32(i32 22, float %1702)  ; Frc(value)
  %1705 = fmul fast float %1626, %1703
  %1706 = fmul fast float %1627, %1704
  %1707 = fptosi float %1705 to i32
  %1708 = fptosi float %1706 to i32
  %1709 = add i32 %1707, %62
  %1710 = add i32 %1708, %64
  %1711 = and i32 %1709, %1633
  %1712 = and i32 %1710, %1634
  %1713 = and i32 %1635, %1699
  %1714 = mul i32 %1713, %1625
  %1715 = add i32 %1712, %1714
  %1716 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %1641, i32 0, i32 %1711, i32 %1715, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %1717 = extractvalue %dx.types.ResRet.f32 %1716, 0
  %1718 = extractvalue %dx.types.ResRet.f32 %1716, 1
  %1719 = fmul fast float %1717, 2.000000e+00
  %1720 = fmul fast float %1718, 2.000000e+00
  %1721 = fadd fast float %1719, 0xBFEFFFFFE0000000
  %1722 = fadd fast float %1720, 0xBFEFFFFFE0000000
  %1723 = call float @dx.op.unary.f32(i32 6, float %1721)  ; FAbs(value)
  %1724 = call float @dx.op.unary.f32(i32 6, float %1722)  ; FAbs(value)
  %1725 = call float @dx.op.binary.f32(i32 36, float %1723, float %1724)  ; FMin(a,b)
  %1726 = call float @dx.op.binary.f32(i32 35, float %1723, float %1724)  ; FMax(a,b)
  %1727 = fadd fast float %1726, 0x3BF0000000000000
  %1728 = fdiv fast float %1725, %1727
  %1729 = fcmp fast oge float %1724, %1723
  %1730 = uitofp i1 %1729 to float
  %1731 = fmul fast float %1730, 2.000000e+00
  %1732 = fadd fast float %1728, %1731
  %1733 = fmul fast float %1732, 0x3FE921FB60000000
  %1734 = call float @dx.op.unary.f32(i32 12, float %1733)  ; Cos(value)
  %1735 = call float @dx.op.unary.f32(i32 13, float %1733)  ; Sin(value)
  %1736 = bitcast float %1734 to i32
  %1737 = bitcast float %1735 to i32
  %1738 = and i32 %1736, 2147483647
  %1739 = and i32 %1737, 2147483647
  %1740 = bitcast float %1721 to i32
  %1741 = bitcast float %1722 to i32
  %1742 = and i32 %1740, -2147483648
  %1743 = and i32 %1741, -2147483648
  %1744 = or i32 %1738, %1742
  %1745 = or i32 %1739, %1743
  %1746 = bitcast i32 %1744 to float
  %1747 = bitcast i32 %1745 to float
  %1748 = fmul fast float %1726, %228
  %1749 = fmul fast float %1748, %1746
  %1750 = fmul fast float %1748, %1747
  %1751 = call float @dx.op.unary.f32(i32 6, float %215)  ; FAbs(value)
  %1752 = fcmp fast ogt float %1751, 0x3EB0C6F7A0000000
  %1753 = select i1 %1752, float 1.000000e+00, float 0.000000e+00
  %1754 = select i1 %1752, float 0.000000e+00, float 1.000000e+00
  %1755 = fmul fast float %217, %1754
  %1756 = fsub fast float -0.000000e+00, %1755
  %1757 = fmul fast float %1753, %217
  %1758 = fmul fast float %1754, %215
  %1759 = fmul fast float %1753, %216
  %1760 = fsub fast float %1758, %1759
  %1761 = fmul fast float %1757, %217
  %1762 = fmul fast float %1760, %216
  %1763 = fsub fast float %1761, %1762
  %1764 = fmul fast float %1760, %215
  %1765 = fmul fast float %217, %1756
  %1766 = fsub fast float %1764, %1765
  %1767 = fmul fast float %216, %1756
  %1768 = fmul fast float %1757, %215
  %1769 = fsub fast float %1767, %1768
  %1770 = fmul fast float %1749, %1756
  %1771 = fmul fast float %1749, %1757
  %1772 = fmul fast float %1760, %1749
  %1773 = fmul fast float %1763, %1750
  %1774 = fmul fast float %1766, %1750
  %1775 = fmul fast float %1769, %1750
  %1776 = fadd fast float %1770, %215
  %1777 = fadd fast float %1776, %1773
  %1778 = fadd fast float %1771, %216
  %1779 = fadd fast float %1778, %1774
  %1780 = fadd fast float %1772, %217
  %1781 = fadd fast float %1780, %1775
  %1782 = call float @dx.op.dot3.f32(i32 55, float %1777, float %1779, float %1781, float %1777, float %1779, float %1781)  ; Dot3(ax,ay,az,bx,by,bz)
  %1783 = call float @dx.op.unary.f32(i32 25, float %1782)  ; Rsqrt(value)
  %1784 = fmul fast float %1783, %1698
  %1785 = fmul fast float %1784, %1777
  %1786 = fmul fast float %1784, %1779
  %1787 = fmul fast float %1784, %1781
  %1788 = fadd fast float %1785, %275
  %1789 = fadd fast float %1786, %276
  %1790 = fadd fast float %1787, %277
  %1791 = fmul float %1643, 2.000000e+00
  %1792 = fmul float %1644, 2.000000e+00
  %1793 = fmul float %1651, 2.000000e+00
  %1794 = fadd float %1791, -1.000000e+00
  %1795 = fadd float %1792, -1.000000e+00
  %1796 = fadd float %1793, -1.000000e+00
  %1797 = fmul float %1794, 5.000000e-01
  %1798 = fmul float %1795, 5.000000e-01
  %1799 = fmul float %1796, 5.000000e-01
  br label %1800

; <label>:1800                                    ; preds = %1620, %1618
  %1801 = phi float [ %275, %1620 ], [ 0.000000e+00, %1618 ]
  %1802 = phi float [ %276, %1620 ], [ 0.000000e+00, %1618 ]
  %1803 = phi float [ %277, %1620 ], [ 0.000000e+00, %1618 ]
  %1804 = phi float [ %1788, %1620 ], [ 0.000000e+00, %1618 ]
  %1805 = phi float [ %1789, %1620 ], [ 0.000000e+00, %1618 ]
  %1806 = phi float [ %1790, %1620 ], [ 0.000000e+00, %1618 ]
  %1807 = phi i1 [ true, %1620 ], [ false, %1618 ]
  %1808 = phi float [ %1797, %1620 ], [ -5.000000e-01, %1618 ]
  %1809 = phi float [ %1798, %1620 ], [ -5.000000e-01, %1618 ]
  %1810 = phi float [ %1799, %1620 ], [ -5.000000e-01, %1618 ]
  br i1 %1807, label %1811, label %2078

; <label>:1811                                    ; preds = %1800
  %1812 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %21, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %1813 = extractvalue %dx.types.CBufRet.f32 %1812, 0
  %1814 = fmul fast float %1813, %215
  %1815 = fmul fast float %1813, %216
  %1816 = fmul fast float %1813, %217
  %1817 = fadd fast float %1814, %1808
  %1818 = fadd fast float %1815, %1809
  %1819 = fadd fast float %1816, %1810
  %1820 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %21, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %1821 = extractvalue %dx.types.CBufRet.i32 %1820, 0
  %1822 = extractvalue %dx.types.CBufRet.i32 %1820, 1
  %1823 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %21, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %1824 = extractvalue %dx.types.CBufRet.i32 %1823, 0
  %1825 = extractvalue %dx.types.CBufRet.i32 %1823, 1
  %1826 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %21, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %1827 = extractvalue %dx.types.CBufRet.f32 %1826, 0
  %1828 = extractvalue %dx.types.CBufRet.f32 %1812, 3
  %1829 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %21, i32 3)  ; CBufferLoadLegacy(handle,regIndex)
  %1830 = extractvalue %dx.types.CBufRet.i32 %1829, 0
  %1831 = icmp eq i32 %1830, 0
  br i1 %1831, label %2078, label %1832

; <label>:1832                                    ; preds = %1811
  br label %1833

; <label>:1833                                    ; preds = %2070, %1832
  %1834 = phi float [ %2074, %2070 ], [ %1617, %1832 ]
  %1835 = phi i32 [ %2075, %2070 ], [ 0, %1832 ]
  %1836 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 524, i32 32 })  ; AnnotateHandle(res,props)  resource: StructuredBuffer<stride=32>
  %1837 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %1836, i32 %1835, i32 0, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1838 = extractvalue %dx.types.ResRet.f32 %1837, 0
  %1839 = extractvalue %dx.types.ResRet.f32 %1837, 1
  %1840 = extractvalue %dx.types.ResRet.f32 %1837, 2
  %1841 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1836, i32 %1835, i32 12, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1842 = extractvalue %dx.types.ResRet.i32 %1841, 0
  %1843 = call %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32 139, %dx.types.Handle %1836, i32 %1835, i32 16, i8 7, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1844 = extractvalue %dx.types.ResRet.f32 %1843, 0
  %1845 = extractvalue %dx.types.ResRet.f32 %1843, 1
  %1846 = extractvalue %dx.types.ResRet.f32 %1843, 2
  %1847 = call %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32 139, %dx.types.Handle %1836, i32 %1835, i32 28, i8 1, i32 4)  ; RawBufferLoad(srv,index,elementOffset,mask,alignment)
  %1848 = extractvalue %dx.types.ResRet.i32 %1847, 0
  %1849 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %21, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %1850 = extractvalue %dx.types.CBufRet.i32 %1849, 0
  %1851 = and i32 %1842, 255
  %1852 = lshr i32 %1842, 8
  %1853 = and i32 %1852, 255
  %1854 = lshr i32 %1842, 16
  %1855 = and i32 %1854, 255
  %1856 = mul i32 %1850, %1851
  %1857 = mul i32 %1850, %1853
  %1858 = mul i32 %1850, %1855
  %1859 = and i32 %1848, 4194303
  %1860 = lshr i32 %1848, 22
  %1861 = uitofp i32 %1860 to float
  %1862 = fmul fast float %1861, 0x3F84050140000000
  %1863 = icmp ne i32 %1851, 0
  %1864 = icmp ne i32 %1853, 0
  %1865 = icmp ne i32 %1855, 0
  %1866 = and i1 %1863, %1864
  %1867 = and i1 %1865, %1866
  %1868 = fmul fast float %1862, %1817
  %1869 = fmul fast float %1862, %1818
  %1870 = fmul fast float %1862, %1819
  %1871 = fadd fast float %1868, %1801
  %1872 = fadd fast float %1869, %1802
  %1873 = fadd fast float %1870, %1803
  %1874 = fdiv fast float 0x4059933340000000, %1861
  br i1 %1867, label %1875, label %2070

; <label>:1875                                    ; preds = %1833
  %1876 = fsub fast float %1804, %1871
  %1877 = fsub fast float %1805, %1872
  %1878 = fsub fast float %1806, %1873
  %1879 = fdiv fast float 1.000000e+00, %1876
  %1880 = fdiv fast float 1.000000e+00, %1877
  %1881 = fdiv fast float 1.000000e+00, %1878
  %1882 = fsub fast float %1838, %1871
  %1883 = fsub fast float %1839, %1872
  %1884 = fsub fast float %1840, %1873
  %1885 = fmul fast float %1879, %1882
  %1886 = fmul fast float %1880, %1883
  %1887 = fmul fast float %1881, %1884
  %1888 = fsub fast float %1844, %1871
  %1889 = fsub fast float %1845, %1872
  %1890 = fsub fast float %1846, %1873
  %1891 = fmul fast float %1879, %1888
  %1892 = fmul fast float %1880, %1889
  %1893 = fmul fast float %1881, %1890
  %1894 = call float @dx.op.binary.f32(i32 36, float %1885, float %1891)  ; FMin(a,b)
  %1895 = call float @dx.op.binary.f32(i32 36, float %1886, float %1892)  ; FMin(a,b)
  %1896 = call float @dx.op.binary.f32(i32 36, float %1887, float %1893)  ; FMin(a,b)
  %1897 = call float @dx.op.binary.f32(i32 35, float %1885, float %1891)  ; FMax(a,b)
  %1898 = call float @dx.op.binary.f32(i32 35, float %1886, float %1892)  ; FMax(a,b)
  %1899 = call float @dx.op.binary.f32(i32 35, float %1887, float %1893)  ; FMax(a,b)
  %1900 = call float @dx.op.binary.f32(i32 35, float %1895, float %1896)  ; FMax(a,b)
  %1901 = call float @dx.op.binary.f32(i32 35, float %1894, float %1900)  ; FMax(a,b)
  %1902 = call float @dx.op.binary.f32(i32 36, float %1898, float %1899)  ; FMin(a,b)
  %1903 = call float @dx.op.binary.f32(i32 36, float %1897, float %1902)  ; FMin(a,b)
  %1904 = call float @dx.op.unary.f32(i32 7, float %1901)  ; Saturate(value)
  %1905 = call float @dx.op.unary.f32(i32 7, float %1903)  ; Saturate(value)
  %1906 = fcmp fast olt float %1904, %1905
  br i1 %1906, label %1907, label %2070

; <label>:1907                                    ; preds = %1875
  %1908 = fmul fast float %1904, %1876
  %1909 = fmul fast float %1904, %1877
  %1910 = fmul fast float %1904, %1878
  %1911 = fsub fast float %1905, %1904
  %1912 = fmul fast float %1876, %1911
  %1913 = fsub fast float %1905, %1904
  %1914 = fmul fast float %1877, %1913
  %1915 = fsub fast float %1905, %1904
  %1916 = fmul fast float %1878, %1915
  %1917 = fmul fast float %1912, %1912
  %1918 = fmul fast float %1914, %1914
  %1919 = fadd fast float %1917, %1918
  %1920 = fmul fast float %1916, %1916
  %1921 = fadd fast float %1919, %1920
  %1922 = call float @dx.op.unary.f32(i32 24, float %1921)  ; Sqrt(value)
  %1923 = call float @dx.op.binary.f32(i32 36, float %1922, float 1.000000e+05)  ; FMin(a,b)
  %1924 = call float @dx.op.dot3.f32(i32 55, float %1912, float %1914, float %1916, float %1912, float %1914, float %1916)  ; Dot3(ax,ay,az,bx,by,bz)
  %1925 = call float @dx.op.unary.f32(i32 25, float %1924)  ; Rsqrt(value)
  %1926 = fdiv fast float %1923, %1862
  %1927 = call float @dx.op.unary.f32(i32 28, float %1926)  ; Round_pi(value)
  %1928 = call float @dx.op.binary.f32(i32 36, float %1927, float 1.024000e+03)  ; FMin(a,b)
  %1929 = fdiv fast float %1923, %1928
  %1930 = fcmp fast ogt float %1928, 0.000000e+00
  br i1 %1930, label %1931, label %2070

; <label>:1931                                    ; preds = %1907
  br label %1932

; <label>:1932                                    ; preds = %2061, %1931
  %1933 = phi i32 [ %2019, %2061 ], [ 9999, %1931 ]
  %1934 = phi i32 [ %2020, %2061 ], [ 9999, %1931 ]
  %1935 = phi i32 [ %2021, %2061 ], [ 9999, %1931 ]
  %1936 = phi i32 [ %2022, %2061 ], [ 0, %1931 ]
  %1937 = phi i32 [ %2023, %2061 ], [ 0, %1931 ]
  %1938 = phi i32 [ %2024, %2061 ], [ 0, %1931 ]
  %1939 = phi i32 [ %2025, %2061 ], [ 0, %1931 ]
  %1940 = phi float [ %2065, %2061 ], [ 1.000000e+00, %1931 ]
  %1941 = phi float [ %2066, %2061 ], [ 0.000000e+00, %1931 ]
  %1942 = phi float [ %2062, %2061 ], [ 0.000000e+00, %1931 ]
  %1943 = fmul fast float %1940, %1929
  %1944 = call float @dx.op.binary.f32(i32 35, float %1943, float 0.000000e+00)  ; FMax(a,b)
  %1945 = fmul fast float %1912, %1862
  %1946 = fmul fast float %1945, %1925
  %1947 = fmul fast float %1946, %1941
  %1948 = fmul fast float %1914, %1862
  %1949 = fmul fast float %1948, %1925
  %1950 = fmul fast float %1949, %1941
  %1951 = fmul fast float %1916, %1862
  %1952 = fmul fast float %1951, %1925
  %1953 = fmul fast float %1952, %1941
  %1954 = fmul fast float %1808, %1944
  %1955 = fmul fast float %1809, %1944
  %1956 = fmul fast float %1810, %1944
  %1957 = add i32 %1856, -1
  %1958 = add i32 %1857, -1
  %1959 = add i32 %1858, -1
  %1960 = fsub fast float %1871, %1838
  %1961 = fadd fast float %1960, %1908
  %1962 = fadd fast float %1961, %1947
  %1963 = fadd fast float %1962, %1954
  %1964 = fsub fast float %1872, %1839
  %1965 = fadd fast float %1964, %1909
  %1966 = fadd fast float %1965, %1950
  %1967 = fadd fast float %1966, %1955
  %1968 = fsub fast float %1873, %1840
  %1969 = fadd fast float %1968, %1910
  %1970 = fadd fast float %1969, %1953
  %1971 = fadd fast float %1970, %1956
  %1972 = fsub fast float %1844, %1838
  %1973 = fsub fast float %1845, %1839
  %1974 = fsub fast float %1846, %1840
  %1975 = fdiv fast float %1963, %1972
  %1976 = fdiv fast float %1967, %1973
  %1977 = fdiv fast float %1971, %1974
  %1978 = call float @dx.op.unary.f32(i32 7, float %1975)  ; Saturate(value)
  %1979 = call float @dx.op.unary.f32(i32 7, float %1976)  ; Saturate(value)
  %1980 = call float @dx.op.unary.f32(i32 7, float %1977)  ; Saturate(value)
  %1981 = uitofp i32 %1856 to float
  %1982 = uitofp i32 %1857 to float
  %1983 = uitofp i32 %1858 to float
  %1984 = fmul fast float %1978, %1981
  %1985 = fmul fast float %1979, %1982
  %1986 = fmul fast float %1980, %1983
  %1987 = fptoui float %1984 to i32
  %1988 = fptoui float %1985 to i32
  %1989 = fptoui float %1986 to i32
  %1990 = call i32 @dx.op.binary.i32(i32 40, i32 %1987, i32 %1957)  ; UMin(a,b)
  %1991 = call i32 @dx.op.binary.i32(i32 40, i32 %1988, i32 %1958)  ; UMin(a,b)
  %1992 = call i32 @dx.op.binary.i32(i32 40, i32 %1989, i32 %1959)  ; UMin(a,b)
  %1993 = and i32 %1825, 31
  %1994 = lshr i32 %1990, %1993
  %1995 = lshr i32 %1991, %1993
  %1996 = lshr i32 %1992, %1993
  %1997 = icmp ne i32 %1994, %1933
  %1998 = icmp ne i32 %1995, %1934
  %1999 = icmp ne i32 %1996, %1935
  %2000 = or i1 %1997, %1998
  %2001 = or i1 %2000, %1999
  br i1 %2001, label %2002, label %2018

; <label>:2002                                    ; preds = %1932
  %2003 = mul i32 %1996, %1853
  %2004 = add i32 %2003, %1995
  %2005 = mul i32 %2004, %1851
  %2006 = add i32 %1994, %1859
  %2007 = add i32 %2006, %2005
  %2008 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %4, %dx.types.ResourceProperties { i32 10, i32 261 })  ; AnnotateHandle(res,props)  resource: TypedBuffer<U32>
  %2009 = call %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32 68, %dx.types.Handle %2008, i32 %2007, i32 undef)  ; BufferLoad(srv,index,wot)
  %2010 = extractvalue %dx.types.ResRet.i32 %2009, 0
  %2011 = icmp ne i32 %2010, -1
  %2012 = zext i1 %2011 to i32
  %2013 = mul i32 %1821, %1822
  %2014 = urem i32 %2010, %2013
  %2015 = urem i32 %2014, %1821
  %2016 = udiv i32 %2014, %1821
  %2017 = udiv i32 %2010, %2013
  br label %2018

; <label>:2018                                    ; preds = %2002, %1932
  %2019 = phi i32 [ %1994, %2002 ], [ %1933, %1932 ]
  %2020 = phi i32 [ %1995, %2002 ], [ %1934, %1932 ]
  %2021 = phi i32 [ %1996, %2002 ], [ %1935, %1932 ]
  %2022 = phi i32 [ %2012, %2002 ], [ %1936, %1932 ]
  %2023 = phi i32 [ %2015, %2002 ], [ %1937, %1932 ]
  %2024 = phi i32 [ %2016, %2002 ], [ %1938, %1932 ]
  %2025 = phi i32 [ %2017, %2002 ], [ %1939, %1932 ]
  %2026 = icmp eq i32 %2022, 0
  br i1 %2026, label %2061, label %2027

; <label>:2027                                    ; preds = %2018
  %2028 = shl i32 %2023, %1993
  %2029 = shl i32 %2024, %1993
  %2030 = shl i32 %2025, %1993
  %2031 = shl i32 %1994, %1993
  %2032 = shl i32 %1995, %1993
  %2033 = shl i32 %1996, %1993
  %2034 = sub i32 %1990, %2031
  %2035 = sub i32 %1991, %2032
  %2036 = sub i32 %1992, %2033
  %2037 = add i32 %2034, %2028
  %2038 = add i32 %2035, %2029
  %2039 = add i32 %2036, %2030
  %2040 = fmul fast float %1944, %1874
  %2041 = call float @dx.op.unary.f32(i32 23, float %2040)  ; Log(value)
  %2042 = fptoui float %2041 to i32
  %2043 = and i32 %2042, 31
  %2044 = lshr i32 %2037, %2043
  %2045 = lshr i32 %2038, %2043
  %2046 = lshr i32 %2039, %2043
  %2047 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 4, i32 261 })  ; AnnotateHandle(res,props)  resource: Texture3D<U32>
  %2048 = call %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32 66, %dx.types.Handle %2047, i32 %2042, i32 %2044, i32 %2045, i32 %2046, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2049 = extractvalue %dx.types.ResRet.i32 %2048, 0
  %2050 = icmp sgt i32 %2049, -1
  br i1 %2050, label %2051, label %2057

; <label>:2051                                    ; preds = %2027
  %2052 = and i32 %2049, 16777215
  %2053 = uitofp i32 %2052 to float
  %2054 = fmul fast float %1827, 0x3F50624DE0000000
  %2055 = fmul fast float %2054, %2040
  %2056 = fmul fast float %2055, %2053
  br label %2057

; <label>:2057                                    ; preds = %2051, %2027
  %2058 = phi float [ %2056, %2051 ], [ 0.000000e+00, %2027 ]
  %2059 = fadd fast float %2058, %1942
  %2060 = fcmp fast ogt float %2059, 1.000000e+00
  br i1 %2060, label %2068, label %2061

; <label>:2061                                    ; preds = %2057, %2018
  %2062 = phi float [ %2059, %2057 ], [ %1942, %2018 ]
  %2063 = fmul fast float %1940, %1828
  %2064 = uitofp i32 %1824 to float
  %2065 = call float @dx.op.binary.f32(i32 36, float %2064, float %2063)  ; FMin(a,b)
  %2066 = fadd fast float %2065, %1941
  %2067 = fcmp fast olt float %2066, %1928
  br i1 %2067, label %1932, label %2068

; <label>:2068                                    ; preds = %2061, %2057
  %2069 = phi float [ %2062, %2061 ], [ %2059, %2057 ]
  br label %2070

; <label>:2070                                    ; preds = %2068, %1907, %1875, %1833
  %2071 = phi float [ 0.000000e+00, %1875 ], [ 0.000000e+00, %1833 ], [ 0.000000e+00, %1907 ], [ %2069, %2068 ]
  %2072 = fsub fast float 1.000000e+00, %2071
  %2073 = call float @dx.op.unary.f32(i32 7, float %2072)  ; Saturate(value)
  %2074 = call float @dx.op.binary.f32(i32 36, float %1834, float %2073)  ; FMin(a,b)
  %2075 = add nuw i32 %1835, 1
  %2076 = icmp eq i32 %2075, %1830
  br i1 %2076, label %2077, label %1833

; <label>:2077                                    ; preds = %2070
  br label %2078

; <label>:2078                                    ; preds = %2077, %1811, %1800, %1616, %253
  %2079 = phi float [ %1617, %1616 ], [ %1617, %1800 ], [ 1.000000e+00, %253 ], [ %1617, %1811 ], [ %2074, %2077 ]
  %2080 = fcmp fast ogt float %2079, 0x3F91111120000000
  %2081 = fcmp fast olt float %2079, 1.000000e+00
  %2082 = and i1 %2080, %2081
  br i1 %2082, label %2083, label %2104

; <label>:2083                                    ; preds = %2078
  %2084 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %24, i32 152)  ; CBufferLoadLegacy(handle,regIndex)
  %2085 = extractvalue %dx.types.CBufRet.i32 %2084, 2
  %2086 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %22, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %2087 = extractvalue %dx.types.CBufRet.i32 %2086, 0
  %2088 = extractvalue %dx.types.CBufRet.i32 %2086, 1
  %2089 = extractvalue %dx.types.CBufRet.i32 %2086, 2
  %2090 = and i32 %2087, %62
  %2091 = and i32 %2088, %64
  %2092 = and i32 %2089, %2085
  %2093 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %22, i32 0)  ; CBufferLoadLegacy(handle,regIndex)
  %2094 = extractvalue %dx.types.CBufRet.i32 %2093, 1
  %2095 = mul i32 %2092, %2094
  %2096 = add i32 %2095, %2091
  %2097 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %7, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %2098 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %2097, i32 0, i32 %2090, i32 %2096, i32 undef, i32 0, i32 0, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %2099 = extractvalue %dx.types.ResRet.f32 %2098, 0
  %2100 = fadd fast float %2099, -5.000000e-01
  %2101 = fmul fast float %2100, 0x3FB1111120000000
  %2102 = fadd fast float %2101, %2079
  %2103 = call float @dx.op.unary.f32(i32 7, float %2102)  ; Saturate(value)
  br label %2104

; <label>:2104                                    ; preds = %2083, %2078
  %2105 = phi float [ %2103, %2083 ], [ %2079, %2078 ]
  %2106 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4098, i32 521 })  ; AnnotateHandle(res,props)  resource: RWTexture2D<2xF32>
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %2106, i32 %62, i32 %64, i32 undef, float %2105, float %2105, float %2105, float %2105, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  ret void

; <label>:2107                                    ; preds = %77, %0
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.groupId.i32(i32, i32) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.flattenedThreadIdInGroup.i32(i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #2

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot2.f32(i32, float, float, float, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot3.f32(i32, float, float, float, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.rawBufferLoad.i32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.rawBufferLoad.f32(i32, %dx.types.Handle, i32, i32, i8, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #1

; Function Attrs: nounwind
declare i1 @dx.op.waveAllTrue(i32, i1) #2

; Function Attrs: nounwind readnone
declare i32 @dx.op.binary.i32(i32, i32, i32) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.textureLoad.i32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.i32 @dx.op.bufferLoad.i32(i32, %dx.types.Handle, i32, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #1

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #1

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
!dx.entryPoints = !{!33}

!0 = !{!"dxc(private) 1.7.0.0 (private, 00000000)"}
!1 = !{i32 1, i32 6}
!2 = !{i32 1, i32 7}
!3 = !{!"cs", i32 6, i32 6}
!4 = !{!5, !23, !25, !31}
!5 = !{!6, !8, !9, !10, !11, !12, !14, !16, !17, !18, !19, !20, !22}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i32 0, !7}
!9 = !{i32 2, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 2, i32 1, i32 2, i32 0, !7}
!10 = !{i32 3, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 3, i32 1, i32 2, i32 0, !7}
!11 = !{i32 4, %struct.ByteAddressBuffer* undef, !"", i32 0, i32 4, i32 1, i32 11, i32 0, null}
!12 = !{i32 5, %"class.StructuredBuffer<unsigned int>"* undef, !"", i32 0, i32 5, i32 1, i32 12, i32 0, !13}
!13 = !{i32 1, i32 4}
!14 = !{i32 6, %"class.Texture2DArray<unsigned int>"* undef, !"", i32 0, i32 6, i32 1, i32 7, i32 0, !15}
!15 = !{i32 0, i32 5}
!16 = !{i32 7, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 7, i32 1, i32 2, i32 0, !7}
!17 = !{i32 8, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 8, i32 1, i32 2, i32 0, !7}
!18 = !{i32 9, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 9, i32 1, i32 2, i32 0, !7}
!19 = !{i32 10, %"class.Buffer<unsigned int>"* undef, !"", i32 0, i32 10, i32 1, i32 10, i32 0, !15}
!20 = !{i32 11, %"class.StructuredBuffer<FPackedVirtualVoxelNodeDesc>"* undef, !"", i32 0, i32 11, i32 1, i32 12, i32 0, !21}
!21 = !{i32 1, i32 32}
!22 = !{i32 12, %"class.Texture3D<unsigned int>"* undef, !"", i32 0, i32 12, i32 1, i32 4, i32 0, !15}
!23 = !{!24}
!24 = !{i32 0, %"class.RWTexture2D<vector<float, 2> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!25 = !{!26, !27, !28, !29, !30}
!26 = !{i32 0, %_RootShaderParameters* undef, !"", i32 0, i32 0, i32 1, i32 384, null}
!27 = !{i32 1, %hostlayout.View* undef, !"", i32 0, i32 1, i32 1, i32 5724, null}
!28 = !{i32 2, %VirtualShadowMap* undef, !"", i32 0, i32 2, i32 1, i32 156, null}
!29 = !{i32 3, %BlueNoise* undef, !"", i32 0, i32 3, i32 1, i32 44, null}
!30 = !{i32 4, %VirtualVoxel* undef, !"", i32 0, i32 4, i32 1, i32 212, null}
!31 = !{!32}
!32 = !{i32 0, %struct.SamplerState* undef, !"", i32 0, i32 0, i32 1, i32 0, null}
!33 = !{void ()* @VirtualShadowMapProjection, !"VirtualShadowMapProjection", null, !4, !34}
!34 = !{i32 0, i64 524304, i32 4, !35, i32 5, !36}
!35 = !{i32 8, i32 8, i32 1}
!36 = !{i32 0}
!37 = !{i32 1}

