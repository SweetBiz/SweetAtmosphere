#include "DebugTextureHelper.h"

#include "IImageWrapperModule.h"
#include "ImagePixelData.h"
#include "ImageWriteBlueprintLibrary.h"
#include "Engine/VolumeTexture.h"

void UDebugTextureHelper::SaveTextureToEXR(UTexture* Texture, const FString& FilepathNoExtension)
{
	if (UTexture2D* Tex2D = Cast<UTexture2D>(Texture))
	{
		SaveTexture2DToEXR(Tex2D, FilepathNoExtension);
	}
	else if (UVolumeTexture* VolTex = Cast<UVolumeTexture>(Texture))
	{
		SaveVolumeTextureToEXR(VolTex, FilepathNoExtension);
	}
	else
	{
		UE_LOG(LogBlueprint, Error, TEXT("Unknown Texture type passed to SaveTextureToEXR"))
	}
}

void UDebugTextureHelper::SaveTexture2DToEXR(UTexture2D* Texture, const FString& FilepathNoExtension)
{
	const FString Filepath = FilepathNoExtension + ".exr";
	const int Width = Texture->GetSizeX();
	const int Height = Texture->GetSizeY();

	UImageWriteBlueprintLibrary::ResolvePixelData(Texture,
		[Filepath, Width, Height](TUniquePtr<FImagePixelData>&& PixelData) {
			const void* RawData;
			int64 Size;
			PixelData->GetRawData(RawData, Size);

			auto& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
			const auto ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);

			if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(RawData, Size, Width, Height, PixelData->GetPixelLayout(), PixelData->GetBitDepth()))
			{
				const TArray64<uint8>& ImageData = ImageWrapper->GetCompressed();
				FFileHelper::SaveArrayToFile(ImageData, *Filepath);
			}
		});
}

void UDebugTextureHelper::SaveVolumeTextureToEXR(UVolumeTexture* VolumeTexture, const FString& FilepathNoExtension)
{
	auto* PlatformData = VolumeTexture->GetPlatformData();
	auto Format = PlatformData->PixelFormat;

	// Ensure the format is PF_FloatRGBA.
	check(Format == PF_FloatRGBA);

	auto& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	const auto ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR);

	const int32 Width = VolumeTexture->GetSizeX();
	const int32 Height = VolumeTexture->GetSizeY();
	const int32 Depth = VolumeTexture->GetSizeZ();

	const uint16* Data = static_cast<uint16*>(PlatformData->Mips[0].BulkData.Lock(LOCK_READ_ONLY));

	for (int32 SliceIndex = 0; SliceIndex < Depth; ++SliceIndex)
	{
		if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(Data + SliceIndex * Width * Height * 4, Width * Height * 4 * sizeof(uint16), Width, Height, ERGBFormat::RGBAF, 16))
		{
			const TArray64<uint8>& PNGData = ImageWrapper->GetCompressed();
			const FString FullFilePath = FilepathNoExtension + FString::Printf(TEXT("_%d.exr"), SliceIndex);

			if (!FFileHelper::SaveArrayToFile(PNGData, *FullFilePath))
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to save EXR file for slice %d."), SliceIndex);
			}
		}
	}

	PlatformData->Mips[0].BulkData.Unlock();
}
