#pragma once
#include "Geospatial/GeospatialBlockoutData.h"

struct FBuildingFootprint
{
	FGeospatialBlockoutFeature Geometry;
	FString BuildingId;
	FString SourceFeatureId;
	FString HeightSource;
	FString FloorsSource;
	uint32 Seed = 0;
	int32 Floors = 0;
	double HeightMeters = 0;
	FVector Centroid = FVector::ZeroVector;
};

struct FBuildingFootprintData
{
	FGeoCoordinate Origin;
	FString Dataset;
	TArray<FBuildingFootprint> Footprints;
	int32 BuildingCount = 0;
	FGeospatialLoadMetrics LoadMetrics;
};

/** Adds building semantics to the existing bounded geospatial loader, not another coordinate system. */
class MONTEVIDEOGAME_API FBuildingFootprintLoader
{
public:
	static uint32 StableSeed(const FString& BuildingId);
	static bool Load(const FString& ProjectDirectory, const FString& ManifestPath, FBuildingFootprintData& OutData, FString& Error);
};
