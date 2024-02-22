// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SweetAtmosphere : ModuleRules
{
	public SweetAtmosphere(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new[]
			{
				"Core", "Engine", "SweetAtmosphereShaders"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore", "ImageWriteQueue", "ImageWriteQueue"
				// ... add private dependencies that you statically link with here ...	
			}
		);
	}
}