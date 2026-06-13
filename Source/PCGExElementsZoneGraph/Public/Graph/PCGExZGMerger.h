// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExZGMerger.generated.h"

class UPCGExPointFilterFactoryData;

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

namespace PCGExClusterFilter
{
	class FManager;
}

UENUM(BlueprintType)
enum class EPCGExZGMergeGuardCombine : uint8
{
	All = 0 UMETA(DisplayName="All (AND)", Tooltip="A seam must pass every enabled geometric guard."),
	Any = 1 UMETA(DisplayName="Any (OR)", Tooltip="A seam passes if at least one enabled geometric guard passes."),
};

// Order-free comparisons only -- a seam is an unordered pair of nodes, so </>/<=/>= would depend on
// internal (orientation-driven) endpoint ordering. These four are symmetric and fully predictable.
UENUM(BlueprintType)
enum class EPCGExZGMergeMatch : uint8
{
	Equal    = 0 UMETA(DisplayName=" == ", Tooltip="Adjacent merge-group ids must be strictly equal."),
	NotEqual = 1 UMETA(DisplayName=" != ", Tooltip="Adjacent merge-group ids must differ."),
	Near     = 2 UMETA(DisplayName=" ~= ", Tooltip="Adjacent merge-group ids must be within Tolerance of each other."),
	NotNear  = 3 UMETA(DisplayName=" !~= ", Tooltip="Adjacent merge-group ids must differ by more than Tolerance."),
};

/**
 * Settings driving the optional polygon-merge post-process.
 *
 * Two polygons connected by a single road are fused into one when the road qualifies as a "merge seam":
 *   eligible(a) && eligible(b)            -- the "Merge Conditions" filter (master gate, hidden when the whole feature is off)
 *   && same non-zero group id (optional)  -- bUseMergeGroups
 *   && geometric guards                   -- combined per GuardCombine
 *
 * Merges are transitive: a chain of qualifying seams collapses into a single polygon.
 */
USTRUCT(BlueprintType)
struct PCGEXELEMENTSZONEGRAPH_API FPCGExZGMergeSettings
{
	GENERATED_BODY()

	/** Restrict merging to candidate pairs that share the same non-zero merge-group id. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bUseMergeGroups = false;

	/** Per-node int32 merge-group id. 0 = never merge (either endpoint). How two adjacent non-zero ids must relate is set by Match. Read from: Points (vtx). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Merge Group", EditCondition="bUseMergeGroups"))
	FName MergeGroupAttribute = FName("MergeGroup");

	/** How the two adjacent group ids must relate for the pair to merge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Match", EditCondition="bUseMergeGroups", EditConditionHides))
	EPCGExZGMergeMatch MergeGroupMatch = EPCGExZGMergeMatch::Equal;

	/** Tolerance (in id units) for the ~= / !~= group match. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin="0", UIMin="0", EditCondition="bUseMergeGroups && (MergeGroupMatch == EPCGExZGMergeMatch::Near || MergeGroupMatch == EPCGExZGMergeMatch::NotNear)", EditConditionHides))
	int32 MergeGroupTolerance = 1;

	/** How the enabled geometric guards combine when deciding whether a seam may contract. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExZGMergeGuardCombine GuardCombine = EPCGExZGMergeGuardCombine::All;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bGuardFaceGap = true;

	/** Guard: signed gap between the two facing connection faces (uu) must be at or below this. Positive = road remains between them; 0 = touching; negative = require overlap. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Face Gap", EditCondition="bGuardFaceGap"))
	double MaxFaceGap = 0.0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bGuardAngle = false;

	/** Guard: the two arms must be near-collinear -- their misalignment angle (degrees) must be at or below this. 0 = perfectly inline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Seam Angle", ClampMin="0", ClampMax="180", UIMin="0", UIMax="180", EditCondition="bGuardAngle"))
	double MaxSeamAngle = 30.0;

	/** Guard: only contract single-edge seams (the road between the two polygons is one cluster edge, with no pass-through nodes). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bGuardSingleEdge = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bGuardDegreeCap = false;

	/** Guard: only contract when both polygon nodes have at most this many OTHER arms -- cluster degree excluding the road being merged away. Prevents dissolving busy junctions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Arms", ClampMin="2", UIMin="2", EditCondition="bGuardDegreeCap"))
	int32 MaxArms = 4;
};

namespace PCGExClusterToZoneGraph
{
	class FProcessor;
	class FZGRoad;
	class FZGPolygon;

	/**
	 * Polygon-merge driver. One instance per cluster (owned by FProcessor).
	 *
	 * Runs as a post-process at the tail of FProcessor::CompleteWork, after roads and polygons have been
	 * fully precomputed. It gathers already-computed connection points -- it never recomputes geometry or
	 * re-trims roads:
	 *
	 *   1. EvaluateEligibility - run the shared "Merge Conditions" filter into a per-vtx eligibility cache.
	 *   2. Seam detection      - for each road joining two eligible polygons, test the group + guard predicate.
	 *   3. Union-find          - contract qualifying seams into transitive groups.
	 *   4. FuseGroup           - per multi-member group, gather every member's non-seam connection points,
	 *                            re-sort them by angle around the group centroid, and emit one host polygon.
	 *   5. Mark seam roads degenerate so the compile loop skips them.
	 *
	 * All detection / guard math / union-find / fuse logic lives here; FZGRoad / FZGPolygon only carry data.
	 */
	class FZGMerger : public TSharedFromThis<FZGMerger>
	{
		FProcessor* Processor = nullptr;

		TSharedPtr<PCGExClusterFilter::FManager> EligibilityManager;
		TSharedPtr<TArray<int8>> EligibleCache; // indexed by vtx PointIndex
		TSharedPtr<PCGExData::TBuffer<int32>> GroupIdBuffer;
		bool bMergeGroupsUnavailable = false; // bUseMergeGroups requested but the group attribute was missing -> skip merging (fail-safe)

	public:
		explicit FZGMerger(FProcessor* InProcessor);

		// Builds the eligibility filter manager (own slot, separate from breakpoints) and the optional
		// group-id buffer. Runs during FProcessor::Process. Returns false only on fatal filter init error.
		bool Init(const TArray<TObjectPtr<const UPCGExPointFilterFactoryData>>* InFilterFactories);

		// Off main thread, after road/polygon precompute. Mutates Processor->Polygons (fused set) and flags
		// contracted seam roads degenerate. Safe: a processor's CompleteWork runs single-threaded.
		void Execute();

	private:
		void EvaluateEligibility();
		bool IsCandidate(int32 NodeIndex) const;
		bool IsSeam(const FZGRoad& Road) const;
		bool PassesGuards(const FZGRoad& Road) const;

		// Builds one fused polygon from the member nodes' surviving (non-seam) connection points.
		// Returns null when fewer than two arms survive (degenerate -- caller leaves members un-merged).
		TSharedPtr<FZGPolygon> FuseGroup(const TArray<int32>& MemberNodes, const TMap<int32, TSharedPtr<FZGPolygon>>& NodeToPolygon) const;
	};
}
