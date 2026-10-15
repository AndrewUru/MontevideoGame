#include "World/FacadeInstanceBatches.h"
#include "Geospatial/BuildingFootprintData.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Algo/Reverse.h"
#include <limits>

namespace FacadeOptimizationTests
{
	constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter;
	FBuildingFootprintData Fixture()
	{
		FBuildingFootprintData D;D.BuildingCount=1;FBuildingFootprint B;
		B.BuildingId=TEXT("test:1");B.SourceFeatureId=TEXT("1");B.Seed=2;B.Floors=4;B.HeightMeters=12;
		B.Geometry.Vertices={{0,0,0},{3000,0,0},{3000,1000,0},{1500,1000,0},{1500,2000,0},{0,2000,0},
			{300,300,0},{300,700,0},{700,700,0},{700,300,0}};
		B.Geometry.Rings={{0,1,2,3,4,5},{6,7,8,9}};D.Footprints.Add(B);return D;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeBatchParityTest,"Montevideo.FacadeOptimization.01GoldenLayoutAndBatches",FacadeOptimizationTests::Flags)
bool FFacadeBatchParityTest::RunTest(const FString&)
{
	FBuildingFootprintData Source;FString Error;
	if(!TestTrue(TEXT("Load official M3.2 data"),FBuildingFootprintLoader::Load(FPaths::ProjectDir(),TEXT("Data/processed/montevideo/m3/buildings_manifest.json"),Source,Error))) { return false; }
	FBuildingFacadeData Legacy,Again;FFacadeInstanceData Batches;
	if(!TestTrue(TEXT("Generate unchanged M3.3 layout"),FFacadeLayoutGenerator::Build(Source,Legacy,Error))
		|| !TestTrue(TEXT("Generate again"),FFacadeLayoutGenerator::Build(Source,Again,Error))
		|| !TestTrue(TEXT("Batch layout"),FFacadeInstanceBatches::Build(Again,Batches,Error))) { return false; }
	TestEqual(TEXT("Golden windows"),Legacy.Windows,1762);TestEqual(TEXT("Golden doors"),Legacy.Doors,9);TestEqual(TEXT("Golden balconies"),Legacy.Balconies,465);
	TestEqual(TEXT("Golden segments"),Legacy.Segments.Num(),506);TestEqual(TEXT("Golden bays"),Legacy.Bays,430);
	TestEqual(TEXT("Golden elements/instances"),Batches.Instances,13362);
	TestTrue(TEXT("All legacy fields and oriented corners match"),FFacadeInstanceBatches::Compare(Legacy,Again,Batches,Error));
	TestEqual(TEXT("Batch windows"),Batches.Batches[9].Transforms.Num(),1762);
	TestEqual(TEXT("Batch doors"),Batches.Batches[11].Transforms.Num(),9);
	TestEqual(TEXT("Balcony slab per logical balcony"),Batches.Batches[12].Transforms.Num(),465);
	TestEqual(TEXT("Three rails per logical balcony"),Batches.Batches[13].Transforms.Num(),1395);
	for(const auto& Batch:Batches.Batches) for(int32 ElementIndex:Batch.ElementIndices)
	{
		const auto& E=Legacy.Elements[ElementIndex];const auto& S=Legacy.Segments[E.Segment];
		TestEqual(TEXT("Stable seed through footprint"),Source.Footprints[S.Footprint].Seed,FBuildingFootprintLoader::StableSeed(S.BuildingId));
		TestEqual(TEXT("Stable feature through segment"),S.SourceFeatureId,Source.Footprints[S.Footprint].SourceFeatureId);
		TestTrue(TEXT("No patio ring segment"),S.Edge<Source.Footprints[S.Footprint].Geometry.Rings[0].Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFacadeBatchGeometryTest,"Montevideo.FacadeOptimization.02ConcavityHolesLongShortAndInvalid",FacadeOptimizationTests::Flags)
bool FFacadeBatchGeometryTest::RunTest(const FString&)
{
	auto D=FacadeOptimizationTests::Fixture();auto Other=D.Footprints[0];for(auto& V:Other.Geometry.Vertices) { V+=FVector(5000,0,0); }D.Footprints.Add(Other);
	for(int32 Style=0;Style<4;++Style)
	{
		FString Error;FBuildingFacadeData L;FFacadeInstanceData B;
		if(!TestTrue(TEXT("Concave multipart hole layout"),FFacadeLayoutGenerator::Build(D,L,Error,Style))
			|| !TestTrue(TEXT("Batches"),FFacadeInstanceBatches::Build(L,B,Error))) { return false; }
		TestEqual(TEXT("Exterior edges only"),L.Segments.Num(),12);
		TestEqual(TEXT("One door across both parts"),B.Doors,1);
		TestTrue(TEXT("Oriented transform parity"),FFacadeInstanceBatches::Compare(L,L,B,Error));
	}
	for(auto& B:D.Footprints) for(auto& R:B.Geometry.Rings) { Algo::Reverse(R); }
	FString Error;FBuildingFacadeData Reverse;FFacadeInstanceData ReverseBatches;
	TestTrue(TEXT("Winding independent layout"),FFacadeLayoutGenerator::Build(D,Reverse,Error));
	TestTrue(TEXT("Winding independent batching"),FFacadeInstanceBatches::Build(Reverse,ReverseBatches,Error));
	D.Footprints.SetNum(1);auto& G=D.Footprints[0].Geometry;
	G.Vertices={{0,0,0},{30000,0,0},{30000,20,0},{0,20,0}};G.Rings={{0,1,2,3}};
	FBuildingFacadeData LongShort;FFacadeInstanceData LongShortBatches;
	TestTrue(TEXT("Long/short layout"),FFacadeLayoutGenerator::Build(D,LongShort,Error));
	TestTrue(TEXT("Long/short batching"),FFacadeInstanceBatches::Build(LongShort,LongShortBatches,Error));
	TestTrue(TEXT("Many bays on long edge"),LongShort.Segments[0].Bays>50);
	TestEqual(TEXT("No bay on short edge"),LongShort.Segments[1].Bays,0);
	LongShort.Elements[0].U0=std::numeric_limits<double>::quiet_NaN();
	TestFalse(TEXT("Reject NaN before render"),FFacadeInstanceBatches::Build(LongShort,LongShortBatches,Error));
	LongShort.Elements[0].U0=std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Reject Inf before render"),FFacadeInstanceBatches::Build(LongShort,LongShortBatches,Error));
	return true;
}
#endif
