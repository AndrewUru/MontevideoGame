#pragma once
#include "CoreMinimal.h"
#include "Geospatial/BuildingFootprintData.h"
#include "DynamicMesh/DynamicMesh3.h"

struct FBuildingMeshMetrics
{
	int32 Vertices = 0, Triangles = 0, WallTriangles = 0, RoofTriangles = 0;
	int64 ApproximateMeshBytes = 0;
	double GenerationMilliseconds = 0;
	FBox Bounds = FBox(ForceInit);
};

/** Pure extrusion. Triangle group == Footprints index, preserving source identity without per-wall actors. */
class MONTEVIDEOGAME_API FBuildingMeshGenerator
{
public:
	static bool Build(const FBuildingFootprintData& Data, UE::Geometry::FDynamicMesh3& OutMesh, FBuildingMeshMetrics& OutMetrics, FString& Error);
};
