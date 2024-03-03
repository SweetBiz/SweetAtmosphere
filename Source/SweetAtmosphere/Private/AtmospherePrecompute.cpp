#include "AtmospherePrecompute.h"

#include "Interfaces/IPluginManager.h"
#include "Engine/VolumeTexture.h"

#define PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, Name) \
	ParticleProfile_##ProfileIndex##_##Name

#define APPLY_PARTICLE_PROFILE(Target, ProfileIndex)                                                                                                          \
	if (AtmosphereSettings.ParticleProfiles.Num() > ProfileIndex)                                                                                             \
	{                                                                                                                                                         \
		const auto& Profile = AtmosphereSettings.ParticleProfiles[ProfileIndex];                                                                              \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsR) = Profile.ScatteringCoefficients.X;                                     \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsG) = Profile.ScatteringCoefficients.Y;                                     \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ScatteringCoefficientsB) = Profile.ScatteringCoefficients.Z;                                     \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, PhaseFunction) = static_cast<std::underlying_type<EPhaseFunction>::type>(Profile.PhaseFunction); \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, ExponentFactor) = Profile.ExponentFactor;                                                        \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeInSize) = Profile.LinearFadeInSize;                                                    \
		Target.PARTICLE_PROFILE_PARAMETER_NAME(ProfileIndex, LinearFadeOutSize) = Profile.LinearFadeOutSize;                                                  \
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

inline UTexture2D* CreateTexture2D(const TArray<uint8>& Data, int Width, int Height)
{
	auto* Texture = UTexture2D::CreateTransient(Width, Height, PF_FloatRGBA);

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

inline UVolumeTexture* CreateTexture3D(const TArray<uint8>& Data, int Width, int Height, int Depth)
{
	auto* Texture = UVolumeTexture::CreateTransient(Width, Height, Depth, PF_FloatRGBA);

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

#define CREATE_TEXTURES_FROM_DATA()                                                               \
	FAtmospherePrecomputedTextures Textures;                                                      \
	Textures.TransmittanceTexture = TextureData.TransmittanceTextureData.CreateTexture2D();       \
	Textures.InScatteredLightTexture = TextureData.InScatteredLightTextureData.CreateTexture3D(); \
	FAtmospherePrecomputeDebugTextures DebugTextures;                                             \
	for (const auto& [Name, Data] : DebugTextureData.DebugTextureData)                            \
	{                                                                                             \
		DebugTextures.DebugTextures.Add(Name, Data.CreateTexture());                              \
	}

void UAtmospherePrecomputeAction::PrecomputeAtmosphericScattering(
	FPrecomputedTextureSettings TextureSettings,
	FAtmosphereSettings AtmosphereSettings,
	bool GenerateDebugTextures,
	TFunction<void(FAtmospherePrecomputedTextures, FAtmospherePrecomputeDebugTextures)> Callback)
{
	const auto Ctx = CreatePrecomputeContext(AtmosphereSettings);
	FAtmospherePrecomputeShaderDispatcher::Dispatch(TextureSettings, Ctx, GenerateDebugTextures, [Callback, TextureSettings](FAtmospherePrecomputedTextureData TextureData, FAtmospherePrecomputedDebugTextureData DebugTextureData) {
		CREATE_TEXTURES_FROM_DATA()
		Callback(Textures, DebugTextures);
	});
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
		GenerateDebugTextures,
		[this](FAtmospherePrecomputedTextureData TextureData, FAtmospherePrecomputedDebugTextureData DebugTextureData) {
			CREATE_TEXTURES_FROM_DATA()
			OnComplete.Broadcast(Textures, DebugTextures);
			SetReadyToDestroy();
		});
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
