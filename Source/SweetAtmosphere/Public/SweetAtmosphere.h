#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// include all other headers that expose functions
// ReSharper disable CppUnusedIncludeDirective
#include "AtmospherePrecompute.h"
#include "DebugTextureHelper.h"
// ReSharper restore CppUnusedIncludeDirective

class FSweetAtmosphere : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
