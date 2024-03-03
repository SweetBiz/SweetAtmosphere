#pragma once

#include "CoreMinimal.h"
#include "PrecomputeShaderSettings.h"
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

struct SWEETATMOSPHERESHADERS_API FAtmospherePrecomputedTextureData
{
	/**
	 * The transmittance texture data.
	 */
	TArray<uint8> TransmittanceTextureData;

	/**
	 * The in-scattered light texture data.
	 */
	TArray<uint8> InScatteredLightTextureData;
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

struct SWEETATMOSPHERESHADERS_API FAtmospherePrecomputedDebugTextureData
{
	/**
	 * Mapping of debug texture name to its data and size.
	 */
	TMap<FString, TPair<TArray<uint8>, FIntVector>> DebugTextureData;
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
