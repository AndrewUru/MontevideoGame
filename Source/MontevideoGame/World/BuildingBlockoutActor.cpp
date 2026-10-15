#include "World/BuildingBlockoutActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogMontevideoBuildings,Log,All);

ABuildingBlockoutActor::ABuildingBlockoutActor()
{
	PrimaryActorTick.bCanEverTick=false;
	Buildings=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("BuildingSolids"));
	RootComponent=Buildings;
	Buildings->SetCollisionProfileName(TEXT("BlockAll"));
	Buildings->bUseAsyncCooking=false;
}

bool ABuildingBlockoutActor::Reload(FString& Error)
{
	if (!GetActorTransform().Equals(FTransform::Identity)) { Error=TEXT("Building actor must preserve geographic identity transform"); return false; }
	FBuildingFootprintData Candidate;
	FBuildingMeshMetrics NewMetrics;
	UE::Geometry::FDynamicMesh3 Mesh;
	if (!FBuildingFootprintLoader::Load(FPaths::ProjectDir(),ManifestPath,Candidate,Error)
		|| !FBuildingMeshGenerator::Build(Candidate,Mesh,NewMetrics,Error)) { return false; }
	UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
	if (!Material) { Error=TEXT("Required existing Engine unlit material unavailable"); return false; }
	Buildings->SetMaterial(0,Material);
	Buildings->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	const double CollisionStart=FPlatformTime::Seconds();
	Buildings->SetMesh(MoveTemp(Mesh));
	Buildings->SetComplexAsSimpleCollisionEnabled(true,true);
	CollisionMilliseconds=(FPlatformTime::Seconds()-CollisionStart)*1000;
	Data=MoveTemp(Candidate); Metrics=NewMetrics;
	LogStatus();
	return true;
}

void ABuildingBlockoutActor::LogStatus() const
{
	TSet<FString> Seen;
	int32 Official=0,Derived=0,Synthetic=0;
	double Min=MAX_dbl,Max=0,Sum=0;
	for (const auto& F : Data.Footprints)
	{
		if (Seen.Contains(F.BuildingId)) { continue; }
		Seen.Add(F.BuildingId);
		if (F.HeightSource==TEXT("official")) { ++Official; }
		else if (F.HeightSource==TEXT("synthetic_blockout")) { ++Synthetic; }
		else { ++Derived; }
		Min=FMath::Min(Min,F.HeightMeters); Max=FMath::Max(Max,F.HeightMeters); Sum+=F.HeightMeters;
	}
	UE_LOG(LogMontevideoBuildings,Display,TEXT("Buildings: %d buildings, %d footprints; heights official=%d derived=%d synthetic=%d; min/mean/max=%.3f/%.3f/%.3f m; vertices=%d triangles=%d walls=%d roofs=%d; dataset=%s"),
		Data.BuildingCount,Data.Footprints.Num(),Official,Derived,Synthetic,Seen.IsEmpty()?0:Min,Seen.IsEmpty()?0:Sum/Seen.Num(),Max,Metrics.Vertices,Metrics.Triangles,Metrics.WallTriangles,Metrics.RoofTriangles,*Data.Dataset);
	UE_LOG(LogMontevideoBuildings,Display,TEXT("Buildings metrics: JSON+IO+checksum=%.3f ms; conversion=%.3f ms; mesh=%.3f ms; component+collision=%.3f ms; mesh payload estimate=%lld bytes (excludes adjacency/allocator/collision/GPU); bounds cm [%s] to [%s]. SYNTHETIC HEIGHTS, Z=0 development support."),
		Data.LoadMetrics.JsonMilliseconds,Data.LoadMetrics.ConversionMilliseconds,Metrics.GenerationMilliseconds,CollisionMilliseconds,Metrics.ApproximateMeshBytes,*Metrics.Bounds.Min.ToString(),*Metrics.Bounds.Max.ToString());
}

void ABuildingBlockoutActor::ShowBounds() const
{
	LogStatus();
	if (!Metrics.Bounds.IsValid) { return; }
	DrawDebugBox(GetWorld(),Metrics.Bounds.GetCenter(),Metrics.Bounds.GetExtent(),FColor::Orange,false,30,0,10);
	for (const auto& F : Data.Footprints)
	{
		DrawDebugString(GetWorld(),F.Centroid+FVector(0,0,F.HeightMeters*100+100),F.SourceFeatureId+TEXT(" | synthetic"),nullptr,FColor::White,30);
	}
}

#if !UE_BUILD_SHIPPING
namespace
{
	void Command(UWorld* World,int32 Action)
	{
		if (!World) { return; }
		for (TActorIterator<ABuildingBlockoutActor> It(World);It;++It)
		{
			if (Action==0) { It->LogStatus(); }
			else if (Action==1) { It->ShowBounds(); }
			else { FString Error; if (!It->Reload(Error)) { UE_LOG(LogMontevideoBuildings,Error,TEXT("Reload rejected; previous buildings retained: %s"),*Error); } }
			return;
		}
		UE_LOG(LogMontevideoBuildings,Display,TEXT("No building blockout in this world. Play MVD_Prototype."));
	}
	FAutoConsoleCommandWithWorld Status(TEXT("Montevideo.Buildings.Status"),TEXT("Print building provenance, counts and timing."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){Command(W,0);}));
	FAutoConsoleCommandWithWorld Bounds(TEXT("Montevideo.Buildings.Bounds"),TEXT("Show building bounds and source IDs."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){Command(W,1);}));
	FAutoConsoleCommandWithWorld Reload(TEXT("Montevideo.Buildings.Reload"),TEXT("Validate and replace transient building meshes."),FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* W){Command(W,2);}));
}
#endif
