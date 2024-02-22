#pragma once

#include "AtmosphereSettings.h"
#include "Engine/VolumeTexture.h"
#include "SweetAtmosphereShaders/Public/Precompute/PrecomputeShader.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "AtmospherePrecompute.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAtmospherePrecompute_AsyncExecutionCompleted,
	FAtmospherePrecomputedTextures, Textures,
	FAtmospherePrecomputeDebugTextures, DebugTextures);

/**
 * Async Blueprint action running the atmosphere precomputation shader.
 */
UCLASS(BlueprintType)
class SWEETATMOSPHERE_API UAtmospherePrecomputeAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	/**
	 * Precomputes atmospheric scattering textures for
	 * later use in a material graph or shader.
	 *
	 * @param WorldContextObject World Context Object.
	 * @param TextureSettings Texture settings.
	 * @param AtmosphereSettings Atmosphere settings.
	 * @param GenerateDebugTextures Whether to read intermittent textures into FAtmospherePrecomputeDebugTextures.
	 * @return Async Execution Task.
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject"), Category = "Atmospheric Scattering")
	static UAtmospherePrecomputeAction* PrecomputeAtmosphericScattering(
		const UObject* WorldContextObject,
		const FPrecomputedTextureSettings& TextureSettings,
		const FAtmosphereSettings& AtmosphereSettings,
		bool GenerateDebugTextures);

	/**
	 * Called with the result when shader execution is completed.
	 */
	UPROPERTY(BlueprintAssignable)
	FOnAtmospherePrecompute_AsyncExecutionCompleted OnComplete;

	/**
	 * Initializes this action. Must be called before Activate().
	 * @param TextureSettings Texture settings.
	 * @param PrecomputeContext Atmosphere generation settings.
	 * @param GenerateDebugTextures Whether to read intermittent textures into FAtmospherePrecomputeDebugTextures.
	 */
	void Init(const FPrecomputedTextureSettings& TextureSettings, const FPrecomputeContext& PrecomputeContext, bool GenerateDebugTextures);

	virtual void Activate() override;

private:
	FPrecomputedTextureSettings TextureSettings;
	FPrecomputeContext AtmosphereGenerationSettings;

	bool GenerateDebugTextures;
	FAtmospherePrecomputeDebugTextures DebugTextures;
};

/**
 * Helper functions to precompute atmospheric scattering from blueprints.
 */
UCLASS()
class SWEETATMOSPHERE_API UAtmospherePrecomputeHelper : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Applies the atmosphere settings to the given material instance for rendering.
	 *
	 * @param MaterialInstance The target material instance.
	 * @param Atmosphere The atmosphere settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static void BindAtmosphereSettings(UMaterialInstanceDynamic* MaterialInstance, const FAtmosphereSettings& Atmosphere)
	{
		MaterialInstance->SetScalarParameterValue("AtmosphereScale", Atmosphere.AtmosphereScale);
		MaterialInstance->SetScalarParameterValue("SunIntensity", Atmosphere.SunIntensity);
		MaterialInstance->SetScalarParameterValue("HueShift", Atmosphere.HueShift);
	}

	/**
	 * Applies the precomputed atmosphere textures to the given material instance for rendering.
	 *
	 * @param MaterialInstance The target material instance.
	 * @param Textures The textures to bind.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static void BindPrecomputedTextures(UMaterialInstanceDynamic* MaterialInstance, const FAtmospherePrecomputedTextures& Textures)
	{
		MaterialInstance->SetTextureParameterValue("TransmittanceTexture", Textures.TransmittanceTexture);
		MaterialInstance->SetTextureParameterValue("InScatteredLightTexture", Textures.InScatteredLightTexture);
	}
};
