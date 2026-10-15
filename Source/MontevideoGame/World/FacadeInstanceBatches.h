#pragma once
#include "World/FacadeLayoutGenerator.h"

/** Pure projection of the validated M3.3 layout. Slot identities are stable across reloads. */
struct FFacadeInstanceBatch
{
	FString Name;
	FVector4f Color=FVector4f(1,1,1,1);
	TArray<FTransform> Transforms;
	TArray<int32> ElementIndices; // Identity back to layout, segment, source feature and seed.
};

struct FFacadeInstanceData
{
	TArray<FFacadeInstanceBatch> Batches;
	int32 Instances=0, Windows=0, Doors=0, Balconies=0;
	double BatchingMilliseconds=0;
	FBox Bounds=FBox(ForceInit);
};

class MONTEVIDEOGAME_API FFacadeInstanceBatches
{
public:
	static constexpr int32 SlotCount=15;
	static int32 Slot(const FFacadeElement& Element,const FFacadeSegment& Segment);
	static bool Build(const FBuildingFacadeData& Layout,FFacadeInstanceData& Out,FString& Error);
	/** Exact logical parity plus transform corner checks; leaves production renderer independent of the legacy mesh. */
	static bool Compare(const FBuildingFacadeData& Legacy,const FBuildingFacadeData& Current,const FFacadeInstanceData& Instances,FString& Error);
};
