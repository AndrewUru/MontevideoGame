#include "Geospatial/BuildingFootprintData.h"
#include "World/BuildingMeshGenerator.h"
#include "World/BuildingBlockoutActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "GameFramework/Character.h"

using namespace UE::Geometry;
namespace BuildingTests
{
	constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	const FString Manifest=TEXT("Data/processed/montevideo/m3/buildings_manifest.json");
	bool Load(FAutomationTestBase& Test,FBuildingFootprintData& Data)
	{
		FString Error;
		const bool Result=FBuildingFootprintLoader::Load(FPaths::ProjectDir(),Manifest,Data,Error);
		return Test.TestTrue(*FString::Printf(TEXT("Load actual footprints: %s"),*Error),Result);
	}
	UWorld* PhysicsWorld()
	{
		UWorld::InitializationValues Values;
		Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
		return UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	}
	FBuildingFootprintData Fixture()
	{
		FBuildingFootprintData Data;
		Data.BuildingCount=1;
		FBuildingFootprint Hole;
		Hole.BuildingId=TEXT("fixture:multipart"); Hole.HeightMeters=10;
		Hole.Geometry.Vertices={{0,0,0},{1000,0,0},{1000,1000,0},{0,1000,0},{300,300,0},{300,700,0},{700,700,0},{700,300,0}};
		Hole.Geometry.Rings={{0,1,2,3},{4,5,6,7}};
		Hole.Geometry.Triangles={0,1,7,0,7,4,1,2,6,1,6,7,2,3,5,2,5,6,3,0,4,3,4,5};
		Data.Footprints.Add(Hole);
		FBuildingFootprint Concave;
		Concave.BuildingId=Hole.BuildingId; Concave.HeightMeters=10;
		Concave.Geometry.Vertices={{2000,0,0},{3000,0,0},{3000,400,0},{2400,400,0},{2400,1000,0},{2000,1000,0}};
		Concave.Geometry.Rings={{0,1,2,3,4,5}};
		Concave.Geometry.Triangles={0,1,3,1,2,3,0,3,5,3,4,5};
		Data.Footprints.Add(Concave);
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingsLoadTest,"Montevideo.Buildings.01DataAndDeterminism",BuildingTests::Flags)
bool FBuildingsLoadTest::RunTest(const FString&)
{
	using namespace BuildingTests;
	FBuildingFootprintData A,B;
	if (!Load(*this,A) || !Load(*this,B)) { return false; }
	TestEqual(TEXT("Current limited POC buildings"),A.BuildingCount,9);
	TestEqual(TEXT("Multipart footprints"),A.Footprints.Num(),12);
	TestEqual(TEXT("Stable SHA1 byte order known vector"),FBuildingFootprintLoader::StableSeed(TEXT("abc")),0xa9993e36u);
	for (int32 I=0;I<A.Footprints.Num();++I)
	{
		const auto& F=A.Footprints[I];
		TestTrue(TEXT("Nonempty real footprint"),F.Geometry.Vertices.Num()>=3);
		TestEqual(TEXT("Height explicitly synthetic"),F.HeightSource,FString(TEXT("synthetic_blockout")));
		TestTrue(TEXT("Positive height"),F.HeightMeters>0);
		TestEqual(TEXT("Stable height on second load"),F.HeightMeters,B.Footprints[I].HeightMeters);
		TestEqual(TEXT("Stable seed on second load"),F.Seed,B.Footprints[I].Seed);
		TestEqual(TEXT("Seed matches source identity"),F.Seed,FBuildingFootprintLoader::StableSeed(F.BuildingId));
		FGeoReference Reference; Reference.SetOrigin(A.Origin);
		for (const FVector& Point:F.Geometry.Vertices)
		{
			TestTrue(TEXT("Within M3 and M2 radius"),Point.Size2D()<=50000 && Point.Size()<FGeoReference::MaxLocalRadiusMeters*100);
			FGeoCoordinate Geo; FLocalGeoCoordinate Back;
			TestEqual(TEXT("Local -> Geographic"),Reference.LocalToGeographic(FLocalGeoCoordinate(Point),Geo),EGeoConversionResult::Success);
			TestEqual(TEXT("Geographic -> Local"),Reference.GeographicToLocal(Geo,Back),EGeoConversionResult::Success);
			TestTrue(TEXT("Footprint round trip"),Back.Centimeters.Equals(Point,0.001));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingsMeshTest,"Montevideo.Buildings.02ClosedMeshAndNormals",BuildingTests::Flags)
bool FBuildingsMeshTest::RunTest(const FString&)
{
	using namespace BuildingTests;
	FBuildingFootprintData Data;
	if (!Load(*this,Data)) { return false; }
	FDynamicMesh3 Mesh,Again;
	FBuildingMeshMetrics Metrics,OtherMetrics; FString Error;
	if (!TestTrue(*FString::Printf(TEXT("Extrude actual data: %s"),*Error),FBuildingMeshGenerator::Build(Data,Mesh,Metrics,Error))) { AddError(Error); return false; }
	TestTrue(TEXT("Repeat extrusion"),FBuildingMeshGenerator::Build(Data,Again,OtherMetrics,Error));
	TestEqual(TEXT("Same vertex count"),Mesh.VertexCount(),Again.VertexCount());
	TestEqual(TEXT("Same triangle count"),Mesh.TriangleCount(),Again.TriangleCount());
	TestTrue(TEXT("Bounds valid"),Metrics.Bounds.IsValid && Metrics.Bounds.GetSize().Z>0);
	TArray<double> RoofAreas; RoofAreas.Init(0,Data.Footprints.Num());
	TSet<int32> Groups;
	int32 Walls=0,Roofs=0,Bases=0;
	for (int32 V:Mesh.VertexIndicesItr()) { TestTrue(TEXT("Exactly deterministic vertex"),Mesh.GetVertex(V)==Again.GetVertex(V)); }
	for (int32 T:Mesh.TriangleIndicesItr())
	{
		const auto Triangle=Mesh.GetTriangle(T);
		TestTrue(TEXT("Exactly deterministic triangle"),Triangle==Again.GetTriangle(T));
		const int32 Group=Mesh.GetTriangleGroup(T); Groups.Add(Group);
		TestTrue(TEXT("Group identifies footprint and building metadata"),Data.Footprints.IsValidIndex(Group) && !Data.Footprints[Group].SourceFeatureId.IsEmpty());
		const FVector Cross=FVector::CrossProduct(Mesh.GetVertex(Triangle.B)-Mesh.GetVertex(Triangle.A),Mesh.GetVertex(Triangle.C)-Mesh.GetVertex(Triangle.A));
		TestTrue(TEXT("No degenerate triangles"),Cross.SizeSquared()>1e-16);
		const auto Normals=Mesh.Attributes()->PrimaryNormals();
		const FVector Normal(Normals->GetElement(Normals->GetTriangle(T).A));
		TestTrue(TEXT("Overlay normal matches Unreal front face"),FVector::DotProduct(Normal,-Cross.GetSafeNormal())>0.99);
		if (Normal.Z>0.99) { ++Roofs; RoofAreas[Group]+=Cross.Size()/20000; }
		else if (Normal.Z< -0.99) { ++Bases; }
		else { ++Walls; TestTrue(TEXT("Vertical wall normal"),FMath::Abs(Normal.Z)<0.01); }
	}
	for (int32 E:Mesh.EdgeIndicesItr()) { TestFalse(TEXT("Closed solid, no roof-wall T junctions"),Mesh.IsBoundaryEdge(E)); }
	TestEqual(TEXT("One group per footprint"),Groups.Num(),Data.Footprints.Num());
	TestEqual(TEXT("Bases equal roofs"),Bases,Roofs);
	TestEqual(TEXT("Roof accounting"),Roofs,Metrics.RoofTriangles);
	TestEqual(TEXT("Wall accounting"),Walls,Metrics.WallTriangles);
	for (int32 I=0;I<RoofAreas.Num();++I) { TestTrue(TEXT("Roof area agrees with converted footprint within 1e-6 m2"),FMath::Abs(RoofAreas[I]-Data.Footprints[I].Geometry.AreaSquareMeters)<1e-6); }
	AddInfo(FString::Printf(TEXT("Mesh vertices=%d triangles=%d roofs=%d bases=%d walls=%d groups=%d extrusion_ms=%.3f payload_bytes=%lld"),Metrics.Vertices,Metrics.Triangles,Roofs,Bases,Walls,Groups.Num(),Metrics.GenerationMilliseconds,Metrics.ApproximateMeshBytes));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingsHolesTest,"Montevideo.Buildings.03HolesConcavityMultipart",BuildingTests::Flags)
bool FBuildingsHolesTest::RunTest(const FString&)
{
	using namespace BuildingTests;
	FBuildingFootprintData Data=Fixture(); FDynamicMesh3 Mesh; FBuildingMeshMetrics Metrics; FString Error;
	// Reverse rings to test winding independence without changing roof triangulation.
	for (auto& F:Data.Footprints) { for (auto& Ring:F.Geometry.Rings) { Algo::Reverse(Ring); } }
	if (!TestTrue(TEXT("Extrude concave and holed multipart fixture"),FBuildingMeshGenerator::Build(Data,Mesh,Metrics,Error))) { AddError(Error); return false; }
	UWorld* World=PhysicsWorld();
	if (!TestNotNull(TEXT("Physics world"),World)) { return false; }
	AActor* Actor=World->SpawnActor<AActor>();
	UDynamicMeshComponent* Component=NewObject<UDynamicMeshComponent>(Actor);
	Actor->SetRootComponent(Component); Component->RegisterComponent();
	Component->SetCollisionProfileName(TEXT("BlockAll")); Component->bUseAsyncCooking=false;
	Component->SetMesh(MoveTemp(Mesh)); Component->SetComplexAsSimpleCollisionEnabled(true,true);
	FHitResult Hit;
	auto Vertical=[&](double X,double Y){return World->LineTraceSingleByChannel(Hit,FVector(X,Y,2000),FVector(X,Y,-100),ECC_Visibility);};
	TestTrue(TEXT("Roof outside courtyard exists"),Vertical(150,150));
	TestFalse(TEXT("Hole has no roof or base collision"),Vertical(500,500));
	TestFalse(TEXT("Concave notch remains empty"),Vertical(2700,700));
	TestTrue(TEXT("Second polygon of same building has collision"),Vertical(2200,700));
	TestTrue(TEXT("Courtyard wall is solid"),World->LineTraceSingleByChannel(Hit,FVector(500,500,500),FVector(100,500,500),ECC_Visibility));
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingsCollisionTest,"Montevideo.Buildings.04RuntimeCollisionAndReload",BuildingTests::Flags)
bool FBuildingsCollisionTest::RunTest(const FString&)
{
	using namespace BuildingTests;
	UWorld* World=PhysicsWorld();
	if (!TestNotNull(TEXT("Physics world"),World)) { return false; }
	ABuildingBlockoutActor* Actor=World->SpawnActor<ABuildingBlockoutActor>(); FString Error;
	if (TestTrue(TEXT("Load runtime building actor"),Actor->Reload(Error)))
	{
		const auto& F=Actor->GetData().Footprints[0]; const auto& G=F.Geometry;
		double BestArea=0; FVector RoofPoint;
		for (int32 I=0;I<G.Triangles.Num();I+=3)
		{
			const FVector A=G.Vertices[G.Triangles[I]],B=G.Vertices[G.Triangles[I+1]],C=G.Vertices[G.Triangles[I+2]];
			const double Area=FVector::CrossProduct(B-A,C-A).Size();
			if (Area>BestArea) { BestArea=Area; RoofPoint=(A+B+C)/3; }
		}
		FHitResult Hit;
		TestTrue(TEXT("Vertical ray hits roof"),World->LineTraceSingleByChannel(Hit,RoofPoint+FVector(0,0,F.HeightMeters*100+500),RoofPoint-FVector(0,0,100),ECC_Visibility));
		TestTrue(TEXT("Roof height and upward normal"),FMath::Abs(Hit.ImpactPoint.Z-F.HeightMeters*100)<0.1 && Hit.ImpactNormal.Z>0.99);
		TestFalse(TEXT("No building collision in plaza origin"),World->LineTraceSingleByChannel(Hit,FVector(0,0,10000),FVector(0,0,-100),ECC_Visibility));
		const auto& Ring=G.Rings[0]; double Longest=0,Area2=0; FVector A,B;
		for (int32 I=0;I<Ring.Num();++I)
		{
			const FVector P=G.Vertices[Ring[I]],Q=G.Vertices[Ring[(I+1)%Ring.Num()]];
			Area2+=P.X*Q.Y-Q.X*P.Y;
			if ((P-Q).SizeSquared()>Longest) { Longest=(P-Q).SizeSquared();A=P;B=Q; }
		}
		const FVector Outside=FVector::CrossProduct(B-A,FVector::UpVector).GetSafeNormal()*(Area2>0?1:-1);
		const FVector Wall=(A+B)/2+FVector(0,0,150);
		TestTrue(TEXT("Lateral ray hits wall"),World->LineTraceSingleByChannel(Hit,Wall+Outside*500,Wall-Outside*100,ECC_Visibility));
		TestTrue(TEXT("Wall normal faces exterior"),FVector::DotProduct(Hit.ImpactNormal,Outside)>0.99);
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* Character=World->SpawnActor<ACharacter>(ACharacter::StaticClass(),Wall+Outside*500,FRotator::ZeroRotator,Spawn);
		if (TestNotNull(TEXT("Character collision fixture"),Character))
		{
			Character->GetCapsuleComponent()->SetCapsuleSize(42,96);
			Character->SetActorLocation(Wall-Outside*100,true,&Hit);
			TestTrue(TEXT("Character swept capsule cannot cross wall"),Hit.bBlockingHit && FVector::DotProduct(Character->GetActorLocation()-Wall,Outside)>40);
		}
		const int32 Count=Actor->GetMetrics().Triangles;
		TestTrue(TEXT("Reload valid buildings"),Actor->Reload(Error));
		TestEqual(TEXT("No duplicated triangles"),Actor->GetMetrics().Triangles,Count);
		Actor->ManifestPath=TEXT("Data/processed/montevideo/m3/absent.json");
		TestFalse(TEXT("Bad reload rejected"),Actor->Reload(Error));
		TestEqual(TEXT("Old geometry retained"),Actor->GetMetrics().Triangles,Count);
		TArray<UDynamicMeshComponent*> Components; Actor->GetComponents(Components);
		TestEqual(TEXT("One aggregated mesh component"),Components.Num(),1);
	}
	else { AddError(Error); }
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBuildingsInvalidTest,"Montevideo.Buildings.05RejectInvalidExtrusion",BuildingTests::Flags)
bool FBuildingsInvalidTest::RunTest(const FString&)
{
	using namespace BuildingTests;
	auto Data=Fixture(); FDynamicMesh3 Mesh; FBuildingMeshMetrics Metrics; FString Error;
	Data.Footprints[0].HeightMeters=0;
	TestFalse(TEXT("Reject zero height"),FBuildingMeshGenerator::Build(Data,Mesh,Metrics,Error));
	Data=Fixture(); Data.Footprints[0].Geometry.Triangles[0]=9999;
	TestFalse(TEXT("Reject invalid indices"),FBuildingMeshGenerator::Build(Data,Mesh,Metrics,Error));
	Data=Fixture(); Data.Footprints[0].Geometry.Rings[0].RemoveAt(1);
	TestFalse(TEXT("Reject wall/roof mismatch"),FBuildingMeshGenerator::Build(Data,Mesh,Metrics,Error));
	Data=Fixture(); Data.Footprints[0].Geometry.Vertices[0].X=2100000;
	TestFalse(TEXT("Reject outside geographic domain"),FBuildingMeshGenerator::Build(Data,Mesh,Metrics,Error));
	return true;
}
#endif
