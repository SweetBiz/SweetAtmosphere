#pragma once

#include "PrecomputeShaderSettings.generated.h"

/**
 * Texture size and quality settings for precomputation of atmospheric scattering.
 */
USTRUCT(BlueprintType)
struct FPrecomputedTextureSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int TransmittanceTextureWidth = 256;

	UPROPERTY(BlueprintReadWrite)
	int TransmittanceTextureHeight = 256;

	UPROPERTY(BlueprintReadWrite)
	int InScatteredLightTextureSize = 256;

	/**
	 * The amount of samples to take along the view ray when precomputing transmittance.
	 */
	UPROPERTY(BlueprintReadWrite)
	int TransmittanceSampleSteps = 25;

	/**
	 * The amount of samples to take along the view ray when precomputing in-scattered light.
	 */
	UPROPERTY(BlueprintReadWrite)
	int InScatteredLightSampleSteps = 50;
};
