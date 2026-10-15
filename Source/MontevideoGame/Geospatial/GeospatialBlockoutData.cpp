#include "Geospatial/GeospatialBlockoutData.h"
#include "Geospatial/GeospatialJson.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"

namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	using FValues = TArray<TSharedPtr<FJsonValue>>;
	bool Number(const FObject& Object, const TCHAR* Key, double& Out)
	{
		return Object && Object->TryGetNumberField(Key, Out) && FMath::IsFinite(Out);
	}
	bool Array(const FObject& Object, const TCHAR* Key, const FValues*& Out)
	{
		return Object && Object->TryGetArrayField(Key, Out);
	}
	bool Text(const FObject& Object, const TCHAR* Key, FString& Out)
	{
		return Object && Object->TryGetStringField(Key, Out) && !Out.IsEmpty();
	}
	FObject ObjectValue(const TSharedPtr<FJsonValue>& Value)
	{
		const FObject* Result = nullptr;
		return Value && Value->TryGetObject(Result) ? *Result : nullptr;
	}
	bool IsNull(const FObject& Object, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue>* Value = Object ? Object->Values.Find(Key) : nullptr;
		return Value && (*Value)->IsNull();
	}
	using GeospatialJson::Read;
	bool Indices(const FValues& Values, int32 VertexCount, TArray<int32>& Out)
	{
		for (const auto& Value : Values)
		{
			double Index;
			if (!Value->TryGetNumber(Index) || !FMath::IsFinite(Index) || Index < 0 || Index >= VertexCount || Index != FMath::FloorToDouble(Index)) { return false; }
			Out.Add(static_cast<int32>(Index));
		}
		return true;
	}
	bool MatchesBounds(const FObject& Object, const FBox& Actual)
	{
		const FValues* Values;
		if (!Array(Object, TEXT("expected_local_bbox_cm"), Values) || Values->Num() != 4) { return false; }
		const double Coordinates[] = {Actual.Min.X, Actual.Min.Y, Actual.Max.X, Actual.Max.Y};
		for (int32 Index = 0; Index < 4; ++Index)
		{
			double Expected;
			if (!(*Values)[Index]->TryGetNumber(Expected) || !FMath::IsFinite(Expected) || FMath::Abs(Expected - Coordinates[Index]) > 0.1) { return false; }
		}
		return true;
	}
}

bool FGeospatialBlockoutLoader::Load(const FString& Root, const FString& ManifestPath, FGeospatialBlockoutData& OutData, FString& Error, FGeospatialLoadMetrics* Metrics)
{
	if (Metrics) { *Metrics = {}; }
	Error = TEXT("Invalid M3 manifest schema/units/CRS");
	FObject Manifest;
	if (!Read(Root, ManifestPath, TEXT(""), Manifest, Error, Metrics)) { return false; }
	double Version, Plane;
	FString CRS, OriginPath, OriginHash, Axes;
	FGeospatialBlockoutData Data;
	if (!Number(Manifest, TEXT("schema_version"), Version) || Version != 1 || !Text(Manifest, TEXT("crs"), CRS) || CRS != TEXT("EPSG:4326")
		|| !Text(Manifest, TEXT("axes"), Axes) || Axes != TEXT("X north, Y east, Z up")
		|| !Number(Manifest, TEXT("development_plane_z_cm"), Plane) || Plane != 0
		|| !IsNull(Manifest, TEXT("altitude")) || !IsNull(Manifest, TEXT("altitude_reference"))
		|| !Number(Manifest, TEXT("max_radius_m"), Data.MaxRadiusMeters) || Data.MaxRadiusMeters <= 0 || Data.MaxRadiusMeters > 500
		|| !Text(Manifest, TEXT("origin_source"), OriginPath) || !Text(Manifest, TEXT("origin_sha1"), OriginHash)) { return false; }
	FObject Origin;
	if (!Read(Root, OriginPath, OriginHash, Origin, Error, Metrics)) { return false; }
	const FObject* Center;
	Error = TEXT("Invalid horizontal origin; altitude must remain null");
	if (!Origin->TryGetObjectField(TEXT("centroid_wgs84"), Center) || !IsNull(Origin, TEXT("altitude")) || !IsNull(Origin, TEXT("altitude_reference"))
		|| !Number(*Center, TEXT("latitude"), Data.MathematicalOrigin.LatitudeDegrees)
		|| !Number(*Center, TEXT("longitude"), Data.MathematicalOrigin.LongitudeDegrees)) { return false; }
	Data.MathematicalOrigin.AltitudeMeters = 0; // Explicit mathematical fixture for XY only, never a measured elevation.
	Data.OriginSource = OriginPath;
	FGeoReference Reference;
	if (Reference.SetOrigin(Data.MathematicalOrigin) != EGeoConversionResult::Success) { return false; }
	const FValues* Datasets;
	if (!Array(Manifest, TEXT("datasets"), Datasets)) { return false; }
	for (const auto& Value : *Datasets)
	{
		const FObject Dataset = ObjectValue(Value);
		FString Source;
		if (!Text(Dataset, TEXT("layer"), Source) && !Text(Dataset, TEXT("source"), Source)) { return false; }
		Data.Sources.Add(Source);
	}
	const FValues* Files;
	Error = TEXT("Invalid feature files");
	if (!Array(Manifest, TEXT("files"), Files) || Files->IsEmpty() || Files->Num() > 16) { return false; }
	for (const auto& FileValue : *Files)
	{
		const FObject File = ObjectValue(FileValue);
		FString Path, Hash;
		FObject Layer;
		if (!Text(File, TEXT("path"), Path) || !Text(File, TEXT("sha1"), Hash) || !Read(Root, Path, Hash, Layer, Error, Metrics)) { return false; }
		const FValues* Features;
		if (!Number(Layer, TEXT("schema_version"), Version) || Version != 1 || !Text(Layer, TEXT("crs"), CRS) || CRS != TEXT("EPSG:4326")
			|| !Array(Layer, TEXT("features"), Features) || Features->IsEmpty() || Features->Num() > 2000) { return false; }
		for (const auto& FeatureValue : *Features)
		{
			FGeospatialBlockoutFeature Feature;
			const FObject Item = ObjectValue(FeatureValue);
			FString Kind;
			const FValues *Vertices, *Triangles, *Rings;
			Error = TEXT("Invalid feature in ") + Path;
			if (!Text(Item, TEXT("id"), Feature.Id) || !Text(Item, TEXT("role"), Feature.Role) || !Text(Item, TEXT("kind"), Kind)
				|| (Kind != TEXT("polygon") && Kind != TEXT("line")) || !IsNull(Item, TEXT("altitude")) || !IsNull(Item, TEXT("altitude_reference"))
				|| !Array(Item, TEXT("vertices"), Vertices) || !Array(Item, TEXT("triangles"), Triangles) || !Array(Item, TEXT("rings"), Rings)) { return false; }
			Feature.bPolygon = Kind == TEXT("polygon");
			if (Vertices->Num() < (Feature.bPolygon ? 3 : 2) || Vertices->Num() > 100000 || Triangles->Num() > 300000) { return false; }
			for (const auto& Vertex : *Vertices)
			{
				const FValues* XY;
				double Longitude, Latitude;
				if (!Vertex->TryGetArray(XY) || XY->Num() != 2 || !(*XY)[0]->TryGetNumber(Longitude) || !(*XY)[1]->TryGetNumber(Latitude)) { return false; }
				const double ConvertStart = FPlatformTime::Seconds();
				FLocalGeoCoordinate Local;
				if (Reference.GeographicToLocal(FGeoCoordinate(Latitude, Longitude, 0), Local) != EGeoConversionResult::Success) { return false; }
				if (Metrics) { Metrics->ConversionMilliseconds += (FPlatformTime::Seconds()-ConvertStart)*1000; }
				Local.Centimeters.Z = 0;
				if (Local.Centimeters.SizeSquared2D() > FMath::Square(Data.MaxRadiusMeters * 100)) { Error = TEXT("Geometry exceeds M3 radius"); return false; }
				Feature.Vertices.Add(Local.Centimeters);
				Feature.Bounds += Local.Centimeters;
			}
			if (!Indices(*Triangles, Feature.Vertices.Num(), Feature.Triangles) || Triangles->Num() % 3 != 0) { return false; }
			if (Feature.bPolygon)
			{
				if (Triangles->IsEmpty() || Rings->IsEmpty()) { return false; }
				for (int32 Index = 0; Index < Feature.Triangles.Num(); Index += 3)
				{
					const FVector& A = Feature.Vertices[Feature.Triangles[Index]];
					const FVector& B = Feature.Vertices[Feature.Triangles[Index+1]];
					const FVector& C = Feature.Vertices[Feature.Triangles[Index+2]];
					const double TwiceArea = FVector::CrossProduct(B-A, C-A).Z;
					if (TwiceArea <= 1.e-8) { Error = TEXT("Inverted or degenerate triangle: ") + Feature.Id; return false; }
					Feature.AreaSquareMeters += TwiceArea / 20000.0;
				}
				double MetricArea;
				if (!Number(Item, TEXT("area_utm_m2"), MetricArea) || MetricArea <= 0 || FMath::Abs(Feature.AreaSquareMeters / MetricArea - 1) > 0.01) { Error = TEXT("Polygon area/scale mismatch"); return false; }
				for (const auto& RingValue : *Rings)
				{
					const FValues* Ring;
					TArray<int32> IndicesOut;
					if (!RingValue->TryGetArray(Ring) || Ring->Num() < 3 || !Indices(*Ring, Feature.Vertices.Num(), IndicesOut)) { return false; }
					Feature.Rings.Add(MoveTemp(IndicesOut));
				}
				++Data.PolygonCount;
			}
			else { if (!Triangles->IsEmpty() || !Rings->IsEmpty()) { return false; } ++Data.LineCount; }
			if (!MatchesBounds(Item, Feature.Bounds)) { Error = TEXT("Axis/scale mismatch versus independent PROJ bounds: ") + Feature.Id; return false; }
			Data.VertexCount += Feature.Vertices.Num();
			if (Data.VertexCount > 100000) { Error = TEXT("Development vertex budget exceeded"); return false; }
			Data.Bounds += Feature.Bounds;
			Data.Features.Add(MoveTemp(Feature));
		}
	}
	if (!Data.PolygonCount || !MatchesBounds(Manifest, Data.Bounds)) { Error = TEXT("Empty polygons or inconsistent manifest bounds"); return false; }
	const FObject* Counts;
	double PolygonCount, LineCount, VertexCount;
	if (!Manifest->TryGetObjectField(TEXT("counts"), Counts) || !Number(*Counts, TEXT("polygons"), PolygonCount)
		|| !Number(*Counts, TEXT("lines"), LineCount) || !Number(*Counts, TEXT("vertices"), VertexCount)
		|| PolygonCount != Data.PolygonCount || LineCount != Data.LineCount || VertexCount != Data.VertexCount)
	{
		Error = TEXT("Manifest counts disagree with loaded geometry"); return false;
	}
	OutData = MoveTemp(Data);
	Error.Reset();
	return true;
}
