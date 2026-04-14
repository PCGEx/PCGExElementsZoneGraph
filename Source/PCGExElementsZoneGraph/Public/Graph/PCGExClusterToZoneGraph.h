// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "ZoneGraphSettings.h"
#include "ZoneGraphTypes.h"
#include "ZoneShapeComponent.h"
#include "Core/PCGExClustersProcessor.h"
#include "Details/PCGExAttachmentRules.h"
#include "Details/PCGExInputShorthandsDetails.h"

#include "PCGExClusterToZoneGraph.generated.h"

UENUM(BlueprintType)
enum class EPCGExZGOrientationMode : uint8
{
	SortDirection   = 0 UMETA(DisplayName="Sort Direction", Tooltip="Use the Direction Settings sorting rules to determine road orientation."),
	DepthFirst      = 1 UMETA(DisplayName="Depth-First", Tooltip="Use BFS depth ordering to orient roads from lower to higher depth. Consistent for tree-like graphs."),
	GlobalDirection = 2 UMETA(DisplayName="Global Direction", Tooltip="Orient all roads to flow along a global direction vector."),
};

UENUM(BlueprintType)
enum class EPCGExZGAutoRadiusMode : uint8
{
	Disabled       = 0 UMETA(DisplayName="Disabled"),
	WidestLane     = 1 UMETA(DisplayName="Widest Lane"),
	HalfProfile    = 2 UMETA(DisplayName="Half Profile Width"),
	WidestLaneMin  = 3 UMETA(DisplayName="Widest Lane (Min)"),
	HalfProfileMin = 4 UMETA(DisplayName="Half Profile Width (Min)"),
};

UENUM(BlueprintType)
enum class EPCGExZGTangentLengthMode : uint8
{
	Default    = 0 UMETA(DisplayName="Default", Tooltip="Don't set tangent length -- ZoneGraph handles it."),
	Manual     = 1 UMETA(DisplayName="Manual", Tooltip="Use a constant or per-point attribute value."),
	Auto       = 2 UMETA(DisplayName="Auto", Tooltip="Average distance to chain neighbors, divided by 3."),
	CatmullRom = 3 UMETA(DisplayName="Catmull-Rom", Tooltip="|P_next - P_prev| * 0.5."),
};

UENUM(BlueprintType)
enum class EPCGExZGBitmaskMode : uint8
{
	None     = 0 UMETA(DisplayName="None", Tooltip="No connection restrictions."),
	Raw      = 1 UMETA(DisplayName="Raw Bitmask", Tooltip="Read int32 attribute from edges, interpret directly as EZoneShapeLaneConnectionRestrictions bitmask."),
	ValueMap = 2 UMETA(DisplayName="Value Map", Tooltip="Read int32 attribute from edges, look up in TMap to get bitmask."),
};

USTRUCT(BlueprintType)
struct PCGEXELEMENTSZONEGRAPH_API FPCGExZGConnectionRestrictionFlags
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(Bitmask, BitmaskEnum="/Script/ZoneGraph.EZoneShapeLaneConnectionRestrictions"))
	int32 Flags = 0;
};

USTRUCT(BlueprintType)
struct PCGEXELEMENTSZONEGRAPH_API FPCGExZGPolygonSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Geometry"))
	double PolygonRadius = 100;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverridePolygonRadius = false;

	/** Per-point polygon radius override. Read from: Points. Attribute type: double. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Radius (Attr)", EditCondition="bOverridePolygonRadius"))
	FName PolygonRadiusAttribute = FName("PolygonRadius");

	/** Auto-compute polygon radius from connected road lane profiles. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExZGAutoRadiusMode AutoRadiusMode = EPCGExZGAutoRadiusMode::Disabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Routing"))
	EZoneShapePolygonRoutingType PolygonRoutingType = EZoneShapePolygonRoutingType::Arcs;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverridePolygonRoutingType = false;

	/** Per-point polygon routing override. Read from: Points. Attribute type: int32. Values: 0=Bezier, 1=Arcs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Polygon Routing (Attr)", EditCondition="bOverridePolygonRoutingType"))
	FName PolygonRoutingTypeAttribute = FName("PolygonRoutingType");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Point Type"))
	FZoneShapePointType PolygonPointType = FZoneShapePointType::LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverridePolygonPointType = false;

	/** Per-point polygon shape point type override. Read from: Points. Attribute type: int32. Values: 0=Sharp, 1=Bezier, 2=AutoBezier, 3=LaneProfile. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Polygon Point Type (Attr)", EditCondition="bOverridePolygonPointType"))
	FName PolygonPointTypeAttribute = FName("PolygonPointType");

	/** Default intersection tags applied to all polygons. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Intersection Tags"))
	FZoneGraphTagMask AdditionalIntersectionTags = FZoneGraphTagMask::None;

	/** How additional intersection tags are sourced from attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExZGBitmaskMode IntersectionTagsMode = EPCGExZGBitmaskMode::None;

	/** Attribute name for intersection tag values. Read from: Points. Attribute type: int32. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditConditionHides, EditCondition="IntersectionTagsMode != EPCGExZGBitmaskMode::None"))
	FName IntersectionTagsAttribute = FName("IntersectionTags");

	/** Maps integer attribute values to intersection tag masks. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditConditionHides, EditCondition="IntersectionTagsMode == EPCGExZGBitmaskMode::ValueMap"))
	TMap<int32, FZoneGraphTagMask> IntersectionTagsValueMap;

	
	/** Inner turn radius for polygon arc routing. Read from: Edges (nearest edge to polygon connection). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Primary Turn Radius", CategorySeparator="Inner Turn Radius"))
	FPCGExInputShorthandNameFloat InnerTurnRadiusPrimary = FPCGExInputShorthandNameFloat(FName("InnerTurnRadius"), 100.0f, false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bSeparateInnerTurnRadiusSecondary = false;

	/** Separate inner turn radius for the end polygon connection. When disabled, Primary value is used for both. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Secondary Turn Radius", EditCondition="bSeparateInnerTurnRadiusSecondary"))
	FPCGExInputShorthandNameFloat InnerTurnRadiusSecondary = FPCGExInputShorthandNameFloat(FName("InnerTurnRadius"), 100.0f, false);
	
	
	/** Roll angle (degrees) applied to polygon connection points. Read from: Edges (nearest edge to polygon connection). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Primary Roll", CategorySeparator="Roll"))
	FPCGExInputShorthandNameFloat RollPrimary = FPCGExInputShorthandNameFloat(FName("Roll"), 0.0f, false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bSeparateRollSecondary = false;

	/** Separate roll for the end polygon connection. When disabled, Primary value is used for both. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Secondary Roll", EditCondition="bSeparateRollSecondary"))
	FPCGExInputShorthandNameFloat RollSecondary = FPCGExInputShorthandNameFloat(FName("Roll"), 0.0f, false);

	
	/** How lane connection restrictions are sourced. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Connection Restrictions"))
	EPCGExZGBitmaskMode ConnectionRestrictionMode = EPCGExZGBitmaskMode::None;

	/** Attribute name for connection restriction values. Read from: Edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connection Restrictions", meta=(PCG_Overridable, EditConditionHides, EditCondition="ConnectionRestrictionMode != EPCGExZGBitmaskMode::None"))
	FName ConnectionRestrictionsAttribute = FName("ConnectionRestrictions");

	/** Maps integer attribute values to connection restriction flags. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Connection Restrictions", meta=(EditConditionHides, EditCondition="ConnectionRestrictionMode == EPCGExZGBitmaskMode::ValueMap"))
	TMap<int32, FPCGExZGConnectionRestrictionFlags> ConnectionRestrictionsValueMap;
};

USTRUCT(BlueprintType)
struct PCGEXELEMENTSZONEGRAPH_API FPCGExZGRoadSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Point Type"))
	FZoneShapePointType RoadPointType = FZoneShapePointType::AutoBezier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideRoadPointType = false;

	/** Per-edge road shape point type override. Read from: Points. Attribute type: int32. Values: 0=Sharp, 1=Bezier, 2=AutoBezier, 3=LaneProfile. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Road Point Type (Attr)", EditCondition="bOverrideRoadPointType"))
	FName RoadPointTypeAttribute = FName("RoadPointType");

	/** How road tangent lengths are determined. Controls bezier curve tightness. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Tangent Length Mode", EditCondition="bCachedSupportsCustomLength", EditConditionHides, HideEditConditionToggle))
	EPCGExZGTangentLengthMode RoadTangentLengthMode = EPCGExZGTangentLengthMode::Default;

	/** Tangent length value -- constant or per-point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Length",
		EditCondition="bCachedSupportsCustomLength && RoadTangentLengthMode == EPCGExZGTangentLengthMode::Manual", EditConditionHides, HideEditConditionToggle))
	FPCGExInputShorthandNameDoubleAbs TangentLength = FPCGExInputShorthandNameDoubleAbs(FName("TangentLength"), 100.0, false);

	/** Scale multiplier applied to all tangent lengths (manual or computed). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Scale", ClampMin=0, UIMin=0,
		EditCondition="bCachedSupportsCustomLength && RoadTangentLengthMode != EPCGExZGTangentLengthMode::Default", EditConditionHides, HideEditConditionToggle))
	double TangentLengthScale = 1.0;

	/** Trim road shape points inside the polygon boundary so roads start/end precisely at the polygon edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle, CategorySeparator="Endpoints"))
	bool bTrimRoadEndpoints = true;

	/** After trimming, remove road points closer than this distance to the polygon boundary. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTrimRoadEndpoints", ClampMin="0"))
	double EndpointTrimBuffer = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(CategorySeparator="Lane Profile"))
	FZoneLaneProfileRef LaneProfile;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bOverrideLaneProfile = false;

	/** Lane profile override. Read from: Edges (majority vote). Attribute type: FName. Must match a registered lane profile name in ZoneGraph settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Lane Profile (Attr)", EditCondition="bOverrideLaneProfile"))
	FName LaneProfileAttribute = FName("LaneProfile");

	UPROPERTY()
	bool bCachedSupportsCustomLength = false;
};

namespace PCGExClusters
{
	class FNodeChain;
}

namespace PCGExMT
{
	class FTimeSlicedMainThreadLoop;
}

namespace PCGExZoneGraph
{
	namespace Labels
	{
		const FName SourceEdgeFlipFiltersLabel = FName("Flip Conditions");
	}
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="cluster-to-zone-graph"))
class UPCGExClusterToZoneGraphSettings : public UPCGExClustersProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExClusterToZoneGraphSettings()
	{
		if (const UZoneGraphSettings* ZoneGraphSettings = GetDefault<UZoneGraphSettings>())
		{
			const TArray<FZoneLaneProfile>& Profiles = ZoneGraphSettings->GetLaneProfiles();
			if (!Profiles.IsEmpty())
			{
				RoadSettings.LaneProfile = Profiles[0];
			}
		}
	}

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ClusterToZoneGraph, "Cluster to Zone Graph", "Create Zone Graph from clusters.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterOp; }
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual bool OutputPinsCanBeDeactivated() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	virtual bool ShouldCache() const override { return false; }
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual bool SupportsEdgeSorting() const override { return DirectionSettings.RequiresSortingRules(); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(FName("Break Conditions"), "Filters used to know which points are 'break' points. Use those if you want to create more polygon shapes.", PCGExFactories::ClusterNodeFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Defines the direction in which points will be ordered to form the final paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Direction", meta=(PCG_Overridable))
	FPCGExEdgeDirectionSettings DirectionSettings;

	/** How road orientation is determined. Affects lane profile alignment at intersections. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Direction")
	EPCGExZGOrientationMode OrientationMode = EPCGExZGOrientationMode::DepthFirst;

	/** Flip all road orientations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Direction", meta=(EditCondition="OrientationMode != EPCGExZGOrientationMode::SortDirection", EditConditionHides))
	bool bInvertOrientation = false;

	/** Global direction vector used to orient roads. Each road is oriented so its travel direction aligns with this vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Direction", meta=(EditCondition="OrientationMode == EPCGExZGOrientationMode::GlobalDirection", EditConditionHides))
	FVector OrientationDirection = FVector::ForwardVector;

	/** Enable the use of filters to define whether a road direction should be flipped or not (reversing the auto-computed direction). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Direction")
	bool bEnableFlipFilters = false;

	/** Polygon intersection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExZGPolygonSettings PolygonSettings;

	/** Road spline settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExZGRoadSettings RoadSettings;

	/** Output polygon shapes as closed PCG paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bOutputPolygonPaths = false;

	/** Output road splines as PCG paths with tangent attributes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output")
	bool bOutputRoadPaths = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputRoadPaths", EditConditionHides))
	FName ArriveName = "ArriveTangent";

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputRoadPaths", EditConditionHides))
	FName LeaveName = "LeaveTangent";


	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable), AdvancedDisplay)
	FString CommaSeparatedComponentTags = TEXT("PCGExZoneGraph");

	/** Specify a list of functions to be called on the target actor after dynamic mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	TArray<FName> PostProcessFunctionNames;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	FPCGExAttachmentRules AttachmentRules;

private:
	friend class FPCGExClusterToZoneGraphElement;
};

struct FPCGExClusterToZoneGraphContext final : FPCGExClustersProcessorContext
{
	friend class FPCGExClusterToZoneGraphElement;

	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> FlipEdgeFilterFactories;

	TArray<FString> ComponentTags;

	TMap<FName, FZoneLaneProfileRef> LaneProfileMap;

	TSharedPtr<PCGExData::FPointIOCollection> OutputPolygonPaths;
	TSharedPtr<PCGExData::FPointIOCollection> OutputRoadPaths;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExClusterToZoneGraphElement final : public FPCGExClustersProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ClusterToZoneGraph)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
};

namespace PCGExClusterToZoneGraph
{
	class FProcessor;

	class FZGBase : public TSharedFromThis<FZGBase>
	{
	protected:
		FProcessor* Processor = nullptr;
		TArray<FZoneShapePoint> PrecomputedPoints;

	public:
		UZoneShapeComponent* Component = nullptr;
		double StartRadius = 0;
		double EndRadius = 0;

		explicit FZGBase(FProcessor* InProcessor);
		void InitComponent(AActor* InTargetActor);
	};

	class FZGRoad : public FZGBase
	{
	public:
		struct FPolygonEndpoint
		{
			FVector PolygonCenter = FVector::ZeroVector;
			FVector Direction = FVector::ZeroVector; // outward from polygon along road
			double Radius = 0;
			bool bValid = false;
		};

		TSharedPtr<PCGExClusters::FNodeChain> Chain;
		bool bIsReversed = false;

		FPolygonEndpoint StartEndpoint;
		FPolygonEndpoint EndEndpoint;
		bool bDegenerate = false;

		FZoneLaneProfileRef CachedLaneProfile;
		double CachedMaxLaneWidth = 0;
		double CachedTotalProfileWidth = 0;
		bool bReverseLaneProfileOverride = false;

		// Per-endpoint cached polygon point properties (resolved from edge shorthands)
		float CachedInnerTurnRadiusStart = 100.0f;
		float CachedInnerTurnRadiusEnd = 100.0f;
		float CachedRollStart = 0.0f;
		float CachedRollEnd = 0.0f;
		int32 CachedConnectionRestrictionsStart = 0;
		int32 CachedConnectionRestrictionsEnd = 0;

		explicit FZGRoad(FProcessor* InProcessor, const TSharedPtr<PCGExClusters::FNodeChain>& InChain, const bool InReverse);
		void ResolveLaneProfile(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void ResolveReverseLaneProfile(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void ResolvePolygonPointProperties(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void Compile();
		void BuildPathOutput(const TSharedPtr<PCGExData::FPointIO>& InPathIO) const;
	};

	class FZGPolygon : public FZGBase
	{
	protected:
		TArray<TSharedPtr<FZGRoad>> Roads;
		TBitArray<> FromStart;

		double CachedRadius = 0;
		TArray<double> CachedRoadRadii;
		EZoneShapePolygonRoutingType CachedRoutingType = EZoneShapePolygonRoutingType::Arcs;
		FZoneShapePointType CachedPointType = FZoneShapePointType::LaneProfile;
		FZoneGraphTagMask CachedAdditionalTags = FZoneGraphTagMask::None;
		FZoneLaneProfileRef CachedLaneProfile;
		TArray<FZoneLaneProfileRef> CachedPointLaneProfiles;
		TArray<double> CachedPointHalfWidths;

	public:
		int32 NodeIndex = -1;

		explicit FZGPolygon(FProcessor* InProcessor, const PCGExClusters::FNode* InNode);

		void Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart);
		void Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster);
		void SyncRadiusToRoads();
		void BuildPathOutput(const TSharedPtr<PCGExData::FPointIO>& InPathIO) const;
		void Compile();
	};

	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>
	{
		friend class FBatch;
		friend class FZGRoad;
		friend class FZGPolygon;

	protected:
		FPCGExEdgeDirectionSettings DirectionSettings;

		AActor* TargetActor = nullptr;
		FAttachmentTransformRules CachedAttachmentRules = FAttachmentTransformRules::KeepWorldTransform;

		TSharedPtr<PCGExMT::FTimeSlicedMainThreadLoop> MainCompileLoop;

		TArray<TSharedPtr<PCGExClusters::FNodeChain>> ProcessedChains;

		TArray<TSharedPtr<FZGRoad>> Roads;
		TArray<TSharedPtr<FZGPolygon>> Polygons;

		TSharedPtr<PCGExData::TBuffer<double>> PolygonRadiusBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> PolygonRoutingTypeBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> PolygonPointTypeBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> RoadPointTypeBuffer;
		TSharedPtr<PCGExData::TBuffer<int32>> AdditionalIntersectionTagsBuffer;
		TSharedPtr<PCGExData::TBuffer<FName>> EdgeLaneProfileBuffer;

		TSharedPtr<PCGExDetails::TSettingValue<double>> TangentLengthGetter;

		// Edge-sourced polygon point property getters
		TSharedPtr<PCGExDetails::TSettingValue<float>> InnerTurnRadiusStartGetter;
		TSharedPtr<PCGExDetails::TSettingValue<float>> InnerTurnRadiusEndGetter;
		TSharedPtr<PCGExDetails::TSettingValue<float>> RollStartGetter;
		TSharedPtr<PCGExDetails::TSettingValue<float>> RollEndGetter;
		TSharedPtr<PCGExData::TBuffer<int32>> ConnectionRestrictionsBuffer;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		bool BuildChains();
		virtual void CompleteWork() override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		virtual void Output() override;

		virtual void Cleanup() override;

		void ComputeDFSOrientation(TArray<bool>& OutReversed) const;
		FZoneLaneProfileRef ResolveLaneProfileByName(FName ProfileName) const;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

	protected:
		TSharedPtr<TArray<int8>> Breakpoints;
		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
			: TBatch(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
			DefaultVtxFilterValue = false;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}
