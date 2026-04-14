// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElementsZoneGraphEditor.h"

#include "PCGExAssetTypesMacros.h"
#include "PropertyEditorModule.h"
#include "PCGExZGSettingsCustomization.h"
#include "PCGExZGPropertyTypes.h"
#include "Details/PCGExPropertyCompiledCustomization.h"

void FPCGExElementsZoneGraphEditorModule::StartupModule()
{
	IPCGExEditorModuleInterface::StartupModule();

	PCGEX_REGISTER_CUSTO_START

	PCGEX_REGISTER_CUSTO("PCGExZGPolygonSettings", FPCGExZGSettingsCustomization)
	PCGEX_REGISTER_CUSTO("PCGExZGRoadSettings", FPCGExZGSettingsCustomization)

	// ZoneGraph property types use the same compiled customization as core properties
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExProperty_ZGIntersectionTags::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyCompiledCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FPCGExProperty_ZGConnectionRestrictions::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExPropertyCompiledCustomization::MakeInstance));
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsZoneGraphEditorModule, PCGExElementsZoneGraphEditor)
