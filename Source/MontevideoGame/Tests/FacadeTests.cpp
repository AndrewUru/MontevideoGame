#include "World/FacadeLayoutGenerator.h"
#include "World/FacadeMeshGenerator.h"
#include "World/FacadeBlockoutActor.h"
#include "World/BuildingBlockoutActor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/DynamicMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Algo/Reverse.h"
#include <limits>

namespace FacadeTests
{
	constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	bool Load(FAutomationTestBase& Test,FBuildingFootprintData& D)
	{
		FString Error;return Test.TestTrue(*FString::Printf(TEXT("Load M3.2 %s"),*Error),FBuildingFootprintLoader::Load(FPaths::ProjectDir(),TEXT("Data/processed/montevideo/m3/buildings_manifest.json"),D,Error));
	}
	FBuildingFootprintData Fixture()
	{
		FBuildingFootprintData D;D.BuildingCount=1;FBuildingFootprint B;
		B.BuildingId=TEXT("test:1");B.SourceFeatureId=TEXT("1");B.Seed=2;B.Floors=4;B.HeightMeters=12;
		B.Geometry.Vertices={{0,0,0},{3000,0,0},{3000,1000,0},{1500,1000,0},{1500,2000,0},{0,2000,0},
			{300,300,0},{300,700,0},{700,700,0},{700,300,0}};
		B.Geometry.Rings={{0,1,2,3,4,5},{6,7,8,9}};D.Footprints.Add(B);return D;
	}
	void Validate(FAutomationTestBase& T,const FBuildingFootprintData& D,const FBuildingFacadeData& L)
	{
		TMap<FString,int32> Doors;
		for(const auto& S:L.Segments)
		{
			const auto& B=D.Footprints[S.Footprint];
			T.TestEqual(TEXT("Floors preserved"),S.Floors,B.Floors);T.TestEqual(TEXT("Exact height preserved"),S.Height,B.HeightMeters*100);
			T.TestEqual(TEXT("Floor boundaries count"),S.FloorZ.Num(),B.Floors+1);
			T.TestEqual(TEXT("Top exact"),S.FloorZ.Last(),S.Height);
			T.TestTrue(TEXT("Unit tangent and outward"),S.Tangent.IsNormalized() && S.Outward.IsNormalized());
			T.TestTrue(TEXT("Orthogonal horizontal basis"),FMath::Abs(FVector::DotProduct(S.Tangent,S.Outward))<1e-9 && S.Outward.Z==0);
			T.TestTrue(TEXT("Outer ring only"),S.Edge<B.Geometry.Rings[0].Num());
			for(int32 I=1;I<S.FloorZ.Num();++I) { T.TestTrue(TEXT("Positive floor height"),S.FloorZ[I]>S.FloorZ[I-1]); }
		}
		for(const auto& E:L.Elements)
		{
			const auto& S=L.Segments[E.Segment];
			T.TestTrue(TEXT("Finite ordered local bounds"),FMath::IsFinite(E.U0)&&FMath::IsFinite(E.U1)&&FMath::IsFinite(E.D0)&&FMath::IsFinite(E.D1)&&FMath::IsFinite(E.Z0)&&FMath::IsFinite(E.Z1)&&E.U1>E.U0&&E.D1>E.D0&&E.Z1>E.Z0);
			T.TestTrue(TEXT("Within edge and height"),E.U0>=0&&E.U1<=S.Length&&E.Z0>=0&&E.Z1<=S.Height);
			if(E.Kind==EFacadePiece::Window || E.Kind==EFacadePiece::Door || E.Kind==EFacadePiece::Balcony)
			{
				T.TestTrue(TEXT("Corner margin"),E.U0>=S.Margin && E.U1<=S.Length-S.Margin);
				T.TestTrue(TEXT("Module within its floor"),E.Z0>=S.FloorZ[E.Floor] && E.Z1<=S.FloorZ[E.Floor+1]);
				T.TestTrue(TEXT("No penetration of any footprint"),FFacadeLayoutGenerator::IsClear(D,S,E.U0,E.U1,E.D0,E.D1));
			}
			if(E.Kind==EFacadePiece::Balcony) { T.TestTrue(TEXT("No ground balcony"),E.Floor>0); }
			if(E.Kind==EFacadePiece::Door) { ++Doors.FindOrAdd(S.BuildingId);T.TestEqual(TEXT("Door ground only"),E.Floor,0); }
		}
		for(const auto& Entry:Doors) { T.TestEqual(TEXT("One door per building across parts"),Entry.Value,1); }
	}
	bool Same(const FBuildingFacadeData& A,const FBuildingFacadeData& B)
	{
		if(A.Segments.Num()!=B.Segments.Num()||A.Elements.Num()!=B.Elements.Num()) { return false; }
		for(int32 I=0;I<A.Segments.Num();++I)
		{
			const auto& X=A.Segments[I];const auto& Y=B.Segments[I];
			if(X.BuildingId!=Y.BuildingId||X.SourceFeatureId!=Y.SourceFeatureId||X.Style!=Y.Style||X.Origin!=Y.Origin||X.Tangent!=Y.Tangent||X.Outward!=Y.Outward||X.Length!=Y.Length||X.Height!=Y.Height||X.FloorZ!=Y.FloorZ||X.Bays!=Y.Bays) { return false; }
		}
		for(int32 I=0;I<A.Elements.Num();++I)
		{
			const auto& X=A.Elements[I];const auto& Y=B.Elements[I];
			if(X.Segment!=Y.Segment||X.Floor!=Y.Floor||X.Kind!=Y.Kind||X.U0!=Y.U0||X.U1!=Y.U1||X.D0!=Y.D0||X.D1!=Y.D1||X.Z0!=Y.Z0||X.Z1!=Y.Z1||X.Color!=Y.Color) { return false; }
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeDeterminismTest,"Montevideo.Facades.01DeterminismAndRealData",FacadeTests::Flags)
bool FFacadeDeterminismTest::RunTest(const FString&)
{
	FBuildingFootprintData D;if(!FacadeTests::Load(*this,D)) { return false; }FString Error;FBuildingFacadeData A,B;
	if(!TestTrue(TEXT("Layout actual data"),FFacadeLayoutGenerator::Build(D,A,Error))||!TestTrue(TEXT("Second layout"),FFacadeLayoutGenerator::Build(D,B,Error))) { return false; }
	TestTrue(TEXT("All geometry/color/layout fields exactly deterministic"),FacadeTests::Same(A,B));
	TestEqual(TEXT("Nine buildings"),A.Buildings,9);TestEqual(TEXT("One door each"),A.Doors,9);
	TestTrue(TEXT("Windows and balconies exist"),A.Windows>0&&A.Balconies>0);
	FacadeTests::Validate(*this,D,A);
	auto Fixture=FacadeTests::Fixture();FBuildingFacadeData First,Other;
	FFacadeLayoutGenerator::Build(Fixture,First,Error);
	Fixture.Footprints[0].SourceFeatureId=TEXT("different");Fixture.Footprints[0].BuildingId=TEXT("test:different");Fixture.Footprints[0].Seed=FBuildingFootprintLoader::StableSeed(Fixture.Footprints[0].BuildingId);
	FFacadeLayoutGenerator::Build(Fixture,Other,Error);TestFalse(TEXT("Different identity permits variation"),FacadeTests::Same(First,Other));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeTopologyTest,"Montevideo.Facades.02ConcavityHolesMultipartAndWinding",FacadeTests::Flags)
bool FFacadeTopologyTest::RunTest(const FString&)
{
	auto D=FacadeTests::Fixture();auto Part=D.Footprints[0];for(auto& P:Part.Geometry.Vertices) { P+=FVector(5000,0,0); }D.Footprints.Add(Part);
	for(int32 Style=0;Style<4;++Style)
	{
		FString Error;FBuildingFacadeData L;if(!TestTrue(TEXT("All generic styles"),FFacadeLayoutGenerator::Build(D,L,Error,Style))) { return false; }
		TestEqual(TEXT("Exterior segments only, no courtyard"),L.Segments.Num(),12);TestEqual(TEXT("Single multipart door"),L.Doors,1);
		FacadeTests::Validate(*this,D,L);
		// First edge points east along +X, outside is -Y for this CCW fixture.
		TestTrue(TEXT("Outward facing away from solid"),L.Segments[0].Outward.Equals(FVector(0,-1,0)));
	}
	for(auto& B:D.Footprints) for(auto& R:B.Geometry.Rings) { Algo::Reverse(R); }
	FString Error;FBuildingFacadeData Reverse;TestTrue(TEXT("Reversed winding accepted"),FFacadeLayoutGenerator::Build(D,Reverse,Error));FacadeTests::Validate(*this,D,Reverse);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeEdgesTest,"Montevideo.Facades.03ShortLongAndInvalidInputs",FacadeTests::Flags)
bool FFacadeEdgesTest::RunTest(const FString&)
{
	auto D=FacadeTests::Fixture();auto& G=D.Footprints[0].Geometry;
	G.Vertices={{0,0,0},{30000,0,0},{30000,20,0},{0,20,0}};G.Rings={{0,1,2,3}};
	FString Error;FBuildingFacadeData L;TestTrue(TEXT("Long and short edges"),FFacadeLayoutGenerator::Build(D,L,Error));
	TestTrue(TEXT("Long edge multiple bays"),L.Segments[0].Bays>50);TestEqual(TEXT("Short edge no partial bay"),L.Segments[1].Bays,0);FacadeTests::Validate(*this,D,L);
	G.Vertices[0].X=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("Reject NaN"),FFacadeLayoutGenerator::Build(D,L,Error));
	G.Vertices[0].X=std::numeric_limits<double>::infinity();TestFalse(TEXT("Reject infinity"),FFacadeLayoutGenerator::Build(D,L,Error));
	G.Vertices[0].X=0;D.Footprints[0].Floors=0;TestFalse(TEXT("Reject zero floors"),FFacadeLayoutGenerator::Build(D,L,Error));
	D.Footprints[0].Floors=4;G.Rings[0][0]=999;TestFalse(TEXT("Reject invalid ring index"),FFacadeLayoutGenerator::Build(D,L,Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeGeometryTest,"Montevideo.Facades.04MeshBoundsNormalsAndDeterminism",FacadeTests::Flags)
bool FFacadeGeometryTest::RunTest(const FString&)
{
	using namespace UE::Geometry;
	FBuildingFootprintData D;if(!FacadeTests::Load(*this,D)) { return false; }FBuildingFacadeData L;FString Error;
	if(!FFacadeLayoutGenerator::Build(D,L,Error)) { AddError(Error);return false; }
	FDynamicMesh3 A,B;FFacadeMeshMetrics MA,MB;
	if(!TestTrue(TEXT("Mesh A"),FFacadeMeshGenerator::Build(L,A,MA,Error))||!TestTrue(TEXT("Mesh B"),FFacadeMeshGenerator::Build(L,B,MB,Error))) { return false; }
	TestEqual(TEXT("Box vertices"),MA.Vertices,L.Elements.Num()*8);TestEqual(TEXT("Box triangles"),MA.Triangles,L.Elements.Num()*12);
	TestTrue(TEXT("Finite bounds"),MA.Bounds.IsValid&&!MA.Bounds.Min.ContainsNaN()&&!MA.Bounds.Max.ContainsNaN());
	TestTrue(TEXT("Height not extended"),MA.Bounds.Min.Z>=0&&MA.Bounds.Max.Z<=1500);
	for(int32 V:A.VertexIndicesItr()) { TestTrue(TEXT("Exact deterministic vertex"),A.GetVertex(V)==B.GetVertex(V)); }
	for(int32 T:A.TriangleIndicesItr())
	{
		const auto Tri=A.GetTriangle(T);TestTrue(TEXT("Exact indices"),Tri==B.GetTriangle(T));
		const auto Normal=VectorUtil::Normal(A.GetVertex(Tri.A),A.GetVertex(Tri.B),A.GetVertex(Tri.C));
		TestTrue(TEXT("Nondegenerate normal"),Normal.SquaredLength()>.99);
		const auto N=A.Attributes()->PrimaryNormals()->GetElement(A.Attributes()->PrimaryNormals()->GetTriangle(T).A);
		TestTrue(TEXT("Render normal coherent"),FVector3d(N).Dot(Normal)>.999);
		TestTrue(TEXT("Group maps to facade metadata"),L.Segments.IsValidIndex(A.GetTriangleGroup(T)));
	}
	L.Elements[0].Z1=std::numeric_limits<double>::infinity();TestFalse(TEXT("Mesh rejects nonfinite dimensions"),FFacadeMeshGenerator::Build(L,B,MB,Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeRuntimeTest,"Montevideo.Facades.05ReloadAndCollisionIsolation",FacadeTests::Flags)
bool FFacadeRuntimeTest::RunTest(const FString&)
{
	UWorld::InitializationValues Values;Values.AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Values);
	if(!World) { return false; }FString Error;
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	auto* Mass=World->SpawnActor<ABuildingBlockoutActor>();auto* Facade=World->SpawnActor<AFacadeBlockoutActor>();
	if(!Mass->Reload(Error)||!Facade->Initialize(Mass,Error)) { AddError(Error);GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return false; }
	const int32 Count=Facade->GetMetrics().Triangles;const FBuildingFacadeData Original=Facade->GetLayout();
	TestTrue(TEXT("Reload succeeds"),Facade->Reload(Error));TestEqual(TEXT("No accumulated unique triangles"),Facade->GetMetrics().Triangles,Count);
	TestTrue(TEXT("Reload identical layout"),FacadeTests::Same(Original,Facade->GetLayout()));
	TArray<UInstancedStaticMeshComponent*> Components;Facade->GetComponents(Components);
	TestEqual(TEXT("Component pool matches metric"),Components.Num(),Facade->GetMetrics().Components);
	int32 TotalInstances=0;
	for(auto* Component:Components) { TotalInstances+=Component->GetInstanceCount();TestEqual(TEXT("Decorations do not collide"),Component->GetCollisionEnabled(),ECollisionEnabled::NoCollision); }
	TestEqual(TEXT("Every element instanced once"),TotalInstances,Facade->GetLayout().Elements.Num());
	TestTrue(TEXT("Legacy data and transforms match"),Facade->CompareLegacy(Error));
	for(int32 Repeat=0;Repeat<3;++Repeat)
	{
		TestTrue(TEXT("Repeated reload"),Facade->Reload(Error));
		TArray<UInstancedStaticMeshComponent*> Current;Facade->GetComponents(Current);
		TestEqual(TEXT("No duplicated components"),Current.Num(),Components.Num());
		int32 Instances=0;for(auto* C:Current) { Instances+=C->GetInstanceCount(); }
		TestEqual(TEXT("No duplicated instances"),Instances,TotalInstances);
	}
	TArray<UDynamicMeshComponent*> MassComponents;Mass->GetComponents(MassComponents);TestTrue(TEXT("Massing retains collision"),MassComponents[0]->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Override supported"),Facade->Reload(Error,1));TestEqual(TEXT("All modern"),Facade->GetLayout().StyleBuildings[1],9);
	TestFalse(TEXT("Invalid override rejected"),Facade->Reload(Error,9));TestEqual(TEXT("Old override remains"),Facade->GetLayout().StyleBuildings[1],9);
	TestTrue(TEXT("Reset seed styles"),Facade->Reload(Error));TestTrue(TEXT("Reset deterministic"),FacadeTests::Same(Original,Facade->GetLayout()));
	TestTrue(TEXT("Reset parity"),Facade->CompareLegacy(Error));
	Mass->Destroy();TestFalse(TEXT("Missing mass rejected"),Facade->Reload(Error));
	Facade->Destroy();
	for(auto* Component:Components) { TestFalse(TEXT("Facade instance component unregisters with actor"),Component->IsRegistered()); }
	GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return true;
}
#endif
