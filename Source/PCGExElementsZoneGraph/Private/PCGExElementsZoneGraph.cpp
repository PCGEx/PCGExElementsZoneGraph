// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsZoneGraph.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FPCGExElementsZoneGraphModule"

void FPCGExElementsZoneGraphModule::StartupModule()
{
	OldBaseModules.Add(TEXT("PCGExtendedToolkitZoneGraph"));
	IPCGExLegacyModuleInterface::StartupModule();
}

void FPCGExElementsZoneGraphModule::ShutdownModule()
{
	IPCGExLegacyModuleInterface::ShutdownModule();
}

#undef LOCTEXT_NAMESPACE

PCGEX_IMPLEMENT_MODULE(FPCGExElementsZoneGraphModule, PCGExElementsZoneGraph)
