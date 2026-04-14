// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "ZoneGraphTypes.h"
#include "PCGExPropertyTypes.h"

#include "PCGExZGPropertyTypes.generated.h"

/**
 * ZoneGraph Intersection Tags property.
 * Authored as FZoneGraphTagMask (uses the engine's tag picker), outputs as int32 attribute.
 */
USTRUCT(BlueprintType, /*meta=(PCGExInlineValue),*/ DisplayName="ZG Intersection Tags")
struct PCGEXELEMENTSZONEGRAPH_API FPCGExProperty_ZGIntersectionTags : public FPCGExProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property")
	FZoneGraphTagMask Value;

protected:
	TSharedPtr<PCGExData::TBuffer<int32>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const override;
	virtual void CopyValueFrom(const FPCGExProperty* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Integer32; }
	virtual FName GetTypeName() const override { return FName("ZGIntersectionTags"); }
	virtual FPCGMetadataAttributeBase* CreateMetadataAttribute(UPCGMetadata* Metadata, FName AttributeName) const override;
	virtual void WriteMetadataValue(FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const override;
};

/**
 * ZoneGraph Connection Restriction Flags property.
 * Authored as a bitmask of EZoneShapeLaneConnectionRestrictions, outputs as int32 attribute.
 */
USTRUCT(BlueprintType, meta=(PCGExInlineValue), DisplayName="ZG Connection Restrictions")
struct PCGEXELEMENTSZONEGRAPH_API FPCGExProperty_ZGConnectionRestrictions : public FPCGExProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Property", meta=(Bitmask, BitmaskEnum="/Script/ZoneGraph.EZoneShapeLaneConnectionRestrictions"))
	int32 Value = 0;

protected:
	TSharedPtr<PCGExData::TBuffer<int32>> OutputBuffer;

public:
	virtual bool InitializeOutput(const TSharedRef<PCGExData::FFacade>& OutputFacade, FName OutputName) override;
	virtual void WriteOutput(int32 PointIndex) const override;
	virtual void WriteOutputFrom(int32 PointIndex, const FPCGExProperty* Source) const override;
	virtual void CopyValueFrom(const FPCGExProperty* Source) override;
	virtual bool SupportsOutput() const override { return true; }
	virtual EPCGMetadataTypes GetOutputType() const override { return EPCGMetadataTypes::Integer32; }
	virtual FName GetTypeName() const override { return FName("ZGConnectionRestrictions"); }
	virtual FPCGMetadataAttributeBase* CreateMetadataAttribute(UPCGMetadata* Metadata, FName AttributeName) const override;
	virtual void WriteMetadataValue(FPCGMetadataAttributeBase* Attribute, int64 EntryKey) const override;
};
