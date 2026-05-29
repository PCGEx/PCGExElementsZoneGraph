// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExZGMerger.h"

#include "Graph/PCGExClusterToZoneGraph.h"

#include "PCGExCoreMacros.h"
#include "Clusters/PCGExCluster.h"
#include "Clusters/Artifacts/PCGExChain.h"
#include "Core/PCGExClusterFilter.h"
#include "Core/PCGExPointFilter.h"
#include "Data/PCGExData.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Math/PCGExMathAxis.h"

#define LOCTEXT_NAMESPACE "PCGExClusterToZoneGraph"

namespace PCGExClusterToZoneGraph
{
	FZGMerger::FZGMerger(FProcessor* InProcessor)
		: Processor(InProcessor)
	{
	}

	bool FZGMerger::Init(const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories)
	{
		const FPCGExZGMergeSettings& MS = Processor->GetSettings()->MergeSettings;

		// Optional int32 group-id gate. Missing attribute -> warn and fall back to filter+guards only
		// (consistent with how the node handles other optional broadcasters).
		if (MS.bUseMergeGroups)
		{
			GroupIdBuffer = Processor->VtxDataFacade->GetBroadcaster<int32>(MS.MergeGroupAttribute);
			if (!GroupIdBuffer)
			{
				// Grouping was explicitly requested but the attribute is missing. Fail SAFE: disable merging
				// entirely rather than silently dropping the group restriction (which would over-merge).
				PCGEX_LOG_INVALID_ATTR_C(Processor->GetContext(), MergeGroup, MS.MergeGroupAttribute)
				bMergeGroupsUnavailable = true;
			}
		}

		const bool bHasFilters = InFilterFactories && !InFilterFactories->IsEmpty();

		// Eligibility cache, indexed by vtx PointIndex. With a filter wired, default to "not eligible" and
		// let passing nodes flip true. With no filter wired, eligibility is unrestricted -- group / guards drive.
		EligibleCache = MakeShared<TArray<int8>>();
		EligibleCache->Init(bHasFilters ? 0 : 1, Processor->VtxDataFacade->GetNum());

		if (bHasFilters)
		{
			// Its own manager -- the built-in VtxFiltersManager slot is already consumed by breakpoints.
			EligibilityManager = MakeShared<PCGExClusterFilter::FManager>(Processor->Cluster.ToSharedRef(), Processor->VtxDataFacade, Processor->EdgeDataFacade);
			EligibilityManager->SetSupportedTypes(&PCGExFactories::ClusterNodeFilters);
			if (!EligibilityManager->Init(Processor->GetContext(), *InFilterFactories))
			{
				return false;
			}
		}

		return true;
	}

	void FZGMerger::EvaluateEligibility()
	{
		if (!EligibilityManager)
		{
			return; // cache already defaulted (all eligible when no filter, none when filter init produced nothing)
		}

		const TSharedPtr<PCGExClusters::FCluster>& Cluster = Processor->Cluster;
		EligibilityManager->Test(MakeArrayView(*Cluster->Nodes.Get()), EligibleCache, false);
	}

	bool FZGMerger::IsCandidate(const int32 NodeIndex) const
	{
		const int32 PtIndex = Processor->Cluster->GetNode(NodeIndex)->PointIndex;
		return EligibleCache.IsValid() && static_cast<bool>((*EligibleCache)[PtIndex]);
	}

	bool FZGMerger::IsSeam(const FZGRoad& Road) const
	{
		// A seam joins two polygon nodes; a leaf-attached endpoint stays invalid.
		// Note: a road flagged bDegenerate is intentionally still seam-eligible. A poly<->poly road only
		// degenerates when it gets swallowed on trim, i.e. its two polygons overlap -- exactly the case
		// merging should collapse, and fusing also removes the otherwise-dangling connection points.
		// Eligibility is decided by endpoint geometry below, not by whether the road spline survived.
		if (!Road.StartEndpoint.bValid || !Road.EndEndpoint.bValid)
		{
			return false;
		}

		const int32 NodeA = Road.StartEndpoint.NodeIndex;
		const int32 NodeB = Road.EndEndpoint.NodeIndex;
		if (NodeA < 0 || NodeB < 0 || NodeA == NodeB)
		{
			return false; // self-connecting loop / unset
		}

		// Master gate: both polygons must pass the Merge Conditions filter.
		if (!IsCandidate(NodeA) || !IsCandidate(NodeB))
		{
			return false;
		}

		// Optional group gate: 0 = opt out (either endpoint), otherwise the two ids must relate per Match.
		if (GroupIdBuffer)
		{
			const int32 Ga = GroupIdBuffer->Read(Processor->Cluster->GetNode(NodeA)->PointIndex);
			const int32 Gb = GroupIdBuffer->Read(Processor->Cluster->GetNode(NodeB)->PointIndex);
			if (Ga == 0 || Gb == 0)
			{
				return false;
			}

			const FPCGExZGMergeSettings& MS = Processor->GetSettings()->MergeSettings;
			bool bMatch;
			switch (MS.MergeGroupMatch)
			{
			default:
			case EPCGExZGMergeMatch::Equal:
				bMatch = (Ga == Gb);
				break;
			case EPCGExZGMergeMatch::NotEqual:
				bMatch = (Ga != Gb);
				break;
			case EPCGExZGMergeMatch::Near:
				bMatch = FMath::Abs(static_cast<int64>(Ga) - static_cast<int64>(Gb)) <= MS.MergeGroupTolerance;
				break;
			case EPCGExZGMergeMatch::NotNear:
				bMatch = FMath::Abs(static_cast<int64>(Ga) - static_cast<int64>(Gb)) > MS.MergeGroupTolerance;
				break;
			}
			if (!bMatch)
			{
				return false;
			}
		}

		return PassesGuards(Road);
	}

	bool FZGMerger::PassesGuards(const FZGRoad& Road) const
	{
		const FPCGExZGMergeSettings& MS = Processor->GetSettings()->MergeSettings;
		const FZGRoad::FPolygonEndpoint& EPa = Road.StartEndpoint;
		const FZGRoad::FPolygonEndpoint& EPb = Road.EndEndpoint;

		bool bAny = false;
		bool bAll = true;
		bool bAnyEnabled = false;

		auto Apply = [&](const bool bPass)
		{
			bAnyEnabled = true;
			bAny |= bPass;
			bAll &= bPass;
		};

		if (MS.bGuardFaceGap)
		{
			// Signed gap between the two facing connection faces, measured along the center-to-center axis.
			// > 0 : road remains between them; 0 : touching; < 0 : faces have crossed (overlap).
			const FVector Pa = EPa.PolygonCenter + EPa.Direction * EPa.Radius;
			const FVector Pb = EPb.PolygonCenter + EPb.Direction * EPb.Radius;
			const FVector Delta = EPb.PolygonCenter - EPa.PolygonCenter;
			// Coincident polygon centers have no defined axis; treat them as maximally overlapping
			// (deepest possible negative gap) so the verdict is meaningful for any threshold sign.
			const double Gap = Delta.IsNearlyZero()
				? -(EPa.Radius + EPb.Radius)
				: FVector::DotProduct(Pb - Pa, Delta.GetSafeNormal());
			Apply(Gap <= MS.MaxFaceGap);
		}

		if (MS.bGuardAngle)
		{
			// Outward arm directions point at each other when collinear (EPa.Direction ~ -EPb.Direction),
			// so misalignment 0 deg == clean inline seam.
			const double Dot = FVector::DotProduct(EPa.Direction, EPb.Direction);
			const double Misalign = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-Dot, -1.0, 1.0)));
			Apply(Misalign <= MS.MaxSeamAngle);
		}

		if (MS.bGuardSingleEdge)
		{
			Apply(Road.Chain.IsValid() && Road.Chain->SingleEdge != -1);
		}

		if (MS.bGuardDegreeCap)
		{
			// Exclude the seam edge itself (it is about to be dissolved): MaxArms reads as "max OTHER arms
			// each junction may keep", so two N-arm junctions sharing a seam aren't off-by-one rejected.
			const int32 DegA = Processor->Cluster->GetNode(EPa.NodeIndex)->Num() - 1;
			const int32 DegB = Processor->Cluster->GetNode(EPb.NodeIndex)->Num() - 1;
			Apply(DegA <= MS.MaxArms && DegB <= MS.MaxArms);
		}

		if (!bAnyEnabled)
		{
			return true; // no geometric guard enabled -- gate alone decides
		}

		return MS.GuardCombine == EPCGExZGMergeGuardCombine::All ? bAll : bAny;
	}

	void FZGMerger::Execute()
	{
		if (bMergeGroupsUnavailable)
		{
			return; // group restriction requested but its attribute was missing -> fail safe, merge nothing
		}

		EvaluateEligibility();

		const TSharedPtr<PCGExClusters::FCluster>& Cluster = Processor->Cluster;
		TArray<TSharedPtr<FZGPolygon>>& Polygons = Processor->Polygons;
		TArray<TSharedPtr<FZGRoad>>& Roads = Processor->Roads;

		if (Polygons.Num() < 2)
		{
			return; // need at least two polygons to fuse anything
		}

		// Polygon node -> polygon (pre-fuse, 1:1).
		TMap<int32, TSharedPtr<FZGPolygon>> NodeToPolygon;
		NodeToPolygon.Reserve(Polygons.Num());
		for (const TSharedPtr<FZGPolygon>& Poly : Polygons)
		{
			NodeToPolygon.Add(Poly->NodeIndex, Poly);
		}

		// Union-find over node indices (sized to all nodes; only polygon nodes ever get unioned).
		const int32 NumNodes = Cluster->Nodes->Num();
		TArray<int32> Parent;
		Parent.SetNumUninitialized(NumNodes);
		for (int32 i = 0; i < NumNodes; i++)
		{
			Parent[i] = i;
		}

		auto Find = [&Parent](int32 X) -> int32
		{
			while (Parent[X] != X)
			{
				Parent[X] = Parent[Parent[X]]; // path halving
				X = Parent[X];
			}
			return X;
		};

		auto Union = [&](const int32 A, const int32 B)
		{
			const int32 RA = Find(A);
			const int32 RB = Find(B);
			if (RA != RB)
			{
				Parent[RA] = RB;
			}
		};

		// Seam detection + contraction. A seam always unites two nodes into the same component, so any
		// seam road touching a group is internal to that group by construction.
		bool bAnySeam = false;
		for (const TSharedPtr<FZGRoad>& Road : Roads)
		{
			if (!Road || !IsSeam(*Road))
			{
				continue;
			}
			Road->bIsMergeSeam = true;
			Union(Road->StartEndpoint.NodeIndex, Road->EndEndpoint.NodeIndex);
			bAnySeam = true;
		}

		if (!bAnySeam)
		{
			return;
		}

		// Group polygon nodes by component root.
		TMap<int32, TArray<int32>> Groups;
		for (const TSharedPtr<FZGPolygon>& Poly : Polygons)
		{
			Groups.FindOrAdd(Find(Poly->NodeIndex)).Add(Poly->NodeIndex);
		}

		TArray<TSharedPtr<FZGPolygon>> Survivors;
		Survivors.Reserve(Groups.Num());
		TSet<int32> AcceptedRoots;
		int32 DegenerateGroups = 0;

		for (TPair<int32, TArray<int32>>& Pair : Groups)
		{
			const TArray<int32>& Members = Pair.Value;
			if (Members.Num() == 1)
			{
				Survivors.Add(NodeToPolygon[Members[0]]);
				continue;
			}

			if (TSharedPtr<FZGPolygon> Host = FuseGroup(Members, NodeToPolygon))
			{
				AcceptedRoots.Add(Pair.Key);
				Survivors.Add(Host);
			}
			else
			{
				// Degenerate (< 2 surviving arms): leave the members un-merged. Their seams are restored below.
				DegenerateGroups++;
				for (const int32 NodeIdx : Members)
				{
					Survivors.Add(NodeToPolygon[NodeIdx]);
				}
			}
		}

		// Finalize seam roads: drop those of accepted groups, restore those of rejected groups.
		for (const TSharedPtr<FZGRoad>& Road : Roads)
		{
			if (!Road || !Road->bIsMergeSeam)
			{
				continue;
			}
			if (AcceptedRoots.Contains(Find(Road->StartEndpoint.NodeIndex)))
			{
				Road->bDegenerate = true; // contracted -- compile loop skips degenerate roads
			}
			else
			{
				Road->bIsMergeSeam = false; // group rejected -- emit as a normal road
			}
		}

		if (DegenerateGroups > 0)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Processor->GetContext(), FTEXT("Polygon merge: one or more groups were left unmerged because fewer than two connections would remain after merging."));
		}

		Polygons = MoveTemp(Survivors);
	}

	TSharedPtr<FZGPolygon> FZGMerger::FuseGroup(const TArray<int32>& MemberNodes, const TMap<int32, TSharedPtr<FZGPolygon>>& NodeToPolygon) const
	{
		struct FArm
		{
			FZoneShapePoint Point;
			FZoneLaneProfileRef LaneProfile;
			double HalfWidth = 0;
			TSharedPtr<FZGRoad> Road;
		};

		TArray<FArm> Arms;
		int32 PrimaryNode = MemberNodes[0];
		int32 PrimaryArmCount = -1;

		// Gather every member's surviving (non-seam) connection points.
		for (const int32 NodeIdx : MemberNodes)
		{
			const TSharedPtr<FZGPolygon>& Poly = NodeToPolygon[NodeIdx];
			int32 SurvivingArms = 0;

			for (int32 i = 0; i < Poly->PrecomputedPoints.Num(); i++)
			{
				const TSharedPtr<FZGRoad>& Road = Poly->CachedPointRoads[i];
				if (Road && Road->bIsMergeSeam)
				{
					continue;
				} // contracted seam connection -- dropped

				FArm& Arm = Arms.Emplace_GetRef();
				Arm.Point = Poly->PrecomputedPoints[i];
				Arm.LaneProfile = Poly->CachedPointLaneProfiles[i];
				Arm.HalfWidth = Poly->CachedPointHalfWidths.IsValidIndex(i) ? Poly->CachedPointHalfWidths[i] : 0.0;
				Arm.Road = Road;
				SurvivingArms++;
			}

			// Primary member: most surviving arms (tiebreak lowest node index). Donates scalar props + identity.
			if (SurvivingArms > PrimaryArmCount || (SurvivingArms == PrimaryArmCount && NodeIdx < PrimaryNode))
			{
				PrimaryArmCount = SurvivingArms;
				PrimaryNode = NodeIdx;
			}
		}

		if (Arms.Num() < 2)
		{
			return nullptr; // not a valid polygon
		}

		// Centroid of surviving connection points -> angular-sort center.
		FVector Center = FVector::ZeroVector;
		for (const FArm& Arm : Arms)
		{
			Center += Arm.Point.Position;
		}
		Center /= static_cast<double>(Arms.Num());

		// Order by bearing around the centroid (same convention as FZGPolygon::Precompute).
		TArray<FVector> RadialDirs;
		RadialDirs.SetNumUninitialized(Arms.Num());
		for (int32 i = 0; i < Arms.Num(); i++)
		{
			RadialDirs[i] = (Arms[i].Point.Position - Center).GetSafeNormal();
		}

		TArray<int32> Order;
		PCGExArrayHelpers::ArrayOfIndices(Order, Arms.Num());
		Order.Sort(
			[&](const int32 A, const int32 B)
			{
				return PCGExMath::GetRadiansBetweenVectors(RadialDirs[A], FVector::ForwardVector) > PCGExMath::GetRadiansBetweenVectors(RadialDirs[B], FVector::ForwardVector);
			});

		// Build the host from the primary member, then overwrite its connection ring with the fused arms.
		const TSharedPtr<FZGPolygon>& PrimaryPoly = NodeToPolygon[PrimaryNode];
		TSharedPtr<FZGPolygon> Host = MakeShared<FZGPolygon>(Processor, Processor->Cluster->GetNode(PrimaryNode));

		Host->CachedRoutingType = PrimaryPoly->CachedRoutingType;
		Host->CachedPointType = PrimaryPoly->CachedPointType;
		Host->CachedLaneProfile = PrimaryPoly->CachedLaneProfile;
		Host->CachedRadius = PrimaryPoly->CachedRadius;

		// Intersection tags: union across all members.
		FZoneGraphTagMask Tags = FZoneGraphTagMask::None;
		for (const int32 NodeIdx : MemberNodes)
		{
			Tags = Tags | NodeToPolygon[NodeIdx]->CachedAdditionalTags;
		}
		Host->CachedAdditionalTags = Tags;

		const int32 Count = Arms.Num();
		PCGExArrayHelpers::InitArray(Host->PrecomputedPoints, Count);
		Host->CachedPointLaneProfiles.SetNum(Count);
		Host->CachedPointHalfWidths.SetNum(Count);
		Host->CachedPointRoads.SetNum(Count);

		for (int32 i = 0; i < Count; i++)
		{
			const FArm& Arm = Arms[Order[i]];
			Host->PrecomputedPoints[i] = Arm.Point;
			Host->CachedPointLaneProfiles[i] = Arm.LaneProfile;
			Host->CachedPointHalfWidths[i] = Arm.HalfWidth;
			Host->CachedPointRoads[i] = Arm.Road;
		}

		return Host;
	}
}

#undef LOCTEXT_NAMESPACE
