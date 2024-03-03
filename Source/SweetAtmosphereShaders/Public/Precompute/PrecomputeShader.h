#pragma once

#include "CoreMinimal.h"
#include "PrecomputeShaderSettings.h"
#include "Engine/VolumeTexture.h"
#include "PrecomputeShader.generated.h"

/**
 * Precomputation output struct containing all the created textures.
 */
USTRUCT(BlueprintType)
struct SWEETATMOSPHERESHADERS_API FAtmospherePrecomputedTextures
{
	GENERATED_BODY()

	/**
	 * The transmittance texture.
	 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTexture2D> TransmittanceTexture;

	/**
	 * The in-scattered light texture.
	 */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UVolumeTexture> InScatteredLightTexture;
};

/**
 * Struct holding the intermediate textures at every step of atmosphere precomputation.
 */
USTRUCT(BlueprintType)
struct SWEETATMOSPHERESHADERS_API FAtmospherePrecomputeDebugTextures
{
	GENERATED_BODY()

	/**
	 * Debug textures for every step, by their debug name.
	 */
	UPROPERTY(BlueprintReadOnly)
	TMap<FString, TObjectPtr<UTexture>> DebugTextures;
};

struct SWEETATMOSPHERESHADERS_API FTextureData
{
	FIntVector Size;
	EPixelFormat PixelFormat;
	TArray<uint8> Data;

	FTextureData() = default;

	FTextureData(const FIntVector& Size, const EPixelFormat PixelFormat, const TArray<uint8>& Data)
		: Size(Size), PixelFormat(PixelFormat), Data(Data) {}

	bool IsVolumeTexture() const
	{
		return Size.Z > 0;
	}

	UTexture* CreateTexture() const
	{
		if (IsVolumeTexture())
		{
			return CreateTexture3D();
		}
		return CreateTexture2D();
	}

	UTexture2D* CreateTexture2D() const
	{
		check(!IsVolumeTexture());

		auto* Texture = UTexture2D::CreateTransient(Size.X, Size.Y, PixelFormat);

#if WITH_EDITORONLY_DATA
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->NeverStream = true;
		Texture->SRGB = 0;
		Texture->LODGroup = TEXTUREGROUP_Pixels2D;

		uint8* TargetData = static_cast<uint8*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		FMemory::Memcpy(TargetData, Data.GetData(), Data.Num());
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

		Texture->UpdateResource();
		return Texture;
	}

	UVolumeTexture* CreateTexture3D() const
	{
		check(IsVolumeTexture());

		auto* Texture = UVolumeTexture::CreateTransient(Size.X, Size.Y, Size.Z, PixelFormat);

#if WITH_EDITORONLY_DATA
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->NeverStream = true;
		Texture->SRGB = 0;

		uint8* TargetData = static_cast<uint8*>(Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		FMemory::Memcpy(TargetData, Data.GetData(), Data.Num());
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();

		Texture->UpdateResource();
		return Texture;
	}
};

struct SWEETATMOSPHERESHADERS_API FAtmospherePrecomputedTextureData
{
	/**
	 * The transmittance texture data.
	 */
	FTextureData TransmittanceTextureData;

	/**
	 * The in-scattered light texture data.
	 */
	FTextureData InScatteredLightTextureData;
};

struct SWEETATMOSPHERESHADERS_API FAtmospherePrecomputedDebugTextureData
{
	/**
	 * Debug texture data by texture name.
	 */
	TMap<FString, FTextureData> DebugTextureData;
};

#define PARTICLE_PROFILE_PARAMETER(ProfileIndex, Type, Name) \
	SHADER_PARAMETER(Type, ParticleProfile_##ProfileIndex##_##Name)

#define PARTICLE_PROFILE_PARAMETERS(Index)                            \
	PARTICLE_PROFILE_PARAMETER(Index, float, ScatteringCoefficientsR) \
	PARTICLE_PROFILE_PARAMETER(Index, float, ScatteringCoefficientsG) \
	PARTICLE_PROFILE_PARAMETER(Index, float, ScatteringCoefficientsB) \
	PARTICLE_PROFILE_PARAMETER(Index, int, PhaseFunction)             \
	PARTICLE_PROFILE_PARAMETER(Index, float, ExponentFactor)          \
	PARTICLE_PROFILE_PARAMETER(Index, float, LinearFadeInSize)        \
	PARTICLE_PROFILE_PARAMETER(Index, float, LinearFadeOutSize)

#define DEFINE_PRECOMPUTE_CONTEXT_PARAMETERS() \
	SHADER_PARAMETER(float, AtmosphereScale)   \
	SHADER_PARAMETER(int, NumParticleProfiles) \
	PARTICLE_PROFILE_PARAMETERS(0)             \
	PARTICLE_PROFILE_PARAMETERS(1)             \
	PARTICLE_PROFILE_PARAMETERS(2)             \
	PARTICLE_PROFILE_PARAMETERS(3)             \
	PARTICLE_PROFILE_PARAMETERS(4)

BEGIN_SHADER_PARAMETER_STRUCT(FPrecomputeContext, SWEETATMOSPHERESHADERS_API)
DEFINE_PRECOMPUTE_CONTEXT_PARAMETERS()
END_SHADER_PARAMETER_STRUCT()

/**
 * Static functions to safely dispatch the atmosphere precompute shader.
 */
class SWEETATMOSPHERESHADERS_API FAtmospherePrecomputeShaderDispatcher
{
public:
	static void Dispatch(
		FPrecomputedTextureSettings TextureSettings,
		FPrecomputeContext Ctx,
		bool GenerateDebugTextures,
		TFunction<void(FAtmospherePrecomputedTextureData, FAtmospherePrecomputedDebugTextureData)> AsyncCallback);

private:
	static void DispatchGameThread(
		FPrecomputedTextureSettings TextureSettings,
		FPrecomputeContext GenerationSettings,
		bool GenerateDebugTextures,
		TFunction<void(FAtmospherePrecomputedTextureData, FAtmospherePrecomputedDebugTextureData)> AsyncCallback);

	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FPrecomputedTextureSettings TextureSettings,
		FPrecomputeContext Ctx,
		bool GenerateDebugTextures,
		TFunction<void(FAtmospherePrecomputedTextureData, FAtmospherePrecomputedDebugTextureData)> AsyncCallback);
};
