// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExZGSettingsCustomization.h"

#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "Graph/PCGExClusterToZoneGraph.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "PCGExZGSettingsCustomization"

void FPCGExZGSettingsCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
}

void FPCGExZGSettingsCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 i = 0; i < NumChildren; i++)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(i);
		if (!ChildHandle.IsValid()) { continue; }

		// Check for CategorySeparator meta tag
		if (ChildHandle->HasMetaData(TEXT("CategorySeparator")))
		{
			const FString Title = ChildHandle->GetMetaData(TEXT("CategorySeparator"));

			ChildBuilder.AddCustomRow(FText::FromString(Title))
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(-16, 0, 0, 0))
				.HeightOverride(16)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Title.ToUpper()))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.35f, 0.35f)))
				]
			];
		}

		ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
	}
}

void FPCGExZGConvexFitSettingsCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> ClampHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExZGConvexFitSettings, Clamp));
	const TSharedPtr<IPropertyHandle> FallbackHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExZGConvexFitSettings, Fallback));

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.Padding(0, 0, 4, 0)
		[
			ClampHandle.IsValid() ? ClampHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.6f)
		[
			FallbackHandle.IsValid() ? FallbackHandle->CreatePropertyValueWidget() : SNullWidget::NullWidget
		]
	];
}

#undef LOCTEXT_NAMESPACE
