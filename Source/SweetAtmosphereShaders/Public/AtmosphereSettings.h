#pragma once

#include "CoreMinimal.h"
#include "AtmosphereSettings.generated.h"

UENUM(BlueprintType)
enum class EPhaseFunction : uint8
{
	None,
	Rayleigh
};

USTRUCT(BlueprintType)
struct SWEETATMOSPHERESHADERS_API FParticleProfile
{
	GENERATED_BODY()

	/**
	 * The phase function to apply for this particle profile.
	 */
	UPROPERTY(BlueprintReadWrite)
	EPhaseFunction PhaseFunction = EPhaseFunction::None;

	/**
	 * The scattering coefficients for this particle type at maximum density.
	 */
	UPROPERTY(BlueprintReadWrite)
	FVector ScatteringCoefficients;

	/**
	 * The factor f in the density formula exp(-h * f)
	 */
	UPROPERTY(BlueprintReadWrite)
	float ExponentFactor;

	/**
	 * The part of the atmosphere over which density should fade in.
	 */
	UPROPERTY(BlueprintReadWrite)
	float LinearFadeInSize;

	/**
	 * The part of the atmosphere over which density should fade out.
	 */
	UPROPERTY(BlueprintReadWrite)
	float LinearFadeOutSize;
};

/**
 * Settings used to configure an atmosphere.
 * These are passed to the material by their field name.
 */
USTRUCT(BlueprintType)
struct SWEETATMOSPHERESHADERS_API FAtmosphereSettings
{
	GENERATED_BODY()

	/**
	 * The atmosphere's size relative to the planet radius.
	 * A value of 1 makes the atmosphere as high as the planet radius.
	 */
	UPROPERTY(BlueprintReadWrite)
	float AtmosphereScale = 0.2;

	/**
	 * The strength of sunlight.
	 */
	UPROPERTY(BlueprintReadWrite)
	float SunIntensity = 1;

	/**
	 * The amount of hue shift to apply. A value of 1 equals a hue shift of 360 degrees.
	 */
	UPROPERTY(BlueprintReadWrite)
	float HueShift = 0;

	/**
	 * The particle profiles that make up the atmosphere.
	 */
	UPROPERTY(BlueprintReadWrite)
	TArray<FParticleProfile> ParticleProfiles;
};
