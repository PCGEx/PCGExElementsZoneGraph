// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExZGPropertyTypes.h"

#include "Metadata/PCGMetadata.h"

#pragma region FPCGExProperty_ZGIntersectionTags

// FZoneGraphTagMask -> int32 conversion via GetValue()

bool FPCGExProperty_ZGIntersectionTags::InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName)
{
	OutputBuffer = OutputFacade->GetWritable<int32>(OutputName, static_cast<int32>(Value.GetValue()), true, PCGExData::EBufferInit::Inherit);
	return OutputBuffer.IsValid();
}

void FPCGExProperty_ZGIntersectionTags::WriteOutput(int32 PointIndex) const
{
	check(OutputBuffer);
	OutputBuffer->SetValue(PointIndex, static_cast<int32>(Value.GetValue()));
}

void FPCGExProperty_ZGIntersectionTags::WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const
{
	check(OutputBuffer);
	const FPCGExProperty_ZGIntersectionTags* Typed = static_cast<const FPCGExProperty_ZGIntersectionTags*>(Source);
	OutputBuffer->SetValue(PointIndex, static_cast<int32>(Typed->Value.GetValue()));
}

void FPCGExProperty_ZGIntersectionTags::CopyValueFrom(const FPCGExProperty* Source)
{
	const FPCGExProperty_ZGIntersectionTags* Typed = static_cast<const FPCGExProperty_ZGIntersectionTags*>(Source);
	Value = Typed->Value;
}

FPCGMetadataAttributeBase* FPCGExProperty_ZGIntersectionTags::CreateMetadataAttribute(UPCGMetadata* Metadata, FName AttributeName) const
{
	return Metadata->CreateAttribute<int32>(AttributeName, static_cast<int32>(Value.GetValue()), true, true);
}

void FPCGExProperty_ZGIntersectionTags::WriteMetadataValue(FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const
{
	static_cast<FPCGMetadataAttribute<int32>*>(Attribute)->SetValue(EntryKey, static_cast<int32>(Value.GetValue()));
}

#pragma endregion

#pragma region FPCGExProperty_ZGConnectionRestrictions

// int32 bitmask, no conversion needed (same as FPCGExProperty_Int32 but with Bitmask meta on the Value field)

bool FPCGExProperty_ZGConnectionRestrictions::InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName)
{
	OutputBuffer = OutputFacade->GetWritable<int32>(OutputName, Value, true, PCGExData::EBufferInit::Inherit);
	return OutputBuffer.IsValid();
}

void FPCGExProperty_ZGConnectionRestrictions::WriteOutput(int32 PointIndex) const
{
	check(OutputBuffer);
	OutputBuffer->SetValue(PointIndex, Value);
}

void FPCGExProperty_ZGConnectionRestrictions::WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const
{
	check(OutputBuffer);
	const FPCGExProperty_ZGConnectionRestrictions* Typed = static_cast<const FPCGExProperty_ZGConnectionRestrictions*>(Source);
	OutputBuffer->SetValue(PointIndex, Typed->Value);
}

void FPCGExProperty_ZGConnectionRestrictions::CopyValueFrom(const FPCGExProperty* Source)
{
	const FPCGExProperty_ZGConnectionRestrictions* Typed = static_cast<const FPCGExProperty_ZGConnectionRestrictions*>(Source);
	Value = Typed->Value;
}

FPCGMetadataAttributeBase* FPCGExProperty_ZGConnectionRestrictions::CreateMetadataAttribute(UPCGMetadata* Metadata, FName AttributeName) const
{
	return Metadata->CreateAttribute<int32>(AttributeName, Value, true, true);
}

void FPCGExProperty_ZGConnectionRestrictions::WriteMetadataValue(FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const
{
	static_cast<FPCGMetadataAttribute<int32>*>(Attribute)->SetValue(EntryKey, Value);
}

#pragma endregion
