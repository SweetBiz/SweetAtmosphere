#include "Precompute/PrecomputeShader.h"

#include "RHIGPUReadback.h"
#include "RenderGraphUtils.h"

#define PARTICLE_PROFILE_FIELD_NAME(ProfileIndex, Name) ParticleProfile_##ProfileIndex##_##Name

#define PARTICLE_PROFILE_FIELD(ProfileIndex, Name) \
	LAYOUT_FIELD(FShaderParameter, PARTICLE_PROFILE_FIELD_NAME(ProfileIndex, Name))

#define PARTICLE_PROFILE_FIELDS(Index)                     \
	PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsR) \
	PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsG) \
	PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsB) \
	PARTICLE_PROFILE_FIELD(Index, PhaseFunction)           \
	PARTICLE_PROFILE_FIELD(Index, ExponentFactor)          \
	PARTICLE_PROFILE_FIELD(Index, LinearFadeInSize)        \
	PARTICLE_PROFILE_FIELD(Index, LinearFadeOutSize)

#define DEFINE_PRECOMPUTE_CONTEXT_FIELDS()                          \
	LAYOUT_FIELD(FShaderParameter, AtmosphereScale)		/* float */ \
	LAYOUT_FIELD(FShaderParameter, NumParticleProfiles) /* int */   \
	PARTICLE_PROFILE_FIELDS(0)                                      \
	PARTICLE_PROFILE_FIELDS(1)                                      \
	PARTICLE_PROFILE_FIELDS(2)                                      \
	PARTICLE_PROFILE_FIELDS(3)                                      \
	PARTICLE_PROFILE_FIELDS(4)

#define __STRINGIFY(s) #s
#define STRINGIFY(s) __STRINGIFY(s)

#define BIND_PARTICLE_PROFILE_FIELD(ProfileIndex, Name) \
	PARTICLE_PROFILE_FIELD_NAME(ProfileIndex, Name).Bind(Initializer.ParameterMap, TEXT(STRINGIFY(PARTICLE_PROFILE_FIELD_NAME(ProfileIndex, Name))));

#define BIND_PARTICLE_PROFILE_FIELDS(Index)                     \
	BIND_PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsR) \
	BIND_PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsG) \
	BIND_PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsB) \
	BIND_PARTICLE_PROFILE_FIELD(Index, PhaseFunction)           \
	BIND_PARTICLE_PROFILE_FIELD(Index, ExponentFactor)          \
	BIND_PARTICLE_PROFILE_FIELD(Index, LinearFadeInSize)        \
	BIND_PARTICLE_PROFILE_FIELD(Index, LinearFadeOutSize)

#define BIND_PRECOMPUTE_CONTEXT_FIELDS(Index)                                        \
	AtmosphereScale.Bind(Initializer.ParameterMap, TEXT("AtmosphereScale"));         \
	NumParticleProfiles.Bind(Initializer.ParameterMap, TEXT("NumParticleProfiles")); \
	BIND_PARTICLE_PROFILE_FIELDS(0)                                                  \
	BIND_PARTICLE_PROFILE_FIELDS(1)                                                  \
	BIND_PARTICLE_PROFILE_FIELDS(2)                                                  \
	BIND_PARTICLE_PROFILE_FIELDS(3)                                                  \
	BIND_PARTICLE_PROFILE_FIELDS(4)

#define SET_PARTICLE_PROFILE_FIELD(ProfileIndex, Name) \
	SetShaderValue(BatchedParameters, PARTICLE_PROFILE_FIELD_NAME(ProfileIndex, Name), Ctx.PARTICLE_PROFILE_FIELD_NAME(ProfileIndex, Name));

#define SET_PARTICLE_PROFILE_FIELDS(Index)                     \
	SET_PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsR) \
	SET_PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsG) \
	SET_PARTICLE_PROFILE_FIELD(Index, ScatteringCoefficientsB) \
	SET_PARTICLE_PROFILE_FIELD(Index, PhaseFunction)           \
	SET_PARTICLE_PROFILE_FIELD(Index, ExponentFactor)          \
	SET_PARTICLE_PROFILE_FIELD(Index, LinearFadeInSize)        \
	SET_PARTICLE_PROFILE_FIELD(Index, LinearFadeOutSize)

#define SET_PRECOMPUTE_CONTEXT_FIELDS()                                              \
	SetShaderValue(BatchedParameters, AtmosphereScale, Ctx.AtmosphereScale);         \
	SetShaderValue(BatchedParameters, NumParticleProfiles, Ctx.NumParticleProfiles); \
	SET_PARTICLE_PROFILE_FIELDS(0)                                                   \
	SET_PARTICLE_PROFILE_FIELDS(1)                                                   \
	SET_PARTICLE_PROFILE_FIELDS(2)                                                   \
	SET_PARTICLE_PROFILE_FIELDS(3)                                                   \
	SET_PARTICLE_PROFILE_FIELDS(4)

class FTransmittancePrecomputeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTransmittancePrecomputeCS, Global);

	LAYOUT_FIELD(FShaderResourceParameter, TransmittanceTextureOut); // RWBuffer<float4>
	LAYOUT_FIELD(FShaderParameter, TransmittanceTextureWidth);		 // int
	LAYOUT_FIELD(FShaderParameter, TransmittanceTextureHeight);		 // int
	LAYOUT_FIELD(FShaderParameter, NumSteps);						 // int

	DEFINE_PRECOMPUTE_CONTEXT_FIELDS()

	/** Default constructor. */
	FTransmittancePrecomputeCS() {}

	/** Initialization constructor. */
	FTransmittancePrecomputeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TransmittanceTextureOut.Bind(Initializer.ParameterMap, TEXT("TransmittanceTextureOut"));
		TransmittanceTextureWidth.Bind(Initializer.ParameterMap, TEXT("TransmittanceTextureWidth"));
		TransmittanceTextureHeight.Bind(Initializer.ParameterMap, TEXT("TransmittanceTextureHeight"));
		NumSteps.Bind(Initializer.ParameterMap, TEXT("NumSteps"));
		BIND_PRECOMPUTE_CONTEXT_FIELDS();
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters,
		FRHIUnorderedAccessView* _TransmittanceTextureOut,
		int _TransmittanceTextureWidth, int _TransmittanceTextureHeight,
		int _NumSteps,
		const FPrecomputeContext& Ctx) const
	{
		SetUAVParameter(BatchedParameters, TransmittanceTextureOut, _TransmittanceTextureOut);
		SetShaderValue(BatchedParameters, TransmittanceTextureWidth, _TransmittanceTextureWidth);
		SetShaderValue(BatchedParameters, TransmittanceTextureHeight, _TransmittanceTextureHeight);
		SetShaderValue(BatchedParameters, NumSteps, _NumSteps);
		SET_PRECOMPUTE_CONTEXT_FIELDS();
	}
};

IMPLEMENT_SHADER_TYPE(,
	FTransmittancePrecomputeCS,
	TEXT("/SweetAtmosphere/Precompute/PrecomputeTransmittance.usf"),
	TEXT("PrecomputeTransmittanceCS"),
	SF_Compute);

class FInScatteredLightPrecomputeCS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FInScatteredLightPrecomputeCS, Global);

	LAYOUT_FIELD(FShaderResourceParameter, TransmittanceTextureIn);		// Buffer<float4>
	LAYOUT_FIELD(FShaderParameter, TransmittanceTextureWidth);			// int
	LAYOUT_FIELD(FShaderParameter, TransmittanceTextureHeight);			// int
	LAYOUT_FIELD(FShaderResourceParameter, InScatteredLightTextureOut); // RWBuffer<float4>
	LAYOUT_FIELD(FShaderParameter, InScatteredLightTextureSize);		// int
	LAYOUT_FIELD(FShaderParameter, NumSteps);							// int

	DEFINE_PRECOMPUTE_CONTEXT_FIELDS()

	/** Default constructor. */
	FInScatteredLightPrecomputeCS() {}

	/** Initialization constructor. */
	FInScatteredLightPrecomputeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		TransmittanceTextureIn.Bind(Initializer.ParameterMap, TEXT("TransmittanceTextureIn"));
		TransmittanceTextureWidth.Bind(Initializer.ParameterMap, TEXT("TransmittanceTextureWidth"));
		TransmittanceTextureHeight.Bind(Initializer.ParameterMap, TEXT("TransmittanceTextureHeight"));
		InScatteredLightTextureOut.Bind(Initializer.ParameterMap, TEXT("InScatteredLightTextureOut"));
		InScatteredLightTextureSize.Bind(Initializer.ParameterMap, TEXT("InScatteredLightTextureSize"));
		NumSteps.Bind(Initializer.ParameterMap, TEXT("NumSteps"));
		BIND_PRECOMPUTE_CONTEXT_FIELDS();
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters,
		FRHIShaderResourceView* _TransmittanceTextureIn,
		int _TransmittanceTextureWidth, int _TransmittanceTextureHeight,
		FRHIUnorderedAccessView* _InScatteredLightTextureOut,
		int _InScatteredLightTextureSize,
		int _NumSteps,
		const FPrecomputeContext& Ctx) const
	{
		SetSRVParameter(BatchedParameters, TransmittanceTextureIn, _TransmittanceTextureIn);
		SetShaderValue(BatchedParameters, TransmittanceTextureWidth, _TransmittanceTextureWidth);
		SetShaderValue(BatchedParameters, TransmittanceTextureHeight, _TransmittanceTextureHeight);
		SetUAVParameter(BatchedParameters, InScatteredLightTextureOut, _InScatteredLightTextureOut);
		SetShaderValue(BatchedParameters, InScatteredLightTextureSize, _InScatteredLightTextureSize);
		SetShaderValue(BatchedParameters, NumSteps, _NumSteps);
		SET_PRECOMPUTE_CONTEXT_FIELDS();
	}
};

IMPLEMENT_SHADER_TYPE(,
	FInScatteredLightPrecomputeCS,
	TEXT("/SweetAtmosphere/Precompute/PrecomputeInScatteredLight.usf"),
	TEXT("PrecomputeInScatteredLightCS"),
	SF_Compute);

void FAtmospherePrecomputeShaderDispatcher::Dispatch(
	FPrecomputedTextureSettings TextureSettings,
	FPrecomputeContext Ctx,
	bool GenerateDebugTextures,
	TFunction<void(FAtmospherePrecomputedTextureData, FAtmospherePrecomputedDebugTextureData)> AsyncCallback)
{
	if (IsInRenderingThread())
	{
		DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(),
			TextureSettings, Ctx, GenerateDebugTextures, AsyncCallback);
	}
	else
	{
		DispatchGameThread(TextureSettings, Ctx, GenerateDebugTextures, AsyncCallback);
	}
}

void FAtmospherePrecomputeShaderDispatcher::DispatchGameThread(
	FPrecomputedTextureSettings TextureSettings,
	FPrecomputeContext GenerationSettings,
	bool GenerateDebugTextures,
	TFunction<void(FAtmospherePrecomputedTextureData, FAtmospherePrecomputedDebugTextureData)> AsyncCallback)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)
	(
		[TextureSettings, GenerationSettings, AsyncCallback, GenerateDebugTextures](FRHICommandListImmediate& RHICmdList) {
			DispatchRenderThread(RHICmdList, TextureSettings, GenerationSettings, GenerateDebugTextures, AsyncCallback);
		});
}

/**
 * Wrapper class to operate on float4 buffers instead of textures.
 * This is required since macOS Metal doesn't seem to support 3D textures
 * in a compute shader (or I'm too stupid to figure it out).
 */
struct FRHITextureData
{
	const FBufferRHIRef Buffer;
	const FIntVector Size;
	const EPixelFormat PixelFormat;
	const uint64 NumBytes;

	static FRHITextureData Create2D(
		FRHICommandList& RHICmdList,
		const int Width, const int Height,
		const EPixelFormat PixelFormat,
		const FString& Name)
	{
		return Create(RHICmdList, FIntVector(Width, Height, 0), PixelFormat, Name);
	}

	static FRHITextureData Create3D(FRHICommandList& RHICmdList,
		const FIntVector& Size,
		const EPixelFormat PixelFormat,
		const FString& Name)
	{
		check(Size.Z > 0);
		return Create(RHICmdList, Size, PixelFormat, Name);
	}

	FShaderResourceViewRHIRef CreateSRV(FRHICommandList& RHICmdList) const
	{
		return RHICmdList.CreateShaderResourceView(Buffer,
			FRHIViewDesc::CreateBufferSRV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PixelFormat)
				.SetNumElements(Size.X * Size.Y * FMath::Max(Size.Z, 1)));
	}

	FUnorderedAccessViewRHIRef CreateUAV(FRHICommandList& RHICmdList) const
	{
		return RHICmdList.CreateUnorderedAccessView(Buffer,
			FRHIViewDesc::CreateBufferUAV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PixelFormat)
				.SetNumElements(Size.X * Size.Y * FMath::Max(Size.Z, 1)));
	}

private:
	FRHITextureData(const FBufferRHIRef& Buffer, const FIntVector& Size, EPixelFormat PixelFormat, const uint32 NumBytes)
		: Buffer(Buffer), Size(Size), PixelFormat(PixelFormat), NumBytes(NumBytes) {}

	static FRHITextureData Create(FRHICommandList& RHICmdList,
		const FIntVector& Size,
		const EPixelFormat PixelFormat,
		const FString& Name)
	{
		const auto NumBytes = GPixelFormats[PixelFormat].Get3DImageSizeInBytes(Size.X, Size.Y, FMath::Max(1, Size.Z));

		FRHIResourceCreateInfo BufferCreateInfo(*Name);
		const auto Buffer = RHICmdList.CreateBuffer(NumBytes,
			EBufferUsageFlags::ShaderResource | EBufferUsageFlags::UnorderedAccess, 1,
			ERHIAccess::None,
			BufferCreateInfo);

		return FRHITextureData(Buffer, Size, PixelFormat, NumBytes);
	}
};

struct FTextureDataReadback
{
	static FTextureDataReadback* CreateAndEnqueue(
		FRHICommandList& RHICmdList,
		const FRHITextureData& Resource, const int Pass, const FString& Name)
	{
		const auto NameIncludingPass = FString::Printf(TEXT("%d %s"), Pass, *Name);
		const auto NumBytes = GPixelFormats[Resource.PixelFormat].Get3DImageSizeInBytes(
			Resource.Size.X, Resource.Size.Y, Resource.Size.Z);

		auto* Readback = new FRHIGPUBufferReadback(FName(NameIncludingPass + " Readback"));
		Readback->EnqueueCopy(RHICmdList, Resource.Buffer, NumBytes);
		return new FTextureDataReadback(NameIncludingPass, Resource.Size, Resource.PixelFormat, Readback);
	}

	const FString Name;
	FTextureData ReadTextureData;

	bool IsReady() const
	{
		return !ReadTextureData.Data.IsEmpty() || (Readback && Readback->IsReady());
	}

	FTextureData Read()
	{
		check(IsReady());
		if (!ReadTextureData.Data.IsEmpty())
		{
			return ReadTextureData;
		}

		const auto NumBytes = GPixelFormats[ReadTextureData.PixelFormat].Get3DImageSizeInBytes(
			ReadTextureData.Size.X, ReadTextureData.Size.Y, ReadTextureData.Size.Z);

		uint8* GPUData = static_cast<uint8*>(Readback->Lock(NumBytes));

		ReadTextureData.Data.SetNumUninitialized(NumBytes);
		FMemory::Memcpy(ReadTextureData.Data.GetData(), GPUData, NumBytes);

		Readback->Unlock();
		delete Readback;
		Readback = nullptr;

		return ReadTextureData;
	}

private:
	/**
	 * The underlying buffer readback.
	 */
	FRHIGPUBufferReadback* Readback;

	FTextureDataReadback(const FString& Name, const FIntVector& Size, const EPixelFormat PixelFormat, FRHIGPUBufferReadback* const Readback)
		: Name(Name), ReadTextureData(Size, PixelFormat, {}), Readback(Readback) {}
};

#define DEBUG_READBACK(Pass, Resource)                                                                     \
	if (GenerateDebugTextures)                                                                             \
	{                                                                                                      \
		DebugReadbacks.Add(FTextureDataReadback::CreateAndEnqueue(RHICmdList, Resource, Pass, #Resource)); \
	}

#define PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, Name) \
	ParticleProfile_##ProfileIndex##_##Name

#define APPLY_PARTICLE_PROFILE(Target, ProfileIndex)                                                                                                                 \
	if (Ctx.NumParticleProfiles > ProfileIndex)                                                                                                                      \
	{                                                                                                                                                                \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsR) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsR); \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsG) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsG); \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsB) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsB); \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, PhaseFunction) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, PhaseFunction);                     \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ExponentFactor) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ExponentFactor);                   \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeInSize) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeInSize);               \
		Target->PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeOutSize) = Ctx.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeOutSize);             \
	}

#define APPLY_PRECOMPUTE_CONTEXT()                             \
	PassParams->AtmosphereScale = Ctx.AtmosphereScale;         \
	PassParams->NumParticleProfiles = Ctx.NumParticleProfiles; \
	APPLY_PARTICLE_PROFILE(PassParams, 0)                      \
	APPLY_PARTICLE_PROFILE(PassParams, 1)                      \
	APPLY_PARTICLE_PROFILE(PassParams, 2)                      \
	APPLY_PARTICLE_PROFILE(PassParams, 3)                      \
	APPLY_PARTICLE_PROFILE(PassParams, 4)

DECLARE_STATS_GROUP(TEXT("Atmosphere Precompute"), STATGROUP_AtmospherePrecompute, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Atmosphere Precompute Execute"), STAT_AtmospherePrecompute_Execute, STATGROUP_AtmospherePrecompute);

void FAtmospherePrecomputeShaderDispatcher::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FPrecomputedTextureSettings TextureSettings,
	FPrecomputeContext Ctx,
	bool GenerateDebugTextures,
	TFunction<void(FAtmospherePrecomputedTextureData, FAtmospherePrecomputedDebugTextureData)> AsyncCallback)
{
	FAtmospherePrecomputeDebugTextures DebugTextures;
	TArray<FTextureDataReadback*> DebugReadbacks;

	FTextureDataReadback* TransmittanceReadback;
	FTextureDataReadback* InScatteredLightReadback;

	constexpr auto PixelFormat4 = PF_FloatRGBA;
	{
		DECLARE_GPU_STAT(AtmospherePrecompute);
		SCOPE_CYCLE_COUNTER(STAT_AtmospherePrecompute_Execute);
		SCOPED_DRAW_EVENT(RHICmdList, AtmospherePrecompute);

		// initialize all textures

		/// output textures
		const auto Transmittance = FRHITextureData::Create2D(
			RHICmdList,
			TextureSettings.TransmittanceTextureWidth, TextureSettings.TransmittanceTextureHeight,
			PixelFormat4,
			TEXT("Transmittance Texture"));

		const auto InScatteredLight = FRHITextureData::Create3D(
			RHICmdList,
			FIntVector(TextureSettings.InScatteredLightTextureSize),
			PixelFormat4,
			TEXT("In-Scattered Light Texture"));

		{
			// pass 1: transmittance
			TShaderMapRef<FTransmittancePrecomputeCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			if (!Shader.IsValid())
			{
				UE_LOG(LogShaders, Error, TEXT("Transmittance Precompute shader is not valid"));
				return;
			}

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
				FIntVector(TextureSettings.TransmittanceTextureWidth, TextureSettings.TransmittanceTextureHeight, 1),
				FComputeShaderUtils::kGolden2DGroupSize);

			SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

			SetShaderParametersLegacyCS(RHICmdList, Shader,
				Transmittance.CreateUAV(RHICmdList), Transmittance.Size.X, Transmittance.Size.Y,
				TextureSettings.TransmittanceSampleSteps, Ctx);

			RHICmdList.DispatchComputeShader(GroupCount.X, GroupCount.Y, GroupCount.Z);

			UnsetShaderUAVs(RHICmdList, Shader, Shader.GetComputeShader());

			DEBUG_READBACK(1, Transmittance)
		}

		{
			// pass 2: in-scattered light
			TShaderMapRef<FInScatteredLightPrecomputeCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			if (!Shader.IsValid())
			{
				UE_LOG(LogShaders, Error, TEXT("In-Scattered Light Precompute shader is not valid"));
				return;
			}

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
				FIntVector(TextureSettings.InScatteredLightTextureSize),
				FComputeShaderUtils::kGolden2DGroupSize);

			SetComputePipelineState(RHICmdList, Shader.GetComputeShader());

			SetShaderParametersLegacyCS(RHICmdList, Shader,
				Transmittance.CreateSRV(RHICmdList), Transmittance.Size.X, Transmittance.Size.Y,
				InScatteredLight.CreateUAV(RHICmdList), TextureSettings.InScatteredLightTextureSize,
				TextureSettings.InScatteredLightSampleSteps,
				Ctx);

			RHICmdList.DispatchComputeShader(GroupCount.X, GroupCount.Y, GroupCount.Z);

			UnsetShaderUAVs(RHICmdList, Shader, Shader.GetComputeShader());

			DEBUG_READBACK(2, InScatteredLight)
		}

		// texture readback
		TransmittanceReadback = FTextureDataReadback::CreateAndEnqueue(RHICmdList, Transmittance, 0, "Transmittance");
		InScatteredLightReadback = FTextureDataReadback::CreateAndEnqueue(RHICmdList, InScatteredLight, 0, "In-Scattered Light");
	}

	// create a lambda that schedules itself to wait without blocking the render thread
	// until buffer readbacks can be performed
	auto RunnerFunc = [TransmittanceReadback, InScatteredLightReadback, DebugReadbacks, AsyncCallback](auto&& RunnerFunc) -> void {
		bool AllReady = TransmittanceReadback->IsReady() && InScatteredLightReadback->IsReady();
		if (AllReady)
		{
			for (const auto* DebugReadback : DebugReadbacks)
			{
				if (!DebugReadback->IsReady())
				{
					AllReady = false;
					break;
				}
			}
		}

		if (AllReady)
		{
			FAtmospherePrecomputedTextureData TextureData;
			TextureData.TransmittanceTextureData = TransmittanceReadback->Read();
			TextureData.InScatteredLightTextureData = InScatteredLightReadback->Read();

			delete TransmittanceReadback;
			delete InScatteredLightReadback;

			FAtmospherePrecomputedDebugTextureData DebugTextureData;
			for (auto* DebugReadback : DebugReadbacks)
			{
				DebugTextureData.DebugTextureData.Add(
					DebugReadback->Name, DebugReadback->Read());
				delete DebugReadback;
			}

			AsyncTask(ENamedThreads::GameThread, [AsyncCallback, TextureData, DebugTextureData] {
				AsyncCallback(TextureData, DebugTextureData);
			});

			return;
		}

		AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc] {
			RunnerFunc(RunnerFunc);
		});
	};

	AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc] {
		RunnerFunc(RunnerFunc);
	});
}