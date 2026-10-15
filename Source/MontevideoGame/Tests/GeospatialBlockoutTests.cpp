#include "Geospatial/GeospatialBlockoutData.h"
#include "World/GeospatialBlockoutActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/World.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	const FString ManifestPath = TEXT("Data/processed/montevideo/m3/manifest.json");
	bool Load(FAutomationTestBase& Test, FGeospatialBlockoutData& Data)
	{
		FString Error;
		const bool bLoaded = FGeospatialBlockoutLoader::Load(FPaths::ProjectDir(), ManifestPath, Data, Error);
		return Test.TestTrue(*FString::Printf(TEXT("Load validated M3: %s"), *Error), bLoaded);
	}
	TSharedPtr<FJsonObject> ReadJson(const FString& Path)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *Path);
		TSharedPtr<FJsonObject> Result;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result);
		return Result;
	}
	void WriteJson(const FString& Path, const TSharedPtr<FJsonObject>& Object)
	{
		FString Text;
		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
	struct FFixture
	{
		FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("M3/Fixtures"), FGuid::NewGuid().ToString());
		FFixture()
		{
			const auto Manifest = ReadJson(FPaths::Combine(FPaths::ProjectDir(), ManifestPath));
			TArray<FString> Paths{ManifestPath, Manifest->GetStringField(TEXT("origin_source"))};
			for (const auto& File : Manifest->GetArrayField(TEXT("files"))) { Paths.Add(File->AsObject()->GetStringField(TEXT("path"))); }
			for (const FString& Path : Paths)
			{
				const FString Target = FPaths::Combine(Root, Path);
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
				IFileManager::Get().Copy(*Target, *FPaths::Combine(FPaths::ProjectDir(), Path));
			}
		}
		~FFixture() { IFileManager::Get().DeleteDirectory(*Root, false, true); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3DataTest, "Montevideo.M3.01OfficialGeometry", Flags)
bool FM3DataTest::RunTest(const FString&)
{
	FGeospatialBlockoutData Data;
	if (!Load(*this, Data)) { return false; }
	TestTrue(TEXT("Surrounding polygons and roads loaded"), Data.PolygonCount > 1 && Data.LineCount > 0);
	int32 PlazaCount = 0;
	for (const auto& Feature : Data.Features)
	{
		if (Feature.Role == TEXT("plaza"))
		{
			++PlazaCount;
			TestTrue(TEXT("Plaza polygon has at least three vertices"), Feature.bPolygon && Feature.Vertices.Num() >= 3);
			TestTrue(TEXT("Plaza local area agrees with ~15842 m2 UTM within 0.2%"), FMath::Abs(Feature.AreaSquareMeters / 15842.005955819597 - 1) < 0.002);
			const FVector Size = Feature.Bounds.GetSize() / 100;
			TestTrue(TEXT("Plaza north-south ~104 m; east-west ~178 m"), Size.X > 100 && Size.X < 110 && Size.Y > 175 && Size.Y < 185);
			AddInfo(FString::Printf(TEXT("Plaza area %.6f m2; local bounds %s to %s; size %.6f x %.6f m"), Feature.AreaSquareMeters, *Feature.Bounds.Min.ToString(), *Feature.Bounds.Max.ToString(), Size.X, Size.Y));
		}
		for (const FVector& Point : Feature.Vertices)
		{
			TestTrue(TEXT("Within 500 m development AOI"), Point.Size2D() <= 50000);
			TestEqual(TEXT("Development plane only"), Point.Z, 0.0);
		}
	}
	TestEqual(TEXT("Exactly one plaza"), PlazaCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3AxesTest, "Montevideo.M3.02OriginAxesScale", Flags)
bool FM3AxesTest::RunTest(const FString&)
{
	FGeospatialBlockoutData Data;
	if (!Load(*this, Data)) { return false; }
	FGeoReference Reference;
	Reference.SetOrigin(Data.MathematicalOrigin);
	FLocalGeoCoordinate Local;
	Reference.GeographicToLocal(Data.MathematicalOrigin, Local);
	TestTrue(TEXT("Verified origin maps to zero"), Local.Centimeters.Equals(FVector::ZeroVector, 0.001));
	FGeoCoordinate North = Data.MathematicalOrigin;
	North.LatitudeDegrees += 0.0001;
	Reference.GeographicToLocal(North, Local);
	TestTrue(TEXT("North -> +X, ~11.1 m in centimeters"), Local.Centimeters.X > 1100 && Local.Centimeters.X < 1120 && FMath::Abs(Local.Centimeters.Y) < 0.1);
	FGeoCoordinate East = Data.MathematicalOrigin;
	East.LongitudeDegrees += 0.0001;
	Reference.GeographicToLocal(East, Local);
	TestTrue(TEXT("East -> +Y, ~9.1 m in centimeters"), Local.Centimeters.Y > 900 && Local.Centimeters.Y < 925 && FMath::Abs(Local.Centimeters.X) < 0.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3RejectTest, "Montevideo.M3.03RejectCorruptOrInvertedData", Flags)
bool FM3RejectTest::RunTest(const FString&)
{
	FFixture Fixture;
	auto Manifest = ReadJson(FPaths::Combine(Fixture.Root, ManifestPath));
	auto File = Manifest->GetArrayField(TEXT("files"))[0]->AsObject();
	const FString LayerPath = FPaths::Combine(Fixture.Root, File->GetStringField(TEXT("path")));
	auto Layer = ReadJson(LayerPath);
	auto Feature = Layer->GetArrayField(TEXT("features"))[0]->AsObject();
	TArray<TSharedPtr<FJsonValue>> Triangles = Feature->GetArrayField(TEXT("triangles"));
	Triangles.Swap(0, 1);
	Feature->SetArrayField(TEXT("triangles"), Triangles);
	WriteJson(LayerPath, Layer);
	FGeospatialBlockoutData Data;
	FString Error;
	TestFalse(TEXT("Tampered file rejected by checksum"), FGeospatialBlockoutLoader::Load(Fixture.Root, ManifestPath, Data, Error));
	TestTrue(TEXT("Checksum diagnostic"), Error.Contains(TEXT("checksum")));
	TArray<uint8> Bytes;
	FFileHelper::LoadFileToArray(Bytes, *LayerPath);
	File->SetStringField(TEXT("sha1"), FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num()).ToString());
	WriteJson(FPaths::Combine(Fixture.Root, ManifestPath), Manifest);
	TestFalse(TEXT("Inverted geometry rejected even with updated checksum"), FGeospatialBlockoutLoader::Load(Fixture.Root, ManifestPath, Data, Error));
	TestTrue(TEXT("Winding diagnostic"), Error.Contains(TEXT("Inverted")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3BoundsRejectTest, "Montevideo.M3.04RejectRadiusAndAxes", Flags)
bool FM3BoundsRejectTest::RunTest(const FString&)
{
	FFixture Fixture;
	auto Manifest = ReadJson(FPaths::Combine(Fixture.Root, ManifestPath));
	Manifest->SetNumberField(TEXT("max_radius_m"), 10);
	WriteJson(FPaths::Combine(Fixture.Root, ManifestPath), Manifest);
	FGeospatialBlockoutData Data;
	FString Error;
	TestFalse(TEXT("Out-of-radius geometry rejected"), FGeospatialBlockoutLoader::Load(Fixture.Root, ManifestPath, Data, Error));
	TestTrue(TEXT("Radius diagnostic"), Error.Contains(TEXT("radius")));
	Manifest->SetNumberField(TEXT("max_radius_m"), 500);
	Manifest->SetStringField(TEXT("axes"), TEXT("X east, Y north, Z up"));
	WriteJson(FPaths::Combine(Fixture.Root, ManifestPath), Manifest);
	TestFalse(TEXT("Swapped axis contract rejected"), FGeospatialBlockoutLoader::Load(Fixture.Root, ManifestPath, Data, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3CollisionTest, "Montevideo.M3.05CollisionAndReload", Flags)
bool FM3CollisionTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Isolated physics world"), World)) { return false; }
	AGeospatialBlockoutActor* Actor = World->SpawnActor<AGeospatialBlockoutActor>();
	FString Error;
	if (TestTrue(TEXT("Build meshes and synchronous collision"), Actor->Reload(Error)))
	{
		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByChannel(Hit, FVector(0,0,1000), FVector(0,0,-1000), ECC_Visibility);
		AddInfo(FString::Printf(TEXT("Collision hit=%d component=%s position=%s normal=%s"), bHit, *GetNameSafe(Hit.GetComponent()), *Hit.ImpactPoint.ToString(), *Hit.ImpactNormal.ToString()));
		TestTrue(TEXT("Walkable surface at origin"), bHit);
		TestTrue(TEXT("Hit polygon at development Z=0, not support cube"), bHit && FMath::Abs(Hit.ImpactPoint.Z) < 0.1 && Hit.Component.IsValid() && Hit.Component->IsA<UDynamicMeshComponent>());
		TestTrue(TEXT("Upward collision normal"), bHit && Hit.ImpactNormal.Z > 0.99);
		const int32 Count = Actor->GetData().VertexCount;
		TestTrue(TEXT("Reload valid data"), Actor->Reload(Error));
		TestEqual(TEXT("No duplicated geometry"), Actor->GetData().VertexCount, Count);
		Actor->ManifestPath = TEXT("Data/processed/montevideo/m3/missing.json");
		TestFalse(TEXT("Missing reload rejected"), Actor->Reload(Error));
		TestEqual(TEXT("Existing scene retained"), Actor->GetData().VertexCount, Count);
	}
	else { AddError(Error); }
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM3VisualTest, "Montevideo.M3.06RenderColors", Flags)
bool FM3VisualTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;
	Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Isolated render-data world"), World)) { return false; }
	AGeospatialBlockoutActor* Actor = World->SpawnActor<AGeospatialBlockoutActor>();
	FString Error;
	if (TestTrue(TEXT("Build debug visualization"), Actor->Reload(Error)))
	{
		TArray<UDynamicMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		TestEqual(TEXT("Surfaces and markings"), Meshes.Num(), 2);
		for (const UDynamicMeshComponent* Component : Meshes)
		{
			const auto* Mesh = Component->GetMesh();
			const auto* Colors = Mesh->HasAttributes() ? Mesh->Attributes()->PrimaryColors() : nullptr;
			if (!TestNotNull(TEXT("GPU color overlay exists (vertex colors alone render white)"), Colors)) { continue; }
			TestTrue(TEXT("Unlit shader avoids sun/sky changing cartographic colors"), Component->GetMaterial(0)->GetMaterial()->GetShadingModels().HasOnlyShadingModel(MSM_Unlit));
			TestEqual(TEXT("Verified Engine vertex-color shader"), Component->GetMaterial(0)->GetPathName(), FString(TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly")));
			bool bComplete = true, bMatches = true;
			for (int32 Id : Mesh->TriangleIndicesItr())
			{
				if (!Colors->IsSetTriangle(Id)) { bComplete = false; continue; }
				const auto Vertices = Mesh->GetTriangle(Id);
				const auto Elements = Colors->GetTriangle(Id);
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const FVector3f Expected = Mesh->GetVertexColor(Vertices[Corner]);
					const FVector4f Actual = Colors->GetElement(Elements[Corner]);
					bMatches &= Actual.Equals(FVector4f(Expected.X, Expected.Y, Expected.Z, 1), 0.0001f);
				}
			}
			TestTrue(TEXT("Every rendered triangle has color data"), bComplete);
			TestTrue(TEXT("Every corner passes its intended color to the renderer"), bMatches);
			if (Component->GetFName() == TEXT("OfficialPolygons"))
			{
				int32 VertexId = 0;
				for (const auto& Feature : Actor->GetData().Features)
				{
					if (!Feature.bPolygon) { continue; }
					for (const FVector& Position : Feature.Vertices)
					{
						TestTrue(TEXT("Surface coordinates remain identical to GIS conversion"), FVector(Mesh->GetVertex(VertexId)).Equals(Position, 0.0001));
						const FVector3f Color = Mesh->GetVertexColor(VertexId++);
						TestTrue(TEXT("Plaza is green; blocks are dark blue-gray"), Feature.Role == TEXT("plaza")
							? Color.Y > 3 * Color.X && Color.Y > 3 * Color.Z
							: Color.Z > Color.Y && Color.Y > Color.X && Color.Z < 0.2f);
					}
				}
			}
		}
		const auto* Floor = Actor->FindComponentByClass<UStaticMeshComponent>();
		const auto* FloorStyle = Floor ? Cast<UMaterialInstanceDynamic>(Floor->GetMaterial(0)) : nullptr;
		if (TestNotNull(TEXT("Runtime floor material instance"), FloorStyle))
		{
			TestTrue(TEXT("Floor uses unlit shader"), FloorStyle->GetMaterial()->GetShadingModels().HasOnlyShadingModel(MSM_Unlit));
			const FLinearColor Color = FloorStyle->K2_GetVectorParameterValue(TEXT("Color"));
			TestTrue(TEXT("Floor is dark neutral gray, never default white"), Color.R > 0 && Color.R < 0.05f && Color.R == Color.G && Color.G == Color.B);
			TestTrue(TEXT("Reload preserves transient floor style"), Actor->Reload(Error));
			TestTrue(TEXT("No accumulation of floor material instances on reload"), Floor->GetMaterial(0) == FloorStyle);
		}
	}
	else { AddError(Error); }
	World->DestroyWorld(false);
	return true;
}
#endif
