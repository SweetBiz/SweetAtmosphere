using UnrealBuildTool;

public class SweetAtmosphereShaders : ModuleRules

{
	public SweetAtmosphereShaders(ReadOnlyTargetRules Target) : base(Target)

	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor) PrivateDependencyModuleNames.Add("TargetPlatform");
		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("Engine");
		PublicDependencyModuleNames.Add("MaterialShaderQualitySettings");

		PrivateDependencyModuleNames.AddRange(new[]
		{
			"CoreUObject",
			"Renderer",
			"RenderCore",
			"RHI",
			"Projects"
		});
	}
}