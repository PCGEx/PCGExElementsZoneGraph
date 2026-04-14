// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExClusterToZoneGraph.h"

#include "PCGComponent.h"
#include "PCGExSubSystem.h"
#include "ZoneShapeComponent.h"
#include "Core/PCGExPointFilter.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/Artifacts/PCGExChain.h"
#include "Clusters/Artifacts/PCGExCachedChain.h"
#include "Containers/PCGExManagedObjects.h"
#include "Core/PCGExMT.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExStreamingHelpers.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Helpers/PCGExPointArrayDataHelpers.h"
#include "Paths/PCGExPathsHelpers.h"
#include "PCGExCoreMacros.h"

#define LOCTEXT_NAMESPACE "PCGExClusterToZoneGraph"
#define PCGEX_NAMESPACE ClusterToZoneGraph

namespace PCGExClusterToZoneGraph
{
	const FName OutputPolygonPathsLabel = TEXT("Polygon Paths");
	const FName OutputRoadPathsLabel = TEXT("Road Paths");
}

PCGExData::EIOInit UPCGExClusterToZoneGraphSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExClusterToZoneGraphSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

#if WITH_EDITOR
void UPCGExClusterToZoneGraphSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	RoadSettings.bCachedSupportsCustomLength = RoadSettings.bOverrideRoadPointType || RoadSettings.RoadPointType == FZoneShapePointType::Bezier;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExClusterToZoneGraphSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	
	if (bEnableFlipFilters) { PCGEX_PIN_FILTERS(PCGExZoneGraph::Labels::SourceEdgeFlipFiltersLabel, "Road direction flip filters.", Normal) }
	else { PCGEX_PIN_FILTERS(PCGExZoneGraph::Labels::SourceEdgeFlipFiltersLabel, "Road direction flip filters.", Advanced) }
	
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExClusterToZoneGraphSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	if (bOutputPolygonPaths) { PCGEX_PIN_POINTS(PCGExClusterToZoneGraph::OutputPolygonPathsLabel, "Polygon shapes as closed paths", Normal) }
	else { PCGEX_PIN_POINTS(PCGExClusterToZoneGraph::OutputPolygonPathsLabel, "Polygon shapes as closed paths", Advanced) }

	if (bOutputRoadPaths) { PCGEX_PIN_POINTS(PCGExClusterToZoneGraph::OutputRoadPathsLabel, "Road splines as paths with tangent attributes", Normal) }
	else { PCGEX_PIN_POINTS(PCGExClusterToZoneGraph::OutputRoadPathsLabel, "Road splines as paths with tangent attributes", Advanced) }

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(ClusterToZoneGraph)

PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(ClusterToZoneGraph)

bool FPCGExClusterToZoneGraphElement::Boot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)

	if (!FPCGExClustersProcessorElement::Boot(InContext)) { return false; }

	if (Settings->bEnableFlipFilters)
	{
		GetInputFactories(Context, PCGExZoneGraph::Labels::SourceEdgeFlipFiltersLabel, Context->FlipEdgeFilterFactories, PCGExFactories::ClusterEdgeFilters, false);
	}
	
	if (const UPCGComponent* PCGComponent = InContext->GetComponent())
	{
		/*
		if (PCGComponent->GenerationTrigger == EPCGComponentGenerationTrigger::GenerateAtRuntime)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Zone Graph PCG Nodes should not be used in runtime-generated PCG components."));
			return false;
		}
		*/
	}

	if (Settings->RoadSettings.bOverrideLaneProfile)
	{
		if (const UZoneGraphSettings* ZGSettings = GetDefault<UZoneGraphSettings>())
		{
			for (const FZoneLaneProfile& Profile : ZGSettings->GetLaneProfiles())
			{
				Context->LaneProfileMap.Add(Profile.Name, FZoneLaneProfileRef(Profile));
			}
		}
	}

	if (Settings->bOutputPolygonPaths)
	{
		Context->OutputPolygonPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->OutputPolygonPaths->OutputPin = PCGExClusterToZoneGraph::OutputPolygonPathsLabel;
	}

	if (Settings->bOutputRoadPaths)
	{
		Context->OutputRoadPaths = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->OutputRoadPaths->OutputPin = PCGExClusterToZoneGraph::OutputRoadPathsLabel;
	}

	return true;
}

bool FPCGExClusterToZoneGraphElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClusterToZoneGraphElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
				NewBatch->VtxFilterFactories = &Context->FilterFactories;
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->OutputBatches();
	Context->OutputPointsAndEdges();
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

	if (Context->OutputPolygonPaths) { Context->OutputPolygonPaths->StageOutputs(); }
	else { Context->OutputData.InactiveOutputPinBitmask |= (1ULL << 2); }

	if (Context->OutputRoadPaths) { Context->OutputRoadPaths->StageOutputs(); }
	else { Context->OutputData.InactiveOutputPinBitmask |= (1ULL << 3); }

	return Context->TryComplete();
}

namespace PCGExClusterToZoneGraph
{
// Shared base for FZGRoad and FZGPolygon.
// Owns the UZoneShapeComponent and handles main-thread component creation.
#pragma region FZGBase

	FZGBase::FZGBase(FProcessor* InProcessor)
		: Processor(InProcessor)
	{
	}

	void FZGBase::InitComponent(AActor* InTargetActor)
	{
		if (!InTargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, Processor->GetContext(), FTEXT("Invalid target actor."));
			return;
		}

		// This executes on the main thread for safety
		const FString ComponentName = TEXT("PCGZoneGraphComponent");
		const EObjectFlags ObjectFlags = (Processor->GetContext()->GetComponent()->IsInPreviewMode() ? RF_Transient : RF_NoFlags);
		Component = Processor->GetContext()->ManagedObjects->New<UZoneShapeComponent>(InTargetActor, MakeUniqueObjectName(InTargetActor, UZoneShapeComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

		Component->ComponentTags.Reserve(Component->ComponentTags.Num() + Processor->GetContext()->ComponentTags.Num());
		for (const FString& ComponentTag : Processor->GetContext()->ComponentTags) { Component->ComponentTags.Add(FName(ComponentTag)); }
	}

#pragma endregion

// Represents a single road spline, built from a chain of cluster edges between two polygon nodes (or a polygon and a leaf).
//
// Resolution pipeline (called in order by FProcessor::CompleteWork):
//   1. ResolveLaneProfile        - Majority-vote lane profile FName across all chain edges
//   2. ResolveReverseLaneProfile - Majority-vote edge flip filter results (bool) across all chain edges
//   3. ResolvePolygonPointProperties - Read InnerTurnRadius, Roll, ConnectionRestrictions from first/last edge
//   4. Precompute                - Build shape points, trim endpoints at polygon boundaries, compute tangents
//   5. Compile                   - Push shape points to UZoneShapeComponent (main thread)
//
// Cached values (CachedInnerTurnRadiusStart/End, CachedRollStart/End, CachedConnectionRestrictionsStart/End)
// are consumed by FZGPolygon::Precompute when building polygon connection points.
// "Start" refers to the road's logical start endpoint, "End" to the logical end.
// When bIsReversed is true, the cached Start/End values are swapped after resolution
// so they always match the logical direction regardless of the chain's geometric ordering.
#pragma region FZGRoad

	FZGRoad::FZGRoad(FProcessor* InProcessor, const TSharedPtr<PCGExClusters::FNodeChain>& InChain, const bool InReverse)
		: FZGBase(InProcessor), Chain(InChain), bIsReversed(InReverse)
	{
	}

	void FZGRoad::ResolveLaneProfile(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		const auto* S = Processor->GetSettings();

		if (Processor->EdgeLaneProfileBuffer)
		{
			// Majority vote across chain edges
			TMap<FName, int32> ProfileCounts;
			for (const PCGExClusters::FLink& Link : Chain->Links)
			{
				if (Link.Edge < 0) { continue; }
				const PCGExClusters::FEdge* Edge = Cluster->GetEdge(Link);
				ProfileCounts.FindOrAdd(Processor->EdgeLaneProfileBuffer->Read(Edge->PointIndex))++;
			}

			FName MostCommon = NAME_None;
			int32 MaxCount = 0;
			for (const auto& [Name, Count] : ProfileCounts)
			{
				if (Count > MaxCount)
				{
					MaxCount = Count;
					MostCommon = Name;
				}
			}

			CachedLaneProfile = Processor->ResolveLaneProfileByName(MostCommon);
		}
		else
		{
			CachedLaneProfile = S->RoadSettings.LaneProfile;
		}

		// Cache lane widths from resolved profile
		if (const UZoneGraphSettings* ZGSettings = GetDefault<UZoneGraphSettings>())
		{
			if (const FZoneLaneProfile* Profile = ZGSettings->GetLaneProfileByRef(CachedLaneProfile))
			{
				CachedTotalProfileWidth = Profile->GetLanesTotalWidth();
				for (const FZoneLaneDesc& Lane : Profile->Lanes)
				{
					CachedMaxLaneWidth = FMath::Max(CachedMaxLaneWidth, static_cast<double>(Lane.Width));
				}
			}
		}
	}

	void FZGRoad::ResolveReverseLaneProfile(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		if (Processor->EdgeFilterCache.IsEmpty()) { return; }

		// Majority vote across chain edges
		int32 TrueCount = 0;
		int32 TotalCount = 0;
		for (const PCGExClusters::FLink& Link : Chain->Links)
		{
			if (Link.Edge < 0) { continue; }
			if (Processor->EdgeFilterCache[Link.Edge]) { TrueCount++; }
			TotalCount++;
		}

		bReverseLaneProfileOverride = TotalCount > 0 && TrueCount > TotalCount / 2;
	}

	void FZGRoad::ResolvePolygonPointProperties(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		const auto* S = Processor->GetSettings();
		const FPCGExZGPolygonSettings& PS = S->PolygonSettings;

		const PCGExClusters::FEdge* FirstEdge = Chain->Links.IsEmpty() ? nullptr : Cluster->GetEdge(Chain->Links[0]);
		const PCGExClusters::FEdge* LastEdge = Chain->Links.IsEmpty() ? nullptr : Cluster->GetEdge(Chain->Links.Last());
		if (!FirstEdge) { return; }

		const int32 FirstEdgePI = FirstEdge->PointIndex;
		const int32 LastEdgePI = LastEdge ? LastEdge->PointIndex : FirstEdgePI;

		// InnerTurnRadius
		if (Processor->InnerTurnRadiusStartGetter)
		{
			CachedInnerTurnRadiusStart = Processor->InnerTurnRadiusStartGetter->Read(FirstEdgePI);
			CachedInnerTurnRadiusEnd = (Processor->InnerTurnRadiusEndGetter)
				? Processor->InnerTurnRadiusEndGetter->Read(LastEdgePI)
				: CachedInnerTurnRadiusStart;
		}

		// Roll
		if (Processor->RollStartGetter)
		{
			CachedRollStart = Processor->RollStartGetter->Read(FirstEdgePI);
			CachedRollEnd = (Processor->RollEndGetter)
				? Processor->RollEndGetter->Read(LastEdgePI)
				: CachedRollStart;
		}

		// ConnectionRestrictions
		if (Processor->ConnectionRestrictionsBuffer)
		{
			const int32 RawStart = Processor->ConnectionRestrictionsBuffer->Read(FirstEdgePI);
			const int32 RawEnd = Processor->ConnectionRestrictionsBuffer->Read(LastEdgePI);

			if (PS.ConnectionRestrictionMode == EPCGExZGBitmaskMode::Raw)
			{
				CachedConnectionRestrictionsStart = RawStart;
				CachedConnectionRestrictionsEnd = RawEnd;
			}
			else if (PS.ConnectionRestrictionMode == EPCGExZGBitmaskMode::ValueMap)
			{
				const FPCGExZGConnectionRestrictionFlags* MappedStart = PS.ConnectionRestrictionsValueMap.Find(RawStart);
				const FPCGExZGConnectionRestrictionFlags* MappedEnd = PS.ConnectionRestrictionsValueMap.Find(RawEnd);
				CachedConnectionRestrictionsStart = MappedStart ? MappedStart->Flags : 0;
				CachedConnectionRestrictionsEnd = MappedEnd ? MappedEnd->Flags : 0;
			}
		}

		if (bIsReversed)
		{
			Swap(CachedInnerTurnRadiusStart, CachedInnerTurnRadiusEnd);
			Swap(CachedRollStart, CachedRollEnd);
			Swap(CachedConnectionRestrictionsStart, CachedConnectionRestrictionsEnd);
		}
	}

	void FZGRoad::Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		const auto* S = Processor->GetSettings();
		const FZoneShapePointType DefaultPointType = S->RoadSettings.RoadPointType;

		TArray<int32> Nodes;
		const int32 ChainSize = Chain->GetNodes(Cluster, Nodes, bIsReversed);

		// Single-edge chains: GetNodes uses edge Start/End topology which may not match
		// the chain's Seed/Last ordering. Fix ordering for correct endpoint processing.
		if (ChainSize == 2 && !Chain->bIsClosedLoop)
		{
			const int32 ExpectedFirst = bIsReversed ? Chain->Links.Last().Node : Chain->Seed.Node;
			if (Nodes[0] != ExpectedFirst) { Swap(Nodes[0], Nodes[1]); }
		}

		PCGExArrayHelpers::InitArray(PrecomputedPoints, ChainSize);

		if (Chain->bIsClosedLoop)
		{
			const int32 FirstNode = Nodes[0];
			Nodes.Add(FirstNode);
		}

		for (int i = 0; i < ChainSize; i++)
		{
			const FVector Position = Cluster->GetPos(Nodes[i]);
			const int32 NodePointIndex = Cluster->GetNode(Nodes[i])->PointIndex;

			// Average of prev→current and current→next for a smoother direction hint.
			// Closed loops have Nodes[ChainSize] == Nodes[0], so i+1 is always valid.
			// Open chains: endpoints fall back to single-neighbor chord.
			FVector Forward;
			if (i == 0 && !Chain->bIsClosedLoop)
			{
				Forward = (Cluster->GetPos(Nodes[1]) - Position).GetSafeNormal();
			}
			else if (i == ChainSize - 1 && !Chain->bIsClosedLoop)
			{
				Forward = (Position - Cluster->GetPos(Nodes[i - 1])).GetSafeNormal();
			}
			else
			{
				const int32 PrevIdx = (i == 0) ? ChainSize - 1 : i - 1;
				const FVector DirPrev = (Position - Cluster->GetPos(Nodes[PrevIdx])).GetSafeNormal();
				const FVector DirNext = (Cluster->GetPos(Nodes[i + 1]) - Position).GetSafeNormal();
				Forward = (DirPrev + DirNext).GetSafeNormal();
				if (Forward.IsNearlyZero()) { Forward = DirNext; }
			}

			FZoneShapePoint ShapePoint = FZoneShapePoint(Position);
			ShapePoint.SetRotationFromForwardAndUp(Forward, FVector::UpVector);

			if (Processor->RoadPointTypeBuffer)
			{
				ShapePoint.Type = static_cast<FZoneShapePointType>(FMath::Clamp(Processor->RoadPointTypeBuffer->Read(NodePointIndex), 0, 3));
			}
			else
			{
				ShapePoint.Type = DefaultPointType;
			}

			if (S->RoadSettings.RoadTangentLengthMode == EPCGExZGTangentLengthMode::Manual && Processor->TangentLengthGetter)
			{
				ShapePoint.TangentLength = Processor->TangentLengthGetter->Read(NodePointIndex) * S->RoadSettings.TangentLengthScale;
			}

			PrecomputedPoints[i] = ShapePoint;
		}

		const PCGExClusters::FNode* FirstNode = Cluster->GetNode(Nodes[0]);
		const PCGExClusters::FNode* LastNode = Cluster->GetNode(Nodes.Last());

		if (!Chain->bIsClosedLoop)
		{
			const bool bTrim = S->RoadSettings.bTrimRoadEndpoints;
			const double BufferSq = S->RoadSettings.EndpointTrimBuffer * S->RoadSettings.EndpointTrimBuffer;

			// --- Start endpoint ---
			if (!FirstNode->IsLeaf())
			{
				if (StartEndpoint.bValid && bTrim)
				{
					// Walk backward from end to find the outermost half-space boundary crossing.
					bool bFoundCrossing = false;

					for (int32 j = PrecomputedPoints.Num() - 1; j > 0; j--)
					{
						const double ProjJ = (PrecomputedPoints[j].Position - StartEndpoint.PolygonCenter) | StartEndpoint.Direction;
						const double ProjPrev = (PrecomputedPoints[j - 1].Position - StartEndpoint.PolygonCenter) | StartEndpoint.Direction;

						if (ProjJ >= StartEndpoint.Radius && ProjPrev < StartEndpoint.Radius)
						{
							// Capture tangent lengths before removal for interpolation
							const double TL_Prev = PrecomputedPoints[j - 1].TangentLength;
							const double TL_J = PrecomputedPoints[j].TangentLength;

							PrecomputedPoints.RemoveAt(0, j);

							// Snap to polygon connector position for exact alignment
							const FVector SnapPos = StartEndpoint.PolygonCenter + StartEndpoint.Direction * StartEndpoint.Radius;
							FVector CrossingDir = (PrecomputedPoints[0].Position - SnapPos).GetSafeNormal();
							if (CrossingDir.IsNearlyZero()) { CrossingDir = StartEndpoint.Direction; }

							FZoneShapePoint CrossingPoint(SnapPos);
							CrossingPoint.SetRotationFromForwardAndUp(CrossingDir, FVector::UpVector);
							CrossingPoint.Type = DefaultPointType;

							if (S->RoadSettings.RoadTangentLengthMode == EPCGExZGTangentLengthMode::Manual)
							{
								const double Alpha = (StartEndpoint.Radius - ProjPrev) / (ProjJ - ProjPrev);
								CrossingPoint.TangentLength = FMath::Lerp(TL_Prev, TL_J, Alpha);
							}

							PrecomputedPoints.Insert(CrossingPoint, 0);

							// Remove nearby points that would cause auto-bezier bulging
							if (BufferSq > 0)
							{
								while (PrecomputedPoints.Num() > 2 &&
									(PrecomputedPoints[1].Position - PrecomputedPoints[0].Position).SizeSquared() < BufferSq)
								{
									PrecomputedPoints.RemoveAt(1);
								}
							}

							bFoundCrossing = true;
							break;
						}
					}

					if (!bFoundCrossing)
					{
						const double FirstProj = (PrecomputedPoints[0].Position - StartEndpoint.PolygonCenter) | StartEndpoint.Direction;
						if (FirstProj < StartEndpoint.Radius)
						{
							bDegenerate = true;
							return;
						}
					}
				}
				else
				{
					if (bIsReversed) { PrecomputedPoints[0].Position += PrecomputedPoints[0].Rotation.RotateVector(FVector::BackwardVector) * StartRadius; }
					else { PrecomputedPoints[0].Position += PrecomputedPoints[0].Rotation.RotateVector(FVector::ForwardVector) * StartRadius; }
				}
			}

			// --- End endpoint ---
			if (!LastNode->IsLeaf())
			{
				if (EndEndpoint.bValid && bTrim)
				{
					// Walk backward from end to find the outermost half-space boundary crossing.
					// Walking backward (not forward) prevents removing valid outside points
					// that appear after an intermediate inside dip on curved roads.
					bool bFoundCrossing = false;

					for (int32 j = PrecomputedPoints.Num() - 1; j > 0; j--)
					{
						const double ProjJ = (PrecomputedPoints[j].Position - EndEndpoint.PolygonCenter) | EndEndpoint.Direction;
						const double ProjPrev = (PrecomputedPoints[j - 1].Position - EndEndpoint.PolygonCenter) | EndEndpoint.Direction;

						if (ProjJ < EndEndpoint.Radius && ProjPrev >= EndEndpoint.Radius)
						{
							// Capture tangent lengths before removal for interpolation
							const double TL_Prev = PrecomputedPoints[j - 1].TangentLength;
							const double TL_J = PrecomputedPoints[j].TangentLength;

							PrecomputedPoints.RemoveAt(j, PrecomputedPoints.Num() - j);

							// Snap to polygon connector position for exact alignment
							const FVector SnapPos = EndEndpoint.PolygonCenter + EndEndpoint.Direction * EndEndpoint.Radius;
							FVector CrossingDir = (SnapPos - PrecomputedPoints.Last().Position).GetSafeNormal();
							if (CrossingDir.IsNearlyZero()) { CrossingDir = -EndEndpoint.Direction; }

							FZoneShapePoint CrossingPoint(SnapPos);
							CrossingPoint.SetRotationFromForwardAndUp(CrossingDir, FVector::UpVector);
							CrossingPoint.Type = DefaultPointType;

							if (S->RoadSettings.RoadTangentLengthMode == EPCGExZGTangentLengthMode::Manual)
							{
								const double Alpha = (EndEndpoint.Radius - ProjPrev) / (ProjJ - ProjPrev);
								CrossingPoint.TangentLength = FMath::Lerp(TL_Prev, TL_J, Alpha);
							}

							PrecomputedPoints.Add(CrossingPoint);

							// Remove nearby points that would cause auto-bezier bulging
							if (BufferSq > 0)
							{
								while (PrecomputedPoints.Num() > 2 &&
									(PrecomputedPoints[PrecomputedPoints.Num() - 2].Position - PrecomputedPoints.Last().Position).SizeSquared() < BufferSq)
								{
									PrecomputedPoints.RemoveAt(PrecomputedPoints.Num() - 2);
								}
							}

							bFoundCrossing = true;
							break;
						}
					}

					if (!bFoundCrossing)
					{
						const double LastProj = (PrecomputedPoints.Last().Position - EndEndpoint.PolygonCenter) | EndEndpoint.Direction;
						if (LastProj < EndEndpoint.Radius)
						{
							bDegenerate = true;
							return;
						}
					}
				}
				else
				{
					if (bIsReversed) { PrecomputedPoints.Last().Position += PrecomputedPoints.Last().Rotation.RotateVector(FVector::ForwardVector) * EndRadius; }
					else { PrecomputedPoints.Last().Position += PrecomputedPoints.Last().Rotation.RotateVector(FVector::BackwardVector) * EndRadius; }
				}
			}

			// Failsafe: ZoneGraph requires at least 2 shape points
			if (PrecomputedPoints.Num() < 2)
			{
				bDegenerate = true;
			}
		}

		// --- Auto/CatmullRom tangent pass ---
		// Computed from final PrecomputedPoints positions (after trimming, crossing points included).
		// Also overrides rotation with smooth tangent direction for Bezier point types.
		if (!bDegenerate && (S->RoadSettings.RoadTangentLengthMode == EPCGExZGTangentLengthMode::Auto || S->RoadSettings.RoadTangentLengthMode == EPCGExZGTangentLengthMode::CatmullRom))
		{
			const int32 Num = PrecomputedPoints.Num();
			const bool bLoop = Chain->bIsClosedLoop;
			const double Scale = S->RoadSettings.TangentLengthScale;
			const bool bCatmullRom = (S->RoadSettings.RoadTangentLengthMode == EPCGExZGTangentLengthMode::CatmullRom);

			for (int32 k = 0; k < Num; k++)
			{
				FVector Prev, Next;
				if (bLoop)
				{
					Prev = PrecomputedPoints[(k - 1 + Num) % Num].Position;
					Next = PrecomputedPoints[(k + 1) % Num].Position;
				}
				else if (k == 0)
				{
					Next = PrecomputedPoints[1].Position;
					Prev = PrecomputedPoints[0].Position - (Next - PrecomputedPoints[0].Position); // mirror
				}
				else if (k == Num - 1)
				{
					Prev = PrecomputedPoints[Num - 2].Position;
					Next = PrecomputedPoints[Num - 1].Position + (PrecomputedPoints[Num - 1].Position - Prev); // mirror
				}
				else
				{
					Prev = PrecomputedPoints[k - 1].Position;
					Next = PrecomputedPoints[k + 1].Position;
				}

				// Smooth tangent direction -- override rotation
				const FVector TangentDir = (Next - Prev).GetSafeNormal();
				if (!TangentDir.IsNearlyZero())
				{
					PrecomputedPoints[k].SetRotationFromForwardAndUp(TangentDir, FVector::UpVector);
				}

				// Tangent magnitude
				if (bCatmullRom)
				{
					PrecomputedPoints[k].TangentLength = FVector::Dist(Prev, Next) / 6.0 * Scale;
				}
				else // Auto
				{
					const double DistPrev = FVector::Dist(PrecomputedPoints[k].Position, Prev);
					const double DistNext = FVector::Dist(PrecomputedPoints[k].Position, Next);
					PrecomputedPoints[k].TangentLength = (DistPrev + DistNext) * 0.5 / 3.0 * Scale;
				}
			}
		}
	}

	void FZGRoad::Compile()
	{
		Component->SetShapeType(FZoneShapeType::Spline);
		Component->SetCommonLaneProfile(CachedLaneProfile);
		Component->GetMutablePoints() = MoveTemp(PrecomputedPoints);
		Component->UpdateShape();
	}

	void FZGRoad::BuildPathOutput(const TSharedPtr<PCGExData::FPointIO>& InPathIO) const
	{
		const auto* S = Processor->GetSettings();
		const TArrayView<const FZoneShapePoint> Points = Component->GetPoints();
		const int32 NumPoints = Points.Num();

		PCGExPointArrayDataHelpers::SetNumPointsAllocated(InPathIO->GetOut(), NumPoints);
		TPCGValueRange<FTransform> Transforms = InPathIO->GetOut()->GetTransformValueRange();

		for (int32 i = 0; i < NumPoints; i++)
		{
			const FZoneShapePoint& Pt = Points[i];
			Transforms[i] = FTransform(Pt.Rotation, Pt.Position);
		}

		PCGEX_MAKE_SHARED(PathFacade, PCGExData::FFacade, InPathIO.ToSharedRef())

		TSharedPtr<PCGExData::TBuffer<FVector>> ArriveWriter = PathFacade->GetWritable<FVector>(S->ArriveName, FVector::ZeroVector, true, PCGExData::EBufferInit::New);
		TSharedPtr<PCGExData::TBuffer<FVector>> LeaveWriter = PathFacade->GetWritable<FVector>(S->LeaveName, FVector::ZeroVector, true, PCGExData::EBufferInit::New);

		for (int32 i = 0; i < NumPoints; i++)
		{
			const FZoneShapePoint& Pt = Points[i];
			const FVector Forward = Pt.Rotation.RotateVector(FVector::ForwardVector);
			const double TL = Pt.TangentLength;

			ArriveWriter->SetValue(i, -Forward * TL);
			LeaveWriter->SetValue(i, Forward * TL);
		}

		PathFacade->WriteFastest(Processor->TaskManager);
	}

#pragma endregion

// Represents a polygon intersection shape, built from a non-leaf cluster node where multiple roads meet.
//
// Each road connecting to this polygon registers itself via Add(Road, bFromStart).
// FromStart[i] tracks whether road i connects at its logical start or end.
// This determines which of the road's cached Start/End values are used for that connection point.
//
// Precompute:
//   1. Reads per-vertex polygon properties (radius, routing type, point type, tags) from vertex buffers or defaults
//   2. Computes per-road radii based on auto-radius mode and lane profile widths
//   3. Sorts connections by angle around the polygon center to establish winding order
//   4. Builds FZoneShapePoint per connection with position, rotation (facing inward), lane profile,
//      bReverseLaneProfile, InnerTurnRadius, Roll, and ConnectionRestrictions from the connected road
//   5. Stores polygon boundary data (center, direction, radius) back on each road for endpoint trimming
//
// Compile (main thread): sets shape type, routing, tags, per-point lane profiles, then pushes points to the component.
#pragma region FZGPolygon

	FZGPolygon::FZGPolygon(FProcessor* InProcessor, const PCGExClusters::FNode* InNode)
		: FZGBase(InProcessor), NodeIndex(InNode->Index)
	{
		FromStart.Init(false, InNode->Num());
		Roads.Reserve(InNode->Num());
	}

	void FZGPolygon::Add(const TSharedPtr<FZGRoad>& InRoad, bool bFromStart)
	{
		FromStart[Roads.Add(InRoad)] = bFromStart;
	}

	void FZGPolygon::Precompute(const TSharedPtr<PCGExClusters::FCluster>& Cluster)
	{
		const auto* S = Processor->GetSettings();
		const auto* P = Processor;
		const PCGExClusters::FNode* Center = Cluster->GetNode(NodeIndex);
		const int32 PointIndex = Center->PointIndex;
		const FVector CenterPosition = Cluster->GetPos(Center);

		CachedRadius = P->PolygonRadiusBuffer ? P->PolygonRadiusBuffer->Read(PointIndex) : S->PolygonSettings.PolygonRadius;
		CachedRoutingType = P->PolygonRoutingTypeBuffer ? static_cast<EZoneShapePolygonRoutingType>(FMath::Clamp(P->PolygonRoutingTypeBuffer->Read(PointIndex), 0, 1)) : S->PolygonSettings.PolygonRoutingType;
		CachedPointType = P->PolygonPointTypeBuffer ? static_cast<FZoneShapePointType>(FMath::Clamp(P->PolygonPointTypeBuffer->Read(PointIndex), 0, 3)) : S->PolygonSettings.PolygonPointType;
		if (P->AdditionalIntersectionTagsBuffer)
		{
			const int32 RawTags = P->AdditionalIntersectionTagsBuffer->Read(PointIndex);
			if (S->PolygonSettings.IntersectionTagsMode == EPCGExZGBitmaskMode::ValueMap)
			{
				const FZoneGraphTagMask* Mapped = S->PolygonSettings.IntersectionTagsValueMap.Find(RawTags);
				CachedAdditionalTags = Mapped ? *Mapped : FZoneGraphTagMask::None;
			}
			else
			{
				CachedAdditionalTags = FZoneGraphTagMask(static_cast<uint32>(RawTags));
			}
			// Merge with default tags
			CachedAdditionalTags = CachedAdditionalTags | S->PolygonSettings.AdditionalIntersectionTags;
		}
		else
		{
			CachedAdditionalTags = S->PolygonSettings.AdditionalIntersectionTags;
		}
		CachedLaneProfile = S->RoadSettings.LaneProfile;

		// Compute per-road radii based on auto-radius mode
		CachedRoadRadii.SetNum(Roads.Num());
		for (int32 i = 0; i < Roads.Num(); i++)
		{
			double Radius = CachedRadius;

			if (S->PolygonSettings.AutoRadiusMode != EPCGExZGAutoRadiusMode::Disabled)
			{
				const double MaxLane = Roads[i]->CachedMaxLaneWidth;
				const double HalfProfile = Roads[i]->CachedTotalProfileWidth * 0.5;

				switch (S->PolygonSettings.AutoRadiusMode)
				{
				case EPCGExZGAutoRadiusMode::WidestLane:
					Radius = MaxLane;
					break;
				case EPCGExZGAutoRadiusMode::HalfProfile:
					Radius = HalfProfile;
					break;
				case EPCGExZGAutoRadiusMode::WidestLaneMin:
					Radius = FMath::Max(Radius, MaxLane);
					break;
				case EPCGExZGAutoRadiusMode::HalfProfileMin:
					Radius = FMath::Max(Radius, HalfProfile);
					break;
				default: break;
				}
			}

			CachedRoadRadii[i] = Radius;
		}

		TArray<int32> Order;
		PCGExArrayHelpers::ArrayOfIndices(Order, Roads.Num());
		Order.Sort(
			[&](const int32 A, const int32 B)
			{
				const bool bSeedA = (NodeIndex == Roads[A]->Chain->Seed.Node);
				const bool bEndA = (NodeIndex == Roads[A]->Chain->Links.Last().Node);
				const FVector DirA = Roads[A]->Chain->GetEdgeDir(Cluster, (bSeedA && bEndA) ? FromStart[A] : bSeedA);

				const bool bSeedB = (NodeIndex == Roads[B]->Chain->Seed.Node);
				const bool bEndB = (NodeIndex == Roads[B]->Chain->Links.Last().Node);
				const FVector DirB = Roads[B]->Chain->GetEdgeDir(Cluster, (bSeedB && bEndB) ? FromStart[B] : bSeedB);

				return PCGExMath::GetRadiansBetweenVectors(DirA, FVector::ForwardVector) > PCGExMath::GetRadiansBetweenVectors(DirB, FVector::ForwardVector);
			});

		PCGExArrayHelpers::InitArray(PrecomputedPoints, Order.Num());
		CachedPointLaneProfiles.SetNum(Order.Num());
		CachedPointHalfWidths.SetNum(Order.Num());

		for (int i = 0; i < Order.Num(); i++)
		{
			const int32 Ri = Order[i];
			const TSharedPtr<FZGRoad>& Road = Roads[Ri];
			const bool bAtChainSeed = (NodeIndex == Road->Chain->Seed.Node);
			const bool bAtChainEnd = (NodeIndex == Road->Chain->Links.Last().Node);

			// For lollipop chains (single breakpoint on closed loop), seed==end node.
			// Use FromStart to disambiguate: start connection → first edge dir, end connection → last edge dir.
			const FVector RoadDirection = Road->Chain->GetEdgeDir(Cluster, (bAtChainSeed && bAtChainEnd) ? FromStart[Ri] : bAtChainSeed);

			// Store polygon boundary data on the road for precise intersection
			FZGRoad::FPolygonEndpoint EP;
			EP.PolygonCenter = CenterPosition;
			EP.Direction = RoadDirection;
			EP.Radius = CachedRoadRadii[Ri];
			EP.bValid = true;

			if (FromStart[Ri]) { Road->StartEndpoint = EP; }
			else { Road->EndEndpoint = EP; }

			FZoneShapePoint ShapePoint = FZoneShapePoint(CenterPosition + RoadDirection * CachedRoadRadii[Ri]);
			ShapePoint.SetRotationFromForwardAndUp(RoadDirection * -1, FVector::UpVector);
			ShapePoint.Type = CachedPointType;
			ShapePoint.bReverseLaneProfile = FromStart[Ri] != Road->bReverseLaneProfileOverride;

			// Per-road polygon point properties
			ShapePoint.InnerTurnRadius = FromStart[Ri] ? Road->CachedInnerTurnRadiusStart : Road->CachedInnerTurnRadiusEnd;
			ShapePoint.Rotation.Roll = FromStart[Ri] ? Road->CachedRollStart : Road->CachedRollEnd;
			ShapePoint.LaneConnectionRestrictions = FromStart[Ri] ? Road->CachedConnectionRestrictionsStart : Road->CachedConnectionRestrictionsEnd;

			PrecomputedPoints[i] = ShapePoint;
			CachedPointLaneProfiles[i] = Road->CachedLaneProfile;
			CachedPointHalfWidths[i] = Road->CachedTotalProfileWidth * 0.5;
		}
	}

	void FZGPolygon::SyncRadiusToRoads()
	{
		for (int32 i = 0; i < Roads.Num(); i++)
		{
			if (FromStart[i])
			{
				Roads[i]->StartRadius = CachedRoadRadii[i];
			}
			else
			{
				Roads[i]->EndRadius = CachedRoadRadii[i];
			}
		}
	}

	void FZGPolygon::BuildPathOutput(const TSharedPtr<PCGExData::FPointIO>& InPathIO) const
	{
		const TArrayView<const FZoneShapePoint> Points = Component->GetPoints();
		const int32 NumConnections = Points.Num();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(InPathIO->GetOut(), NumConnections * 2);

		TPCGValueRange<FTransform> Transforms = InPathIO->GetOut()->GetTransformValueRange();
		for (int32 i = 0; i < NumConnections; i++)
		{
			const FZoneShapePoint& Pt = Points[i];
			const double HalfWidth = Pt.TangentLength;

			const FVector Left = Pt.Position + Pt.Rotation.RotateVector(FVector::LeftVector) * HalfWidth;
			const FVector Right = Pt.Position + Pt.Rotation.RotateVector(FVector::RightVector) * HalfWidth;

			Transforms[i * 2] = FTransform(Pt.Rotation, Right);
			Transforms[i * 2 + 1] = FTransform(Pt.Rotation, Left);
		}
	}

	void FZGPolygon::Compile()
	{
		Component->SetShapeType(FZoneShapeType::Polygon);
		Component->SetPolygonRoutingType(CachedRoutingType);
		Component->SetTags(Component->GetTags() | CachedAdditionalTags);
		Component->SetCommonLaneProfile(CachedLaneProfile);

		// Register per-point lane profiles so each polygon connection uses its road's profile
		for (int32 i = 0; i < PrecomputedPoints.Num(); i++)
		{
			const int32 ProfileIdx = Component->AddUniquePerPointLaneProfile(CachedPointLaneProfiles[i]);
			PrecomputedPoints[i].LaneProfile = static_cast<uint8>(ProfileIdx);
		}

		Component->GetMutablePoints() = MoveTemp(PrecomputedPoints);
		Component->UpdateShape();
	}

#pragma endregion

// Per-cluster processor. Orchestrates the full conversion pipeline:
//
// Process():
//   - Initializes direction settings and buffer getters (vertex and edge sourced)
//   - Runs vertex filters to identify breakpoints, then builds chains
//   - Optionally runs edge filters for the lane profile flip feature
//
// CompleteWork() (off main thread):
//   - Computes road orientation (DFS / global direction / sort direction)
//   - Creates FZGRoad and FZGPolygon instances from chains and non-leaf nodes
//   - Runs the resolution pipeline in phases:
//       Phase 1:  Resolve lane profiles (majority vote across chain edges)
//       Phase 1b: Resolve reverse lane profile override (edge filter majority vote)
//       Phase 1c: Resolve polygon point properties (InnerTurnRadius, Roll, ConnectionRestrictions from edge endpoints)
//       Phase 2:  Polygon precompute (reads vertex properties, builds connection points, sorts by angle)
//       Phase 3:  Sync polygon radii back to road endpoints
//       Phase 4:  Road precompute (build shape points, trim at polygon boundaries, compute tangents)
//   - Queues a time-sliced main-thread loop for component creation and compilation
//
// The main-thread loop (MainCompileLoop) handles InitComponent, Compile, and optional path output
// for both polygons and roads, since UZoneShapeComponent creation requires the game thread.
#pragma region FProcessor

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExClusterToZoneGraph::Process);

		DefaultEdgeFilterValue = false;
		EdgeFilterFactories = &Context->FlipEdgeFilterFactories; // So filters can be initialized
		
		if (!IProcessor::Process(InTaskManager)) { return false; }

		if (!DirectionSettings.InitFromParent(ExecutionContext, GetParentBatch<FBatch>()->DirectionSettings, EdgeDataFacade)) { return false; }

		const FPCGExZGPolygonSettings& PS = Settings->PolygonSettings;
		const FPCGExZGRoadSettings& RS = Settings->RoadSettings;

		if (PS.bOverridePolygonRadius) { PolygonRadiusBuffer = VtxDataFacade->GetBroadcaster<double>(PS.PolygonRadiusAttribute); }
		if (PS.bOverridePolygonRoutingType) { PolygonRoutingTypeBuffer = VtxDataFacade->GetBroadcaster<int32>(PS.PolygonRoutingTypeAttribute); }
		if (PS.bOverridePolygonPointType) { PolygonPointTypeBuffer = VtxDataFacade->GetBroadcaster<int32>(PS.PolygonPointTypeAttribute); }
		if (RS.bOverrideRoadPointType) { RoadPointTypeBuffer = VtxDataFacade->GetBroadcaster<int32>(RS.RoadPointTypeAttribute); }
		if (PS.IntersectionTagsMode != EPCGExZGBitmaskMode::None) { AdditionalIntersectionTagsBuffer = VtxDataFacade->GetBroadcaster<int32>(PS.IntersectionTagsAttribute); }
		if (Settings->RoadSettings.bOverrideLaneProfile) { EdgeLaneProfileBuffer = EdgeDataFacade->GetBroadcaster<FName>(Settings->RoadSettings.LaneProfileAttribute); }

		if (RS.RoadTangentLengthMode == EPCGExZGTangentLengthMode::Manual)
		{
			TangentLengthGetter = RS.TangentLength.GetValueSetting();
			if (!TangentLengthGetter->Init(VtxDataFacade, false)) { return false; }
		}

		// Edge-sourced polygon point property getters
		{
			InnerTurnRadiusStartGetter = PS.InnerTurnRadiusPrimary.GetValueSetting();
			if (!InnerTurnRadiusStartGetter->Init(EdgeDataFacade, false)) { InnerTurnRadiusStartGetter.Reset(); }

			if (PS.bSeparateInnerTurnRadiusSecondary)
			{
				InnerTurnRadiusEndGetter = PS.InnerTurnRadiusSecondary.GetValueSetting();
				if (!InnerTurnRadiusEndGetter->Init(EdgeDataFacade, false)) { InnerTurnRadiusEndGetter.Reset(); }
			}

			RollStartGetter = PS.RollPrimary.GetValueSetting();
			if (!RollStartGetter->Init(EdgeDataFacade, false)) { RollStartGetter.Reset(); }

			if (PS.bSeparateRollSecondary)
			{
				RollEndGetter = PS.RollSecondary.GetValueSetting();
				if (!RollEndGetter->Init(EdgeDataFacade, false)) { RollEndGetter.Reset(); }
			}

			if (PS.ConnectionRestrictionMode != EPCGExZGBitmaskMode::None)
			{
				ConnectionRestrictionsBuffer = EdgeDataFacade->GetBroadcaster<int32>(PS.ConnectionRestrictionsAttribute);
			}
		}

		if (VtxFiltersManager)
		{
			PCGEX_ASYNC_GROUP_CHKD(TaskManager, FilterBreakpoints)

			FilterBreakpoints->OnCompleteCallback =
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS
					This->BuildChains();
				};

			FilterBreakpoints->OnSubLoopStartCallback =
				[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
				{
					PCGEX_ASYNC_THIS
					This->FilterVtxScope(Scope);
				};

			FilterBreakpoints->StartSubLoops(NumNodes, GetDefault<UPCGExGlobalSettings>()->GetClusterBatchChunkSize());
		}
		else
		{
			return BuildChains();
		}

		return true;
	}

	bool FProcessor::BuildChains()
	{
		bIsProcessorValid = PCGExClusters::ChainHelpers::GetOrBuildChains(
			Cluster.ToSharedRef(),
			ProcessedChains,
			VtxFilterCache,
			false);

		if (!bIsProcessorValid) { return false; }

		Polygons.Reserve(NumNodes / 2);

		if (Settings->bEnableFlipFilters && !EdgeFilterFactories->IsEmpty())
		{
			StartParallelLoopForEdges();
		}
		
		return bIsProcessorValid;
	}

	void FProcessor::CompleteWork()
	{
		if (ProcessedChains.IsEmpty())
		{
			bIsProcessorValid = false;
			return;
		}

		TMap<int32, TSharedPtr<FZGPolygon>> Map;

		const int32 NumChains = ProcessedChains.Num();

		Roads.Reserve(NumChains);

		TArray<bool> DFSReversed;
		if (Settings->OrientationMode == EPCGExZGOrientationMode::DepthFirst) { ComputeDFSOrientation(DFSReversed); }

		for (int i = 0; i < NumChains; i++)
		{
			const TSharedPtr<PCGExClusters::FNodeChain>& Chain = ProcessedChains[i];
			if (!Chain) { continue; }

			int32 StartNode = Chain->Seed.Node;
			int32 EndNode = Chain->Links.Last().Node;

			bool bReverse;
			switch (Settings->OrientationMode)
			{
			case EPCGExZGOrientationMode::DepthFirst:
				bReverse = DFSReversed[i] != Settings->bInvertOrientation;
				if (bReverse) { Swap(StartNode, EndNode); }
				break;

			case EPCGExZGOrientationMode::GlobalDirection:
				{
					const FVector RoadDir = (Cluster->GetPos(EndNode) - Cluster->GetPos(StartNode)).GetSafeNormal();
					bReverse = (FVector::DotProduct(RoadDir, Settings->OrientationDirection) < 0) != Settings->bInvertOrientation;
					if (bReverse) { Swap(StartNode, EndNode); }
				}
				break;

			default: // SortDirection
				bReverse = DirectionSettings.SortExtrapolation(Cluster.Get(), Chain->Seed.Edge, StartNode, EndNode);
				break;
			}

			TSharedPtr<FZGRoad> Road = MakeShared<FZGRoad>(this, Chain, bReverse);
			Roads.Add(Road);

			const PCGExClusters::FNode* Start = Cluster->GetNode(StartNode);
			const PCGExClusters::FNode* End = Cluster->GetNode(EndNode);

			if (Chain->bIsClosedLoop && Start->IsBinary() && End->IsBinary())
			{
				// Roaming closed loop, road only!
				continue;
			}

			if (!Start->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(StartNode);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(this, Start);
					Polygons.Add(NewPolygon);
					Map.Add(StartNode, NewPolygon);
					PolygonPtr = &NewPolygon;
				}
				(*PolygonPtr)->Add(Road, true);
			}

			if (!End->IsLeaf())
			{
				const TSharedPtr<FZGPolygon>* PolygonPtr = Map.Find(EndNode);

				if (!PolygonPtr)
				{
					TSharedPtr<FZGPolygon> NewPolygon = MakeShared<FZGPolygon>(this, End);
					Polygons.Add(NewPolygon);
					Map.Add(EndNode, NewPolygon);
					PolygonPtr = &NewPolygon;
				}

				(*PolygonPtr)->Add(Road, false);
			}
		}

		// Precompute all geometry off main thread
		// Phase 1: Resolve lane profiles + cache widths (needed by auto-radius)
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Road->ResolveLaneProfile(Cluster); }
		// Phase 1b: Resolve per-road reverse lane profile override (majority vote)
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Road->ResolveReverseLaneProfile(Cluster); }
		// Phase 1c: Resolve per-road polygon point properties (InnerTurnRadius, Roll, ConnectionRestrictions)
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Road->ResolvePolygonPointProperties(Cluster); }
		// Phase 2: Polygon precompute (uses road widths for auto-radius)
		for (const TSharedPtr<FZGPolygon>& Polygon : Polygons) { Polygon->Precompute(Cluster); }
		// Phase 3: Push final polygon radii back to road endpoints
		for (const TSharedPtr<FZGPolygon>& Polygon : Polygons) { Polygon->SyncRadiusToRoads(); }
		// Phase 4: Road precompute (uses synced radii for endpoint offsets)
		for (const TSharedPtr<FZGRoad>& Road : Roads) { Road->Precompute(Cluster); }

		const int32 NumPolygons = Polygons.Num();
		const int32 TotalCount = NumPolygons + Roads.Num();

		if (TotalCount == 0) { return; }

		CachedAttachmentRules = Settings->AttachmentRules.GetRules();

		const int32 IOBase = (VtxDataFacade->Source->IOIndex + 1) * 100000;

		// Create the time-sliced main-thread loop and register it as a handle.
		// The registered handle prevents the task manager from completing until
		// all iterations finish. No separate token or deferred tick action needed.
		MainCompileLoop = MakeShared<PCGExMT::FTimeSlicedMainThreadLoop>(TotalCount);
		MainCompileLoop->OnIterationCallback = [PCGEX_ASYNC_THIS_CAPTURE, NumPolygons, IOBase](const int32 Index, const PCGExMT::FScope& Scope)
		{
			PCGEX_ASYNC_THIS

			// Resolve TargetActor lazily on first iteration (runs on main thread)
			if (Index == 0)
			{
				This->TargetActor = This->ExecutionContext->GetTargetActor(nullptr);
				if (!This->TargetActor)
				{
					PCGE_LOG_C(Error, GraphAndLog, This->ExecutionContext, FTEXT("Invalid target actor."));
					This->bIsProcessorValid = false;
				}
			}

			if (!This->TargetActor) { return; }

			if (Index < NumPolygons)
			{
				auto& Polygon = This->Polygons[Index];
				Polygon->InitComponent(This->TargetActor);
				This->Context->AttachManagedComponent(This->TargetActor, Polygon->Component, This->CachedAttachmentRules);
				Polygon->Compile();

				if (This->Context->OutputPolygonPaths)
				{
					const int32 PointIndex = This->Cluster->GetNode(Polygon->NodeIndex)->PointIndex;
					TSharedPtr<PCGExData::FPointIO> PathIO = This->Context->OutputPolygonPaths->Emplace_GetRef(This->VtxDataFacade->Source, PCGExData::EIOInit::New);
					PathIO->IOIndex = IOBase + PointIndex;
					Polygon->BuildPathOutput(PathIO);
					PCGExPaths::Helpers::SetClosedLoop(PathIO, true);
				}
			}
			else
			{
				const int32 RoadIndex = Index - NumPolygons;
				auto& Road = This->Roads[RoadIndex];
				if (Road->bDegenerate) { return; }
				Road->InitComponent(This->TargetActor);
				This->Context->AttachManagedComponent(This->TargetActor, Road->Component, This->CachedAttachmentRules);
				Road->Compile();

				if (This->Context->OutputRoadPaths)
				{
					TSharedPtr<PCGExData::FPointIO> PathIO = This->Context->OutputRoadPaths->Emplace_GetRef(This->VtxDataFacade->Source, PCGExData::EIOInit::New);
					PathIO->IOIndex = IOBase + This->Cluster->GetNode(Road->Chain->Seed.Node)->PointIndex;
					Road->BuildPathOutput(PathIO);
				}
			}
		};

		MainCompileLoop->OnCompleteCallback = [PCGEX_ASYNC_THIS_CAPTURE]()
		{
			PCGEX_ASYNC_THIS
			if (This->TargetActor) { This->Context->AddNotifyActor(This->TargetActor); }
		};

		PCGEX_ASYNC_HANDLE_CHKD_VOID(TaskManager, MainCompileLoop)
	}

	void FProcessor::ProcessEdges(const PCGExMT::FScope& Scope)
	{
		//EdgeDataFacade->Fetch(Scope);
		FilterEdgeScope(Scope);
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		// No longer used - road compilation moved to main thread via RoadCompileLoop
	}

	void FProcessor::OnRangeProcessingComplete()
	{
	}

	void FProcessor::Output()
	{
		// Component creation, attachment, and notify are handled in MainCompileLoop
		// which runs on the main thread via the time-sliced loop mechanism.
	}

	void FProcessor::ComputeDFSOrientation(TArray<bool>& OutReversed) const
	{
		const int32 NumChains = ProcessedChains.Num();
		OutReversed.Init(false, NumChains);

		// BFS to assign depths to polygon nodes, then orient chains from lower to higher depth.
		// Leaf edges always flow toward the polygon (leaf is start, polygon is end).
		// This produces consistent lane profiles at intersections: incoming roads face toward
		// the polygon and outgoing roads face away, giving the same global forward direction
		// for through-traffic.

		struct FAdj
		{
			int32 ChainIdx;
			int32 OtherNode;
			bool bIsSeed;
		};

		TMap<int32, TArray<FAdj>> NodeAdj;

		for (int32 i = 0; i < NumChains; i++)
		{
			const auto& Chain = ProcessedChains[i];
			if (!Chain) { continue; }

			const int32 SN = Chain->Seed.Node;
			const int32 EN = Chain->Links.Last().Node;

			if (!Cluster->GetNode(SN)->IsLeaf()) { NodeAdj.FindOrAdd(SN).Add({i, EN, true}); }
			if (!Cluster->GetNode(EN)->IsLeaf()) { NodeAdj.FindOrAdd(EN).Add({i, SN, false}); }
		}

		// BFS to assign depths
		TMap<int32, int32> NodeDepth;
		TArray<int32> Queue;

		for (auto& [Node, _] : NodeAdj)
		{
			if (NodeDepth.Contains(Node)) { continue; }
			NodeDepth.Add(Node, 0);
			Queue.Add(Node);

			int32 Head = Queue.Num() - 1;
			while (Head < Queue.Num())
			{
				const int32 Current = Queue[Head++];
				const int32 CurrDepth = NodeDepth[Current];

				if (const TArray<FAdj>* Neighbors = NodeAdj.Find(Current))
				{
					for (const FAdj& A : *Neighbors)
					{
						if (!Cluster->GetNode(A.OtherNode)->IsLeaf() && !NodeDepth.Contains(A.OtherNode))
						{
							NodeDepth.Add(A.OtherNode, CurrDepth + 1);
							Queue.Add(A.OtherNode);
						}
					}
				}
			}
		}

		// Orient chains based on depth ordering
		for (int32 i = 0; i < NumChains; i++)
		{
			const auto& Chain = ProcessedChains[i];
			if (!Chain) { continue; }

			const int32 SN = Chain->Seed.Node;
			const int32 EN = Chain->Links.Last().Node;
			const bool bSeedIsLeaf = Cluster->GetNode(SN)->IsLeaf();
			const bool bEndIsLeaf = Cluster->GetNode(EN)->IsLeaf();

			if (bSeedIsLeaf && !bEndIsLeaf)
			{
				// Seed is leaf, End is polygon → flow leaf→polygon → no reverse
				OutReversed[i] = false;
			}
			else if (!bSeedIsLeaf && bEndIsLeaf)
			{
				// Seed is polygon, End is leaf → flow leaf→polygon → reverse
				OutReversed[i] = true;
			}
			else if (!bSeedIsLeaf && !bEndIsLeaf)
			{
				// Both polygon nodes → flow from lower BFS depth to higher
				const int32 SeedDepth = NodeDepth.FindRef(SN);
				const int32 EndDepth = NodeDepth.FindRef(EN);

				if (SeedDepth > EndDepth || (SeedDepth == EndDepth && SN > EN))
				{
					OutReversed[i] = true;
				}
			}
			// Both leaf → keep default (false)
		}
	}

	FZoneLaneProfileRef FProcessor::ResolveLaneProfileByName(FName ProfileName) const
	{
		if (ProfileName.IsNone()) { return Settings->RoadSettings.LaneProfile; }
		if (const FZoneLaneProfileRef* Found = Context->LaneProfileMap.Find(ProfileName))
		{
			return *Found;
		}
		return Settings->RoadSettings.LaneProfile;
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExClusterToZoneGraphContext, UPCGExClusterToZoneGraphSettings>::Cleanup();
		TargetActor = nullptr;
		ProcessedChains.Empty();
		Roads.Empty();
		Polygons.Empty();

		PolygonRadiusBuffer.Reset();
		PolygonRoutingTypeBuffer.Reset();
		PolygonPointTypeBuffer.Reset();
		RoadPointTypeBuffer.Reset();
		AdditionalIntersectionTagsBuffer.Reset();
		EdgeLaneProfileBuffer.Reset();
		TangentLengthGetter.Reset();
		InnerTurnRadiusStartGetter.Reset();
		InnerTurnRadiusEndGetter.Reset();
		RollStartGetter.Reset();
		RollEndGetter.Reset();
		ConnectionRestrictionsBuffer.Reset();
	}

#pragma endregion

// Batch-level setup. Runs once per vertex/edge set before processors are created.
// Handles direction settings initialization and vertex buffer preloading.
// Edge buffers (lane profile, polygon point properties) are lazily initialized in FProcessor::Process()
// since the edge facade preloader isn't available here.
#pragma region FBatch

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TBatch<FProcessor>::RegisterBuffersDependencies(FacadePreloader);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)
		DirectionSettings.RegisterBuffersDependencies(ExecutionContext, FacadePreloader);

		const FPCGExZGPolygonSettings& PS = Settings->PolygonSettings;
		const FPCGExZGRoadSettings& RS = Settings->RoadSettings;

		if (PS.bOverridePolygonRadius) { FacadePreloader.Register<double>(ExecutionContext, PS.PolygonRadiusAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (PS.bOverridePolygonRoutingType) { FacadePreloader.Register<int32>(ExecutionContext, PS.PolygonRoutingTypeAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (PS.bOverridePolygonPointType) { FacadePreloader.Register<int32>(ExecutionContext, PS.PolygonPointTypeAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (RS.bOverrideRoadPointType) { FacadePreloader.Register<int32>(ExecutionContext, RS.RoadPointTypeAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }
		if (PS.IntersectionTagsMode != EPCGExZGBitmaskMode::None) { FacadePreloader.Register<int32>(ExecutionContext, PS.IntersectionTagsAttribute, PCGExData::EBufferPreloadType::BroadcastFromName); }

		if (RS.RoadTangentLengthMode == EPCGExZGTangentLengthMode::Manual)
		{
			RS.TangentLength.RegisterBufferDependencies(ExecutionContext, FacadePreloader);
		}
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(ClusterToZoneGraph)

		DirectionSettings = Settings->DirectionSettings;
		if (!DirectionSettings.Init(Context, VtxDataFacade, Context->GetEdgeSortingRules()))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Some vtx are missing the specified Direction attribute."));
			return;
		}

		TBatch<FProcessor>::OnProcessingPreparationComplete();
	}

#pragma endregion

}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE
