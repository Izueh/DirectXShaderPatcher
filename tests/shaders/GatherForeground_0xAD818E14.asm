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
; shader hash: ea696eb014c7b0b17cc349ee2e73e823
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
;       float4 ViewportSize;                          ; Offset:    0
;       uint4 ViewportRect;                           ; Offset:   16
;       float2 DispatchThreadIdToInputBufferUV;       ; Offset:   40
;       float2 ConsiderCocRadiusAffineTransformation0;; Offset:   48
;       float2 ConsiderCocRadiusAffineTransformation1;; Offset:   56
;       float2 ConsiderAbsCocRadiusAffineTransformation;; Offset:   64
;       float2 InputBufferUVToOutputPixel;            ; Offset:   72
;       float MaxRecombineAbsCocRadius;               ; Offset:   84
;       float CocInvSqueeze;                          ; Offset:   92
;       float MinGatherRadius;                        ; Offset:   96
;       float SlightOutOfFocusRadiusBoundary;         ; Offset:  100
;       float4 GatherInputSize;                       ; Offset:  128
;       float2 GatherInputViewportSize;               ; Offset:  144
;   
;   } _RootShaderParameters;                          ; Offset:    0 Size:   152
;
; }
;
;
; Resource Bindings:
;
; Name                                 Type  Format         Dim      ID      HLSL Bind  Count
; ------------------------------ ---------- ------- ----------- ------- -------------- ------
; _RootShaderParameters             cbuffer      NA          NA     CB0            cb0     1
; D3DStaticPointClampedSampler      sampler      NA          NA      S0   s1,space1000     1
; D3DStaticBilinearClampedSampler   sampler      NA          NA      S1   s3,space1000     1
; GatherInput_SceneColor            texture     f32          2d      T0             t0     1
; TileClassification_Foreground     texture     f32          2d      T1             t1     1
; ConvolutionOutput_SceneColor          UAV     f32          2d      U0             u0     1
;
target datalayout = "e-m:e-p:32:32-i1:32-i8:32-i16:32-i32:32-i64:64-f16:32-f32:32-f64:64-n8:16:32:64"
target triple = "dxil-ms-dx"

%dx.types.Handle = type { i8* }
%dx.types.ResBind = type { i32, i32, i32, i8 }
%dx.types.ResourceProperties = type { i32, i32 }
%dx.types.CBufRet.f32 = type { float, float, float, float }
%dx.types.ResRet.f32 = type { float, float, float, float, i32 }
%dx.types.CBufRet.i32 = type { i32, i32, i32, i32 }
%"class.Texture2D<vector<float, 4> >" = type { <4 x float>, %"class.Texture2D<vector<float, 4> >::mips_type" }
%"class.Texture2D<vector<float, 4> >::mips_type" = type { i32 }
%"class.RWTexture2D<vector<float, 4> >" = type { <4 x float> }
%_RootShaderParameters = type { <4 x float>, <4 x i32>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, <2 x float>, float, float, float, float, <4 x float>, <2 x float> }
%struct.SamplerState = type { i32 }

define void @GatherMainCS() {
  %1 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 1 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %2 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 0, i8 0 }, i32 1, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %3 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind zeroinitializer, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %4 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 3, i32 3, i32 1000, i8 3 }, i32 3, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %5 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 1, i32 1, i32 1000, i8 3 }, i32 1, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %6 = call %dx.types.Handle @dx.op.createHandleFromBinding(i32 217, %dx.types.ResBind { i32 0, i32 0, i32 0, i8 2 }, i32 0, i1 false)  ; CreateHandleFromBinding(bind,index,nonUniformIndex)
  %7 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %6, %dx.types.ResourceProperties { i32 13, i32 152 })  ; AnnotateHandle(res,props)  resource: CBuffer
  %8 = call i32 @dx.op.threadId.i32(i32 93, i32 0)  ; ThreadId(component)
  %9 = call i32 @dx.op.threadId.i32(i32 93, i32 1)  ; ThreadId(component)
  %10 = call i32 @dx.op.groupId.i32(i32 94, i32 0)  ; GroupId(component)
  %11 = call i32 @dx.op.groupId.i32(i32 94, i32 1)  ; GroupId(component)
  %12 = uitofp i32 %8 to float
  %13 = uitofp i32 %9 to float
  %14 = fadd fast float %12, 5.000000e-01
  %15 = fadd fast float %13, 5.000000e-01
  %16 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 2)  ; CBufferLoadLegacy(handle,regIndex)
  %17 = extractvalue %dx.types.CBufRet.f32 %16, 2
  %18 = extractvalue %dx.types.CBufRet.f32 %16, 3
  %19 = fmul fast float %17, %14
  %20 = fmul fast float %18, %15
  %21 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %22 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %2, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %23 = call %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32 66, %dx.types.Handle %22, i32 0, i32 %10, i32 %11, i32 undef, i32 undef, i32 undef, i32 undef)  ; TextureLoad(srv,mipLevelOrSampleCount,coord0,coord1,coord2,offset0,offset1,offset2)
  %24 = extractvalue %dx.types.ResRet.f32 %23, 0
  %25 = fsub fast float -0.000000e+00, %24
  %26 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 6)  ; CBufferLoadLegacy(handle,regIndex)
  %27 = extractvalue %dx.types.CBufRet.f32 %26, 0
  %28 = fcmp fast ult float %27, %25
  %29 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %24)  ; WaveReadLaneFirst(value)
  %30 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %25)  ; WaveReadLaneFirst(value)
  %31 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float 1.638400e+04)  ; WaveReadLaneFirst(value)
  %32 = call i32 @dx.op.waveReadLaneFirst.i32(i32 118, i32 5)  ; WaveReadLaneFirst(value)
  br i1 %28, label %42, label %33, !dx.controlflow.hints !20

; <label>:33                                      ; preds = %0
  %34 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %7, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %35 = extractvalue %dx.types.CBufRet.i32 %34, 2
  %36 = extractvalue %dx.types.CBufRet.i32 %34, 3
  %37 = icmp uge i32 %8, %35
  %38 = icmp uge i32 %9, %36
  %39 = or i1 %37, %38
  br i1 %39, label %618, label %40

; <label>:40                                      ; preds = %33
  %41 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4098, i32 1033 })  ; AnnotateHandle(res,props)  resource: RWTexture2D<4xF32>
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %41, i32 %8, i32 %9, i32 undef, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, float 0.000000e+00, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %618

; <label>:42                                      ; preds = %0
  %43 = fadd fast float %12, 0x4040551EC0000000
  %44 = fadd fast float %13, 0x4027A147A0000000
  %45 = call float @dx.op.dot2.f32(i32 54, float %43, float %44, float 0x3FB12E2860000000, float 0x3F77E8B200000000)  ; Dot2(ax,ay,bx,by)
  %46 = call float @dx.op.unary.f32(i32 22, float %45)  ; Frc(value)
  %47 = fmul fast float %46, 0x404A7DD040000000
  %48 = call float @dx.op.unary.f32(i32 22, float %47)  ; Frc(value)
  %49 = call float @dx.op.dot2.f32(i32 54, float %12, float %13, float 0x3FB12E2860000000, float 0x3F77E8B200000000)  ; Dot2(ax,ay,bx,by)
  %50 = call float @dx.op.unary.f32(i32 22, float %49)  ; Frc(value)
  %51 = fmul fast float %50, 0x404A7DD040000000
  %52 = call float @dx.op.unary.f32(i32 22, float %51)  ; Frc(value)
  %53 = extractvalue %dx.types.CBufRet.f32 %21, 1
  %54 = extractvalue %dx.types.ResRet.f32 %23, 1
  %55 = fsub fast float %54, %24
  %56 = fmul fast float %24, 0xBFA99999A0000000
  %57 = fcmp fast olt float %55, %56
  %58 = call float @dx.op.unary.f32(i32 24, float %52)  ; Sqrt(value)
  %59 = fmul fast float %58, 0x3FDEB851E0000000
  %60 = fmul fast float %48, 0x401921FB60000000
  %61 = call float @dx.op.unary.f32(i32 12, float %60)  ; Cos(value)
  %62 = call float @dx.op.unary.f32(i32 13, float %60)  ; Sin(value)
  %63 = fmul fast float %30, 0x3FC745D180000000
  %64 = call float @dx.op.unary.f32(i32 23, float %63)  ; Log(value)
  %65 = fadd fast float %64, 5.000000e-01
  %66 = call float @dx.op.unary.f32(i32 27, float %65)  ; Round_ni(value)
  %67 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %68 = extractvalue %dx.types.CBufRet.f32 %67, 2
  %69 = extractvalue %dx.types.CBufRet.f32 %67, 3
  %70 = fptoui float %66 to i32
  %71 = uitofp i32 %70 to float
  %72 = call float @dx.op.unary.f32(i32 21, float %71)  ; Exp(value)
  %73 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 9)  ; CBufferLoadLegacy(handle,regIndex)
  %74 = extractvalue %dx.types.CBufRet.f32 %73, 0
  %75 = extractvalue %dx.types.CBufRet.f32 %73, 1
  %76 = fmul fast float %72, 5.000000e-01
  %77 = fsub fast float %74, %76
  %78 = fsub fast float %75, %76
  %79 = fmul fast float %77, %68
  %80 = fmul fast float %78, %69
  %81 = fdiv fast float 1.000000e+00, %72
  %82 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %83 = fsub fast float 1.000000e+00, %53
  br i1 %57, label %84, label %260

; <label>:84                                      ; preds = %42
  %85 = fmul fast float %59, %61
  %86 = fmul fast float %62, %59
  %87 = fadd fast float %30, -5.000000e+00
  %88 = fsub fast float %87, %72
  %89 = call float @dx.op.binary.f32(i32 35, float %88, float 0.000000e+00)  ; FMax(a,b)
  %90 = fmul fast float %89, 0x3FC745D180000000
  %91 = fmul fast float %85, %90
  %92 = fmul fast float %86, %90
  %93 = fmul fast float %91, %68
  %94 = fmul fast float %92, %69
  %95 = fadd fast float %93, %19
  %96 = fadd fast float %94, %20
  %97 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %63)  ; WaveReadLaneFirst(value)
  %98 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %79)  ; WaveReadLaneFirst(value)
  %99 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %80)  ; WaveReadLaneFirst(value)
  %100 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %66)  ; WaveReadLaneFirst(value)
  %101 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %81)  ; WaveReadLaneFirst(value)
  %102 = fsub fast float -0.000000e+00, %95
  %103 = fsub fast float -0.000000e+00, %96
  %104 = call float @dx.op.binary.f32(i32 35, float %95, float %102)  ; FMax(a,b)
  %105 = call float @dx.op.binary.f32(i32 35, float %96, float %103)  ; FMax(a,b)
  %106 = fmul fast float %98, 2.000000e+00
  %107 = fmul fast float %99, 2.000000e+00
  %108 = fsub fast float %106, %104
  %109 = fsub fast float %107, %105
  %110 = call float @dx.op.binary.f32(i32 36, float %104, float %108)  ; FMin(a,b)
  %111 = call float @dx.op.binary.f32(i32 36, float %105, float %109)  ; FMin(a,b)
  %112 = call float @dx.op.binary.f32(i32 36, float %110, float %98)  ; FMin(a,b)
  %113 = call float @dx.op.binary.f32(i32 36, float %111, float %99)  ; FMin(a,b)
  %114 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %4, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %115 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %82, %dx.types.Handle %114, float %112, float %113, float undef, float undef, i32 0, i32 0, i32 undef, float %100)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %116 = extractvalue %dx.types.ResRet.f32 %115, 0
  %117 = extractvalue %dx.types.ResRet.f32 %115, 1
  %118 = extractvalue %dx.types.ResRet.f32 %115, 2
  br label %119

; <label>:119                                     ; preds = %233, %84
  %120 = phi i32 [ %234, %233 ], [ 2, %84 ]
  %121 = phi i32 [ %125, %233 ], [ 0, %84 ]
  %122 = phi float [ %225, %233 ], [ %116, %84 ]
  %123 = phi float [ %226, %233 ], [ %117, %84 ]
  %124 = phi float [ %227, %233 ], [ %118, %84 ]
  %125 = add nuw nsw i32 %121, 1
  %126 = uitofp i32 %125 to float
  %127 = fmul fast float %126, %97
  %128 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %127)  ; WaveReadLaneFirst(value)
  %129 = shl i32 %121, 2
  %130 = add nuw nsw i32 %129, 4
  %131 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %127)  ; WaveReadLaneFirst(value)
  %132 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %30)  ; WaveReadLaneFirst(value)
  %133 = uitofp i32 %130 to float
  %134 = fdiv fast float 0x400921FB60000000, %133
  %135 = call float @dx.op.unary.f32(i32 12, float %134)  ; Cos(value)
  %136 = call float @dx.op.unary.f32(i32 13, float %134)  ; Sin(value)
  %137 = fsub fast float -0.000000e+00, %136
  %138 = and i32 %121, 1
  %139 = uitofp i32 %138 to float
  %140 = fmul fast float %139, 0x3FF921FB60000000
  %141 = fdiv fast float %140, %133
  %142 = call float @dx.op.unary.f32(i32 12, float %141)  ; Cos(value)
  %143 = call float @dx.op.unary.f32(i32 13, float %141)  ; Sin(value)
  %144 = fmul fast float %142, %131
  %145 = fmul fast float %143, %131
  %146 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %142)  ; WaveReadLaneFirst(value)
  %147 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %143)  ; WaveReadLaneFirst(value)
  %148 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %144)  ; WaveReadLaneFirst(value)
  %149 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %145)  ; WaveReadLaneFirst(value)
  br label %150

; <label>:150                                     ; preds = %230, %119
  %151 = phi float [ %167, %230 ], [ %146, %119 ]
  %152 = phi float [ %168, %230 ], [ %147, %119 ]
  %153 = phi float [ %169, %230 ], [ %148, %119 ]
  %154 = phi float [ %170, %230 ], [ %149, %119 ]
  %155 = phi i32 [ %231, %230 ], [ 0, %119 ]
  %156 = phi float [ %225, %230 ], [ %122, %119 ]
  %157 = phi float [ %226, %230 ], [ %123, %119 ]
  %158 = phi float [ %227, %230 ], [ %124, %119 ]
  %159 = fmul fast float %151, %135
  %160 = call float @dx.op.tertiary.f32(i32 46, float %152, float %137, float %159)  ; FMad(a,b,c)
  %161 = fmul fast float %151, %136
  %162 = call float @dx.op.tertiary.f32(i32 46, float %152, float %135, float %161)  ; FMad(a,b,c)
  %163 = fmul fast float %153, %135
  %164 = call float @dx.op.tertiary.f32(i32 46, float %154, float %137, float %163)  ; FMad(a,b,c)
  %165 = fmul fast float %153, %136
  %166 = call float @dx.op.tertiary.f32(i32 46, float %154, float %135, float %165)  ; FMad(a,b,c)
  %167 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %160)  ; WaveReadLaneFirst(value)
  %168 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %162)  ; WaveReadLaneFirst(value)
  %169 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %164)  ; WaveReadLaneFirst(value)
  %170 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %166)  ; WaveReadLaneFirst(value)
  br label %171

; <label>:171                                     ; preds = %171, %150
  %172 = phi i32 [ %228, %171 ], [ 0, %150 ]
  %173 = phi float [ %225, %171 ], [ %156, %150 ]
  %174 = phi float [ %226, %171 ], [ %157, %150 ]
  %175 = phi float [ %227, %171 ], [ %158, %150 ]
  %176 = icmp eq i32 %172, 1
  %177 = fsub fast float -0.000000e+00, %170
  %178 = select i1 %176, float %177, float %169
  %179 = select i1 %176, float %169, float %170
  %180 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %181 = extractvalue %dx.types.CBufRet.f32 %180, 3
  %182 = fmul fast float %181, %178
  %183 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %184 = extractvalue %dx.types.CBufRet.f32 %183, 2
  %185 = extractvalue %dx.types.CBufRet.f32 %183, 3
  %186 = fmul fast float %182, %184
  %187 = fmul fast float %185, %179
  %188 = fadd fast float %186, %95
  %189 = fadd fast float %187, %96
  %190 = fsub fast float -0.000000e+00, %188
  %191 = fsub fast float -0.000000e+00, %189
  %192 = call float @dx.op.binary.f32(i32 35, float %188, float %190)  ; FMax(a,b)
  %193 = call float @dx.op.binary.f32(i32 35, float %189, float %191)  ; FMax(a,b)
  %194 = fsub fast float %106, %192
  %195 = fsub fast float %107, %193
  %196 = call float @dx.op.binary.f32(i32 36, float %192, float %194)  ; FMin(a,b)
  %197 = call float @dx.op.binary.f32(i32 36, float %193, float %195)  ; FMin(a,b)
  %198 = call float @dx.op.binary.f32(i32 36, float %196, float %98)  ; FMin(a,b)
  %199 = call float @dx.op.binary.f32(i32 36, float %197, float %99)  ; FMin(a,b)
  %200 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %201 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %4, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %202 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %200, %dx.types.Handle %201, float %198, float %199, float undef, float undef, i32 0, i32 0, i32 undef, float %100)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %203 = extractvalue %dx.types.ResRet.f32 %202, 0
  %204 = extractvalue %dx.types.ResRet.f32 %202, 1
  %205 = extractvalue %dx.types.ResRet.f32 %202, 2
  %206 = fadd fast float %203, %173
  %207 = fadd fast float %204, %174
  %208 = fadd fast float %205, %175
  %209 = fsub fast float %95, %186
  %210 = fsub fast float %96, %187
  %211 = fsub fast float -0.000000e+00, %209
  %212 = fsub fast float -0.000000e+00, %210
  %213 = call float @dx.op.binary.f32(i32 35, float %209, float %211)  ; FMax(a,b)
  %214 = call float @dx.op.binary.f32(i32 35, float %210, float %212)  ; FMax(a,b)
  %215 = fsub fast float %106, %213
  %216 = fsub fast float %107, %214
  %217 = call float @dx.op.binary.f32(i32 36, float %213, float %215)  ; FMin(a,b)
  %218 = call float @dx.op.binary.f32(i32 36, float %214, float %216)  ; FMin(a,b)
  %219 = call float @dx.op.binary.f32(i32 36, float %217, float %98)  ; FMin(a,b)
  %220 = call float @dx.op.binary.f32(i32 36, float %218, float %99)  ; FMin(a,b)
  %221 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %200, %dx.types.Handle %201, float %219, float %220, float undef, float undef, i32 0, i32 0, i32 undef, float %100)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %222 = extractvalue %dx.types.ResRet.f32 %221, 0
  %223 = extractvalue %dx.types.ResRet.f32 %221, 1
  %224 = extractvalue %dx.types.ResRet.f32 %221, 2
  %225 = fadd fast float %206, %222
  %226 = fadd fast float %207, %223
  %227 = fadd fast float %208, %224
  %228 = add nuw nsw i32 %172, 1
  %229 = icmp eq i32 %228, 2
  br i1 %229, label %230, label %171

; <label>:230                                     ; preds = %171
  %231 = add nuw nsw i32 %155, 1
  %232 = icmp eq i32 %231, %120
  br i1 %232, label %233, label %150

; <label>:233                                     ; preds = %230
  %234 = add nuw nsw i32 %120, 2
  %235 = icmp eq i32 %125, 5
  br i1 %235, label %236, label %119

; <label>:236                                     ; preds = %233
  %237 = fadd fast float %83, %30
  %238 = call float @dx.op.unary.f32(i32 7, float %237)  ; Saturate(value)
  %239 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %240 = extractvalue %dx.types.CBufRet.f32 %239, 2
  %241 = extractvalue %dx.types.CBufRet.f32 %239, 3
  %242 = fmul fast float %240, %19
  %243 = fmul fast float %241, %20
  %244 = fptoui float %242 to i32
  %245 = fptoui float %243 to i32
  %246 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %7, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %247 = extractvalue %dx.types.CBufRet.i32 %246, 2
  %248 = extractvalue %dx.types.CBufRet.i32 %246, 3
  %249 = icmp uge i32 %244, %247
  %250 = icmp uge i32 %245, %248
  %251 = or i1 %249, %250
  br i1 %251, label %618, label %252

; <label>:252                                     ; preds = %236
  %253 = fmul fast float %227, 0x3F80ECF560000000
  %254 = fmul fast float %226, 0x3F80ECF560000000
  %255 = fmul fast float %225, 0x3F80ECF560000000
  %256 = fmul fast float %255, %238
  %257 = fmul fast float %254, %238
  %258 = fmul fast float %253, %238
  %259 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4098, i32 1033 })  ; AnnotateHandle(res,props)  resource: RWTexture2D<4xF32>
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %259, i32 %244, i32 %245, i32 undef, float %256, float %257, float %258, float %238, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %618

; <label>:260                                     ; preds = %42
  %261 = fmul fast float %61, %63
  %262 = fmul fast float %261, %59
  %263 = fmul fast float %262, %68
  %264 = fmul fast float %59, %63
  %265 = fmul fast float %264, %62
  %266 = fmul fast float %265, %69
  %267 = fadd fast float %263, %19
  %268 = fadd fast float %266, %20
  %269 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %63)  ; WaveReadLaneFirst(value)
  %270 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %79)  ; WaveReadLaneFirst(value)
  %271 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %80)  ; WaveReadLaneFirst(value)
  %272 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %66)  ; WaveReadLaneFirst(value)
  %273 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %81)  ; WaveReadLaneFirst(value)
  %274 = fsub fast float -0.000000e+00, %267
  %275 = fsub fast float -0.000000e+00, %268
  %276 = call float @dx.op.binary.f32(i32 35, float %267, float %274)  ; FMax(a,b)
  %277 = call float @dx.op.binary.f32(i32 35, float %268, float %275)  ; FMax(a,b)
  %278 = fmul fast float %270, 2.000000e+00
  %279 = fmul fast float %271, 2.000000e+00
  %280 = fsub fast float %278, %276
  %281 = fsub fast float %279, %277
  %282 = call float @dx.op.binary.f32(i32 36, float %276, float %280)  ; FMin(a,b)
  %283 = call float @dx.op.binary.f32(i32 36, float %277, float %281)  ; FMin(a,b)
  %284 = call float @dx.op.binary.f32(i32 36, float %282, float %270)  ; FMin(a,b)
  %285 = call float @dx.op.binary.f32(i32 36, float %283, float %271)  ; FMin(a,b)
  %286 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %5, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %287 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %82, %dx.types.Handle %286, float %284, float %285, float undef, float undef, i32 0, i32 0, i32 undef, float %272)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %288 = extractvalue %dx.types.ResRet.f32 %287, 0
  %289 = extractvalue %dx.types.ResRet.f32 %287, 1
  %290 = extractvalue %dx.types.ResRet.f32 %287, 2
  %291 = extractvalue %dx.types.ResRet.f32 %287, 3
  %292 = fsub fast float %83, %291
  %293 = call float @dx.op.unary.f32(i32 7, float %292)  ; Saturate(value)
  %294 = fmul fast float %291, %291
  %295 = fdiv fast float 0x3FD45F3060000000, %294
  %296 = call float @dx.op.binary.f32(i32 36, float %295, float 0x40145F3060000000)  ; FMin(a,b)
  %297 = fcmp fast olt float %291, 0.000000e+00
  %298 = select i1 %297, float %293, float 0.000000e+00
  %299 = fmul fast float %293, %298
  %300 = fsub fast float %291, %29
  %301 = fmul fast float %300, 5.000000e-01
  %302 = call float @dx.op.unary.f32(i32 7, float %301)  ; Saturate(value)
  %303 = call float @dx.op.unary.f32(i32 7, float %302)  ; Saturate(value)
  %304 = fmul fast float %303, 2.000000e+00
  %305 = fsub fast float 3.000000e+00, %304
  %306 = fmul fast float %303, %303
  %307 = fmul fast float %306, %305
  %308 = fsub fast float 1.000000e+00, %307
  %309 = fmul fast float %299, %296
  %310 = fmul fast float %309, %307
  %311 = fmul fast float %310, %288
  %312 = fmul fast float %310, %289
  %313 = fmul fast float %310, %290
  %314 = fmul fast float %309, %308
  %315 = fmul fast float %314, %288
  %316 = fmul fast float %314, %289
  %317 = fmul fast float %314, %290
  %318 = select i1 %297, float 1.000000e+00, float 0.000000e+00
  %319 = fmul fast float %293, %318
  %320 = icmp sgt i32 %32, 0
  br i1 %320, label %321, label %549

; <label>:321                                     ; preds = %260
  br label %322

; <label>:322                                     ; preds = %536, %321
  %323 = phi i32 [ %546, %536 ], [ 4, %321 ]
  %324 = phi float [ %537, %536 ], [ %310, %321 ]
  %325 = phi float [ %538, %536 ], [ %314, %321 ]
  %326 = phi float [ %539, %536 ], [ %319, %321 ]
  %327 = phi float [ %540, %536 ], [ %317, %321 ]
  %328 = phi float [ %541, %536 ], [ %313, %321 ]
  %329 = phi float [ %542, %536 ], [ %316, %321 ]
  %330 = phi float [ %543, %536 ], [ %312, %321 ]
  %331 = phi float [ %544, %536 ], [ %315, %321 ]
  %332 = phi float [ %545, %536 ], [ %311, %321 ]
  %333 = phi i32 [ %335, %536 ], [ 0, %321 ]
  %334 = lshr exact i32 %323, 1
  %335 = add nuw nsw i32 %333, 1
  %336 = uitofp i32 %335 to float
  %337 = fmul fast float %336, %269
  %338 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %337)  ; WaveReadLaneFirst(value)
  %339 = shl i32 %333, 2
  %340 = add i32 %339, 4
  %341 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %337)  ; WaveReadLaneFirst(value)
  %342 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %30)  ; WaveReadLaneFirst(value)
  %343 = uitofp i32 %340 to float
  %344 = fdiv fast float 0x400921FB60000000, %343
  %345 = call float @dx.op.unary.f32(i32 12, float %344)  ; Cos(value)
  %346 = call float @dx.op.unary.f32(i32 13, float %344)  ; Sin(value)
  %347 = fsub fast float -0.000000e+00, %346
  %348 = and i32 %333, 1
  %349 = uitofp i32 %348 to float
  %350 = fmul fast float %349, 0x3FF921FB60000000
  %351 = fdiv fast float %350, %343
  %352 = call float @dx.op.unary.f32(i32 12, float %351)  ; Cos(value)
  %353 = call float @dx.op.unary.f32(i32 13, float %351)  ; Sin(value)
  %354 = fmul fast float %352, %341
  %355 = fmul fast float %353, %341
  %356 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %352)  ; WaveReadLaneFirst(value)
  %357 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %353)  ; WaveReadLaneFirst(value)
  %358 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %354)  ; WaveReadLaneFirst(value)
  %359 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %355)  ; WaveReadLaneFirst(value)
  %360 = icmp eq i32 %340, 0
  br i1 %360, label %536, label %361

; <label>:361                                     ; preds = %322
  br label %362

; <label>:362                                     ; preds = %532, %361
  %363 = phi float [ %517, %532 ], [ %324, %361 ]
  %364 = phi float [ %528, %532 ], [ %325, %361 ]
  %365 = phi float [ %529, %532 ], [ %326, %361 ]
  %366 = phi float [ %523, %532 ], [ %327, %361 ]
  %367 = phi float [ %512, %532 ], [ %328, %361 ]
  %368 = phi float [ %522, %532 ], [ %329, %361 ]
  %369 = phi float [ %511, %532 ], [ %330, %361 ]
  %370 = phi float [ %521, %532 ], [ %331, %361 ]
  %371 = phi float [ %510, %532 ], [ %332, %361 ]
  %372 = phi float [ %385, %532 ], [ %356, %361 ]
  %373 = phi float [ %386, %532 ], [ %357, %361 ]
  %374 = phi float [ %387, %532 ], [ %358, %361 ]
  %375 = phi float [ %388, %532 ], [ %359, %361 ]
  %376 = phi i32 [ %533, %532 ], [ 0, %361 ]
  %377 = fmul fast float %372, %345
  %378 = call float @dx.op.tertiary.f32(i32 46, float %373, float %347, float %377)  ; FMad(a,b,c)
  %379 = fmul fast float %372, %346
  %380 = call float @dx.op.tertiary.f32(i32 46, float %373, float %345, float %379)  ; FMad(a,b,c)
  %381 = fmul fast float %374, %345
  %382 = call float @dx.op.tertiary.f32(i32 46, float %375, float %347, float %381)  ; FMad(a,b,c)
  %383 = fmul fast float %374, %346
  %384 = call float @dx.op.tertiary.f32(i32 46, float %375, float %345, float %383)  ; FMad(a,b,c)
  %385 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %378)  ; WaveReadLaneFirst(value)
  %386 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %380)  ; WaveReadLaneFirst(value)
  %387 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %382)  ; WaveReadLaneFirst(value)
  %388 = call float @dx.op.waveReadLaneFirst.f32(i32 118, float %384)  ; WaveReadLaneFirst(value)
  br label %389

; <label>:389                                     ; preds = %389, %362
  %390 = phi float [ %517, %389 ], [ %363, %362 ]
  %391 = phi float [ %528, %389 ], [ %364, %362 ]
  %392 = phi float [ %529, %389 ], [ %365, %362 ]
  %393 = phi float [ %523, %389 ], [ %366, %362 ]
  %394 = phi float [ %512, %389 ], [ %367, %362 ]
  %395 = phi float [ %522, %389 ], [ %368, %362 ]
  %396 = phi float [ %511, %389 ], [ %369, %362 ]
  %397 = phi float [ %521, %389 ], [ %370, %362 ]
  %398 = phi float [ %510, %389 ], [ %371, %362 ]
  %399 = phi i32 [ %530, %389 ], [ 0, %362 ]
  %400 = icmp eq i32 %399, 1
  %401 = fsub fast float -0.000000e+00, %388
  %402 = select i1 %400, float %401, float %387
  %403 = select i1 %400, float %387, float %388
  %404 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 5)  ; CBufferLoadLegacy(handle,regIndex)
  %405 = extractvalue %dx.types.CBufRet.f32 %404, 3
  %406 = fmul fast float %405, %402
  %407 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 8)  ; CBufferLoadLegacy(handle,regIndex)
  %408 = extractvalue %dx.types.CBufRet.f32 %407, 2
  %409 = extractvalue %dx.types.CBufRet.f32 %407, 3
  %410 = fmul fast float %406, %408
  %411 = fmul fast float %409, %403
  %412 = fadd fast float %410, %267
  %413 = fadd fast float %411, %268
  %414 = fsub fast float -0.000000e+00, %412
  %415 = fsub fast float -0.000000e+00, %413
  %416 = call float @dx.op.binary.f32(i32 35, float %412, float %414)  ; FMax(a,b)
  %417 = call float @dx.op.binary.f32(i32 35, float %413, float %415)  ; FMax(a,b)
  %418 = fsub fast float %278, %416
  %419 = fsub fast float %279, %417
  %420 = call float @dx.op.binary.f32(i32 36, float %416, float %418)  ; FMin(a,b)
  %421 = call float @dx.op.binary.f32(i32 36, float %417, float %419)  ; FMin(a,b)
  %422 = call float @dx.op.binary.f32(i32 36, float %420, float %270)  ; FMin(a,b)
  %423 = call float @dx.op.binary.f32(i32 36, float %421, float %271)  ; FMin(a,b)
  %424 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %3, %dx.types.ResourceProperties { i32 2, i32 1033 })  ; AnnotateHandle(res,props)  resource: Texture2D<4xF32>
  %425 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %5, %dx.types.ResourceProperties { i32 14, i32 0 })  ; AnnotateHandle(res,props)  resource: SamplerState
  %426 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %424, %dx.types.Handle %425, float %422, float %423, float undef, float undef, i32 0, i32 0, i32 undef, float %272)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %427 = extractvalue %dx.types.ResRet.f32 %426, 0
  %428 = extractvalue %dx.types.ResRet.f32 %426, 1
  %429 = extractvalue %dx.types.ResRet.f32 %426, 2
  %430 = extractvalue %dx.types.ResRet.f32 %426, 3
  %431 = call float @dx.op.unary.f32(i32 6, float %430)  ; FAbs(value)
  %432 = fsub fast float %431, %341
  %433 = fmul fast float %432, %273
  %434 = fadd fast float %433, 5.000000e-01
  %435 = call float @dx.op.unary.f32(i32 7, float %434)  ; Saturate(value)
  %436 = fsub fast float %267, %410
  %437 = fsub fast float %268, %411
  %438 = fsub fast float -0.000000e+00, %436
  %439 = fsub fast float -0.000000e+00, %437
  %440 = call float @dx.op.binary.f32(i32 35, float %436, float %438)  ; FMax(a,b)
  %441 = call float @dx.op.binary.f32(i32 35, float %437, float %439)  ; FMax(a,b)
  %442 = fsub fast float %278, %440
  %443 = fsub fast float %279, %441
  %444 = call float @dx.op.binary.f32(i32 36, float %440, float %442)  ; FMin(a,b)
  %445 = call float @dx.op.binary.f32(i32 36, float %441, float %443)  ; FMin(a,b)
  %446 = call float @dx.op.binary.f32(i32 36, float %444, float %270)  ; FMin(a,b)
  %447 = call float @dx.op.binary.f32(i32 36, float %445, float %271)  ; FMin(a,b)
  %448 = call %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32 62, %dx.types.Handle %424, %dx.types.Handle %425, float %446, float %447, float undef, float undef, i32 0, i32 0, i32 undef, float %272)  ; SampleLevel(srv,sampler,coord0,coord1,coord2,coord3,offset0,offset1,offset2,LOD)
  %449 = extractvalue %dx.types.ResRet.f32 %448, 0
  %450 = extractvalue %dx.types.ResRet.f32 %448, 1
  %451 = extractvalue %dx.types.ResRet.f32 %448, 2
  %452 = extractvalue %dx.types.ResRet.f32 %448, 3
  %453 = call float @dx.op.unary.f32(i32 6, float %452)  ; FAbs(value)
  %454 = fsub fast float %453, %341
  %455 = fmul fast float %454, %273
  %456 = fadd fast float %455, 5.000000e-01
  %457 = call float @dx.op.unary.f32(i32 7, float %456)  ; Saturate(value)
  %458 = fmul fast float %430, %430
  %459 = fdiv fast float 0x3FD45F3060000000, %458
  %460 = call float @dx.op.binary.f32(i32 36, float %459, float 0x40145F3060000000)  ; FMin(a,b)
  %461 = fmul fast float %452, %452
  %462 = fdiv fast float 0x3FD45F3060000000, %461
  %463 = call float @dx.op.binary.f32(i32 36, float %462, float 0x40145F3060000000)  ; FMin(a,b)
  %464 = fsub fast float %83, %430
  %465 = call float @dx.op.unary.f32(i32 7, float %464)  ; Saturate(value)
  %466 = fmul fast float %465, %435
  %467 = fsub fast float %83, %452
  %468 = call float @dx.op.unary.f32(i32 7, float %467)  ; Saturate(value)
  %469 = fmul fast float %468, %457
  %470 = fcmp fast ogt float %452, %430
  %471 = select i1 %470, float %460, float %463
  %472 = select i1 %470, float %466, float %469
  %473 = select i1 %470, float %430, float %452
  %474 = fcmp fast olt float %473, 0.000000e+00
  %475 = select i1 %474, float %472, float 0.000000e+00
  %476 = fsub fast float %83, %473
  %477 = call float @dx.op.unary.f32(i32 7, float %476)  ; Saturate(value)
  %478 = fmul fast float %475, %477
  %479 = fsub fast float %473, %29
  %480 = fmul fast float %479, 5.000000e-01
  %481 = call float @dx.op.unary.f32(i32 7, float %480)  ; Saturate(value)
  %482 = call float @dx.op.unary.f32(i32 7, float %481)  ; Saturate(value)
  %483 = fmul fast float %482, 2.000000e+00
  %484 = fsub fast float 3.000000e+00, %483
  %485 = fmul fast float %482, %482
  %486 = fmul fast float %485, %484
  %487 = fsub fast float 1.000000e+00, %486
  %488 = fmul fast float %486, %478
  %489 = fmul fast float %488, %471
  %490 = fmul fast float %489, %427
  %491 = fmul fast float %489, %428
  %492 = fmul fast float %489, %429
  %493 = fadd fast float %490, %398
  %494 = fadd fast float %491, %396
  %495 = fadd fast float %492, %394
  %496 = fmul fast float %487, %478
  %497 = fmul fast float %496, %471
  %498 = fmul fast float %497, %427
  %499 = fmul fast float %497, %428
  %500 = fmul fast float %497, %429
  %501 = fadd fast float %498, %397
  %502 = fadd fast float %499, %395
  %503 = fadd fast float %500, %393
  %504 = select i1 %474, float 1.000000e+00, float 0.000000e+00
  %505 = fmul fast float %477, %504
  %506 = fadd fast float %505, %392
  %507 = fmul fast float %489, %449
  %508 = fmul fast float %489, %450
  %509 = fmul fast float %489, %451
  %510 = fadd fast float %493, %507
  %511 = fadd fast float %494, %508
  %512 = fadd fast float %495, %509
  %513 = fadd fast float %477, %477
  %514 = fmul fast float %475, %513
  %515 = fmul fast float %486, %514
  %516 = fmul fast float %515, %471
  %517 = fadd fast float %516, %390
  %518 = fmul fast float %497, %449
  %519 = fmul fast float %497, %450
  %520 = fmul fast float %497, %451
  %521 = fadd fast float %501, %518
  %522 = fadd fast float %502, %519
  %523 = fadd fast float %503, %520
  %524 = fadd fast float %477, %477
  %525 = fmul fast float %475, %524
  %526 = fmul fast float %487, %525
  %527 = fmul fast float %526, %471
  %528 = fadd fast float %527, %391
  %529 = fadd fast float %506, %505
  %530 = add nuw nsw i32 %399, 1
  %531 = icmp eq i32 %530, 2
  br i1 %531, label %532, label %389

; <label>:532                                     ; preds = %389
  %533 = add nuw nsw i32 %376, 1
  %534 = icmp eq i32 %533, %334
  br i1 %534, label %535, label %362

; <label>:535                                     ; preds = %532
  br label %536

; <label>:536                                     ; preds = %535, %322
  %537 = phi float [ %324, %322 ], [ %517, %535 ]
  %538 = phi float [ %325, %322 ], [ %528, %535 ]
  %539 = phi float [ %326, %322 ], [ %529, %535 ]
  %540 = phi float [ %327, %322 ], [ %523, %535 ]
  %541 = phi float [ %328, %322 ], [ %512, %535 ]
  %542 = phi float [ %329, %322 ], [ %522, %535 ]
  %543 = phi float [ %330, %322 ], [ %511, %535 ]
  %544 = phi float [ %331, %322 ], [ %521, %535 ]
  %545 = phi float [ %332, %322 ], [ %510, %535 ]
  %546 = add i32 %323, 4
  %547 = icmp eq i32 %335, %32
  br i1 %547, label %548, label %322

; <label>:548                                     ; preds = %536
  br label %549

; <label>:549                                     ; preds = %548, %260
  %550 = phi float [ %310, %260 ], [ %537, %548 ]
  %551 = phi float [ %314, %260 ], [ %538, %548 ]
  %552 = phi float [ %319, %260 ], [ %539, %548 ]
  %553 = phi float [ %317, %260 ], [ %540, %548 ]
  %554 = phi float [ %313, %260 ], [ %541, %548 ]
  %555 = phi float [ %316, %260 ], [ %542, %548 ]
  %556 = phi float [ %312, %260 ], [ %543, %548 ]
  %557 = phi float [ %315, %260 ], [ %544, %548 ]
  %558 = phi float [ %311, %260 ], [ %545, %548 ]
  %559 = shl i32 %32, 2
  %560 = add i32 %32, 1
  %561 = mul i32 %559, %560
  %562 = or i32 %561, 1
  %563 = uitofp i32 %562 to float
  %564 = fmul fast float %30, %30
  %565 = fdiv fast float 0x3FD0D612C0000000, %564
  %566 = call float @dx.op.binary.f32(i32 36, float %565, float 0x40145F3060000000)  ; FMin(a,b)
  %567 = fdiv fast float 1.000000e+00, %566
  %568 = fcmp fast oeq float %550, 0.000000e+00
  %569 = select i1 %568, float 1.000000e+00, float 0.000000e+00
  %570 = fdiv fast float 1.000000e+00, %563
  %571 = fmul fast float %567, %551
  %572 = fmul fast float %571, %570
  %573 = fadd fast float %572, %569
  %574 = call float @dx.op.unary.f32(i32 7, float %573)  ; Saturate(value)
  %575 = fcmp fast ogt float %551, 0.000000e+00
  %576 = fdiv fast float 1.000000e+00, %551
  %577 = select i1 %575, float %576, float 0.000000e+00
  %578 = fcmp fast ogt float %550, 0.000000e+00
  %579 = fdiv fast float 1.000000e+00, %550
  %580 = select i1 %578, float %579, float 0.000000e+00
  %581 = fmul fast float %580, %558
  %582 = fmul fast float %580, %556
  %583 = fmul fast float %580, %554
  %584 = fmul fast float %570, %552
  %585 = fadd fast float %551, %550
  %586 = fcmp fast ogt float %585, 0.000000e+00
  %587 = select i1 %586, float %584, float 0.000000e+00
  %588 = call %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32 59, %dx.types.Handle %7, i32 4)  ; CBufferLoadLegacy(handle,regIndex)
  %589 = extractvalue %dx.types.CBufRet.f32 %588, 2
  %590 = extractvalue %dx.types.CBufRet.f32 %588, 3
  %591 = fmul fast float %589, %19
  %592 = fmul fast float %590, %20
  %593 = fptoui float %591 to i32
  %594 = fptoui float %592 to i32
  %595 = call %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32 59, %dx.types.Handle %7, i32 1)  ; CBufferLoadLegacy(handle,regIndex)
  %596 = extractvalue %dx.types.CBufRet.i32 %595, 2
  %597 = extractvalue %dx.types.CBufRet.i32 %595, 3
  %598 = icmp uge i32 %593, %596
  %599 = icmp uge i32 %594, %597
  %600 = or i1 %598, %599
  br i1 %600, label %618, label %601

; <label>:601                                     ; preds = %549
  %602 = fmul fast float %577, %553
  %603 = fsub fast float %602, %583
  %604 = fmul fast float %603, %574
  %605 = fadd fast float %604, %583
  %606 = fmul fast float %577, %555
  %607 = fsub fast float %606, %582
  %608 = fmul fast float %607, %574
  %609 = fadd fast float %608, %582
  %610 = fmul fast float %577, %557
  %611 = fsub fast float %610, %581
  %612 = fmul fast float %611, %574
  %613 = fadd fast float %612, %581
  %614 = fmul fast float %613, %587
  %615 = fmul fast float %609, %587
  %616 = fmul fast float %605, %587
  %617 = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %1, %dx.types.ResourceProperties { i32 4098, i32 1033 })  ; AnnotateHandle(res,props)  resource: RWTexture2D<4xF32>
  call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %617, i32 %593, i32 %594, i32 undef, float %614, float %615, float %616, float %587, i8 15)  ; TextureStore(srv,coord0,coord1,coord2,value0,value1,value2,value3,mask)
  br label %618

; <label>:618                                     ; preds = %601, %549, %252, %236, %40, %33
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @dx.op.threadId.i32(i32, i32) #0

; Function Attrs: nounwind readnone
declare i32 @dx.op.groupId.i32(i32, i32) #0

; Function Attrs: nounwind
declare float @dx.op.waveReadLaneFirst.f32(i32, float) #1

; Function Attrs: nounwind
declare i32 @dx.op.waveReadLaneFirst.i32(i32, i32) #1

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.dot2.f32(i32, float, float, float, float) #0

; Function Attrs: nounwind
declare void @dx.op.textureStore.f32(i32, %dx.types.Handle, i32, i32, i32, float, float, float, float, i8) #1

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.textureLoad.f32(i32, %dx.types.Handle, i32, i32, i32, i32, i32, i32, i32) #2

; Function Attrs: nounwind readnone
declare float @dx.op.binary.f32(i32, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.ResRet.f32 @dx.op.sampleLevel.f32(i32, %dx.types.Handle, %dx.types.Handle, float, float, float, float, i32, i32, i32, float) #2

; Function Attrs: nounwind readnone
declare float @dx.op.tertiary.f32(i32, float, float, float) #0

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.i32 @dx.op.cbufferLoadLegacy.i32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readonly
declare %dx.types.CBufRet.f32 @dx.op.cbufferLoadLegacy.f32(i32, %dx.types.Handle, i32) #2

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.annotateHandle(i32, %dx.types.Handle, %dx.types.ResourceProperties) #0

; Function Attrs: nounwind readnone
declare %dx.types.Handle @dx.op.createHandleFromBinding(i32, %dx.types.ResBind, i32, i1) #0

attributes #0 = { nounwind readnone }
attributes #1 = { nounwind }
attributes #2 = { nounwind readonly }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.valver = !{!2}
!dx.shaderModel = !{!3}
!dx.resources = !{!4}
!dx.entryPoints = !{!16}

!0 = !{!"dxc(private) 1.7.0.0 (private, 00000000)"}
!1 = !{i32 1, i32 6}
!2 = !{i32 1, i32 7}
!3 = !{!"cs", i32 6, i32 6}
!4 = !{!5, !9, !11, !13}
!5 = !{!6, !8}
!6 = !{i32 0, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i32 0, !7}
!7 = !{i32 0, i32 9}
!8 = !{i32 1, %"class.Texture2D<vector<float, 4> >"* undef, !"", i32 0, i32 1, i32 1, i32 2, i32 0, !7}
!9 = !{!10}
!10 = !{i32 0, %"class.RWTexture2D<vector<float, 4> >"* undef, !"", i32 0, i32 0, i32 1, i32 2, i1 false, i1 false, i1 false, !7}
!11 = !{!12}
!12 = !{i32 0, %_RootShaderParameters* undef, !"", i32 0, i32 0, i32 1, i32 152, null}
!13 = !{!14, !15}
!14 = !{i32 0, %struct.SamplerState* undef, !"", i32 1000, i32 1, i32 1, i32 0, null}
!15 = !{i32 1, %struct.SamplerState* undef, !"", i32 1000, i32 3, i32 1, i32 0, null}
!16 = !{void ()* @GatherMainCS, !"GatherMainCS", null, !4, !17}
!17 = !{i32 0, i64 524288, i32 4, !18, i32 5, !19}
!18 = !{i32 8, i32 8, i32 1}
!19 = !{i32 0}
!20 = distinct !{!20, !"dx.controlflow.hints", i32 1}

