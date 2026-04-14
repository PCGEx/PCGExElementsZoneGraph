// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsZoneGraphEditor.h"

#include "PCGExAssetTypesMacros.h"
#include "PropertyEditorModule.h"
#include "PCGExZGSettingsCustomization.h"

void FPCGExElementsZoneGraphEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	PCGEX_REGISTER_CUSTO_START

	PCGEX_REGISTER_CUSTO("PCGExZGPolygonSettings", FPCGExZGSettingsCustomization)
	PCGEX_REGISTER_CUSTO("PCGExZGRoadSettings", FPCGExZGSettingsCustomization)
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsZoneGraphEditorModule, PCGExElementsZoneGraphEditor)
