#include "World/FacadeBlockoutActor.h"
#include "World/BuildingBlockoutActor.h"
#include "Components/DynamicMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogMontevideoFacades,Log,All);

AFacadeBlockoutActor::AFacadeBlockoutActor()
{
	PrimaryActorTick.bCanEverTick=false;
	Facades=CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("FacadePresentation"));RootComponent=Facades;
	Facades->SetCollisionEnabled(ECollisionEnabled::NoCollision);Facades->SetCanEverAffectNavigation(false);
}

bool AFacadeBlockoutActor::Initialize(ABuildingBlockoutActor* InSource,FString& Error)
{
	Source=InSource;return Reload(Error);
}

bool AFacadeBlockoutActor::Reload(FString& Error,int32 Style)
{
	const double Start=FPlatformTime::Seconds();
	if(!Source.IsValid() || !GetActorTransform().Equals(FTransform::Identity) || !Source->GetActorTransform().Equals(FTransform::Identity))
	{ Error=TEXT("Facades require a live massing source and identity transforms");return false; }
	FBuildingFacadeData Candidate;FFacadeMeshMetrics NewMetrics;UE::Geometry::FDynamicMesh3 Mesh;
	if(!FFacadeLayoutGenerator::Build(Source->GetData(),Candidate,Error,Style)||!FFacadeMeshGenerator::Build(Candidate,Mesh,NewMetrics,Error)) { return false; }
	UMaterialInterface* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/EngineDebugMaterials/VertexColorViewMode_ColorOnly.VertexColorViewMode_ColorOnly"));
	if(!Material) { Error=TEXT("Existing Engine vertex color material unavailable");return false; }
	Facades->SetMaterial(0,Material);Facades->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	const double ComponentStart=FPlatformTime::Seconds();Facades->SetMesh(MoveTemp(Mesh));ComponentMilliseconds=(FPlatformTime::Seconds()-ComponentStart)*1000;
	Layout=MoveTemp(Candidate);Metrics=NewMetrics;TotalMilliseconds=(FPlatformTime::Seconds()-Start)*1000;LogStatus();return true;
}

void AFacadeBlockoutActor::LogStatus() const
{
	UE_LOG(LogMontevideoFacades,Display,TEXT("Facades: buildings=%d facades=%d bays=%d windows=%d doors=%d balconies=%d short-edges=%d clearance-rejections=%d styles Classic/Modern/Residential/Civic=%d/%d/%d/%d; vertices=%d triangles=%d; layout=%.3f ms mesh=%.3f ms component=%.3f ms total=%.3f ms payload-estimate=%lld bytes (excludes allocator/adjacency/GPU/layout). GENERIC SYNTHETIC STYLES."),
		Layout.Buildings,Layout.Segments.Num(),Layout.Bays,Layout.Windows,Layout.Doors,Layout.Balconies,Layout.ShortEdges,Layout.RejectedDecorations,Layout.StyleBuildings[0],Layout.StyleBuildings[1],Layout.StyleBuildings[2],Layout.StyleBuildings[3],Metrics.Vertices,Metrics.Triangles,Layout.LayoutMilliseconds,Metrics.MeshMilliseconds,ComponentMilliseconds,TotalMilliseconds,Metrics.PayloadBytes);
}

void AFacadeBlockoutActor::ShowBuilding(const FString& FeatureId) const
{
	FBox Bounds(ForceInit);int32 Count=0;
	for(const auto& S:Layout.Segments) if(S.SourceFeatureId==FeatureId || S.BuildingId==FeatureId)
	{
		Bounds+=S.Origin;Bounds+=S.Origin+S.Tangent*S.Length+FVector(0,0,S.Height);++Count;
		DrawDebugLine(GetWorld(),S.Origin+FVector(0,0,S.Height+30),S.Origin+S.Tangent*S.Length+FVector(0,0,S.Height+30),FColor::Cyan,false,30,0,6);
	}
	if(Bounds.IsValid) { DrawDebugBox(GetWorld(),Bounds.GetCenter(),Bounds.GetExtent(),FColor::Cyan,false,30,0,8);DrawDebugString(GetWorld(),Bounds.GetCenter()+FVector(0,0,Bounds.GetExtent().Z+100),FeatureId,nullptr,FColor::White,30); }
	UE_LOG(LogMontevideoFacades,Display,TEXT("Building %s: %d exterior segments, bounds %s"),*FeatureId,Count,*Bounds.ToString());
}

#if !UE_BUILD_SHIPPING
namespace FacadeCommands
{
	void Run(const TArray<FString>& Args,UWorld* World,int32 Action)
	{
		if(!World) { return; }
		for(TActorIterator<AFacadeBlockoutActor> It(World);It;++It)
		{
			if(Action==0) { It->LogStatus(); }
			else if(Action==3) { if(Args.Num()==1) { It->ShowBuilding(Args[0]); } }
			else
			{
				int32 Style=-1;
				if(Action==2 && (Args.Num()!=1 || !LexTryParseString(Style,*Args[0]) || Style < -1 || Style>3)) { UE_LOG(LogMontevideoFacades,Warning,TEXT("Style requires -1 (seed) or 0 Classic, 1 Modern, 2 Residential, 3 Civic"));return; }
				FString Error;if(!It->Reload(Error,Style)) { UE_LOG(LogMontevideoFacades,Error,TEXT("Reload rejected, old facade retained: %s"),*Error); }
			}
			return;
		}
	}
	FAutoConsoleCommandWithWorldAndArgs Status(TEXT("Montevideo.Facades.Status"),TEXT("Facade counts/styles/timings."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,0);}));
	FAutoConsoleCommandWithWorldAndArgs Reload(TEXT("Montevideo.Facades.Reload"),TEXT("Rebuild from current massing, reset seed styles."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,1);}));
	FAutoConsoleCommandWithWorldAndArgs Style(TEXT("Montevideo.Facades.Style"),TEXT("Temporary style -1(seed), 0..3. No source mutation."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,2);}));
	FAutoConsoleCommandWithWorldAndArgs Building(TEXT("Montevideo.Facades.Building"),TEXT("Highlight source_feature_id for 30 seconds."),FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A,UWorld* W){Run(A,W,3);}));
}
#endif
