#pragma once

#include "AtmosphereSettings.h"
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
	 * @param TextureSettings Texture settings.
	 * @param AtmosphereSettings Atmosphere settings.
	 * @param Callback The callback to run on the game thread when precomputation has finished.
	 */
	static void PrecomputeAtmosphericScattering(
		FPrecomputedTextureSettings TextureSettings,
		FAtmosphereSettings AtmosphereSettings,
		bool GenerateDebugTextures, TFunction<void(FAtmospherePrecomputedTextures, FAtmospherePrecomputeDebugTextures)> Callback);

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
};

/**
 * Helper functions to apply the precomputed atmosphere to materials.
 */
UCLASS()
class SWEETATMOSPHERE_API UAtmosphereMaterialHelper : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Creates an instance of the atmosphere material using to the given parameters.
	 *
	 * @param ParentMaterial The parent material to create an instance from.
	 * @param AtmosphereSettings The atmosphere settings.
	 * @param PrecomputedTextures The precomputed textures using these atmosphere settings.
	 * @return The configured material instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static UMaterialInstanceDynamic* CreateAtmosphereMaterial(UMaterialInterface* ParentMaterial, const FAtmosphereSettings& AtmosphereSettings, const FAtmospherePrecomputedTextures& PrecomputedTextures);

	/**
	 * Applies the atmosphere settings to the given material instance for rendering.
	 *
	 * @param MaterialInstance The target material instance.
	 * @param Atmosphere The atmosphere settings.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static void BindAtmosphereSettings(UMaterialInstanceDynamic* MaterialInstance, const FAtmosphereSettings& Atmosphere);

	/**
	 * Applies the precomputed atmosphere textures to the given material instance for rendering.
	 *
	 * @param MaterialInstance The target material instance.
	 * @param PrecomputedTextures The precomputed textures to bind.
	 */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	static void BindPrecomputedTextures(UMaterialInstanceDynamic* MaterialInstance, const FAtmospherePrecomputedTextures& PrecomputedTextures);
};
