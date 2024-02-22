#pragma once

#include "DebugTextureHelper.generated.h"

UCLASS()
class SWEETATMOSPHERE_API UDebugTextureHelper : public UObject
{
	GENERATED_BODY()
public:
	/**
	 * Writes a 2D or Volume texture to EXR file(s).
	 *
	 * @param Texture The input texture.
	 * @param FilepathNoExtension The output filename including path without the ".png" extension.
	 */
	UFUNCTION(BlueprintCallable, Category = "Texture")
	static void SaveTextureToEXR(UTexture* Texture, const FString& FilepathNoExtension);

	/**
	 * Writes a 2D texture to an EXR file.
	 * Texture pixel format must be PF_FloatRGBA.
	 *
	 * @param Texture The input texture.
	 * @param FilepathNoExtension The output filename including path without the ".png" extension.
	 */
	UFUNCTION(BlueprintCallable, Category = "Texture")
	static void SaveTexture2DToEXR(UTexture2D* Texture, const FString& FilepathNoExtension);

	/**
	 * Writes a volume texture to individual EXR files for each Z layer.
	 * Texture pixel format must be PF_FloatRGBA.
	 *
	 * @param VolumeTexture The input texture.
	 * @param FilepathNoExtension The output filename including path without the ".png" extension.
	 */
	UFUNCTION(BlueprintCallable, Category = "Texture")
	static void SaveVolumeTextureToEXR(UVolumeTexture* VolumeTexture, const FString& FilepathNoExtension);
};