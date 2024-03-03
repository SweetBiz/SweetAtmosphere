#include "Precompute/PrecomputeShader.h"

#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

#define SUPPORTS_3D_TEXTURES 0 // PLATFORM_WINDOWS

class FTransmittancePrecomputeCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FTransmittancePrecomputeCS, Global);
	SHADER_USE_PARAMETER_STRUCT(FTransmittancePrecomputeCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	// texture bindings
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, TransmittanceTextureOut)
	DEFINE_PRECOMPUTE_CONTEXT_PARAMETERS()
	SHADER_PARAMETER(int, NumSteps)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_SHADER_TYPE(,
	FTransmittancePrecomputeCS,
	TEXT("/SweetAtmosphere/Precompute/PrecomputeTransmittance.usf"),
	TEXT("PrecomputeTransmittanceCS"),
	SF_Compute);

class FInScatteredLightPrecomputeCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FInScatteredLightPrecomputeCS, Global);
	SHADER_USE_PARAMETER_STRUCT(FInScatteredLightPrecomputeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	//  texture bindings
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, TransmittanceTextureIn)
#if SUPPORTS_3D_TEXTURES
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, InScatteredLightTextureOut)
#else
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, InScatteredLightTextureOut)
	SHADER_PARAMETER(int, InScatteredLightTextureSize)
#endif
	SHADER_PARAMETER(int, NumSteps)
	//DEFINE_PRECOMPUTE_CONTEXT_PARAMETERS()å
	END_SHADER_PARAMETER_STRUCT()
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

struct FRDGTextureResource
{
	FRDGTextureRef RDG;
	FRDGTextureUAVRef UAV;
	FRDGTextureSRVRef SRV;
	FIntVector Size;
	bool IsVolume;

private:
	FRDGTextureResource() = default;

public:
	static FRDGTextureResource Create2D(FRDGBuilder& GraphBuilder, const int Width, const int Height, const EPixelFormat PixelFormat, const FString& Name)
	{
		FRDGTextureResource Res;
		Res.RDG = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(FIntPoint(Width, Height), PixelFormat, FClearValueBinding::BlackMaxAlpha, TexCreate_UAV | TexCreate_ShaderResource),
			*Name);
		Res.UAV = GraphBuilder.CreateUAV(Res.RDG);
		Res.SRV = GraphBuilder.CreateSRV(Res.RDG);

		Res.Size = FIntVector(Width, Height, 0);
		Res.IsVolume = false;

		return Res;
	}

	static FRDGTextureResource Create3D(FRDGBuilder& GraphBuilder, const FIntVector& Size, const EPixelFormat PixelFormat, const FString& Name)
	{
		check(Size.Z > 0);

		FRDGTextureResource Res;
#if SUPPORTS_3D_TEXTURES
		Res.RDG = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(
				Size, PixelFormat,
				FClearValueBinding::BlackMaxAlpha,
				TexCreate_UAV | TexCreate_ShaderResource),
			*Name);
#else
		Res.RDG = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create2D(
				FIntPoint(Size.X, Size.Y * Size.Z),
				PixelFormat,
				FClearValueBinding::BlackMaxAlpha,
				TexCreate_UAV | TexCreate_ShaderResource),
			*Name);
#endif
		Res.UAV = GraphBuilder.CreateUAV(Res.RDG);
		Res.SRV = GraphBuilder.CreateSRV(Res.RDG);

		Res.Size = Size;
		Res.IsVolume = true;

		return Res;
	}
};

struct FTextureReadback
{
	const FIntVector Size;
	const FString Name;
	FRHIGPUTextureReadback* Readback;

	static FTextureReadback* Create(FRDGBuilder& GraphBuilder, const FRDGTextureResource& Resource, const int Pass, const FString& Name)
	{
		const auto NameIncludingPass = FString::Printf(TEXT("%d %s"), Pass, *Name);
		auto* Readback = new FRHIGPUTextureReadback(FName(NameIncludingPass + " Readback"));
		AddEnqueueCopyPass(GraphBuilder, Readback, Resource.RDG);
		return new FTextureReadback(Resource.Size, NameIncludingPass, Readback);
	}

	TArray<uint8> ReadToByteArray()
	{
		check(Readback->IsReady());

		int32 Width, Height;
		uint8* GPUData = static_cast<uint8*>(Readback->Lock(Width, &Height));

		const auto NumBytes = Size.X * Size.Y * FMath::Max(Size.Z, 1) * 2 * 4; // floats are packed into 16 bits
		TArray<uint8> Data;
		Data.SetNumUninitialized(NumBytes);

		FMemory::Memcpy(Data.GetData(), GPUData, NumBytes);

		Readback->Unlock();
		delete Readback;
		Readback = nullptr;

		return Data;
	}

private:
	FTextureReadback(const FIntVector& Size, const FString& Name, FRHIGPUTextureReadback* const Readback)
		: Size(Size), Name(Name), Readback(Readback) {}
};

#define DEBUG_READBACK(Pass, Resource)                                                         \
	if (GenerateDebugTextures)                                                                 \
	{                                                                                          \
		DebugReadbacks.Add(FTextureReadback::Create(GraphBuilder, Resource, Pass, #Resource)); \
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

	FRDGBuilder GraphBuilder(RHICmdList);

	TArray<FTextureReadback*> DebugReadbacks;

	FTextureReadback* TransmittanceReadback;
	FTextureReadback* InScatteredLightReadback;

#if SUPPORTS_3D_TEXTURES
	const int InScatteredLightTextureSize = TextureSettings.InScatteredLightTextureSize;
#else
	// hard limit on macOS for texture height
	const int InScatteredLightTextureSize = FMath::Min(128, TextureSettings.InScatteredLightTextureSize);
#endif

	constexpr auto PixelFormat4 = PF_FloatRGBA;
	{
		DECLARE_GPU_STAT(AtmospherePrecompute);
		SCOPE_CYCLE_COUNTER(STAT_AtmospherePrecompute_Execute);
		RDG_EVENT_SCOPE(GraphBuilder, "Atmosphere Precompute Graph");
		RDG_GPU_STAT_SCOPE(GraphBuilder, AtmospherePrecompute);

		// initialize all textures

		/// output textures
		const auto Transmittance = FRDGTextureResource::Create2D(
			GraphBuilder,
			TextureSettings.TransmittanceTextureWidth, TextureSettings.TransmittanceTextureHeight,
			PixelFormat4,
			TEXT("Transmittance Texture"));

		const auto InScatteredLight = FRDGTextureResource::Create3D(
			GraphBuilder,
			FIntVector(InScatteredLightTextureSize),
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

			auto* PassParams = GraphBuilder.AllocParameters<FTransmittancePrecomputeCS::FParameters>();
			APPLY_PRECOMPUTE_CONTEXT()
			PassParams->NumSteps = TextureSettings.TransmittanceSampleSteps;
			PassParams->TransmittanceTextureOut = GraphBuilder.CreateUAV(Transmittance.RDG);

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
				FIntVector(TextureSettings.TransmittanceTextureWidth, TextureSettings.TransmittanceTextureHeight, 1),
				FComputeShaderUtils::kGolden2DGroupSize);

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("Precompute Transmittance"),
				Shader, PassParams, GroupCount);

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

			auto* PassParams = GraphBuilder.AllocParameters<FInScatteredLightPrecomputeCS::FParameters>();
			// APPLY_PRECOMPUTE_CONTEXT()
			PassParams->NumSteps = TextureSettings.InScatteredLightSampleSteps;
			PassParams->TransmittanceTextureIn = GraphBuilder.CreateSRV(Transmittance.RDG);
#if SUPPORTS_3D_TEXTURES
			PassParams->InScatteredLightTextureOut = GraphBuilder.CreateUAV(InScatteredLight.RDG);
#else
			PassParams->InScatteredLightTextureSize = InScatteredLightTextureSize;
			PassParams->InScatteredLightTextureOut = GraphBuilder.CreateUAV(InScatteredLight.RDG);
#endif

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
				FIntVector(InScatteredLightTextureSize),
				FComputeShaderUtils::kGolden2DGroupSize);

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("Precompute In-Scattered Light"),
				Shader, PassParams, GroupCount);

			DEBUG_READBACK(2, InScatteredLight)
		}

		// texture readback
		TransmittanceReadback = FTextureReadback::Create(GraphBuilder, Transmittance, 0, "Transmittance");
		InScatteredLightReadback = FTextureReadback::Create(GraphBuilder, InScatteredLight, 0, "In-Scattered Light");
	}

	auto RunnerFunc = [TransmittanceReadback, InScatteredLightReadback, DebugReadbacks, AsyncCallback](auto&& RunnerFunc) -> void {
		bool AllReady = TransmittanceReadback->Readback->IsReady() && InScatteredLightReadback->Readback->IsReady();
		if (AllReady)
		{
			for (const auto* DebugReadback : DebugReadbacks)
			{
				if (!DebugReadback->Readback->IsReady())
				{
					AllReady = false;
					break;
				}
			}
		}

		if (AllReady)
		{
			FAtmospherePrecomputedTextureData TextureData;
			TextureData.TransmittanceTextureData = TransmittanceReadback->ReadToByteArray();
			TextureData.InScatteredLightTextureData = InScatteredLightReadback->ReadToByteArray();

			FAtmospherePrecomputedDebugTextureData DebugTextureData;
			for (auto* DebugReadback : DebugReadbacks)
			{
				DebugTextureData.DebugTextureData.Add(
					DebugReadback->Name,
					TTuple<TArray<uint8>, FIntVector>(DebugReadback->ReadToByteArray(), DebugReadback->Size));
				delete DebugReadback;
			}

			AsyncTask(ENamedThreads::GameThread, [AsyncCallback, TextureData, DebugTextureData] {
				AsyncCallback(TextureData, DebugTextureData);
			});

			delete TransmittanceReadback;
			delete InScatteredLightReadback;

			return;
		}

		AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc] {
			RunnerFunc(RunnerFunc);
		});
	};

	AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc] {
		RunnerFunc(RunnerFunc);
	});

	// execute the graph
	GraphBuilder.Execute();
}