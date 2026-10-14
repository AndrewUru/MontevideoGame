#pragma once

#include "CoreMinimal.h"
#include "Geospatial/GeoReference.h"

/** Development interchange data. Z is flattened after geodetic conversion; it is NOT terrain height. */
struct FGeospatialBlockoutFeature
{
	FString Id;
	FString Role;
	bool bPolygon = false;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<TArray<int32>> Rings;
	FBox Bounds = FBox(ForceInit);
	double AreaSquareMeters = 0;
};

struct FGeospatialBlockoutData
{
	FGeoCoordinate MathematicalOrigin;
	FString OriginSource;
	TArray<FString> Sources;
	TArray<FGeospatialBlockoutFeature> Features;
	FBox Bounds = FBox(ForceInit);
	int32 PolygonCount = 0;
	int32 LineCount = 0;
	int32 VertexCount = 0;
	double MaxRadiusMeters = 0;
};

/** Strict offline contract reader; replaces OutData only on success. No actors or assets. */
class MONTEVIDEOGAME_API FGeospatialBlockoutLoader
{
public:
	static bool Load(const FString& ProjectDirectory, const FString& ManifestRelativePath,
		FGeospatialBlockoutData& OutData, FString& OutError);
};
