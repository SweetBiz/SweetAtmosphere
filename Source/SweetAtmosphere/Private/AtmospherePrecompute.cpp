#include "AtmospherePrecompute.h"

#include "Interfaces/IPluginManager.h"
#include "Engine/VolumeTexture.h"

#define PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, Name) \
	ParticleProfile_##ProfileIndex##_##Name

#define APPLY_PARTICLE_PROFILE(Target, ProfileIndex)                                                                              \
	if (AtmosphereSettings.ParticleProfiles.Num() > ProfileIndex)                                                                 \
	{                                                                                                                             \
		const auto& Profile = AtmosphereSettings.ParticleProfiles[ProfileIndex];                                                  \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficients) = FVector3f(Profile.ScatteringCoefficients); \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ExponentFactor) = Profile.ExponentFactor;                            \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeInSize) = Profile.LinearFadeInSize;                        \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeOutSize) = Profile.LinearFadeOutSize;                      \
	}

FPrecomputeContext CreatePrecomputeContext(
	const FAtmosphereSettings& AtmosphereSettings)
{
	FPrecomputeContext Ctx;
	Ctx.AtmosphereScale = AtmosphereSettings.AtmosphereScale;

	check(AtmosphereSettings.ParticleProfiles.Num() <= 5);
	Ctx.NumParticleProfiles = AtmosphereSettings.ParticleProfiles.Num();
	APPLY_PARTICLE_PROFILE(Ctx, 0)
	APPLY_PARTICLE_PROFILE(Ctx, 1)
	APPLY_PARTICLE_PROFILE(Ctx, 2)
	APPLY_PARTICLE_PROFILE(Ctx, 3)
	APPLY_PARTICLE_PROFILE(Ctx, 4)

	return Ctx;
}

void UAtmospherePrecomputeAction::PrecomputeAtmosphericScattering(
	const FPrecomputedTextureSettings& TextureSettings,
	const FAtmosphereSettings& AtmosphereSettings,
	const TFunction<void(FAtmospherePrecomputedTextures Textures)>& Callback)
{
	const auto Ctx = CreatePrecomputeContext(AtmosphereSettings);
	FAtmospherePrecomputeShaderDispatcher::Dispatch(TextureSettings, Ctx, Callback);
}

UAtmospherePrecomputeAction* UAtmospherePrecomputeAction::PrecomputeAtmosphericScattering(
	const UObject* WorldContextObject,
	const FPrecomputedTextureSettings& TextureSettings,
	const FAtmosphereSettings& AtmosphereSettings,
	const bool GenerateDebugTextures)
{
	const auto Ctx = CreatePrecomputeContext(AtmosphereSettings);

	auto* Action = NewObject<UAtmospherePrecomputeAction>();
	Action->Init(TextureSettings, Ctx, GenerateDebugTextures);
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UAtmospherePrecomputeAction::Init(
	const FPrecomputedTextureSettings& _TextureSettings,
	const FPrecomputeContext& _AtmosphereGenerationSettings,
	const bool _GenerateDebugTextures)
{
	TextureSettings = _TextureSettings;
	AtmosphereGenerationSettings = _AtmosphereGenerationSettings;
	GenerateDebugTextures = _GenerateDebugTextures;
}

void UAtmospherePrecomputeAction::Activate()
{
	// Dispatch the compute shader and call the event when it completes
	FAtmospherePrecomputeShaderDispatcher::Dispatch(
		TextureSettings,
		AtmosphereGenerationSettings,
		[this](const auto& Result) {
			OnComplete.Broadcast(Result, DebugTextures);
			SetReadyToDestroy();
		},
		GenerateDebugTextures ? &DebugTextures : nullptr);
}

UMaterialInstanceDynamic* UAtmosphereMaterialHelper::CreateAtmosphereMaterial(
	UMaterialInterface* ParentMaterial,
	const FAtmosphereSettings& AtmosphereSettings,
	const FAtmospherePrecomputedTextures& PrecomputedTextures)
{
	auto* Instance = UMaterialInstanceDynamic::Create(ParentMaterial, nullptr);

	BindAtmosphereSettings(Instance, AtmosphereSettings);
	BindPrecomputedTextures(Instance, PrecomputedTextures);

	return Instance;
}

void UAtmosphereMaterialHelper::BindAtmosphereSettings(UMaterialInstanceDynamic* MaterialInstance, const FAtmosphereSettings& Atmosphere)
{
	MaterialInstance->SetScalarParameterValue("AtmosphereScale", Atmosphere.AtmosphereScale);
	MaterialInstance->SetScalarParameterValue("SunIntensity", Atmosphere.SunIntensity);
	MaterialInstance->SetScalarParameterValue("HueShift", Atmosphere.HueShift);
}

void UAtmosphereMaterialHelper::BindPrecomputedTextures(UMaterialInstanceDynamic* MaterialInstance, const FAtmospherePrecomputedTextures& PrecomputedTextures)
{
	MaterialInstance->SetTextureParameterValue("TransmittanceTexture", PrecomputedTextures.TransmittanceTexture);
	MaterialInstance->SetTextureParameterValue("InScatteredLightTexture", PrecomputedTextures.InScatteredLightTexture);
}
