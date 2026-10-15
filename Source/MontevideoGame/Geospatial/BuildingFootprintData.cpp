#include "Geospatial/BuildingFootprintData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"

namespace
{
	using FObject = TSharedPtr<FJsonObject>;
	bool Read(const FString& Root, const FString& Relative, const FString& Hash, FObject& Out)
	{
		FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, Relative));
		FPaths::CollapseRelativeDirectories(Path);
		if (!FPaths::IsRelative(Relative) || !FPaths::IsUnderDirectory(Path, FPaths::ConvertRelativePathToFull(Root))) { return false; }
		TArray<uint8> Bytes;
		const int64 Size = IFileManager::Get().FileSize(*Path);
		if (Size < 0 || Size > 25*1024*1024 || !FFileHelper::LoadFileToArray(Bytes, *Path)) { return false; }
		if (!Hash.IsEmpty() && !FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString().Equals(Hash, ESearchCase::IgnoreCase)) { return false; }
		FString Content;
		FFileHelper::BufferToString(Content, Bytes.GetData(), Bytes.Num());
		return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content), Out) && Out.IsValid();
	}
	bool Number(const FObject& Object, const TCHAR* Key, double& Out)
	{
		return Object && Object->TryGetNumberField(Key, Out) && FMath::IsFinite(Out);
	}
}

uint32 FBuildingFootprintLoader::StableSeed(const FString& Id)
{
	const FTCHARToUTF8 Bytes(*Id);
	const FString Hash = FSHA1::HashBuffer(Bytes.Get(), Bytes.Length()).ToString();
	return static_cast<uint32>(FCString::Strtoui64(*Hash.Left(8), nullptr, 16));
}

bool FBuildingFootprintLoader::Load(const FString& Root, const FString& ManifestPath, FBuildingFootprintData& Out, FString& Error)
{
	FBuildingFootprintData Candidate;
	FGeospatialBlockoutData Geometry;
	if (!FGeospatialBlockoutLoader::Load(Root, ManifestPath, Geometry, Error, &Candidate.LoadMetrics)) { return false; }
	const double Start = FPlatformTime::Seconds();
	Error = TEXT("Invalid building schema, IDs or height provenance");
	FObject Manifest, Layer;
	const TArray<TSharedPtr<FJsonValue>> *Files, *Features;
	const FObject *Policy, *Counts;
	FString Mode, FilePath, Hash;
	double FloorHeight, Version, ExpectedCount;
	if (!Read(Root, ManifestPath, TEXT(""), Manifest) || !Manifest->TryGetArrayField(TEXT("files"), Files) || Files->Num()!=1
		|| !Manifest->TryGetObjectField(TEXT("height_policy"), Policy) || !(*Policy)->TryGetStringField(TEXT("mode"), Mode)
		|| Mode != TEXT("synthetic_blockout") || !Number(*Policy, TEXT("floor_height_m"), FloorHeight) || FloorHeight<2 || FloorHeight>5
		|| !Manifest->TryGetObjectField(TEXT("counts"), Counts) || !Number(*Counts,TEXT("buildings"),ExpectedCount)) { return false; }
	const FObject* File;
	if (!(*Files)[0]->TryGetObject(File) || !(*File)->TryGetStringField(TEXT("path"), FilePath) || !(*File)->TryGetStringField(TEXT("sha1"),Hash)
		|| !Read(Root,FilePath,Hash,Layer) || !Number(Layer,TEXT("building_schema_version"),Version) || Version!=1
		|| !Layer->TryGetArrayField(TEXT("features"),Features) || Features->Num()!=Geometry.Features.Num() || Geometry.Sources.Num()!=1) { return false; }
	Candidate.LoadMetrics.JsonMilliseconds += (FPlatformTime::Seconds()-Start)*1000;
	Candidate.Origin = Geometry.MathematicalOrigin;
	Candidate.Dataset = Geometry.Sources[0];
	FGeoReference Reference;
	Reference.SetOrigin(Candidate.Origin);
	TSet<FString> Ids, Buildings;
	for (int32 I=0; I<Features->Num(); ++I)
	{
		const FObject* Item;
		FBuildingFootprint Footprint;
		FString Id;
		double Seed, Floors;
		const TArray<TSharedPtr<FJsonValue>>* Center;
		if (!(*Features)[I]->TryGetObject(Item) || !(*Item)->TryGetStringField(TEXT("id"),Id) || Ids.Contains(Id)
			|| Id != Geometry.Features[I].Id || !Geometry.Features[I].bPolygon || Geometry.Features[I].Role!=TEXT("buildings")
			|| !(*Item)->TryGetStringField(TEXT("building_id"),Footprint.BuildingId)
			|| !(*Item)->TryGetStringField(TEXT("source_feature_id"),Footprint.SourceFeatureId) || Footprint.SourceFeatureId.IsEmpty()
			|| Footprint.BuildingId != Candidate.Dataset+TEXT(":")+Footprint.SourceFeatureId
			|| !(*Item)->TryGetStringField(TEXT("height_source"),Footprint.HeightSource) || Footprint.HeightSource!=Mode
			|| !(*Item)->TryGetStringField(TEXT("floors_source"),Footprint.FloorsSource) || Footprint.FloorsSource!=Mode
			|| !Number(*Item,TEXT("seed"),Seed) || Seed!=StableSeed(Footprint.BuildingId)
			|| !Number(*Item,TEXT("floors"),Floors) || Floors!=3+static_cast<uint32>(Seed)%3
			|| !Number(*Item,TEXT("height_m"),Footprint.HeightMeters) || !FMath::IsNearlyEqual(Footprint.HeightMeters,Floors*FloorHeight,1e-8)
			|| !(*Item)->TryGetArrayField(TEXT("centroid"),Center) || Center->Num()!=2) { return false; }
		double Lon, Lat;
		if (!(*Center)[0]->TryGetNumber(Lon) || !(*Center)[1]->TryGetNumber(Lat)) { return false; }
		FLocalGeoCoordinate Local;
		const double ConvertStart = FPlatformTime::Seconds();
		if (Reference.GeographicToLocal(FGeoCoordinate(Lat,Lon,0),Local)!=EGeoConversionResult::Success || Local.Centimeters.Size2D()>50000) { return false; }
		Candidate.LoadMetrics.ConversionMilliseconds += (FPlatformTime::Seconds()-ConvertStart)*1000;
		Footprint.Centroid = FVector(Local.Centimeters.X,Local.Centimeters.Y,0);
		Footprint.Seed = static_cast<uint32>(Seed);
		Footprint.Floors = static_cast<int32>(Floors);
		Footprint.Geometry = MoveTemp(Geometry.Features[I]);
		Ids.Add(Id); Buildings.Add(Footprint.BuildingId);
		Candidate.Footprints.Add(MoveTemp(Footprint));
	}
	Candidate.BuildingCount = Buildings.Num();
	if (Candidate.BuildingCount<=0 || ExpectedCount!=Candidate.BuildingCount) { return false; }
	Out = MoveTemp(Candidate);
	Error.Reset();
	return true;
}
