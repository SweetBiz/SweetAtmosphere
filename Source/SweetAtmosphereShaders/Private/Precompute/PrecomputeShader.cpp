#include "Precompute/PrecomputeShader.h"

#include "RHIGPUReadback.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Engine/VolumeTexture.h"

class FTransmittancePrecomputeCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FTransmittancePrecomputeCS, Global);
	SHADER_USE_PARAMETER_STRUCT(FTransmittancePrecomputeCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FPrecomputeContext, Ctx)
	SHADER_PARAMETER(int, NumSteps)
	// texture bindings
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float>, TransmittanceTextureOut)
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
	SHADER_PARAMETER_STRUCT_INCLUDE(FPrecomputeContext, Ctx)
	SHADER_PARAMETER(int, NumSteps)
	// texture bindings
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, TransmittanceTextureIn)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D<float>, InScatteredLightTextureOut)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_SHADER_TYPE(,
	FInScatteredLightPrecomputeCS,
	TEXT("/SweetAtmosphere/Precompute/PrecomputeInScatteredLight.usf"),
	TEXT("PrecomputeInScatteredLightCS"),
	SF_Compute);

void FAtmospherePrecomputeShaderDispatcher::Dispatch(
	FPrecomputedTextureSettings TextureSettings,
	FPrecomputeContext AtmosphereGenerationSettings,
	TFunction<void(FAtmospherePrecomputedTextures)> AsyncCallback,
	FAtmospherePrecomputeDebugTextures* DebugTexturesOut)
{
	if (IsInRenderingThread())
	{
		DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(),
			TextureSettings, AtmosphereGenerationSettings, AsyncCallback, DebugTexturesOut);
	}
	else
	{
		DispatchGameThread(TextureSettings, AtmosphereGenerationSettings, AsyncCallback, DebugTexturesOut);
	}
}

void FAtmospherePrecomputeShaderDispatcher::DispatchGameThread(
	FPrecomputedTextureSettings TextureSettings,
	FPrecomputeContext GenerationSettings,
	TFunction<void(FAtmospherePrecomputedTextures)> AsyncCallback,
	FAtmospherePrecomputeDebugTextures* DebugTexturesOut)
{
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)
	(
		[TextureSettings, GenerationSettings, AsyncCallback, DebugTexturesOut](FRHICommandListImmediate& RHICmdList) {
			DispatchRenderThread(RHICmdList, TextureSettings, GenerationSettings, AsyncCallback, DebugTexturesOut);
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
		Res.RDG = GraphBuilder.CreateTexture(
			FRDGTextureDesc::Create3D(Size, PixelFormat, FClearValueBinding::BlackMaxAlpha, TexCreate_UAV | TexCreate_ShaderResource),
			*Name);
		Res.UAV = GraphBuilder.CreateUAV(Res.RDG);
		Res.SRV = GraphBuilder.CreateSRV(Res.RDG);

		Res.Size = Size;
		Res.IsVolume = true;

		return Res;
	}
};

struct FTextureReadback
{
	const FRDGTextureResource& Resource;
	const FString Name;
	TUniquePtr<FRHIGPUTextureReadback> Readback;

	static FTextureReadback* Create(FRDGBuilder& GraphBuilder, const FRDGTextureResource& Resource, const int Pass, const FString& Name)
	{
		const auto NameIncludingPass = FString::Printf(TEXT("%d %s"), Pass, *Name);
		auto* Readback = new FRHIGPUTextureReadback(FName(NameIncludingPass + " Readback"));
		AddEnqueueCopyPass(GraphBuilder, Readback, Resource.RDG);
		return new FTextureReadback(Resource, NameIncludingPass, Readback);
	}

	UTexture* ReadToTexture()
	{
		UTexture* Texture;
		if (Resource.IsVolume)
		{
			Texture = CreateAndReadToTexture<UVolumeTexture>(Readback.Get(), Resource);
		}
		else
		{
			Texture = CreateAndReadToTexture<UTexture2D>(Readback.Get(), Resource);
		}

		Readback = nullptr;

		return Texture;
	}

	template <typename TextureType>
	TextureType* ReadToTexture()
	{
		if constexpr (std::is_same_v<TextureType, UTexture2D>)
		{
			check(!Resource.IsVolume) return static_cast<UTexture2D*>(ReadToTexture());
		}
		else if constexpr (std::is_same_v<TextureType, UVolumeTexture>)
		{
			check(Resource.IsVolume) return static_cast<UVolumeTexture*>(ReadToTexture());
		}
		return nullptr;
	}

private:
	FTextureReadback(const FRDGTextureResource& Resource, const FString& Name, FRHIGPUTextureReadback* const Readback)
		: Resource(Resource), Name(Name), Readback(Readback) {}

	template <typename TextureType>
	static UTexture* CreateAndReadToTexture(FRHIGPUTextureReadback* Readback, const FRDGTextureResource& Resource)
	{
		TextureType* Texture = nullptr;
		if constexpr (std::is_same_v<TextureType, UTexture2D>)
		{
			Texture = TextureType::CreateTransient(Resource.Size.X, Resource.Size.Y, Resource.RDG->Desc.Format);
		}
		else if constexpr (std::is_same_v<TextureType, UVolumeTexture>)
		{
			Texture = TextureType::CreateTransient(Resource.Size.X, Resource.Size.Y, Resource.Size.Z, Resource.RDG->Desc.Format);
		}

#if WITH_EDITORONLY_DATA
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->NeverStream = true;
		Texture->LODGroup = TEXTUREGROUP_Pixels2D;

		FTexture2DMipMap& Mip0 = Texture->GetPlatformData()->Mips[0];
		const uint64 DataSize = Readback->GetGPUSizeBytes();
		int32 _;
		const void* GpuBuffer = Readback->Lock(_, &_);
		void* CpuBuffer = Mip0.BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(CpuBuffer, GpuBuffer, DataSize);
		Mip0.BulkData.Unlock();
		Readback->Unlock();

		return Texture;
	}
};

#define DEBUG_READBACK(Pass, Resource)                                                             \
	if (DebugTexturesOut)                                                                          \
	{                                                                                              \
		DebugReadbacks.Emplace(FTextureReadback::Create(GraphBuilder, Resource, Pass, #Resource)); \
	}

DECLARE_STATS_GROUP(TEXT("Atmosphere Precompute"), STATGROUP_AtmospherePrecompute, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("Atmosphere Precompute Execute"), STAT_AtmospherePrecompute_Execute, STATGROUP_AtmospherePrecompute);

void FAtmospherePrecomputeShaderDispatcher::DispatchRenderThread(
	FRHICommandListImmediate& RHICmdList,
	FPrecomputedTextureSettings TextureSettings,
	FPrecomputeContext Ctx,
	TFunction<void(FAtmospherePrecomputedTextures)> AsyncCallback,
	FAtmospherePrecomputeDebugTextures* DebugTexturesOut)
{
	FRDGBuilder GraphBuilder(RHICmdList);

	TUniquePtr<FTextureReadback> TransmittanceReadback;
	TUniquePtr<FTextureReadback> InScatteredLightReadback;
	TArray<TUniquePtr<FTextureReadback>> DebugReadbacks;

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

			auto* PassParams = GraphBuilder.AllocParameters<FTransmittancePrecomputeCS::FParameters>();
			PassParams->Ctx = Ctx;
			PassParams->NumSteps = TextureSettings.TransmittanceSampleSteps;
			PassParams->TransmittanceTextureOut = Transmittance.UAV;

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
			PassParams->Ctx = Ctx;
			PassParams->NumSteps = TextureSettings.InScatteredLightSampleSteps;
			PassParams->TransmittanceTextureIn = Transmittance.SRV;
			PassParams->InScatteredLightTextureOut = InScatteredLight.UAV;

			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(
				FIntVector(TextureSettings.InScatteredLightTextureSize),
				FComputeShaderUtils::kGolden2DGroupSize);

			FComputeShaderUtils::AddPass(GraphBuilder,
				RDG_EVENT_NAME("Precompute In-Scattered Light"),
				Shader, PassParams, GroupCount);

			DEBUG_READBACK(2, InScatteredLight)
		}

		// texture readback
		TransmittanceReadback.Reset(FTextureReadback::Create(GraphBuilder, Transmittance, 0, "Transmittance"));
		InScatteredLightReadback.Reset(FTextureReadback::Create(GraphBuilder, InScatteredLight, 0, "In-Scattered Light"));
	}

	// execute the graph in blocking fashion
	GraphBuilder.Execute();

	FAtmospherePrecomputedTextures Textures;
	Textures.TransmittanceTexture = TransmittanceReadback->ReadToTexture<UTexture2D>();
	Textures.InScatteredLightTexture = InScatteredLightReadback->ReadToTexture<UVolumeTexture>();

	if (DebugTexturesOut)
	{
		for (const auto& DebugReadback : DebugReadbacks)
		{
			DebugTexturesOut->DebugTextures.Add(
				DebugReadback->Name,
				DebugReadback->ReadToTexture());
		}
	}

	// publish results on game thread
	AsyncTask(ENamedThreads::GameThread, [=] {
		Textures.TransmittanceTexture->UpdateResource();
		Textures.InScatteredLightTexture->UpdateResource();

		if (DebugTexturesOut)
		{
			for (const auto& Entry : DebugTexturesOut->DebugTextures)
			{
				Entry.Value->UpdateResource();
			}
		}

		AsyncCallback(Textures);
	});
}