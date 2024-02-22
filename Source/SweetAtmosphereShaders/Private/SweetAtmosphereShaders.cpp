#include "../Public/SweetAtmosphereShaders.h"
#include "Interfaces/IPluginManager.h"

void FSweetAtmosphereShaders::StartupModule()
{
	// Maps virtual shader source directory to the plugin's actual shaders directory.
	const auto PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("SweetAtmosphere"))->GetBaseDir(), TEXT("Shaders/Private"));
	AddShaderSourceDirectoryMapping(TEXT("/SweetAtmosphere"), PluginShaderDir);
}

void FSweetAtmosphereShaders::ShutdownModule()
{
}

IMPLEMENT_MODULE(FSweetAtmosphereShaders, SweetAtmosphereShaders)